//main.cpp → GUI+스레드 생성
//LeptonThread.cpp → SPI 읽고 QImage 생성
//SPI.cpp → SPI 통신
//Palettes.cpp → 색상 컬러맵
//MyLabel.cpp → 이미지 표시 QLabel

#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QMessageBox>

#include <QColor>
#include <QLabel>
#include <QtDebug>
#include <QString>
#include <QPushButton>

#include "LeptonThread.h"   // Lepton SPI 읽는 스레드
#include "MyLabel.h"        // 화면에 이미지 표시용 라벨(커스텀)


// -------------------------------------------------------
// 사용법 출력 함수
// -------------------------------------------------------
void printUsage(char *cmd) {
    char *cmdname = basename(cmd);
    printf(
        "Usage: %s [OPTION]...\n"
        " -h      display this help and exit\n"
        " -cm x   select colormap\n"
        "           1 : rainbow\n"
        "           2 : grayscale\n"
        "           3 : ironblack [default]\n"
        " -tl x   select type of Lepton\n"
        "           2 : Lepton 2.x [default]\n"
        "           3 : Lepton 3.x\n"
        " -ss x   SPI bus speed [MHz] (10 - 30)\n"
        "           20 : 20MHz [default]\n"
        " -min x  override minimum value for scaling (0 - 65535)\n"
        " -max x  override maximum value for scaling (0 - 65535)\n"
        " -d x    log level (0-255)\n",
        cmdname, cmdname
    );
    return;
}


// -------------------------------------------------------
// 메인 함수
// -------------------------------------------------------
int main(int argc, char **argv)
{
    // -----------------------------
    // 기본 설정값
    // -----------------------------
    int typeColormap = 3;   // ironblack 기본값
    int typeLepton = 2;     // Lepton 2.x 기본값
    int spiSpeed = 20;      // 20MHz 기본값
    int rangeMin = -1;      // 자동 스케일링
    int rangeMax = -1;      // 자동 스케일링
    int loglevel = 0;       // 로그 레벨 기본값

    // -----------------------------
    // 명령줄 인자 파싱
    // -----------------------------
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            exit(0);
        }

        // 로그 레벨
        else if (strcmp(argv[i], "-d") == 0) {
            int val = 3;
            if ((i + 1 != argc) && (strncmp(argv[i + 1], "-", 1) != 0)) {
                val = std::atoi(argv[i + 1]);
                i++;
            }
            loglevel = val & 0xFF;
        }

        // 컬러맵 지정
        else if (strcmp(argv[i], "-cm") == 0 && i + 1 != argc) {
            int val = std::atoi(argv[i + 1]);
            if (val == 1 || val == 2) {
                typeColormap = val;
                i++;
            }
        }

        // Lepton 버전 선택
        else if (strcmp(argv[i], "-tl") == 0 && i + 1 != argc) {
            int val = std::atoi(argv[i + 1]);
            if (val == 3) {
                typeLepton = val;
                i++;
            }
        }

        // SPI 속도
        else if (strcmp(argv[i], "-ss") == 0 && i + 1 != argc) {
            int val = std::atoi(argv[i + 1]);
            if (10 <= val && val <= 30) {
                spiSpeed = val;
                i++;
            }
        }

        // 최소 스케일링 값
        else if (strcmp(argv[i], "-min") == 0 && i + 1 != argc) {
            int val = std::atoi(argv[i + 1]);
            if (0 <= val && val <= 65535) {
                rangeMin = val;
                i++;
            }
        }

        // 최대 스케일링 값
        else if (strcmp(argv[i], "-max") == 0 && i + 1 != argc) {
            int val = std::atoi(argv[i + 1]);
            if (0 <= val && val <= 65535) {
                rangeMax = val;
                i++;
            }
        }
    }


    // -------------------------------------------------------
    // Qt 애플리케이션 객체 생성
    // -------------------------------------------------------
    QApplication a(argc, argv);

    // UI 메인 위젯 생성
    QWidget *myWidget = new QWidget;
    myWidget->setGeometry(400, 300, 340, 290);

    // -------------------------------------------------------
    // 1) 초기 Placeholder 이미지 만들기
    // -------------------------------------------------------
    QImage myImage(320, 240, QImage::Format_RGB888);
    QRgb red = qRgb(255, 0, 0);

    // 화면 왼쪽 위 80x60 영역을 빨간색으로 채움
    // (단순 placeholder)
    for (int i = 0; i < 80; i++) {
        for (int j = 0; j < 60; j++) {
            myImage.setPixel(i, j, red);
        }
    }

    // -------------------------------------------------------
    // 2) MyLabel을 화면에 올리고 이미지 표시
    // -------------------------------------------------------
    MyLabel myLabel(myWidget);
    myLabel.setGeometry(10, 10, 320, 240);
    myLabel.setPixmap(QPixmap::fromImage(myImage));


    // -------------------------------------------------------
    // 3) FFC 버튼 만들기
    // -------------------------------------------------------
    QPushButton *button1 = new QPushButton("Perform FFC", myWidget);
    button1->setGeometry(320/2 - 50, 290 - 35, 100, 30);


    // -------------------------------------------------------
    // 4) Lepton SPI 데이터 읽는 전용 스레드 생성
    // -------------------------------------------------------
    LeptonThread *thread = new LeptonThread();
    thread->setLogLevel(loglevel);
    thread->useColormap(typeColormap);
    thread->useLepton(typeLepton);
    thread->useSpiSpeedMhz(spiSpeed);

    // 자동 스케일링
    thread->setAutomaticScalingRange();
    if (rangeMin >= 0) thread->useRangeMinValue(rangeMin);
    if (rangeMax >= 0) thread->useRangeMaxValue(rangeMax);

    // 스레드 → UI 이미지 업데이트 연결
    QObject::connect(thread, SIGNAL(updateImage(QImage)),
                     &myLabel, SLOT(setImage(QImage)));

    // FFC 버튼 → 스레드의 performFFC() 호출
    QObject::connect(button1, SIGNAL(clicked()),
                     thread, SLOT(performFFC()));

    // 스레드 실행 시작 (Lepton SPI 프레임 읽기 시작)
    thread->start();


    // -------------------------------------------------------
    // UI 표시
    // -------------------------------------------------------
    myWidget->show();

    return a.exec();
}
