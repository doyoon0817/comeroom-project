#ifndef PALETTES_H          // 이 헤더파일이 아직 포함되지 않았다면(중복 방지 시작)
#define PALETTES_H          // PALETTES_H 를 정의해서 다음에는 중복 포함되지 않게 함


// ---------------------------------------------------------
//  외부에 있는 colormap 배열들을 "선언"만 하는 부분
//     이 파일에는 실제 배열 데이터가 없음 (정의는 .cpp 에 있음)
//     extern 은 "다른 곳에 있음, 여기서 쓰기만 하겠다" 라는 뜻
// ---------------------------------------------------------

extern const int colormap_rainbow[];     // Rainbow 컬러맵 배열(실제 데이터는 다른 .cpp 파일에 있음)
extern const int colormap_grayscale[];   // Grayscale(흑백) 컬러맵 배열(역시 선언만 존재)
extern const int colormap_ironblack[];   // IronBlack 컬러맵 배열(열화상에서 유명한 팔레트)


// ---------------------------------------------------------
//  컬러맵 크기(길이)를 리턴하는 함수 선언
//    이 함수들도 실제 구현은 Palettes.cpp 또는 다른 .cpp 에 있음
//    여기서는 사용 가능하도록 선언만 하는 것
// ---------------------------------------------------------

extern int get_size_colormap_rainbow();     // rainbow 배열의 RGB 값 개수를 반환하는 함수
extern int get_size_colormap_grayscale();   // grayscale 배열의 크기를 반환하는 함수
extern int get_size_colormap_ironblack();   // ironblack 배열의 크기를 반환하는 함수


#endif   // PALETTES_H        // 헤더 가드 끝(여기까지가 중복 방지 영역)
