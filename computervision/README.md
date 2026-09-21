# 컴퓨터비전 / 영상처리 과제물
2025-2학기에 진행했던 "컴퓨터비전" 과목의 과제물입니다. 총 3개의 과제를 진행했습니다.

# 프로젝트 환경
- Visual Studio 2022 환경에서 C++로 작성되었습니다
- OpenCV 5.0.0을 사용중입니다.

---------

# 프로젝트 설정 및 실행 가이드

## 설정하기
먼저 OpenCV 5.0.0의 DLL 경로가 (`opencv\build\x64\vc16\bin`) 가 PATH 환경변수에 설정된 것을 전제로 둡니다. 혹은 직접 DLL 파일을 각 프로젝트 실행 파일 경로 (`x64\Debug\...` 혹은 `x64\Release\...`) 에 붙여넣어도 됩니다.

다음으로 OpenCV 설치 경로를 설정합니다. Windows binary 기준, Visual Studio 프로젝트 속성 관리자에서 `opencv_setup` 속성의 `OPENCV_BASE_DIR` 매크로를 OpenCV를 설치한 경로 속 `build` 폴더로 설정합니다. (예시로 `D:\DEV_EXEC\libs\opencv`에 설치했다면 `D:\DEV_EXEC\libs\opencv\build`로...)

## 실행하기 및 외부 이미지 준비
모든게 정상적으로 설정되었다면, Visual Studio에서 바로 실행 가능합니다. 다만 과제물 모두 이미지를 입력으로 받기에, 별도로 이미지를 준비해주셔야 합니다.
- `Assignment1`
    - `burt_apple.png`, `burt_orange.png`: 동일한 크기의 원본 이미지들.
    - `burt_mask.png`: 마스크 이미지. 동일한 크기이며, 정확히 중간을 기준으로 흑/백으로 나뉘는 binary mask면 충분합니다.
- `Assignment2`
    - `coins0.jpg` ~ `coins5.jpg`: 동전 이미지.
- `Assignemnt3`
    - `img1.jpg`, `img2.jpg`: 동일한 위치에서 다른 각도로 찍은, 동일한 크기의 사진. 파노라마 사진처럼 생각하시면 편합니다.

---------

# 과제물 일람
- [Assignment1](Assignment1): 과제 1, Multi-band blending 구현
- [Assignment2](Assignment2): 과제 2, Hough Circle 동전 세기
- [Assignment3](Assignment3): 과제 3, Image stitching으로 파노라마 사진 생성