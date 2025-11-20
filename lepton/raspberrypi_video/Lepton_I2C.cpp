//I2C:FFC,reboot,radiometry 등 카메라 제어
#include "Lepton_I2C.h"
//헤더 파일 포함
#include "leptonSDKEmb32PUB/LEPTON_SDK.h"//Lepton 기본 SDK
#include "leptonSDKEmb32PUB/LEPTON_SYS.h"// FFC 등 시스템 제어
#include "leptonSDKEmb32PUB/LEPTON_OEM.h"//OEM 레벨 제어
#include "leptonSDKEmb32PUB/LEPTON_Types.h"// 공용 자료형 정의

bool _connected;// Lepton과 I2C 연결됐나?

LEP_CAMERA_PORT_DESC_T _port;
//I2C 포트 정보 저장

//================================================
//1)카메라 연결(I2C 포트 오픈)
//================================================
int lepton_connect() {
	
	LEP_OpenPort(1, LEP_CCI_TWI, 400, &_port);//(버스번호,통신타입,속도(kHz),포트 구조체 주소
	//LEP_CCI_TWI->I2C모드
	_connected = true;//연결 성공 표시
	return 0;
}
//===============================================
//2)FFC(센서 화질 보정) 보정
//===============================================
void lepton_perform_ffc() {
	if(!_connected) {
		lepton_connect();
	//카메라 연결X=>1번 함수로 카메라 연결
	}
	//FFC 실행(화질 보정)
	LEP_RunSysFFCNormalization(&_port);
}

//presumably more commands could go here if desired
//===============================================
//3)카메라 부팅
//===============================================
void lepton_reboot() {
	if(!_connected) {
		lepton_connect();
	}
	LEP_RunOemReboot(&_port);// Lepton 모듈 자체 재부팅
}
