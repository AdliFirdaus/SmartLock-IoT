#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>

// ── Config ───────────────────────────────────
const char* ssid        = "Wokwi-GUEST";
const char* password    = "";
const char* mqtt_server = "broker.emqx.io";

// ── Pins ─────────────────────────────────────
#define PIN_BUZZER    23
#define PIN_LED_GREEN 18
#define PIN_LED_RED   19
#define PIN_SERVO     4

// ── Objects ──────────────────────────────────
WiFiClient    espClient;
PubSubClient  client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo         doorServo;

// ── Keypad ───────────────────────────────────
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── State ─────────────────────────────────────
String inputPIN  = "";
String secretPIN = "1234";
int    state     = 0;
// 0: Waiting Face
// 1: Face OK — show message
// 2: Enter PIN
// 3: Granted — door open
// 4: LOCKOUT

int           failCount  = 0;
unsigned long timer      = 0;
unsigned long lockTimer  = 0;
bool          doorOpen   = false;

// ── LCD Helper ────────────────────────────────
void setLcd(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  while (line1.length() < 16) line1 += " ";
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  while (line2.length() < 16) line2 += " ";
  lcd.print(line2.substring(0, 16));
}

// ── Buzzer ────────────────────────────────────
void beepOK() {
  // Satu beep panjang — access granted
  digitalWrite(PIN_BUZZER, HIGH);
  delay(500);
  digitalWrite(PIN_BUZZER, LOW);
}

void beepFail() {
  // Tiga beep pendek — access denied
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
}

void beepLockout() {
  // Lima beep — lockout
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
}

// ── Servo / Door ──────────────────────────────
void openDoor() {
  doorServo.write(90);  // Pintu buka — servo putar 90 darjah
  doorOpen = true;
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED,   LOW);
  Serial.println("DOOR OPENED");
}

void closeDoor() {
  doorServo.write(0);   // Pintu tutup — servo balik 0 darjah
  doorOpen = false;
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED,   LOW);
  Serial.println("DOOR CLOSED");
}

void denyAccess() {
  digitalWrite(PIN_LED_RED,   HIGH);
  digitalWrite(PIN_LED_GREEN, LOW);
  beepFail();
  delay(1000);
  digitalWrite(PIN_LED_RED, LOW);
}

// ── MQTT Callback ─────────────────────────────
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();

  Serial.println("MQTT Received: " + message);

  // Terima FACE_OK dari Python
  if (message.indexOf("FACE_OK") >= 0 && state == 0) {
    state = 1;
    timer = millis();

    // Extract nama
    String name = "USER";
    if (message.indexOf(":") >= 0) {
      name = message.substring(message.indexOf(":") + 1);
    }

    setLcd("Face ID OK!", name);
    digitalWrite(PIN_LED_GREEN, HIGH);
    beepOK();
    delay(300);
    digitalWrite(PIN_LED_GREEN, LOW);
  }

  // Terima override dari dashboard
  if (message.indexOf("unlock") >= 0) {
    openDoor();
    setLcd("OVERRIDE", "Door Unlocked");
    client.publish("smartlock/status", "UNLOCKED");
    timer = millis();
    state = 3;
  }

  if (message == "lock" || message.indexOf("\"action\":\"lock\"") >= 0) {
    closeDoor();
    setLcd("OVERRIDE", "Door Locked");
    client.publish("smartlock/status", "LOCKED");
    state = 0;
    inputPIN = "";
  }

  if (message.indexOf("reset") >= 0) {
    failCount = 0;
    state     = 0;
    inputPIN  = "";
    closeDoor();
    setLcd("System Reset", "Waiting Face ID");
    client.publish("smartlock/status", "LOCKED");
    state = 0;
    setLcd("Waiting Face ID", "Stand in front");
  }
}

// ── WiFi & MQTT Connect ───────────────────────
void connectWifi() {
  Serial.print("WiFi connecting");
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  else
    Serial.println("\nWiFi failed");
}

void connectMqtt() {
  int tries = 0;
  while (!client.connected() && tries < 5) {
    Serial.print("MQTT connecting...");
    if (client.connect("ESP32_SmartLock_Wokwi")) {
      Serial.println("OK");
      client.subscribe("smartlock/face");
      client.subscribe("smartlock/override");
    } else {
      Serial.print("failed: ");
      Serial.println(client.state());
      delay(1000); tries++;
    }
  }
}

// ── Setup ─────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  digitalWrite(PIN_BUZZER,    LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED,   LOW);

  // Servo setup
  doorServo.attach(PIN_SERVO);
  doorServo.write(0);  // Start position — door closed

  // LCD
  lcd.init();
  lcd.backlight();
  setLcd("SmartLock v2.0", "Initializing...");

  connectWifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  connectMqtt();

  setLcd("Waiting Face ID", "Stand in front");
  Serial.println("=== SmartLock Ready ===");
}

// ── Loop ──────────────────────────────────────
void loop() {
  // Reconnect MQTT
  if (!client.connected()) connectMqtt();
  client.loop();

  // ── State 1: Face OK — tunjuk 2 saat pastu masuk state 2
  if (state == 1 && (millis() - timer >= 2000)) {
    state = 2;
    inputPIN = "";
    setLcd("Enter PIN:", "Press A=confirm");
  }

  // ── State 2: PIN entry
  if (state == 2) {
    char key = keypad.getKey();
    if (key) {
      Serial.println("Key: " + String(key));

      if (key == 'A') {
        // Confirm PIN
        if (inputPIN == secretPIN) {
          // CORRECT
          state = 3;
          timer = millis();
          setLcd("Access Granted!", "Door Opening...");
          openDoor();
          beepOK();
          client.publish("smartlock/status", "UNLOCKED");
          failCount = 0;

        } else {
          // WRONG PIN
          failCount++;
          Serial.println("Wrong PIN! Fail: " + String(failCount));

          if (failCount >= 3) {
            // LOCKOUT
            state     = 4;
            lockTimer = millis();
            setLcd("LOCKED OUT!", "Wait 30 seconds");
            denyAccess();
            beepLockout();
            digitalWrite(PIN_LED_RED, HIGH);
            client.publish("smartlock/status", "WRONG_PIN");
            Serial.println("LOCKOUT ACTIVATED");

          } else {
            setLcd("Wrong PIN!", "Attempt " + String(failCount) + "/3");
            denyAccess();
            client.publish("smartlock/status", "WRONG_PIN");
            delay(1500);
            inputPIN = "";
            setLcd("Enter PIN:", "Attempt " + String(failCount) + "/3");
          }
        }
        inputPIN = "";

      } else if (key == 'D') {
        // Clear
        inputPIN = "";
        setLcd("Enter PIN:", "Cleared");
        delay(500);
        setLcd("Enter PIN:", "Press A=confirm");

      } else if (key >= '0' && key <= '9') {
        // Digit
        if (inputPIN.length() < 6) {
          inputPIN += key;
          String masked = "";
          for (int i = 0; i < inputPIN.length(); i++) masked += "*";
          setLcd("Enter PIN:", masked);
        }
      }
    }
  }

  // ── State 3: Door open — auto close selepas 5 saat
  if (state == 3 && (millis() - timer >= 5000)) {
    closeDoor();
    inputPIN = "";
    failCount = 0;
    state     = 0;
    setLcd("Door Locked", "Waiting Face ID");
    client.publish("smartlock/status", "LOCKED");
  }

  // ── State 4: Lockout 30 saat
  if (state == 4) {
    unsigned long elapsed   = (millis() - lockTimer) / 1000;
    int           remaining = 30 - elapsed;

    if (remaining <= 0) {
      // Lockout habis
      state     = 0;
      failCount = 0;
      inputPIN  = "";
      digitalWrite(PIN_LED_RED, LOW);
      closeDoor();
      setLcd("System Ready", "Waiting Face ID");
      Serial.println("Lockout expired");
    } else {
      // Update LCD setiap saat
      if (millis() % 1000 < 50) {
        setLcd("LOCKED OUT!", "Wait " + String(remaining) + "s...");
      }
    }
  }
}