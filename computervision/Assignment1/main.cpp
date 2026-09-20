/**
 * [M0005-1] CV Assignment #1. Multi-band Blending 실습.
 * 202021088 안유빈
 */

// 로딩할 이미지를 변경이 필요하거든 여기를 수정하십시오
#define PATH_IMG_APPLE "burt_apple.png"
#define PATH_IMG_ORANGE "burt_orange.png"
#define PATH_IMG_APPLE_MASK "burt_mask.png"
// Laplacian & Gaussian Pyramid 층 갯수
#define PYRAMID_DEPTH 5

// 아래는 include들과 매직넘버들...
#include <stdio.h>
#include <iostream>
#include <opencv2/opencv.hpp>

// (u8[0..255] -> f[0..1] 변환시 쓰이는 상수)
#define U8_NORMALIZE_FAC (1.0f / 255.0f)

using namespace cv;

/**
 * 교수님이 과제서 요청하신대로 `CV_32F` 포맷으로 변경하고 적절한 상숫값을 곱해 다루기 쉽게 [0..1] range로 normalize 시켜줍니다.
 */
Mat getConv(Mat src) {
    Mat dst;

    src.convertTo(dst, CV_32F);
    dst *= U8_NORMALIZE_FAC;
    return dst;
}

/**
 * 원하는 깊이만큼 Gaussian Pyramid를 만듭니다. 아마 피라미드 '층' 갯수만큼 Multi-band blending의 퀄리티가 좋아지겠지요
 * 첫 인덱스의 이미지는 원본 이미지랑 동일합니다
 */
std::vector<Mat> genGpyramids(Mat src, int depth) {
    std::vector<Mat> pyramids = std::vector<Mat>();
    Mat current = src.clone(),
        downsampled;

    pyramids.push_back(src);
    for (auto i = 0; i < depth; i++) {
        pyrDown(current, downsampled);
        pyramids.push_back(downsampled);

        current = downsampled;
    }

    // 직접 구현은 하긴 했는데 사실 OpenCV에서 자체적으로 `buildPyramid` 라고 간단하게 사용 가능한 함수를 지원해주네요?
    // 궁금하시다면 위 부분을 주석처리 시키고 이 부분을 주석 해제 하셔도 되는데, 동일한 결과가 나오긴 합니다...
    //buildPyramid(src, pyramids, depth);
    return pyramids;
}

/**
 * 원하는 깊이만큼 Laplacian Pyramid를 만듭니다. 최종적으로 (`depth + 1`) 갯수의 이미지가 생성됩니다. 아마 피라미드 '층' 갯수만큼 Multi-band blending의 퀄리티가 좋아지겠지요
 */
std::vector<Mat> genLpyramids(Mat src, int depth) {
    // Gaussian Pyramid처럼 OpenCV에서 Laplacian Pyramid를 생성하는건 바로 지원하지는 않고 직접 구현해야겠습니다. 교수님 강의녹화좀 참고할게요
    std::vector<Mat> pyramids = std::vector<Mat>();
    Mat current = src.clone(),
        downsampled;

    for (auto i = 0; i < depth; i++) {
        Mat resampled;

        pyrDown(current, downsampled);
        pyrUp(downsampled, resampled, current.size());
        pyramids.push_back(current - resampled);

        current = downsampled;
    }
    pyramids.push_back(downsampled);

    return pyramids;
}

/**
 * 주어진 Laplacian Pyramid (그리고 그걸 지지고 볶고 한 결과물을) 다시 합쳐서 원상복구 합니다.
 */
Mat reconstruct(std::vector<Mat>& pyramid) {
    int depth = pyramid.size();
    Mat resampled,
        sum = pyramid.back().clone();

    for (auto i = depth - 1; i > 0; i--) {
        Mat current = pyramid[i - 1];

        pyrUp(sum, resampled, current.size());
        sum = resampled + current;
    }

    return sum;
}

int main() {
    // 로드 + 전처리. CV_32F 포맷으로 변경
    Mat srcA = getConv(imread(PATH_IMG_APPLE, 1)),
        srcB = getConv(imread(PATH_IMG_ORANGE, 1)),
        maskApple = getConv(imread(PATH_IMG_APPLE_MASK, 1));

    std::cout << "imgA.size = " << srcA.size << std::endl << "imgB.size = " << srcB.size << "imgMask.size = " << maskApple.size << std::endl;
    if (srcA.size != srcB.size || maskApple.size != srcA.size) {
        std::cout << "[WARNING] Image size mismatch!!! (" << srcA.size << " vs " << srcB.size << " vs " << maskApple.size << ")" << std::endl;
    }

    // 오렌지를 위해서 마스크 만들기. 아마 과제 내용대로라면 `burt_mask`의 반전된 이미지겠지요
    Mat maskOrange = Scalar(1.0f, 1.0f, 1.0f) - maskApple;

    // 사과랑 오렌지로 Laplacian Pyramid 만들고 Mask들은 Gaussian Pyramid 만들기
    std::vector<Mat>    lpA = genLpyramids(srcA, PYRAMID_DEPTH),
                        lpB = genLpyramids(srcB, PYRAMID_DEPTH),
                        gpMaskA = genGpyramids(maskApple, PYRAMID_DEPTH),
                        gpMaskB = genGpyramids(maskOrange, PYRAMID_DEPTH),
                        added;

    std::cout << "lp size: (" << lpA.size() << ", " << lpB.size() << "), gp size: (" << gpMaskA.size() << ", " << gpMaskB.size() << ")" << std::endl;

    // Laplacian Pyramid를 Mask들이랑 잘 섞어봅시다
    for (auto i = 0; i < PYRAMID_DEPTH + 1; i++) {
        Mat lpAcurrent = lpA[i],
            lpBcurrent = lpB[i],
            maskAcurrent = gpMaskA[i].clone(),
            maskBcurrent = gpMaskB[i].clone();

        resize(maskAcurrent, maskAcurrent, lpAcurrent.size());
        resize(maskBcurrent, maskBcurrent, lpBcurrent.size());

        Mat blended = lpAcurrent.mul(maskAcurrent) + lpBcurrent.mul(maskBcurrent);

        added.push_back(blended);
    }
    
    // 그리고 원상복귀...
    Mat reconstructed = reconstruct(added);

    // 테스트. 이미지좀 띄워봅시다
    Mat allMasks[] = { maskApple, maskOrange },
        allSrcs[] = { srcA, srcB };
    Mat outMasks,
        outSrcs;

    hconcat(allMasks, 2, outMasks);
    hconcat(allSrcs, 2, outSrcs);
    //imshow(std::string("apple @ ") + PATH_IMG_APPLE, imgA);
    //imshow(std::string("orange @ ") + PATH_IMG_ORANGE, imgB);
    //imshow("masks", outMasks);
    imshow("apple & orange (src)", outSrcs);
    imshow("Multi-band blended", reconstructed);
    /*
    int i = 0;
    for (Mat img: lpA) {
        imshow(std::string("pyr #") + std::to_string(i) + " / (" + std::to_string(img.size().width) + "x" + std::to_string(img.size().height) + ")", img);
        i++;
    }
    */
    waitKey(0);
	return 0;
}