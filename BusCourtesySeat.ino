#include "config.h"  // 맨 위에 추가

#include <NimBLEDevice.h>
#include <driver/i2s.h> // 내장 I2S 드라이버
#include <math.h>       // 사인파 계산
#include <Wire.h>       // I2C 통신 라이브러리
#include <LiquidCrystal_I2C.h> // LCD 라이브러리

// --- LCD 설정 ---
// 참고: 2004 LCD는 한글 폰트가 없어 영문만 표시 가능합니다.
LiquidCrystal_I2C lcd(0x27, 20, 4); // 20x4 LCD, 주소: 0x27

// --- I2S 오디오 설정 ---
#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 20
#define I2S_LRC  21
#define I2S_DIN  22

#define SAMPLE_RATE     (44100)
#define TONE_FREQUENCY  (1200)
#define BUFFER_SIZE     (128)
int16_t sine_wave_buffer[BUFFER_SIZE * 2];

// --- 상태 변수 ---
bool isAudioPlaying = false;      // 오디오 재생 상태 (0.5초)
unsigned long audioStartTime = 0; // 오디오 시작 시간

bool isMessageDisplaying = false; // LCD 메시지 표시 상태 (5초)
unsigned long messageStartTime = 0; // LCD 메시지 시작 시간

// --- BLE 설정 ---
NimBLEServer *pServer = NULL;
NimBLECharacteristic *pRxCharacteristic;
NimBLECharacteristic *pTxCharacteristic;

int connectedDevices = 0;
int oldConnectedDevices = 0;

String deviceName = "BF_DREAM_" + String(BUS_NUMBER);

// --- 1200Hz 사인파 데이터를 미리 계산하는 함수 ---
void fill_sine_buffer() {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float angle = 2.0 * PI * TONE_FREQUENCY * i / (float)SAMPLE_RATE;
        int16_t sample = (int16_t)(sin(angle) * 16383.0);
        sine_wave_buffer[i * 2]     = sample; // Left Channel
        sine_wave_buffer[i * 2 + 1] = sample; // Right Channel
    }
}

// --- 내장 I2S 드라이버 설정 함수 ---
void setup_i2s_audio() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(I2S_PORT, &pin_config);
    Serial.println("내장 I2S 드라이버 초기화 완료");
}


class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
      connectedDevices = pServer->getConnectedCount(); // 실제 연결 수 사용
      Serial.print("[DEBUG] onConnect 호출됨! ");
      Serial.print("iOS 기기가 ");
      Serial.print(BUS_NUMBER);
      Serial.print("번 버스에 연결됨 (총 연결: ");
      Serial.print(connectedDevices);
      Serial.println("개)");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
      connectedDevices = pServer->getConnectedCount(); // 실제 연결 수 사용
      Serial.print("[DEBUG] onDisconnect 호출됨! reason=");
      Serial.println(reason);
      Serial.print("iOS 기기 연결 해제 (총 연결: ");
      Serial.print(connectedDevices);
      Serial.println("개)");
      
      // 강제로 LCD 업데이트
      lcd.setCursor(0, 1);
      lcd.print("Devices Connected: ");
      lcd.print(connectedDevices);
      lcd.print("  "); // 공백으로 이전 숫자 지우기
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) {
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.print("수신한 데이터: ");
        Serial.println(rxValue.c_str());

        if(rxValue == "COURTESY_SEAT") {
          
          // 오디오와 메시지 타이머를 별도로 시작
          
          // 1. 오디오 시작 (0.5초)
          if (!isAudioPlaying) {
            isAudioPlaying = true;
            audioStartTime = millis();
            Serial.println("오디오 재생 시작 (1200Hz)");
          }

          // 2. LCD 메시지 시작 (5초)
          isMessageDisplaying = true;
          messageStartTime = millis(); // 5초 타이머 리셋
          
          // 3. LCD에 메시지 즉시 표시
          lcd.setCursor(0, 2); // 3번째 줄
          lcd.print("COURTESY_SEAT Signal");
          lcd.setCursor(0, 3); // 4번째 줄
          lcd.print("Playing 1200Hz Tone");

          Serial.print(BUS_NUMBER);
          Serial.println("번 버스 배려석 알림 수신");
          
          pTxCharacteristic->setValue("ACK");
          pTxCharacteristic->indicate(connInfo.getConnHandle());
          Serial.println("ACK 전송 완료");
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
  Serial.println(" BLE 오디오/LCD 시스템 시작");
  Serial.println("===================");

  // --- LCD 셋업 ---
  Wire.begin(4, 5); // I2C 시작 (SDA=4, SCL=5)
  lcd.init();       // LCD 초기화
  lcd.backlight();  // 백라이트 켜기
  
  lcd.setCursor(0, 0); // 1번째 줄
  lcd.print("BUS_NUM: ");
  lcd.print(BUS_NUMBER);
  lcd.setCursor(0, 1); // 2번째 줄
  lcd.print("Devices Connected: 0");
  Serial.println("LCD 초기화 완료");

  // --- 오디오 셋업 ---
  setup_i2s_audio();
  fill_sine_buffer();
  
  // --- BLE 셋업 ---
  NimBLEDevice::deinit(true);
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

    // --- 오디오 재생 로직 (0.5초 타이머) ---
    if (isAudioPlaying) {
      if (millis() - audioStartTime > 500) {
        // 0.5초가 지났으면 재생 중지
        isAudioPlaying = false;
        i2s_zero_dma_buffer(I2S_PORT);
        Serial.println("오디오 재생 중지 (500ms)");

        // 0.5초 후 "Playing..." 메시지만 먼저 지움
        if (isMessageDisplaying) { // 5초 타이머가 아직 돌고 있다면
          lcd.setCursor(0, 3); // 4번째 줄
          lcd.print("                    "); // 20칸 공백
        }
      } else {
        // 0.5초가 지나지 않았으면, 계속해서 사인파 버퍼를 I2S로 씀
        size_t bytes_written = 0;
        i2s_write(I2S_PORT, sine_wave_buffer, sizeof(sine_wave_buffer), &bytes_written, 0);
      }
    }

    // --- LCD 메시지 표시 로직 (5초 타이머) ---
    if (isMessageDisplaying) {
      if (millis() - messageStartTime > 5000) {
        // 5초가 지났으면 메시지 표시 중지
        isMessageDisplaying = false;

        // LCD 메시지(2, 3번째 줄)를 모두 지움
        lcd.setCursor(0, 2); // 3번째 줄
        lcd.print("                    "); // 20칸 공백
        lcd.setCursor(0, 3); // 4번째 줄
        lcd.print("                    "); // 20칸 공백

        Serial.println("LCD 메시지 표시 중지 (5000ms)");
      }
      // (5초가 지나지 않았으면 아무것도 안함 - 메시지 유지)
    }


    // --- BLE 연결 상태 로직 ---
    if (connectedDevices != oldConnectedDevices) {
      Serial.print("[DEBUG] 연결 상태 변경: ");
      Serial.print(oldConnectedDevices);
      Serial.print(" -> ");
      Serial.println(connectedDevices);

      // LCD 업데이트
      lcd.setCursor(0, 1); // 2번째 줄
      lcd.print("Devices Connected: ");
      lcd.print(connectedDevices);
      lcd.print("  "); // 공백 2칸으로 이전 숫자 완전히 지우기

      oldConnectedDevices = connectedDevices; // 상태 업데이트

      // 연결 해제 시 재광고 시작
      if (connectedDevices == 0) {
        delay(500);
        NimBLEDevice::startAdvertising();
        Serial.println("모든 기기 연결 해제 - 재광고 시작");
      }
    }

    // --- 연결 상태 강제 체크 (5초마다) ---
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 5000) {
        int actualConnections = pServer->getConnectedCount();
        if (actualConnections != connectedDevices) {
            Serial.print("[WARN] 연결 수 불일치 감지: connectedDevices=");
            Serial.print(connectedDevices);
            Serial.print(", actual=");
            Serial.println(actualConnections);
            connectedDevices = actualConnections; // 강제 동기화
        }
        lastCheck = millis();
    }

    // --- 상태 출력 (5초마다) ---
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
      Serial.print(BUS_NUMBER);
      Serial.print("번 버스 상태: ");
      Serial.print("연결된 기기 ");
      Serial.print(connectedDevices);
      Serial.print("개 | isMessageDisplaying=");
      Serial.print(isMessageDisplaying);
      Serial.print(" | isAudioPlaying=");
      Serial.println(isAudioPlaying);

      lastPrint = millis();
    }
}