import cv2
import face_recognition
import serial
import time
import os
import numpy as np
import requests
import datetime

# --- CONFIGURATION ---
PORT = 'COM7'  # Check your ESP32's actual COM port
BAUD_RATE = 115200
BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN_HERE"
CHAT_ID = "YOUR_TELEGRAM_CHAT_ID_HERE"

# Initialize Serial & Camera
try:
    ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
    time.sleep(2)
except Exception as e:
    print(f"Error opening serial port {PORT}: {e}")
    exit()

cap = cv2.VideoCapture(0)

if not os.path.exists("captures"):
    os.makedirs("captures")

# --- TELEGRAM FUNCTIONS ---
def send_msg(text):
    if BOT_TOKEN == "YOUR_TELEGRAM_BOT_TOKEN_HERE": return
    try:
        requests.post(f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage",
                      data={"chat_id": CHAT_ID, "text": text}, timeout=5)
    except:
        print("Telegram Message error")

def send_img(photo_path, caption=""):
    if BOT_TOKEN == "YOUR_TELEGRAM_BOT_TOKEN_HERE": return
    try:
        with open(photo_path, "rb") as ph:
            requests.post(f"https://api.telegram.org/bot{BOT_TOKEN}/sendPhoto",
                          files={"photo": ph},
                          data={"chat_id": CHAT_ID, "caption": caption}, timeout=5)
    except:
        print("Telegram Image error")

# --- LOAD KNOWN FACES ---
known_encodings = []
known_names = []

if not os.path.exists("known_faces"):
    os.makedirs("known_faces")
    print("Please add image files to the 'known_faces' folder.")

for f in os.listdir("known_faces"):
    if f.lower().endswith(('.png', '.jpg', '.jpeg')):
        img = face_recognition.load_image_file(f"known_faces/{f}")
        enc = face_recognition.face_encodings(img)
        if enc:
            known_encodings.append(enc[0])
            known_names.append(f.split('.')[0])

print("Loaded known faces:", known_names)

# --- MAIN LOOP ---
while True:
    ret, frame = cap.read()
    if not ret: continue

    if ser.in_waiting:
        msg = ser.readline().decode().strip()

        if msg == "MOTION":
            print("Motion detected! Scanning for faces...")
            time.sleep(2)

            best_frame = None
            best_score = 0

            # Capture a few frames and find the sharpest one
            for _ in range(5):
                ret, f = cap.read()
                if not ret: continue

                gray = cv2.cvtColor(f, cv2.COLOR_BGR2GRAY)
                score = cv2.Laplacian(gray, cv2.CV_64F).var()

                if score > best_score:
                    best_score = score
                    best_frame = f
                time.sleep(0.05)

            frame = best_frame
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            locs = face_recognition.face_locations(rgb)
            encs = face_recognition.face_encodings(rgb, locs)

            name = "UNKNOWN"
            conf = 0
            color = (0, 0, 255)

            # Process detected faces
            for (top, right, bottom, left), enc in zip(locs, encs):
                matches = face_recognition.compare_faces(known_encodings, enc)
                dist = face_recognition.face_distance(known_encodings, enc)

                if len(dist) > 0:
                    i = np.argmin(dist)
                    if matches[i]:
                        name = known_names[i]
                        conf = int((1 - dist[i]) * 100)
                        color = (0, 255, 0)

                # Draw boxes
                cv2.rectangle(frame, (left, top), (right, bottom), color, 2)
                cv2.rectangle(frame, (left, bottom - 30), (right, bottom), color, -1)
                cv2.putText(frame, f"{name} {conf}%", (left + 5, bottom - 8),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

            # Save Images
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            f1 = f"captures/{ts}_1.jpg"
            f2 = f"captures/{ts}_2.jpg"

            cv2.imwrite(f1, frame)
            time.sleep(0.2)
            cv2.imwrite(f2, frame)

            # Communicate with ESP32
            ser.write("CAPTURED\n".encode())
            time.sleep(0.2)

            if name != "UNKNOWN":
                ser.write(f"KNOWN:{name}:{conf}\n".encode())
                send_msg(f"Hi {name}, welcome home!\nConfidence: {conf}%")
            else:
                ser.write("UNKNOWN\n".encode())
                send_msg("Intruder detected!")
                send_img(f1, "Intruder Alert")
                send_img(f2, "Secondary capture")

    cv2.imshow("AI Security feed", frame)
    if cv2.waitKey(1) == 27: # Press 'ESC' to exit
        break

cap.release()
cv2.destroyAllWindows()
ser.close()
