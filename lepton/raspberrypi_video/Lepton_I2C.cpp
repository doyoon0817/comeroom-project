// I2C: FFC, reboot, radiometry 등 카메라 제어
#include "Lepton_I2C.h"

// Lepton SDK
#include "leptonSDKEmb32PUB/LEPTON_SDK.h"
#include "leptonSDKEmb32PUB/LEPTON_SYS.h"
#include "leptonSDKEmb32PUB/LEPTON_OEM.h"
#include "leptonSDKEmb32PUB/LEPTON_Types.h"

// ---------------------------------------------------------
// 전역 상태 변수
// ---------------------------------------------------------
static bool _connected = false;
static LEP_CAMERA_PORT_DESC_T _port;

//================================================
// 1) Lepton I2C 연결 함수
//================================================
static int lepton_connect()
{
    if (_connected) return 0;

    LEP_RESULT res = LEP_OpenPort(1, LEP_CCI_TWI, 400, &_port);
    if (res == LEP_OK) {
        _connected = true;
        return 0;
    }

    _connected = false;
    return -1;
}

//===============================================
// 2) 수동 FFC 한 번 수행
//===============================================
void lepton_perform_ffc()
{
    if (!_connected) {
        if (lepton_connect() != 0) return;
    }
    LEP_RunSysFFCNormalization(&_port);
}

//===============================================
// 3) 자동 FFC 끄기 (Manual Mode)
//===============================================
//  ▶ 핵심: FFC ShutterMode를 MANUAL 로 변경
//  ▶ 사용되는 함수:
//     - LEP_GetSysFfcShutterModeObj()
//     - LEP_SetSysFfcShutterModeObj()
//===============================================
void lepton_disable_ffc_auto()
{
    if (!_connected) {
        if (lepton_connect() != 0) return;
    }

    LEP_SYS_FFC_SHUTTER_MODE_OBJ_T modeObj;
    LEP_RESULT res;

    // 현재 모드 읽기
    res = LEP_GetSysFfcShutterModeObj(&_port, &modeObj);
    if (res != LEP_OK) {
        return;
    }

    // 자동 → 수동
    modeObj.shutterMode = LEP_SYS_FFC_SHUTTER_MODE_MANUAL;

    // 옵션값은 그대로 두고 모드만 변경
    LEP_SetSysFfcShutterModeObj(&_port, modeObj);
}

//===============================================
// 4) Lepton 모듈 재부팅
//===============================================
void lepton_reboot()
{
    if (!_connected) {
        if (lepton_connect() != 0) return;
    }

    LEP_RunOemReboot(&_port);

    // 재부팅 후 다시 연결 필요
    _connected = false;
}
