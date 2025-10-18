#include "config.h"

#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN    8
#define NUM_PIXELS 1

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

NimBLEServer *pServer = NULL;
NimBLECharacteristic *pRxCharacteristic;
NimBLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool signalReceived = false;
unsigned long signalTime = 0;

// 버스 번호로 디바이스 이름 자동 생성
String deviceName = "BF_DREAM_" + String(BUS_NUMBER);

void setRed() {
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
}

void setBlue() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
}

void setOff() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();
}

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
      deviceConnected = true;
      Serial.print("iOS 기기가 ");
      Serial.print(BUS_NUMBER);
      Serial.println("번 버스에 연결됨");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
      deviceConnected = false;
      Serial.println("iOS 기기 연결 해제");
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) {
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.print("수신한 데이터: ");
        Serial.println(rxValue.c_str());

        if(rxValue == "COURTESY_SEAT") {
          signalReceived = true;
          signalTime = millis();
          
          setBlue();
          Serial.print(BUS_NUMBER);
          Serial.println("번 버스 배려석 알림 수신");
          Serial.println("파란불 점등");
          
          pTxCharacteristic->setValue("ACK");
          pTxCharacteristic->notify();
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===================");
  Serial.print(deviceName);
  Serial.println(" BLE 시스템 시작");
  Serial.println("===================");

  pixels.begin();
  pixels.setBrightness(50);
  setRed();
  Serial.println("빨간불 점등 - 신호 대기 중");

  NimBLEDevice::init(deviceName.c_str());

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
  );

  pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  
  NimBLEAdvertisementData advertisementData;
  advertisementData.setCompleteServices(NimBLEUUID(SERVICE_UUID));
  advertisementData.setName(deviceName.c_str());
  pAdvertising->setAdvertisementData(advertisementData);
  
  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(deviceName.c_str());
  pAdvertising->setScanResponseData(scanResponseData);
  
  pAdvertising->start();

  Serial.print("BLE 시작 - 디바이스 이름: ");
  Serial.println(deviceName);
  Serial.println("iOS에서 연결 대기 중");
}

void loop() {
  if (signalReceived && (millis() - signalTime > 10000)) {
    signalReceived = false;
    setRed();
    Serial.println("타임아웃 (10초) - 빨간불로 복귀");
  }

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    NimBLEDevice::startAdvertising();
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.print(BUS_NUMBER);
    Serial.print("번 버스 상태: ");
    if (signalReceived) {
      Serial.print("신호 수신됨 (파란불) - ");
      Serial.print((10000 - (millis() - signalTime)) / 1000);
      Serial.println("초 남음");
    } else {
      Serial.println("대기 중 (빨간불)");
    }
    lastPrint = millis();
  }

  delay(100);
}