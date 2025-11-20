/*
 * SPI testing utility (using spidev driver)
 *
 * 이 파일은 Linux(spidev) 기반 SPI 통신을 쉽게 사용하기 위한 헤더파일이다.
 * 여기서는 SPI 포트 열기/닫기, 설정에 필요한 변수들을 선언만 해놓고
 * 실제 동작은 SPI.cpp 같은 곳에서 구현된다.
 *
 * GPL 오픈소스 기본 제공 파일이며 Lepton, 센서, SPI 디바이스 제어에 사용됨.
 */

#ifndef SPI_H           // 만약 SPI_H 가 정의되지 않았다면 (= 처음 include)
#define SPI_H           // SPI_H 정의. 이후 중복 include 방지.

// C/C++ 표준 라이브러리들
#include <string>       // string 클래스 사용
#include <stdint.h>     // uint8_t, uint32_t 같은 고정 크기 타입
#include <unistd.h>     // read(), write(), close() 등 POSIX 함수
#include <stdio.h>      // printf, fprintf 등의 표준입출력
#include <stdlib.h>     // malloc, free, atoi 등
#include <getopt.h>     // 명령줄 옵션 파싱 관련
#include <fcntl.h>      // open() 등 파일 제어
#include <sys/ioctl.h>  // ioctl() 사용 (SPI 설정에 필수)
#include <linux/types.h>// Linux 타입 정의
#include <linux/spi/spidev.h>  // ★ 핵심: SPI 제어 구조체/상수/명령코드 포함


// ---------------------------------------------------------------
// 🧩 SPI 전역 변수 선언 (extern)
// ---------------------------------------------------------------
// extern = “다른 .cpp 파일에서 실제로 정의됨. 여기서는 선언만 한다” 라는 뜻.
//
// 즉, 이 변수들의 진짜 공간은 SPI.cpp 같은 곳에 있으며
// 모든 소스파일이 이 값을 공유해서 SPI 설정을 유지할 수 있게 함.

// SPI Chip Select 0(CE0) 파일 핸들
extern int spi_cs0_fd;

// SPI Chip Select 1(CE1) 파일 핸들
extern int spi_cs1_fd;

// SPI 모드 설정 (MODE0, MODE1 등) — 보통 0 사용
extern unsigned char spi_mode;

// 한 번 전송되는 데이터 비트 수 (기본 8비트)
extern unsigned char spi_bitsPerWord;

// SPI 클럭 속도 (Hz 단위, 예: 10000000 = 10MHz)
extern unsigned int spi_speed;



// ---------------------------------------------------------------
// 🔧 함수 선언부 (구현은 SPI.cpp에 있음)
// ---------------------------------------------------------------

/*
 * SpiOpenPort()
 * ---------------------------------------------------
 * 특정 SPI 포트(spidev0.0 또는 spidev0.1 등)를 열고
 * 해당 포트의 모드, 비트수, 속도 등을 설정한다.
 *
 * spi_device:
 *    0 → /dev/spidev0.0 (CE0)
 *    1 → /dev/spidev0.1 (CE1)
 *
 * spi_speed:
 *    설정할 SCLK 속도 (예: 20000000 = 20MHz)
 *
 * return:
 *    0 = 성공
 *   -1 = 실패
 */
int SpiOpenPort(int spi_device, unsigned int spi_speed);



/*
 * SpiClosePort()
 * ---------------------------------------------------
 * SpiOpenPort 로 연 SPI 포트를 닫는다.
 *
 * spi_device:
 *    0 → CE0
 *    1 → CE1
 *
 * return:
 *    0 = 성공
 *   -1 = 실패
 */
int SpiClosePort(int spi_device);



#endif // SPI_H  // 헤더 가드 끝. 중복 include 방지.
