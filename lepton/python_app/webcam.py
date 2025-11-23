import cv2

cap = cv2.VideoCapture(17)

while True:
    ret, frame = cap.read()
    if not ret:
        print("No frame")
        continue

    cv2.imshow("Lepton Webcam", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
