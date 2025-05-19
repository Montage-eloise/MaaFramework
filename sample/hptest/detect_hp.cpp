#include <algorithm>
#include <iostream>
#include <opencv2/opencv.hpp>

// 更鲁棒的红色血条检测
int DetectHPBarPercent(const cv::Mat& image, cv::Rect roi)
{
    cv::Mat hpBar = image(roi);
    cv::Mat hsv;
    cv::cvtColor(hpBar, hsv, cv::COLOR_BGR2HSV);

    // 放宽红色 HSV 范围（支持暗红）
    cv::Mat mask1, mask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 50, 20), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 50, 20), cv::Scalar(180, 255, 255), mask2);
    redMask = mask1 | mask2;

    // DEBUG：显示红色像素总数
    std::cout << "Red pixels detected: " << cv::countNonZero(redMask) << std::endl;

    // 标记每一列是否为“红列”
    std::vector<bool> redCols(redMask.cols, false);
    for (int x = 0; x < redMask.cols; ++x) {
        int nonZero = cv::countNonZero(redMask.col(x));
        redCols[x] = (nonZero > 2); // 宽松阈值
    }

    // 计算最长连续红列
    int maxLen = 0, currLen = 0;
    for (bool isRed : redCols) {
        if (isRed) {
            currLen++;
            maxLen = std::max(maxLen, currLen);
        }
        else {
            currLen = 0;
        }
    }

    int percent = static_cast<int>((double)maxLen / redMask.cols * 100);
    return std::clamp(percent, 0, 100);
}

int main(int argc, char** argv)
{
    std::string imagePath = "1745584343_cap00003.png"; // 默认图片路径
    if (argc > 1) {
        imagePath = argv[1];
    }

    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
        return 1;
    }

    // Pascal VOC 标注框
    int xmin = 74;
    int ymin = 24;
    int xmax = 187;
    int ymax = 34;

    int width = xmax - xmin;
    int height = ymax - ymin;

    // 创建 ROI 区域
    cv::Rect hpBarRect(xmin, ymin, width, height);
    hpBarRect &= cv::Rect(0, 0, image.cols, image.rows);
    int hpPercent = DetectHPBarPercent(image, hpBarRect);
    // cv::Mat hpBar = image(hpBarRect);

    // cv::Scalar lowerRed(0, 0, 150);
    // cv::Scalar upperRed(100, 100, 255);
    // cv::Mat mask;
    // cv::inRange(hpBar, lowerRed, upperRed, mask);

    // double redPixels = cv::countNonZero(mask);
    // double hpPercent = redPixels / (hpBar.cols * hpBar.rows) * 100.0;

    std::cout << "Detected HP: " << hpPercent << "%" << std::endl;
    // 可视化
    cv::rectangle(image, hpBarRect, cv::Scalar(0, 255, 0), 2);
    cv::putText(
        image,
        std::to_string(hpPercent) + "%",
        cv::Point(hpBarRect.x, hpBarRect.y - 5),
        cv::FONT_HERSHEY_SIMPLEX,
        0.5,
        cv::Scalar(255, 255, 255),
        1);

    cv::imshow("HP Detection", image);
    cv::waitKey(0);
    return 0;
}
