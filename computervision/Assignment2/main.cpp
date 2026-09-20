/**
 * [M0005-1] CV Assignment #2. 동전 찾기.
 * 202021088 안유빈
 */

 // 로딩할 이미지를 변경이 필요하거든 여기를 수정하시면 됩니다
#define PATH_IMG_SRC "coins0.jpg"

// 한번에 비교하려면 아래를 true로 만드시면 6개 이미지를 전부 비교합니다.
#define COMPARE_ALL false
// (한번에 비교할때 사용할 파일 갯수)
#define COMPARE_ALL_NUM 6

// 아래는 include들과 매직넘버들...
#include <stdio.h>
#include <iostream>
#include <utility>
#include <opencv2/opencv.hpp>

using namespace cv;

struct CircleEntry {
    int x;
    int y;
    float radius;
};

inline size_t hashPos(int x, int y, int wid) {
    return x + static_cast<size_t>(y) * wid;
}

int main() {
    // 이미지 불러오기 & 원 그릴 이미지 준비
    std::vector<Mat> imgs, grays, results;

    if (COMPARE_ALL) {
        for (auto i = 0; i < COMPARE_ALL_NUM; i++) {
            imgs.push_back(imread(std::string("coins") + std::to_string(i) + ".jpg", ImreadModes::IMREAD_COLOR_BGR));
        }
    }
    else {
        imgs.push_back(imread(PATH_IMG_SRC, ImreadModes::IMREAD_COLOR_BGR));
    }

    for (auto i = 0; i < imgs.size(); i++) {
        Mat current = imgs[i],
            tmpGray = current.clone();
        std::vector<Vec3f> circles;
        int currentWidth = current.cols;

        // OpenCV가 좋아하는 이미지 형식으로 변환
        cvtColor(current, tmpGray, COLOR_BGR2GRAY);

        // 동전 세기
        HoughCircles(tmpGray, circles, HOUGH_GRADIENT_ALT, 1.5, 50, 150, 0.70, 40, 0);
        //HoughCircles(tmpGray, circles, HOUGH_GRADIENT, 1, 40, 140, 150, 40, 0);

        // 필터링: 동심원 (위치가 정확히 일치하는) 원형을 가장 큰 지름만을 `circleMaxima`에 남겨서 동심원을 없애봅시다
        // 원의 x/y좌표로 만들어진 hash로 인덱싱하는 unordered_map을 사용해, 지름이 (이전) 최대지름보다 클 경우에 새 지름을 갱신시켜, 자연스럽게 동일 좌표를 가진 원 중 큰 녀석만 남깁니다
        std::unordered_map<int, CircleEntry> circleMaxima;
        for (int j = 0; j < circles.size(); j++) {
            Vec3f c = circles[j];
            int cx = c[0],
                cy = c[1];
            size_t key = hashPos(cx, cy, currentWidth); // x + static_cast<size_t>(y) * wid;
            auto val = circleMaxima.find(key);
            float crad = c[2], cradPrev = -9999.0;

            if (val != circleMaxima.end()) {
                cradPrev = val->second.radius;
            }

            if (crad > cradPrev) {
                circleMaxima[key] = CircleEntry { cx, cy, crad };
            }

            // DEBUG: 동심원도 그리기. 어떤 동심원이 없어졌는지 확인차
            //circle(current, Point(cx, cy), crad, Scalar(0, 100, 0), 2, LINE_8);
        }

        // 결과 그리기
        std::cout << (i + 1) << "번째 이미지 감지 결과: " << circleMaxima.size() << "개 동전" << std::endl;
        for (auto iter = circleMaxima.begin(); iter != circleMaxima.end(); iter++) {
            float   cx = iter->second.x,
                    cy = iter->second.y,
                    crad = iter->second.radius;

            circle(current, Point(cx, cy), 0, Scalar(255, 0, 0), 2, LINE_8);
            circle(current, Point(cx, cy), crad, Scalar(0, 0, 255), 2, LINE_8);
        }

        // 결과 보여주기
        if (COMPARE_ALL) {
            Mat preview, previewGray;
            float scale = 200.0 / current.rows;

            resize(current, preview, Size(scale * current.cols, scale * current.rows));
            resize(tmpGray, previewGray, Size(scale * current.cols, scale * current.rows));
            results.push_back(preview);
            grays.push_back(previewGray);
        }
        else {
            results.push_back(current);
        }
    }
    
    if (COMPARE_ALL) {
        Mat composite, resultsConcat, resultsConcatGray;

        hconcat(results, resultsConcat);
        hconcat(grays, resultsConcatGray);

        cvtColor(resultsConcatGray, resultsConcatGray, COLOR_GRAY2BGR);
        vconcat(resultsConcat, resultsConcatGray, composite);
        imshow("Detected Coins", composite);
    }
    else {
        imshow("Detected Coins", results[0]);
    }

    waitKey(0);
    return 0;
}