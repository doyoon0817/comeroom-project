#ifndef TEXTTHREAD          // 헤더 중복 포함 방지 시작
#define TEXTTHREAD

#include <ctime>
#include <stdint.h>

#include <QThread>          // Qt 스레드 클래스 (백그라운드 작업용)
#include <QtCore>           // Qt 핵심 기능 (QObject, signal/slot 등)
#include <QPixmap>          // 화면 출력용 이미지(QPixmap) 사용
#include <QImage>           // CPU 메모리 이미지(QImage) 사용

// Lepton 패킷/프레임 관련 고정값 정의
#define PACKET_SIZE 164                           // 패킷 1개는 164바이트
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)        // 16비트 단위로 보면 82개
#define PACKETS_PER_FRAME 60                      // Lepton 프레임당 60패킷
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16 * PACKETS_PER_FRAME)
// 전체 프레임을 16비트 단위로 배열로 보면 총 82 * 60 = 4920개

// LeptonThread 클래스
// =============================
// Lepton 열화상 카메라에서 SPI로 데이터를 읽고
// QImage로 변환한 뒤
// updateImage(QImage) 시그널로 UI(MyLabel 등)에 보내는 역할.
//
// QThread를 상속했기 때문에 run() 함수 내부에서
// 백그라운드로 계속 SPI 패킷을 읽는 구조.
class LeptonThread : public QThread
{
  Q_OBJECT;   // Qt signal/slot 기반 객체임을 표시

public:
  LeptonThread();     // 생성자
  ~LeptonThread();    // 소멸자

  // 설정 함수들 ------------------------
  void setLogLevel(uint16_t);           // 디버그 로그 레벨 설정
  void useColormap(int);                // 컬러맵 선택
  void useLepton(int);                  // Lepton 2.x / 3.x 선택
  void useSpiSpeedMhz(unsigned int);    // SPI 속도 설정 (MHz)
  void setAutomaticScalingRange();      // 자동 스케일링(min/max)
  void useRangeMinValue(uint16_t);      // 수동 최소 온도 범위
  void useRangeMaxValue(uint16_t);      // 수동 최대 온도 범위
  void run();                           // QThread 메인 함수 (SPI 루프)

public slots:
  void performFFC();  // FFC 명령을 I2C로 보내는 슬롯

signals:
  void updateText(QString);   // 텍스트 UI 업데이트용 (거의 안 씀)
  void updateImage(QImage);   // 완성된 열화상 QImage를 UI로 보내는 시그널

private:

  // 내부 함수, 로그 찍기용
  void log_message(uint16_t level, std::string msg);

  // -------- 상태 변수들 --------
  uint16_t loglevel;          // 로그 레벨

  int typeColormap;           // 사용 중인 컬러맵 타입
  const int *selectedColormap;// 실제 컬러맵 배열 포인터
  int selectedColormapSize;   // 컬러맵 배열 크기

  int typeLepton;             // 2 = Lepton2.x, 3 = Lepton3.x
  unsigned int spiSpeed;      // SPI 속도 (Hz 단위)

  bool autoRangeMin;          // 자동 최대 온도 찾기 여부
  bool autoRangeMax;          // 자동 최소 온도 찾기 여부
  uint16_t rangeMin;          // 스케일 최소값
  uint16_t rangeMax;          // 스케일 최대값

  int myImageWidth;           // 출력 이미지 가로 크기
  int myImageHeight;          // 출력 이미지 세로 크기
  QImage myImage;             // 최종 출력될 이미지 버퍼

  // raw SPI 패킷 저장 공간
  uint8_t result[PACKET_SIZE * PACKETS_PER_FRAME];

  // Lepton 3.x는 4개의 세그먼트를 조합해야 한 프레임이 완성되므로
  // segment 1~4를 각각 저장할 버퍼
  uint8_t shelf[4][PACKET_SIZE * PACKETS_PER_FRAME];

  uint16_t *frameBuffer;      // (사용되지 않지만) 프레임용 버퍼 포인터

};

#endif   // TEXTTHREAD 끝
