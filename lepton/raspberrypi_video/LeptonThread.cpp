#include <iostream>                     // 콘솔 출력용

#include "LeptonThread.h"               // LeptonThread 클래스 선언부

#include "Palettes.h"                   // 색상 맵(colormap) 데이터
#include "SPI.h"                        // SPI 함수들
#include "Lepton_I2C.h"                 // I2C 기반 FFC/Reboot 제어

// Lepton 데이터 패킷 크기 및 프레임 구조 정의
#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 60
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define FPS 27;                         // 프레임레이트 정의(사용되지 않음)


/* ================================
   LeptonThread 생성자
   ================================ */
LeptonThread::LeptonThread() : QThread()
{
	// 로그 레벨 설정 (0 = 기본)
	loglevel = 0;

	// 컬러맵 기본: ironblack
	typeColormap = 3;
	selectedColormap = colormap_ironblack;
	selectedColormapSize = get_size_colormap_ironblack();

	// Lepton 카메라 종류 (2 = Lepton2.x, 3 = Lepton3.x)
	typeLepton = 2;
	myImageWidth = 80;
	myImageHeight = 60;

	// SPI 속도 설정 (20 MHz)
	spiSpeed = 20 * 1000 * 1000;

	// 자동 최소/최대값 (온도 스케일링)
	autoRangeMin = true;
	autoRangeMax = true;
	rangeMin = 30000;
	rangeMax = 32000;
}

LeptonThread::~LeptonThread() {
}


/* 로그 레벨 설정 */
void LeptonThread::setLogLevel(uint16_t newLoglevel)
{
	loglevel = newLoglevel;
}


/* 사용할 컬러맵 선택 */
void LeptonThread::useColormap(int newTypeColormap)
{
	switch (newTypeColormap) {
	case 1:
		typeColormap = 1;
		selectedColormap = colormap_rainbow;
		selectedColormapSize = get_size_colormap_rainbow();
		break;
	case 2:
		typeColormap = 2;
		selectedColormap = colormap_grayscale;
		selectedColormapSize = get_size_colormap_grayscale();
		break;
	default: // ironblack
		typeColormap = 3;
		selectedColormap = colormap_ironblack;
		selectedColormapSize = get_size_colormap_ironblack();
		break;
	}
}


/* Lepton 2.x vs Lepton 3.x 선택 */
void LeptonThread::useLepton(int newTypeLepton)
{
	switch (newTypeLepton) {
	case 3:   // Lepton 3.x 해상도
		typeLepton = 3;
		myImageWidth = 160;
		myImageHeight = 120;
		break;
	default:  // Lepton 2.x 해상도
		typeLepton = 2;
		myImageWidth = 80;
		myImageHeight = 60;
	}
}


/* SPI 속도 설정 (MHz 단위 입력) */
void LeptonThread::useSpiSpeedMhz(unsigned int newSpiSpeed)
{
	spiSpeed = newSpiSpeed * 1000 * 1000;
}


/* 자동 스케일링 (min/max) */
void LeptonThread::setAutomaticScalingRange()
{
	autoRangeMin = true;
	autoRangeMax = true;
}


/* 수동 최소값 설정 */
void LeptonThread::useRangeMinValue(uint16_t newMinValue)
{
	autoRangeMin = false;
	rangeMin = newMinValue;
}


/* 수동 최대값 설정 */
void LeptonThread::useRangeMaxValue(uint16_t newMaxValue)
{
	autoRangeMax = false;
	rangeMax = newMaxValue;
}


/* ============================================
   LeptonThread::run()
   스레드가 실행되면 여기서 계속 SPI 데이터 읽음
   ============================================ */
void LeptonThread::run()
{
	// 출력할 이미지 생성 (RGB888 포맷)
	myImage = QImage(myImageWidth, myImageHeight, QImage::Format_RGB888);

	// 컬러맵 정보
	const int *colormap = selectedColormap;
	const int colormapSize = selectedColormapSize;

	// 스케일링 범위 값
	uint16_t minValue = rangeMin;
	uint16_t maxValue = rangeMax;
	float diff = maxValue - minValue;
	float scale = 255/diff;

	// Lepton3.5 segment 에러 카운트
	uint16_t n_wrong_segment = 0;
	uint16_t n_zero_value_drop_frame = 0;

	// SPI 포트 오픈
	SpiOpenPort(0, spiSpeed);


	/* ================================
	     메인 루프 (무한 반복)
	   ================================ */
	while(true) {

		/* ----------------------------
		   프레임 읽기 (60 패킷)
		   ---------------------------- */
		int resets = 0;
		int segmentNumber = -1;

		for(int j=0;j<PACKETS_PER_FRAME;j++) {

			// SPI로 패킷 읽기 (각 패킷 164바이트)
			read(spi_cs0_fd,
			     result + sizeof(uint8_t)*PACKET_SIZE*j,
			     sizeof(uint8_t)*PACKET_SIZE);

			// 패킷 header[1] = 패킷 번호
			int packetNumber = result[j*PACKET_SIZE+1];

			// 패킷 번호가 맞지 않으면 sync 깨짐 → 재시도
			if(packetNumber != j) {
				j = -1;
				resets += 1;
				usleep(1000);

				// 너무 많이 깨지면 reboot
				if(resets == 750) {
					SpiClosePort(0);
					lepton_reboot();
					n_wrong_segment = 0;
					n_zero_value_drop_frame = 0;
					usleep(750000);
					SpiOpenPort(0, spiSpeed);
				}
				continue;
			}

			// Lepton 3.x 는 segment 기반 구조
			if ((typeLepton == 3) && (packetNumber == 20)) {
				segmentNumber = (result[j*PACKET_SIZE] >> 4) & 0x0f;
				if ((segmentNumber < 1) || (4 < segmentNumber)) {
					log_message(10, "[ERROR] Wrong segment number " + std::to_string(segmentNumber));
					break;
				}
			}
		}

		if(resets >= 30) {
			log_message(3, "done reading, resets: " + std::to_string(resets));
		}


		/* ----------------------------
		   세그먼트 정렬 (Lepton 3.x)
		   ---------------------------- */
		int iSegmentStart = 1;
		int iSegmentStop;

		if (typeLepton == 3) {
			if ((segmentNumber < 1) || (4 < segmentNumber)) {
				n_wrong_segment++;
				if ((n_wrong_segment % 12) == 0) {
					log_message(5, "[WARNING] Wrong segment number continuously "
						+ std::to_string(n_wrong_segment));
				}
				continue;
			}

			if (n_wrong_segment != 0) {
				log_message(8, "[WARNING] Wrong segment recovered: "
					+ std::to_string(segmentNumber));
				n_wrong_segment = 0;
			}

			// segment 저장
			memcpy(shelf[segmentNumber - 1], result,
			       sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);

			// 4번 segment까지 받아야 전체 프레임 구성됨
			if (segmentNumber != 4) continue;
			iSegmentStop = 4;
		}
		else {
			memcpy(shelf[0], result,
			       sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);
			iSegmentStop = 1;
		}


		/* ----------------------------
		    Auto Range 설정 (온도 스케일)
		   ---------------------------- */
		if (autoRangeMin || autoRangeMax) {

			if (autoRangeMin) maxValue = 65535;
			if (autoRangeMax) maxValue = 0;

			for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
				for(int i=0;i<FRAME_SIZE_UINT16;i++) {

					if(i % PACKET_SIZE_UINT16 < 2) continue;

					uint16_t value =
					  (shelf[iSegment - 1][i*2] << 8) +
					   shelf[iSegment - 1][i*2+1];

					if (value == 0) continue;

					if (autoRangeMax && (value > maxValue)) maxValue = value;
					if (autoRangeMin && (value < minValue)) minValue = value;
				}
			}

			diff = maxValue - minValue;
			scale = 255/diff;
		}


		/* ----------------------------
		   픽셀 변환 및 이미지 렌더링
		   ---------------------------- */
		int row, column;
		uint16_t value;
		uint16_t valueFrameBuffer;
		QRgb color;

		for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {

			int ofsRow = 30 * (iSegment - 1);   // row offset for segment

			for(int i=0;i<FRAME_SIZE_UINT16;i++) {

				if(i % PACKET_SIZE_UINT16 < 2) continue;

				// MSB-LSB 조합
				valueFrameBuffer =
				  (shelf[iSegment - 1][i*2] << 8) +
				   shelf[iSegment - 1][i*2+1];

				if (valueFrameBuffer == 0) {
					n_zero_value_drop_frame++;
					if ((n_zero_value_drop_frame % 12) == 0) {
						log_message(5, "[WARNING] Found zero-value "
							+ std::to_string(n_zero_value_drop_frame));
					}
					break;
				}

				// normalize to 0~255
				value = (valueFrameBuffer - minValue) * scale;

				// colormap lookup → RGB color
				int ofs_r = 3 * value + 0;
				if (ofs_r >= colormapSize) ofs_r = colormapSize - 1;

				int ofs_g = 3 * value + 1;
				if (ofs_g >= colormapSize) ofs_g = colormapSize - 1;

				int ofs_b = 3 * value + 2;
				if (ofs_b >= colormapSize) ofs_b = colormapSize - 1;

				color =
				  qRgb(colormap[ofs_r], colormap[ofs_g], colormap[ofs_b]);

				// coordinates for output image
				if (typeLepton == 3) {
					column = (i % PACKET_SIZE_UINT16) - 2 +
					  (myImageWidth/2) *
					  ((i % (PACKET_SIZE_UINT16*2)) / PACKET_SIZE_UINT16);

					row = (i / PACKET_SIZE_UINT16)/2 + ofsRow;
				}
				else {
					column = (i % PACKET_SIZE_UINT16) - 2;
					row = i / PACKET_SIZE_UINT16;
				}

				// set pixel on QImage
				myImage.setPixel(column, row, color);
			}
		}

		if (n_zero_value_drop_frame != 0) {
			log_message(8, "[WARNING] Zero-value recovered "
				+ std::to_string(n_zero_value_drop_frame));
			n_zero_value_drop_frame = 0;
		}

		/* ----------------------------
		   QImage 완성됨 → UI로 송신
		   ---------------------------- */
		emit updateImage(myImage);
	}

	// 반복 종료 → SPI 닫기
	SpiClosePort(0);
}


/* FFC 수행 슬롯 */
void LeptonThread::performFFC() {
	lepton_perform_ffc();
}


/* 로그 출력 함수 */
void LeptonThread::log_message(uint16_t level, std::string msg)
{
	if (level <= loglevel) {
		std::cerr << msg << std::endl;
	}
}
