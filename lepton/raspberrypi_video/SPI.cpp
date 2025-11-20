#include "SPI.h"
// SPI.h 헤더파일을 포함한다.
// → 여기에는 SPI 포트 번호, 속도, 모드 같은 변수 선언과 함수 선언이 들어 있음.


// ---------------------------------------------------------
// SPI 전역 변수 실제 정의부
// (SPI.h 에서는 extern 으로 '선언'만 했던 것들이 여기서 '정의'됨)
// ---------------------------------------------------------

int spi_cs0_fd = -1;  // SPI 포트 0 (CE0) 의 파일 디스크립터. 처음엔 -1 = 아직 안 열림.
int spi_cs1_fd = -1;  // SPI 포트 1 (CE1) 의 파일 디스크립터.

unsigned char spi_mode = SPI_MODE_3; // 기본 SPI 모드 = MODE 3 (CPOL=1, CPHA=1)
unsigned char spi_bitsPerWord = 8;   // 1 SPI 전송 단위 = 8비트
unsigned int spi_speed = 10000000;   // 기본 속도 = 10MHz



// ======================================================================
// ★★★ SPI 포트 열기 함수 ★★★
// ======================================================================
// spi_device = 0  → /dev/spidev0.0 (CE0)
// spi_device = 1  → /dev/spidev0.1 (CE1)
// useSpiSpeed = 설정할 SPI 속도(Hz)
//
// 이 함수는:
// 1. 파일 오픈 (open())
// 2. ioctl 로 SPI 모드 설정
// 3. bits per word 설정
// 4. SPI 속도 설정
// 을 모두 수행함.
// ======================================================================
int SpiOpenPort (int spi_device, unsigned int useSpiSpeed)
{
	int status_value = -1;    // ioctl 성공 여부 체크용 변수
	int *spi_cs_fd;           // 어느 포트를 열지 결정한 뒤 해당 fd 를 가리키는 포인터


	// ---------------------------------------------------------
	// 1) SPI 모드 설정 (MODE 0~3)
	// ---------------------------------------------------------
	// MODE 3 사용 이유: Lepton 3.5 는 CPOL=1, CPHA=1 을 사용해야 정상 동작함.
	spi_mode = SPI_MODE_3;


	// ---------------------------------------------------------
	// 2) 전송 비트수 설정
	// ---------------------------------------------------------
	// Lepton 은 8bit per word (1바이트 단위)로 수신.
	spi_bitsPerWord = 8;


	// ---------------------------------------------------------
	// 3) SPI 속도 설정
	// ---------------------------------------------------------
	// Lepton 3.5 최대 SPI 속도는 약 20~25MHz.
	// 기본값은 사용자가 넘겨준 useSpiSpeed 를 그대로 적용.
	spi_speed = useSpiSpeed;


	// ---------------------------------------------------------
	// 4) CE0 or CE1 중 어떤 포트를 사용할지 선택
	// ---------------------------------------------------------
	if (spi_device)
		spi_cs_fd = &spi_cs1_fd;  // spi_device == 1 → spidev0.1
	else
		spi_cs_fd = &spi_cs0_fd;  // spi_device == 0 → spidev0.0


	// ---------------------------------------------------------
	// 5) 실제로 /dev/spidev0.x 포트를 open 한다
	// ---------------------------------------------------------
	if (spi_device)
		*spi_cs_fd = open(std::string("/dev/spidev0.1").c_str(), O_RDWR);
	else
		*spi_cs_fd = open(std::string("/dev/spidev0.0").c_str(), O_RDWR);

	if (*spi_cs_fd < 0)
	{
		perror("Error - Could not open SPI device"); // 오류 출력
		exit(1);  // 프로그램 즉시 종료
	}


	// ---------------------------------------------------------
	// 6) ioctl() 을 사용하여 SPI 드라이버 설정
	// ---------------------------------------------------------

	// (1) SPI 모드를 "쓰기" 모드로 설정
	status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_MODE, &spi_mode);
	if(status_value < 0)
	{
		perror("Could not set SPIMode (WR)...ioctl fail");
		exit(1);
	}

	// (2) SPI 모드를 "읽기" 모드에서도 확인하여 적용
	status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_MODE, &spi_mode);
	if(status_value < 0)
	{
		perror("Could not set SPIMode (RD)...ioctl fail");
		exit(1);
	}

	// (3) Bits per word 설정 (WR)
	status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bitsPerWord);
	if(status_value < 0)
	{
		perror("Could not set SPI bitsPerWord (WR)...ioctl fail");
		exit(1);
	}

	// (4) Bits per word 설정 (RD 확인)
	status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bitsPerWord);
	if(status_value < 0)
	{
		perror("Could not set SPI bitsPerWord(RD)...ioctl fail");
		exit(1);
	}

	// (5) SPI 속도 설정 (WR)
	status_value = ioctl(*spi_cs_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
	if(status_value < 0)
	{
		perror("Could not set SPI speed (WR)...ioctl fail");
		exit(1);
	}

	// (6) SPI 속도 설정 (RD 확인)
	status_value = ioctl(*spi_cs_fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
	if(status_value < 0)
	{
		perror("Could not set SPI speed (RD)...ioctl fail");
		exit(1);
	}

	return(status_value);  // 성공 시 0
}



// ======================================================================
// ★★★ SPI 포트 닫기 함수 ★★★
// ======================================================================
int SpiClosePort(int spi_device)
{
	int status_value = -1;
	int *spi_cs_fd;

	// 어떤 포트인지 선택
	if (spi_device)
		spi_cs_fd = &spi_cs1_fd; // spidev0.1
	else
		spi_cs_fd = &spi_cs0_fd; // spidev0.0


	// 포트를 닫는다
	status_value = close(*spi_cs_fd);
	if(status_value < 0)
	{
		perror("Error - Could not close SPI device");
		exit(1);
	}

	return(status_value);  // 성공 시 0
}

