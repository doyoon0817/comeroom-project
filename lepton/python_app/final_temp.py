import time
import cv2
import numpy as np
from ultralytics import YOLO

from read_frame import get_frame  # rgb_frame, raw_frame

# YOLO person 모델
model = YOLO("yolov8n.pt")

WIDTH = 160
HEIGHT = 120

DETECTION_INTERVAL = 1.0
last_det_time = 0.0

# 보정 상수 (radiometric 온도에서 몇 도를 뺄지)
# 예: radiometric 36.8°C일 때 여기서 0.8 빼면 36.0°C 출력
SKIN_OFFSET = -0.8   # 필요하면 -0.5, 0.0 이런 식으로 조금씩 조정 가능

# mouse
mouse_x, mouse_y = -1, -1


def mouse_event(event, x, y, flags, param):
    global mouse_x, mouse_y
    if event == cv2.EVENT_MOUSEMOVE:
        mouse_x, mouse_y = x, y


def raw_to_celsius(raw_val: float) -> float:
    """Lepton radiometric: raw/100 - 273.15"""
    return raw_val * 0.01 - 273.15


def stretch_raw_to_grayscale(raw_frame):
    """RAW16 → 자동 대비조정 8bit grayscale"""
    valid = raw_frame[raw_frame > 0]

    if valid.size == 0:
        return np.zeros_like(raw_frame, dtype=np.uint8)

    raw_min = np.min(valid)
    raw_max = np.max(valid)

    if raw_max - raw_min < 10:
        raw_max = raw_min + 10

    stretched = (raw_frame - raw_min) * (255.0 / (raw_max - raw_min))
    stretched = np.clip(stretched, 0, 255)

    return stretched.astype(np.uint8)


def distance(p1, p2):
    if p1 is None or p2 is None:
        return 0.0
    return ((p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2) ** 0.5


def iou(boxA, boxB):
    """박스 간 IoU (사람 매칭용)"""
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = min(boxA[2], boxB[2])
    yB = min(boxA[3], boxB[3])

    interW = max(0, xB - xA)
    interH = max(0, yB - yA)
    interArea = interW * interH

    if interArea == 0:
        return 0.0

    boxAArea = (boxA[2] - boxA[0]) * (boxA[3] - boxA[1])
    boxBArea = (boxB[2] - boxB[0]) * (boxB[3] - boxB[1])

    return interArea / (boxAArea + boxBArea - interArea + 1e-6)


def find_face_center(raw_frame, box, prev_center=None,
                     grid_rows=6, grid_cols=4):
    """
    YOLO 박스 안에서 '가장 뜨거운 영역'을 찾아 얼굴 중심 추정.
    - 박스 세로 10%~80% 구간만 사용 (다리 제외)
    - grid로 나눠서 각 칸 평균 온도 계산
    - 평균 온도가 가장 높은 칸의 중심을 얼굴 중심으로 사용
    - 이전 중심(prev_center)과 살짝 섞어서 위치 튐 줄임
    """
    x1, y1, x2, y2 = box

    x1 = max(0, min(WIDTH - 1, x1))
    x2 = max(0, min(WIDTH - 1, x2))
    y1 = max(0, min(HEIGHT - 1, y1))
    y2 = max(0, min(HEIGHT - 1, y2))

    if x2 <= x1 or y2 <= y1:
        return prev_center

    H = y2 - y1

    # 검색 영역: 위 10% ~ 아래 80% (머리~상체 위주)
    sy1 = int(y1 + H * 0.10)
    sy2 = int(y1 + H * 0.80)
    sx1 = x1
    sx2 = x2

    sy1 = max(0, min(HEIGHT - 1, sy1))
    sy2 = max(0, min(HEIGHT, sy2))
    sx1 = max(0, min(WIDTH - 1, sx1))
    sx2 = max(0, min(WIDTH, sx2))

    if sy2 <= sy1 or sx2 <= sx1:
        return prev_center

    cell_h = (sy2 - sy1) / grid_rows
    cell_w = (sx2 - sx1) / grid_cols

    best_mean = None
    best_center = None

    for r in range(grid_rows):
        cy1 = int(sy1 + r * cell_h)
        cy2 = int(sy1 + (r + 1) * cell_h)
        for c in range(grid_cols):
            cx1 = int(sx1 + c * cell_w)
            cx2 = int(sx1 + (c + 1) * cell_w)

            patch = raw_frame[cy1:cy2, cx1:cx2]
            if patch.size == 0:
                continue

            valid = patch[patch > 0]
            if valid.size == 0:
                continue

            mean_val = float(valid.mean())
            if (best_mean is None) or (mean_val > best_mean):
                best_mean = mean_val
                center_x = (cx1 + cx2) // 2
                center_y = (cy1 + cy2) // 2
                best_center = (center_x, center_y)

    if best_center is None:
        return prev_center

    # 이전 중심과 위치 smoothing (좌표 튐 방지)
    if prev_center is not None:
        px, py = prev_center
        cx, cy = best_center
        alpha_pos = 0.5  # 0.3~0.6 사이 조절 가능
        sm_x = int(alpha_pos * cx + (1 - alpha_pos) * px)
        sm_y = int(alpha_pos * cy + (1 - alpha_pos) * py)
        return (sm_x, sm_y)
    else:
        return best_center


def compute_face_temp_from_center(raw_frame, center, radius=10, hot_ratio=0.08):
    """
    얼굴 중심 좌표 주변 작은 ROI에서 온도 계산.
    - 중심 주변 (radius) 영역 추출
    - 0 초과 픽셀만 사용
    - 상위 hot_ratio 픽셀만 선택
    - median 기반 필터 후 평균 → 섭씨 변환
    - 환경에 맞게 조정한 피부 보정(temp + SKIN_OFFSET) 적용
    """
    if center is None:
        return None

    cx, cy = center

    x_min = max(0, cx - radius)
    x_max = min(WIDTH, cx + radius + 1)
    y_min = max(0, cy - radius)
    y_max = min(HEIGHT, cy + radius + 1)

    roi = raw_frame[y_min:y_max, x_min:x_max]
    if roi.size == 0:
        return None

    valid = roi[roi > 0]
    if valid.size == 0:
        return None

    flat_sorted = np.sort(valid)
    k = max(1, int(len(flat_sorted) * hot_ratio))
    hottest_vals = flat_sorted[-k:]

    # median 기반 안정화
    if len(hottest_vals) > 6:
        med = np.median(hottest_vals)
        hottest_vals = hottest_vals[hottest_vals >= med]

    avg_raw = float(hottest_vals.mean())
    temp_c = raw_to_celsius(avg_raw)  # radiometric skin

    # ⭐ 환경 맞춘 피부 보정: radiometric + SKIN_OFFSET
    skin_temp = temp_c + SKIN_OFFSET

    return skin_temp


def main():
    global last_det_time, mouse_x, mouse_y

    window_name = "Thermal YOLO (Multi-Person Calibrated)"
    cv2.namedWindow(window_name)
    cv2.namedWindow("People Temperatures")
    cv2.setMouseCallback(window_name, mouse_event)

    # id → { "box":(x1,y1,x2,y2), "center":(cx,cy), "temp":float }
    people = {}
    next_person_id = 1

    while True:
        rgb_frame, raw_frame = get_frame()

        raw_8bit = stretch_raw_to_grayscale(raw_frame)
        gray_3ch = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2RGB)

        now = time.time()

        # YOLO 감지 주기적으로 수행
        if now - last_det_time > DETECTION_INTERVAL:

            results = model(gray_3ch, imgsz=160, conf=0.25)

            detected_boxes = []

            if len(results) > 0 and results[0].boxes is not None:
                boxes = results[0].boxes
                xyxy = boxes.xyxy.cpu().numpy()
                cls = boxes.cls.cpu().numpy()

                ids = np.where(cls == 0)[0]  # person class
                for i in ids:
                    x1, y1, x2, y2 = map(int, xyxy[i])
                    detected_boxes.append((x1, y1, x2, y2))

            # 이전 people과 IoU 기반 매칭해서 id 유지
            new_people = {}

            for box in detected_boxes:
                best_id = None
                best_iou = 0.0

                for pid, st in people.items():
                    if st["box"] is None:
                        continue
                    v = iou(box, st["box"])
                    if v > 0.1 and v > best_iou:
                        best_iou = v
                        best_id = pid

                if best_id is None:
                    pid = next_person_id
                    next_person_id += 1
                    prev_center = None
                    prev_temp = None
                else:
                    pid = best_id
                    prev_center = people[pid]["center"]
                    prev_temp = people[pid]["temp"]

                # 얼굴 중심 찾기
                center = find_face_center(raw_frame, box, prev_center=prev_center)

                # 움직임 크기에 따라 hot_ratio 조절 (많이 움직이면 더 보수적으로)
                move = distance(prev_center, center)
                hot_ratio = 0.05 if move > 5 else 0.08

                frame_temp = compute_face_temp_from_center(
                    raw_frame, center,
                    radius=10,
                    hot_ratio=hot_ratio
                )

                # 프레임 기반 smoothing
                smooth = prev_temp
                if frame_temp is not None:
                    alpha = 0.3  # 반응성/안정성 타협
                    if smooth is None:
                        smooth = frame_temp
                    else:
                        smooth = alpha * frame_temp + (1 - alpha) * smooth

                new_people[pid] = {
                    "box": box,
                    "center": center,
                    "temp": smooth
                }

            people = new_people
            last_det_time = now

        # 시각화
        vis = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2BGR)

        sorted_ids = sorted(people.keys())

        # 메인 화면: 각 사람 박스/라벨
        for idx, pid in enumerate(sorted_ids, start=1):
            st = people[pid]
            x1, y1, x2, y2 = st["box"]
            center = st["center"]
            temp = st["temp"]

            # 사람 박스
            cv2.rectangle(vis, (x1, y1), (x2, y2), (255, 255, 255), 1)

            # 얼굴 중심
            if center is not None:
                cv2.circle(vis, center, 3, (255, 255, 255), -1)

            # 박스 위에 PersonX: YY.YC 표시
            label_y = max(10, y1 - 5)
            if temp is not None:
                text = f"Person{idx}: {temp:.2f}C"
            else:
                text = f"Person{idx}: --.-C"

            cv2.putText(vis, text,
                        (x1, label_y),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.45, (255, 255, 255), 1)

        # 오른쪽 온도 리스트 창
        temp_window = np.zeros((350, 260, 3), dtype=np.uint8)
        if not sorted_ids:
            cv2.putText(temp_window, "No person detected",
                        (5, 30),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.6, (255, 255, 255), 1)
        else:
            for idx, pid in enumerate(sorted_ids, start=1):
                st = people[pid]
                temp = st["temp"]
                if temp is not None:
                    line = f"Person{idx}: {temp:.2f}C"
                else:
                    line = f"Person{idx}: --.-C"
                y = 25 + (idx - 1) * 22
                cv2.putText(temp_window, line,
                            (5, y),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.6, (255, 255, 255), 1)

        cv2.imshow("People Temperatures", temp_window)

        # 마우스 온도 (보정 포함 디버그)
        if 0 <= mouse_x < WIDTH and 0 <= mouse_y < HEIGHT:
            raw_val = int(raw_frame[mouse_y, mouse_x])
            if raw_val > 0:
                t_radiometric = raw_to_celsius(raw_val)
                t_skin = t_radiometric + SKIN_OFFSET
                cv2.putText(vis, f"{t_skin:.2f}C",
                            (mouse_x + 6, mouse_y - 6),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.4, (255, 255, 255), 1)
                cv2.circle(vis, (mouse_x, mouse_y), 2, (255, 255, 255), -1)

        cv2.imshow(window_name, vis)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
