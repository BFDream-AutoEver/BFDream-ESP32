## ESP32, 디바이스 간 블루투스 통신을 하기 위한 코드 파일입니다.

### 설정 방법

1. `config.example.h`를 복사해서 `config.h` 생성:
```bash
   cp config.example.h config.h
```

2. `config.h`에서 버스 번호 수정:
```cpp
   #define BUS_NUMBER "2222"
```

3. Arduino IDE에서 업로드

### 주의사항
- 각 버스마다 `config.h`의 번호를 바꿔서 업로드하세요

---

## TTS 적용 방법
### 1. https://ttsmaker.com/ 에서 tts를 mp3로 생성한다
### 2. https://tomeko.net/online_tools/file_to_hex.php?lang=en 에서 mp3 파일을 hexadecimal 코드로 변경한다
### 3. `sound_data.h`에 (`const unsigned char mom_mp3[] PROGMEM = {}`) 코드를 붙여넣고 저장한다

