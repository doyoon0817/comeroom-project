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

# detection / tracking state
person_box = None
final_temp_c = None
smoothed_temp = None     # 🔥 프레임 단위 체온 smoothing
face_cx, face_cy = None, None  # 🔥 얼굴 중심 추적용

# mouse
mouse_x, mouse_y = -1, -1


def mouse_event(event, x, y, flags, param):
    global mouse_x, mouse_y
    if event == cv2.EVENT_MOUSEMOVE:
        mouse_x, mouse_y = x, y


def raw_to_celsius(raw_val: float) -> float:
    """Lepton radiometric: raw/100 - 273.15"""
    return raw_val * 0.01 - 273.15


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

    # clamp
    x1 = max(0, min(WIDTH - 1, x1))
    x2 = max(0, min(WIDTH - 1, x2))
    y1 = max(0, min(HEIGHT - 1, y1))
    y2 = max(0, min(HEIGHT - 1, y2))

    if x2 <= x1 or y2 <= y1:
        return prev_center

    H = y2 - y1
    W = x2 - x1

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

    region = raw_frame[sy1:sy2, sx1:sx2]
    if region.size == 0:
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
    - 상위 hot_ratio (기본 8%) 픽셀만 선택
    - median 기반 필터 후 평균 → 섭씨 변환
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
    temp_c = raw_to_celsius(avg_raw)

    # 방사율 / 환경 보정용 offset (0.3~0.8 사이에서 튜닝)
    return temp_c + 0.4


def stretch_raw_to_grayscale(raw_frame):
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


def main():
    global last_det_time, person_box, final_temp_c
    global smoothed_temp, face_cx, face_cy

    window_name = "Thermal YOLO (Adaptive Face + Smoothing)"
    cv2.namedWindow(window_name)
    cv2.setMouseCallback(window_name, mouse_event)

    while True:
        rgb_frame, raw_frame = get_frame()

        raw_8bit = stretch_raw_to_grayscale(raw_frame)
        gray_3ch = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2RGB)

        now = time.time()

        if now - last_det_time > DETECTION_INTERVAL:
            person_box = None
            final_temp_c = None

            results = model(gray_3ch, imgsz=160, conf=0.25)

            if len(results) > 0 and results[0].boxes is not None:
                boxes = results[0].boxes
                xyxy = boxes.xyxy.cpu().numpy()
                cls = boxes.cls.cpu().numpy()

                ids = np.where(cls == 0)[0]  # person class
                if ids.size > 0:
                    pxy = xyxy[ids]
                    areas = (pxy[:, 2] - pxy[:, 0]) * (pxy[:, 3] - pxy[:, 1])
                    best = np.argmax(areas)

                    x1, y1, x2, y2 = map(int, pxy[best])
                    person_box = (x1, y1, x2, y2)

                    # 1) 얼굴 중심 후보 찾기 (이전 위치와 섞어서 부드럽게 이동)
                    prev_center = (face_cx, face_cy) if (face_cx is not None and face_cy is not None) else None
                    new_center = find_face_center(raw_frame, person_box, prev_center=prev_center)
                    if new_center is not None:
                        face_cx, face_cy = new_center

                    # 2) 중심 주변에서 온도 계산
                    frame_temp = compute_face_temp_from_center(raw_frame, (face_cx, face_cy))

                    # 3) 프레임 기반 Temporal smoothing
                    if frame_temp is not None:
                        alpha = 0.5  # 프레임 평균 온도용 smoothing
                        if smoothed_temp is None:
                            smoothed_temp = frame_temp
                        else:
                            smoothed_temp = alpha * frame_temp + (1 - alpha) * smoothed_temp

                        final_temp_c = smoothed_temp

            last_det_time = now

        # 시각화
        vis = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2BGR)

        # 사람 박스
        if person_box is not None:
            x1, y1, x2, y2 = person_box
            cv2.rectangle(vis, (x1, y1), (x2, y2), (255, 255, 255), 1)

        # 얼굴 중심 표시
        if face_cx is not None and face_cy is not None:
            cv2.circle(vis, (face_cx, face_cy), 3, (255, 255, 255), -1)

        # 체온 텍스트
        if final_temp_c is not None:
            cv2.putText(vis, f"Temp: {final_temp_c:.2f}C",
                        (5, 15), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (255, 255, 255), 1)
        else:
            cv2.putText(vis, "No Detection",
                        (5, 15), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (255, 255, 255), 1)

        # 마우스 온도 (디버그용)
        if 0 <= mouse_x < WIDTH and 0 <= mouse_y < HEIGHT:
            raw_val = int(raw_frame[mouse_y, mouse_x])
            if raw_val > 0:
                t = raw_to_celsius(raw_val)
                cv2.putText(vis, f"{t:.2f}C",
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
