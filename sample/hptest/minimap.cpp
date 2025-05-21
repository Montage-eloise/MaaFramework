#include <iostream>
#include <opencv2/opencv.hpp>

// 检测小地图中红点数量（代表怪物）
int CountRedDotsInMinimap(const cv::Mat& image, cv::Rect minimapRect = cv::Rect(640, 20, 140, 140))
{
    cv::Mat minimap = image(minimapRect);
    cv::Mat hsv;
    cv::cvtColor(minimap, hsv, cv::COLOR_BGR2HSV);

    // 红色范围（HSV 两段）
    cv::Mat mask1, mask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 70, 50), cv::Scalar(180, 255, 255), mask2);
    redMask = mask1 | mask2;

    // 可选：形态学处理（去除小噪声）
    cv::erode(redMask, redMask, cv::Mat(), cv::Point(-1, -1), 1);
    cv::dilate(redMask, redMask, cv::Mat(), cv::Point(-1, -1), 2);

    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int redDotCount = 0;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area > 3.0 && area < 100.0) { // 可调节，过滤噪点和非红点
            redDotCount++;
        }
    }

    // 可视化调试
    cv::Mat debugView;
    cv::cvtColor(redMask, debugView, cv::COLOR_GRAY2BGR);
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area > 3.0 && area < 100.0) {
            cv::Rect bbox = cv::boundingRect(contour);
            cv::rectangle(minimap, bbox, cv::Scalar(0, 255, 0), 1);
        }
    }

    // 显示小地图检测结果
    cv::imshow("Minimap Detection", minimap);
    return redDotCount;
}

int main()
{
    cv::Mat img = cv::imread("1747571301_cap00011.png");
    if (img.empty()) {
        std::cerr << "Failed to load image" << std::endl;
        return 1;
    }

    // 根据你的截图，右上角小地图范围大概是这个（如有变化可手动调）
    cv::Rect minimapROI(img.cols - 160, 0, 160, 160);

    int count = CountRedDotsInMinimap(img, minimapROI);
    std::cout << "Red dots (monsters) detected: " << count << std::endl;

    cv::waitKey(0);
    return 0;
}
