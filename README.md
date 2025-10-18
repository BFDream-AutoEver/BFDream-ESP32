# ESP32 버스 배려석 시스템

## 설정 방법

1. `config.example.h`를 복사해서 `config.h` 생성:
```bash
   cp config.example.h config.h
```

2. `config.h`에서 버스 번호 수정:
```cpp
   #define BUS_NUMBER "2222"
```

3. Arduino IDE에서 업로드

## 주의사항
- 각 버스마다 `config.h`의 번호를 바꿔서 업로드하세요
