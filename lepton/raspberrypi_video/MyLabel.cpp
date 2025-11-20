//MyLabel=QLabel 상속해 QImage 받아 화면에 띄우는 이미지 위젯
#include "MyLabel.h"

MyLabel::MyLabel(QWidget *parent) : QLabel(parent)// Qwidget 안에 포함된 라벨(이미지 표시 가능) 생성 & MyLabel은 QLabel을 상속했기에 QLabel 기능 사용 가능
{
}
MyLabel::~MyLabel()// 소멸자-> 화면 위젯 삭제시 호출
{
}

//when the system calls setImage, we'll set the label's pixmap 
//시스템이 setImage 함수를 호출시,이 QLabeld의 화면에 표시될 그림을 설정하겠다는 의미.

//Qt GUI에서 이미지를 화면에 내보내는 함수
void MyLabel::setImage(QImage image) { // mylabel 안에 setimage 함수 호출. (QImage image)은 함수 호출할 때 넘겨주는 값
  QPixmap pixmap = QPixmap::fromImage(image);//QImage->Qpixmap 변환
  //QImage: CPU 메모리에 있는 이미지 데이터
  //Qpixmap: GPU 메모리를 사용한 화면 출력용 이미지로 UI 스레드에서만 생성 가능
  int w = this->width();
  int h = this->height();
  //myLabel이 UI에서 실제로 차지하는 가로/세로 픽셀 크기 읽어옴
  setPixmap(pixmap.scaled(w, h, Qt::KeepAspectRatio)); // 라벨에 이미지 그리기
}
