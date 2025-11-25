import os
import mmap
import time

import numpy as np
import cv2

WIDTH = 160   # Lepton 3.5
HEIGHT = 120
CHANNELS = 3
SHM_NAME = "/dev/shm/lepton_frame"

FRAME_SIZE = WIDTH * HEIGHT * CHANNELS

def main():
    print("Waiting for shared memory...")

    # C++에서 shm_open으로 만든 /lepton_frame은
    # 리눅스에서 /dev/shm/lepton_frame 파일로 보인다.
    while not os.path.exists(SHM_NAME):
        time.sleep(0.05)

    fd = os.open(SHM_NAME, os.O_RDONLY)
    mm = mmap.mmap(fd, FRAME_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)

    try:
        while True:
            mm.seek(0)
            buf = mm.read(FRAME_SIZE)

            if len(buf) != FRAME_SIZE:
                # 아직 C++에서 완전히 쓰기 전일 수 있음 → 스킵
                continue

            frame = np.frombuffer(buf, dtype=np.uint8)
            frame = frame.reshape((HEIGHT, WIDTH, CHANNELS))

            cv2.imshow("Lepton 3.5 via Shared Memory", frame)

            if cv2.waitKey(1) & 0xFF == 27:  # ESC
                break
    finally:
        mm.close()
        os.close(fd)
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
