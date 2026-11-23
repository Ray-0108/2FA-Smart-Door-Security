#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define RST_PIN 9
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, A0};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo doorLock;
const int SERVO_PIN  = A1;
const int BUZZER_PIN = A2;
const int RED_LED    = A3;
const int GREEN_LED  = 0;  // 改用 D0，避開 D1 傳輸資料閃爍

const String AUTHORIZED_UID = "46 DC AC DE";
const String CORRECT_PIN    = "2026";

enum SystemState {
  STATE_IDLE,
  STATE_PIN_INPUT,
  STATE_UNLOCKED,
  STATE_LOCKOUT
};

SystemState currentState = STATE_IDLE;
String inputPin = "";
int failCount = 0;
unsigned long stateStartTime = 0;

// 專屬 RGB 單色切換函式：確保每次只亮一種純色，絕不混色
void setRgbColor(bool redOn, bool greenOn) {
  digitalWrite(RED_LED, redOn ? HIGH : LOW);
  digitalWrite(GREEN_LED, greenOn ? HIGH : LOW);
}

void beep(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(80);
  }
}

void resetToIdle() {
  currentState = STATE_IDLE;
  inputPin = "";
  setRgbColor(false, false); // 全滅
  doorLock.write(0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("2FA Security Sys");
  lcd.setCursor(0, 1);
  lcd.print("Scan Your Card..");
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  setRgbColor(false, false);

  doorLock.attach(SERVO_PIN);
  doorLock.write(0);

  lcd.init();
  lcd.backlight();
  
  Serial.println("{\"event\":\"SYSTEM_BOOT\",\"status\":\"READY\"}");
  resetToIdle();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentState == STATE_IDLE) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String uidStr = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
        uidStr += String(mfrc522.uid.uidByte[i], HEX);
        if (i < mfrc522.uid.size - 1) uidStr += " ";
      }
      uidStr.toUpperCase();
      mfrc522.PICC_HaltA();

      if (uidStr == AUTHORIZED_UID) {
        Serial.println("{\"event\":\"RFID_OK\",\"uid\":\"" + uidStr + "\"}");
        beep(1, 100);
        currentState = STATE_PIN_INPUT;
        inputPin = "";
        stateStartTime = currentMillis;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter PIN (10s):");
        lcd.setCursor(0, 1);
        lcd.print("> ");
      } else {
        failCount++;
        Serial.println("{\"event\":\"RFID_DENIED\",\"uid\":\"" + uidStr + "\",\"fail_count\":" + String(failCount) + "}");
        setRgbColor(true, false); // 純紅燈
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Access Denied!");
        lcd.setCursor(0, 1);
        lcd.print("Unknown Card (" + String(failCount) + "/3)");
        beep(2, 150);
        delay(1200);

        if (failCount >= 3) {
          triggerLockout();
        } else {
          resetToIdle();
        }
      }
    }
  }

  else if (currentState == STATE_PIN_INPUT) {
    if (currentMillis - stateStartTime >= 10000) {
      failCount++;
      Serial.println("{\"event\":\"PIN_TIMEOUT\",\"fail_count\":" + String(failCount) + "}");
      setRgbColor(true, false); // 純紅燈
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Input Timeout!");
      lcd.setCursor(0, 1);
      lcd.print("Warning (" + String(failCount) + "/3)");
      beep(2, 200);
      delay(1200);

      if (failCount >= 3) {
        triggerLockout();
      } else {
        resetToIdle();
      }
      return;
    }

    char key = keypad.getKey();
    if (key) {
      beep(1, 40);
      if (key == '*') {
        inputPin = "";
        lcd.setCursor(0, 1);
        lcd.print(">               ");
      } else if (key >= '0' && key <= '9') {
        inputPin += key;
        lcd.setCursor(2, 1);
        for (int i = 0; i < inputPin.length(); i++) lcd.print("*");

        if (inputPin.length() == 4) {
          delay(200);
          if (inputPin == CORRECT_PIN) {
            failCount = 0;
            currentState = STATE_UNLOCKED;
            stateStartTime = currentMillis;

            Serial.println("{\"event\":\"ACCESS_GRANTED\",\"uid\":\"" + AUTHORIZED_UID + "\",\"auth\":\"2FA_PASS\"}");
            setRgbColor(false, true); // 純綠燈
            doorLock.write(90);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("2FA Verified!");
            lcd.setCursor(0, 1);
            lcd.print("Door Unlocked");
            beep(1, 300);
          } else {
            failCount++;
            Serial.println("{\"event\":\"PIN_ERROR\",\"fail_count\":" + String(failCount) + "}");
            setRgbColor(true, false); // 純紅燈
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Wrong PIN Code!");
            lcd.setCursor(0, 1);
            lcd.print("Attempts: " + String(failCount) + "/3");
            beep(3, 120);
            delay(1200);

            if (failCount >= 3) {
              triggerLockout();
            } else {
              resetToIdle();
            }
          }
        }
      }
    }
  }

  else if (currentState == STATE_UNLOCKED) {
    if (currentMillis - stateStartTime >= 4000) {
      Serial.println("{\"event\":\"AUTO_LOCKED\"}");
      resetToIdle();
    }
  }

  else if (currentState == STATE_LOCKOUT) {
    unsigned long elapsed = (currentMillis - stateStartTime) / 1000;
    if (elapsed >= 30) {
      failCount = 0;
      Serial.println("{\"event\":\"LOCKOUT_RELEASED\"}");
      resetToIdle();
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Wait: ");
      lcd.print(30 - elapsed);
      lcd.print("s       ");
    }
  }
}

void triggerLockout() {
  currentState = STATE_LOCKOUT;
  stateStartTime = millis();
  setRgbColor(true, false); // 純紅燈
  Serial.println("{\"event\":\"SECURITY_LOCKOUT\",\"duration\":30}");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SECURITY LOCKOUT");
  beep(4, 250);
}