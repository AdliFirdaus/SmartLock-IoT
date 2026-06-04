import cv2
import paho.mqtt.client as mqtt
import os
import time
import threading
from deepface import DeepFace
import numpy as np

# ===== CONFIG =====
MQTT_BROKER = "broker.emqx.io"
TOPIC_FACE = "smartlock/face"
FACES_FOLDER = "C:/Users/VICTUS/Downloads/SEM 5/IOT/smartlock_faces/smartlock_faces"

# ===== MQTT SETUP =====
client = mqtt.Client(client_id="PythonFaceID_Canggih_99")
client.connect(MQTT_BROKER, 1883, 60)
client.loop_start()

# ===== STATE =====
status = "SCANNING"
is_checking = False
last_check = 0
camera_active = True
identified_name = ""

def check_face(frame):
    global is_checking, status, camera_active, identified_name
    temp_path = "temp.jpg"
    cv2.imwrite(temp_path, frame)
    try:
        # Loop melalui semua gambar dalam folder
        for f in os.listdir(FACES_FOLDER):
            res = DeepFace.verify(temp_path, os.path.join(FACES_FOLDER, f), 
                                  model_name="Facenet", enforce_detection=False, silent=True)
            if res["verified"]:
                identified_name = os.path.splitext(f)[0].upper() 
                status = "RECOGNIZED"
                
                
                client.publish(TOPIC_FACE, f"FACE_OK:{identified_name}") 
                
                camera_active = False 
                return
        status = "UNKNOWN"
    except Exception as e:
        status = "SCANNING"
    finally:
        is_checking = False
        if os.path.exists(temp_path): os.remove(temp_path)

# ===== MAIN LOOP =====
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

print("=== SmartLock Pro - System Ready ===")

while camera_active:
    ret, frame = cap.read()
    if not ret: break
    h, w = frame.shape[:2]

   
    
    overlay = frame.copy()
    cv2.rectangle(overlay, (0, 0), (w, h), (0, 0, 0), -1)
    cv2.addWeighted(overlay, 0.3, frame, 0.7, 0, frame)
    
    box_w, box_h = 280, 320
    box_x, box_y = w // 2 - box_w // 2, h // 2 - box_h // 2
    c = (0, 255, 180) # Cyan-green
    
   
    c_len = 30
    thick = 3
    for x, y in [(box_x, box_y), (box_x+box_w, box_y), (box_x, box_y+box_h), (box_x+box_w, box_y+box_h)]:
        dx = -c_len if x > w//2 else c_len
        dy = -c_len if y > h//2 else c_len
        cv2.line(frame, (x, y), (x + dx, y), c, thick)
        cv2.line(frame, (x, y), (x, y + dy), c, thick)


    scan_y = box_y + int((time.time() * 100) % box_h)
    cv2.line(frame, (box_x, scan_y), (box_x + box_w, scan_y), (0, 255, 180), 2)

    # Bars & Text 
    cv2.rectangle(frame, (0, 0), (w, 50), (0, 0, 0), -1)
    cv2.putText(frame, "SMARTLOCK SECURITY", (20, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.7, c, 1, cv2.LINE_AA)
    
    # Status Teks yang Besar & Berubah Warna
    status_color = (0, 255, 0) if status == "RECOGNIZED" else (0, 0, 255)
    cv2.putText(frame, f"STATUS: {status}", (20, h - 20), cv2.FONT_HERSHEY_DUPLEX, 1.2, status_color, 2, cv2.LINE_AA)

    # Threading Smooth (Setiap 3 saat)
    if not is_checking and time.time() - last_check > 3:
        is_checking = True
        last_check = time.time()
        threading.Thread(target=check_face, args=(frame.copy(),), daemon=True).start()

    cv2.imshow("SECURE ACCESS - HUD", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

# ===== POPS-UP ILLUSTRATION SELEPAS RECOGNIZED =====
if status == "RECOGNIZED":
    print(f"✅ FACE RECOGNIZED: {identified_name}")
    
    
    w_pop, h_pop = 640, 480
    popup = np.zeros((h_pop, w_pop, 3), dtype=np.uint8) # Imej hitam
    
    # 1. ILLUSTRASI PINTU UNLOCKED (Sleek Graphic)
    door_w, door_h = 150, 250
    door_x, door_y = w_pop//2 - door_w//2, h_pop//2 - door_h//2
    # Bingkai Pintu
    cv2.rectangle(popup, (door_x, door_y), (door_x+door_w, door_y+door_h), (0, 255, 180), 2) 
    # Ilustrasi Pintu Buka
    cv2.line(popup, (door_x, door_y), (door_x, door_y+door_h), (0, 255, 0), 4) # Tiang kiri (engsel)
    cv2.line(popup, (door_x+door_w, door_y), (door_x+door_w+50, door_y+30), (0, 255, 0), 4) # Daun pintu buka
    
    # 2. TEKS MESEJ/POPS 
    cv2.putText(popup, "FACE RECOGNIZED!", (door_x, door_y-30), cv2.FONT_HERSHEY_DUPLEX, 0.8, (0, 255, 0), 2, cv2.LINE_AA)
    cv2.putText(popup, f"IDENTIFIED: {identified_name}", (door_x, door_y+door_h+30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1, cv2.LINE_AA)
    
    # 3. ARAHAN PENUTUP 
    cv2.putText(popup, "DOOR UNLOCKED", (w_pop//2-100, h_pop-50), cv2.FONT_HERSHEY_DUPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)
    cv2.putText(popup, "INPUT PIN ON KEYPAD", (w_pop//2-130, h_pop-20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
    
    
    cv2.imshow("SmartLock Pro - Pop-up", popup)
    cv2.waitKey(4000) 

print("Camera Closing Cleanly")
cap.release()
cv2.destroyAllWindows()
client.disconnect()