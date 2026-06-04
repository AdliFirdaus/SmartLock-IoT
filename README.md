# SmartLock — Biometric & Keypad Access Control System

IoT-based smart door lock system combining facial recognition and PIN keypad authentication.

## System Architecture

- **Face Recognition** — Python + DeepFace + Webcam
- **Hardware Simulation** — Wokwi (ESP32 + Keypad + LCD + Servo + LED + Buzzer)
- **MQTT Broker** — broker.emqx.io (port 1883)
- **Dashboard** — Node-RED (localhost:1880/ui)

## MQTT Topics

| Topic | Publisher | Subscriber |
|---|---|---|
| smartlock/face | Python Script | ESP32 Wokwi |
| smartlock/status | ESP32 Wokwi | Node-RED |
| smartlock/override | Node-RED | ESP32 Wokwi |

## Setup Instructions

### 1. Python Face Recognition
```bash
pip install deepface tf-keras opencv-python paho-mqtt
python smartlock_face.py
```

### 2. Wokwi Simulation
Open project at: https://wokwi.com/projects/465608953529417729

### 3. Node-RED Dashboard
```bash
node-red
```
Import `smartlock_dashboard_final.json` into Node-RED.
Open dashboard at: http://localhost:1880/ui

## Login Credentials

| Role | Username | Password |
|---|---|---|
| Admin | admin | 12345 |
| User | user | 1234 |

## Team Members

| Name | Student ID | Role |
|---|---|---|
| [Nama 1] | [ID] | [Role] |
| [Nama 2] | [ID] | [Role] |
| [Nama 3] | [ID] | [Role] |
| [Nama 4] | [ID] | [Role] |

## Subject
Internet of Things (IoT) — UniKL
