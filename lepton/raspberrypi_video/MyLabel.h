#ifndef MYLABEL_H
#define MYLABEL_H

#include <QtCore>//Qt프로그램의 논리,기능을 담당
#include <QWidget>//모든 Qt UI 요소의 기본
#include <QLabel>//QT GUI 안에 텍스트/이미지 표시

// Qt 라이브러리 C++로 GUI를 만드는 라이브러리+개발

// 우리는 QLabel을 상속해서 새로운 라벨 클래스를 만든다.
// 목적: 스레드에서 QImage를 보내면, UI 스레드에서 안전하게 QPixmap으로 바꿔 화면에 표시하기 위함.
// 이유: Qt에서는 QPixmap을 스레드 간 직접 전달하면 오류가 날 수 있기 때문.
// 요약=> 스레드에서 Qpixmap을 직접 넘기면 Qt가 터져서, 대신 Qlmage를 넘기고, 화면 스레드에서 Qpixmap으로 바꿈.
class MyLabel : public QLabel {
  Q_OBJECT; // Qt의 signal/slot 기능을 사용하기 위함

  public:
// 생성자: 부모 위젯이 있으면 그걸 전달, 없으면 0 (기본값)
    MyLabel(QWidget *parent = 0);
    ~MyLabel();
//소멸자
  public slots:
// setImage(QImage)라는 슬롯 함수
// 다른 스레드에서 QImage를 emit(시그널을 보내!) 하면, 이 함수가 호출되어 이미지가 화면에 표시됨.
    void setImage(QImage);
};

#endif
