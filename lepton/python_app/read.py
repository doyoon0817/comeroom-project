import os
import mmap
import time
import numpy as np
import cv2

WIDTH = 160
HEIGHT = 120
RAW_SIZE = WIDTH * HEIGHT * 2

SHM_RAW = "/dev/shm/lepton_raw"

print("Waiting for RAW shared memory...")

while not os.path.exists(SHM_RAW):
    time.sleep(0.1)

fd_raw = os.open(SHM_RAW, os.O_RDONLY)
mm_raw = mmap.mmap(fd_raw, RAW_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)

print("[OK] Connected to /dev/shm/lepton_raw")

# 창 한 번만 생성
cv2.namedWindow("TEMP", cv2.WINDOW_NORMAL)

while True:
    mm_raw.seek(0)
    buf = mm_raw.read(RAW_SIZE)

    raw = np.frombuffer(buf, dtype=np.uint16).reshape((HEIGHT, WIDTH))

    # Radiometry 절대온도 계산
    temp = (raw / 100.0) - 273.15

    # ------------- 핵심 추가 부분 --------------
    max_temp = np.max(temp)              # 최대 온도
    median_temp = np.median(temp)        # 전체 프레임 median 온도
    center_temp = temp[HEIGHT//2, WIDTH//2]  # 중심 점 온도

    print(f"🔥 Max: {max_temp:.2f}°C   |   Median: {median_temp:.2f}°C   |   Center: {center_temp:.2f}°C")
    # ---------------------------------------

    # Heatmap for viewing
    norm = cv2.normalize(temp, None, 0, 255, cv2.NORM_MINMAX)
    norm = norm.astype(np.uint8)
    color = cv2.applyColorMap(norm, cv2.COLORMAP_INFERNO)

    cv2.imshow("TEMP", color)

    if cv2.waitKey(1) == 27:
        break

mm_raw.close()
os.close(fd_raw)
cv2.destroyAllWindows()
