//중복 포함 방지
#ifndef LEPTON_I2C   //"LEPTON_I2C"가 아직 정의되지 않았다면
#define LEPTON_I2C   //"LEPTON_I2C"를 정의

// I2C 기반 Lepton 제어 함수들 선언부

// (내부에서만 쓰는 연결 함수는 .cpp 안에만 둠)
// int lepton_connect();

// 수동 FFC(보정) 한 번 수행 (셔터 닫고 FFC 실행)
void lepton_perform_ffc();

// 자동 FFC(시간/온도 트리거) 끄고 Manual 모드로 전환
//  → 이후에는 사용자가 lepton_perform_ffc()를 호출할 때만 셔터 동작
void lepton_disable_ffc_auto();

// Lepton 카메라 모듈 재부팅
void lepton_reboot();

#endif // 중복 포함 방지 끝
