import time
import cv2
import numpy as np
from ultralytics import YOLO

from read_frame import get_frame  # rgb_frame, raw_frame

# YOLO 모델 (person detection)
model = YOLO("yolov8n.pt")

WIDTH = 160
HEIGHT = 120

DETECTION_INTERVAL = 1.0
last_det_time = 0.0

# detection results
person_box = None
head_temp_c = None

# mouse position
mouse_x, mouse_y = -1, -1


def mouse_event(event, x, y, flags, param):
    """마우스 포인터 좌표 저장"""
    global mouse_x, mouse_y
    if event == cv2.EVENT_MOUSEMOVE:
        mouse_x, mouse_y = x, y


def raw_to_celsius(raw_val: float) -> float:
    """Lepton radiometric 변환: raw/100 - 273.15"""
    return raw_val * 0.01 - 273.15


def find_head_hotspot(raw_frame, box, head_ratio=0.6, radius=2):
    """사람 박스 상단부분에서 가장 뜨거운 지점을 찾아 온도로 변환"""
    x1, y1, x2, y2 = box

    x1 = max(0, min(WIDTH - 1, x1))
    x2 = max(0, min(WIDTH - 1, x2))
    y1 = max(0, min(HEIGHT - 1, y1))
    y2 = max(0, min(HEIGHT - 1, y2))

    if x2 <= x1 or y2 <= y1:
        return None

    h = y2 - y1
    head_y2 = y1 + int(h * head_ratio)
    if head_y2 <= y1:
        head_y2 = y2

    roi = raw_frame[y1:head_y2, x1:x2]
    if roi.size == 0:
        return None

    # 0 값 제외
    mask = roi > 0
    if not mask.any():
        return None

    hot_idx = np.argmax(roi)
    ry, rx = divmod(hot_idx, roi.shape[1])

    hot_x = x1 + rx
    hot_y = y1 + ry

    # 주변 평균
    x_min = max(0, hot_x - radius)
    x_max = min(WIDTH, hot_x + radius + 1)
    y_min = max(0, hot_y - radius)
    y_max = min(HEIGHT, hot_y + radius + 1)

    neigh = raw_frame[y_min:y_max, x_min:x_max]
    valid = neigh[neigh > 0]

    if valid.size == 0:
        return None

    avg_raw = float(valid.mean())
    return raw_to_celsius(avg_raw)


def stretch_raw_to_grayscale(raw_frame):
    """RAW16 → 밝기/대비 자동 보정된 8bit grayscale로 변환"""
    valid_pixels = raw_frame[raw_frame > 0]

    if valid_pixels.size == 0:
        return np.zeros_like(raw_frame, dtype=np.uint8)

    raw_min = np.min(valid_pixels)
    raw_max = np.max(valid_pixels)

    # 너무 범위가 작으면 대비 강제 확장
    if raw_max - raw_min < 10:
        raw_max = raw_min + 10

    stretched = ((raw_frame - raw_min) * (255.0 / (raw_max - raw_min)))
    stretched = np.clip(stretched, 0, 255)

    return stretched.astype(np.uint8)


def main():
    global last_det_time, person_box, head_temp_c

    window_name = "Thermal YOLO (Grayscale Enhanced)"
    cv2.namedWindow(window_name)
    cv2.setMouseCallback(window_name, mouse_event)

    while True:
        # 1) SHM에서 프레임 가져오기 (C++에서 계속 업데이트 중)
        rgb_frame, raw_frame = get_frame()

        # 2) RAW16 → 자동 대비 조정 → 8bit grayscale
        raw_8bit = stretch_raw_to_grayscale(raw_frame)

        # 3) YOLO는 3채널이 필요하므로 1채널을 3채널로 확장
        gray_3ch = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2RGB)

        # 4) 1초에 한 번 YOLO detection
        now = time.time()
        if now - last_det_time > DETECTION_INTERVAL:

            person_box = None
            head_temp_c = None

            results = model(gray_3ch, imgsz=160, conf=0.25)

            if len(results) > 0 and results[0].boxes is not None:
                boxes = results[0].boxes
                xyxy = boxes.xyxy.cpu().numpy()
                cls = boxes.cls.cpu().numpy()

                ids = np.where(cls == 0)[0]  # person만
                if ids.size > 0:
                    pxy = xyxy[ids]
                    areas = (pxy[:, 2] - pxy[:, 0]) * (pxy[:, 3] - pxy[:, 1])
                    best = np.argmax(areas)
                    x1, y1, x2, y2 = map(int, pxy[best])
                    person_box = (x1, y1, x2, y2)

                    # 머리 온도 계산
                    head_temp_c = find_head_hotspot(raw_frame, person_box)

            last_det_time = now

        # 5) 시각화용 grayscale → BGR로 변환
        vis = cv2.cvtColor(raw_8bit, cv2.COLOR_GRAY2BGR)

        # 6) 마우스 온도 표시
        if 0 <= mouse_x < WIDTH and 0 <= mouse_y < HEIGHT:
            raw_val = int(raw_frame[mouse_y, mouse_x])
            if raw_val > 0:
                temp_c = raw_to_celsius(raw_val)
                cv2.putText(vis, f"{temp_c:.2f}C",
                            (mouse_x + 6, mouse_y - 6),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255,255,255),1)
                cv2.circle(vis, (mouse_x, mouse_y), 2, (255,255,255), -1)

        # 7) 사람 박스 그리기
        if person_box is not None:
            x1, y1, x2, y2 = person_box
            cv2.rectangle(vis, (x1, y1), (x2, y2), (255,255,255), 1)

        # 8) 오른쪽 상단에 온도 항상 표시
        if head_temp_c is not None:
            cv2.putText(vis, f"Temp: {head_temp_c:.2f}C",
                        (5, 15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        (255,255,255), 1)
        else:
            cv2.putText(vis, "No Detection",
                        (5, 15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        (255,255,255), 1)

        cv2.imshow(window_name, vis)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
