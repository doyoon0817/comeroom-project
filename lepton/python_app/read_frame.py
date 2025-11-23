# read_frame.py
import numpy as np
import mmap

WIDTH = 160
HEIGHT = 120

# Your shared memory file names
RAW_FILE = "/dev/shm/lepton_raw"
RGB_FILE = "/dev/shm/lepton_frame"

def get_raw16_frame():
    with open(RAW_FILE, "rb") as f:
        mm = mmap.mmap(f.fileno(), WIDTH * HEIGHT * 2, access=mmap.ACCESS_READ)
        data = mm.read(WIDTH * HEIGHT * 2)
        mm.close()
    frame = np.frombuffer(data, dtype=np.uint16).reshape((HEIGHT, WIDTH))
    return frame

def get_rgb_frame():
    with open(RGB_FILE, "rb") as f:
        mm = mmap.mmap(f.fileno(), WIDTH * HEIGHT * 3, access=mmap.ACCESS_READ)
        data = mm.read(WIDTH * HEIGHT * 3)
        mm.close()
    frame = np.frombuffer(data, dtype=np.uint8).reshape((HEIGHT, WIDTH, 3))
    return frame

def get_frame():
    return get_rgb_frame(), get_raw16_frame()
