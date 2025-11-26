#define BUS_NUMBER "2221" 

#include <NimBLEDevice.h>
#include <Wire.h>       
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// === [AUDIO] MP3 라이브러리 (ESP8266Audio 설치 필요) ===
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "sound_data.h" // 우리가 만든 MP3 데이터 (반드시 탭에 있어야 함!)

// ====== 핀 설정 (사용자 최종 검증 완료) ======
#define LED_PIN    3   // LED (GPIO 3)
#define NUM_PIXELS 30
#define I2C_SDA    4   // LCD SDA
#define I2C_SCL    5   // LCD SCL
#define I2S_BCLK   2   // AMP BCLK
#define I2S_LRC    10  // AMP LRC
#define I2S_DIN    11  // AMP DIN

// ====== 객체 생성 ======
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// 오디오 객체
AudioFileSourcePROGMEM *file = NULL;
AudioGeneratorMP3 *mp3 = NULL;
AudioOutputI2S *out = NULL;

// ====== 상태 변수 ======
bool isLedOn = false;
unsigned long ledStartTime = 0;
bool isMessageDisplaying = false; 
unsigned long messageStartTime = 0; 

// BLE 변수
NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic;
int connectedDevices = 0;

#define SERVICE_UUID           "12345678-1234-1234-1234-123456789ABC"
#define CHARACTERISTIC_UUID_RX "12345678-1234-1234-1234-123456789ABD"
#define CHARACTERISTIC_UUID_TX "12345678-1234-1234-1234-123456789ABE"

String deviceName = "BF_DREAM_" + String(BUS_NUMBER);

// --- LED 함수 ---
void setBlue() {
  for(int i=0; i<NUM_PIXELS; i++) pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  pixels.show();
}
void setOff() {
  pixels.clear();
  pixels.show();
}

// --- 오디오 재생 함수 (MP3) ---
void playMP3() {
  // 오디오 객체가 제대로 생성되었는지 확인
  if (mp3 == NULL || file == NULL || out == NULL) {
    Serial.println("Error: Audio objects not initialized!");
    return;
  }

  // 기존 재생 중이면 중지
  if (mp3->isRunning()) mp3->stop();
  
  // 파일 포인터 처음으로 리셋 (mom_mp3 사용)
  file->open(mom_mp3, sizeof(mom_mp3));
  mp3->begin(file, out);
  Serial.println("MP3 Playback Started");
}

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
      connectedDevices = pServer->getConnectedCount();
      Serial.println("BLE Device Connected");
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
      connectedDevices = pServer->getConnectedCount();
      Serial.println("BLE Device Disconnected - Restart Advertising");
      NimBLEDevice::startAdvertising();
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        Serial.print("Received: "); Serial.println(rxValue.c_str());
        
        if(rxValue == "COURTESY_SEAT") {
            // 1. MP3 재생
            playMP3();

            // 2. LCD 표시
            isMessageDisplaying = true;
            messageStartTime = millis(); 
            lcd.setCursor(0, 2); lcd.print("COURTESY_SEAT Signal");
            lcd.setCursor(0, 3); lcd.print("Voice Alert...      ");

            // 3. LED 켜기
            isLedOn = true;
            ledStartTime = millis();
            setBlue(); 

            pTxCharacteristic->setValue("ACK");
            pTxCharacteristic->notify(connInfo.getConnHandle());
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  delay(2000); // 전원 안정화 대기 (중요!)
  Serial.println("\n=== System Booting... ===");
  Serial.print("Free Heap at start: "); Serial.println(ESP.getFreeHeap());

  // 1. BLE 설정 (기본 설정만)
  Serial.println("1. Configuring BLE...");
  NimBLEDevice::init(deviceName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  
  // 2. LED 초기화
  Serial.println("2. Initializing LED...");
  pixels.begin(); pixels.setBrightness(50); setOff();

  // 3. LCD 초기화
  Serial.println("3. Initializing LCD...");
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.setCursor(0, 0); lcd.print("BUS_NUM: "); lcd.print(BUS_NUMBER);
  lcd.setCursor(0, 1); lcd.print("System Ready");

  // 4. 오디오 초기화 (mom_mp3 사용)
  Serial.println("4. Initializing Audio...");
  file = new AudioFileSourcePROGMEM(mom_mp3, sizeof(mom_mp3));
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  mp3 = new AudioGeneratorMP3();
  
  Serial.print("Free Heap after init: "); Serial.println(ESP.getFreeHeap());
  
  // 5. BLE 광고 시작
  Serial.println("5. Starting BLE Advertising...");
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advertisementData;
  advertisementData.setCompleteServices(NimBLEUUID(SERVICE_UUID)); 
  advertisementData.setName(deviceName.c_str());
  pAdvertising->setAdvertisementData(advertisementData);
  
  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(deviceName.c_str());
  pAdvertising->setScanResponseData(scanResponseData);
  
  pAdvertising->start();
  
  Serial.println("=== Setup Complete ===");
}

void loop() {
  // MP3 재생 루프
  if (mp3 != NULL && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("MP3 Playback Finished");
    }
  }

  // LED 타이머
  if (isLedOn && (millis() - ledStartTime > 5000)) {
    isLedOn = false;
    setOff();
  }

  // LCD 타이머
  if (isMessageDisplaying && (millis() - messageStartTime > 5000)) {
    isMessageDisplaying = false;
    lcd.setCursor(0, 2); lcd.print("                    ");
    lcd.setCursor(0, 3); lcd.print("                    ");
  }
  
  // BLE 연결 상태 업데이트
  static int prevDevices = -1;
  if (connectedDevices != prevDevices) {
      lcd.setCursor(0, 1);
      lcd.print("Devices Connected: ");
      lcd.print(connectedDevices);
      lcd.print("  ");
      prevDevices = connectedDevices;
  }

  // 동작 확인용 심장박동
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 1000) {
      lastHeartbeat = millis();
      // Serial.print("."); 
  }
}