#include <opencv2/opencv.hpp>

int main()
{
    // 加载原图（确保是和 labelImg 标注时一样的原图）
    cv::Mat image = cv::imread("1747571301_cap00011.png");
    if (image.empty()) {
        std::cerr << "Image not found!" << std::endl;
        return -1;
    }

    // Pascal VOC 标注框
    int xmin = 74;
    int ymin = 24;
    int xmax = 187;
    int ymax = 34;

    int width = xmax - xmin;
    int height = ymax - ymin;

    // 创建 ROI 区域
    cv::Rect roi(xmin, ymin, width, height);

    // 安全检查：避免越界
    roi &= cv::Rect(0, 0, image.cols, image.rows);

    // 裁剪图像
    cv::Mat cropped = image(roi);

    // 显示
    cv::imshow("Cropped ROI", cropped);
    cv::rectangle(image, roi, cv::Scalar(0, 255, 0), 2);
    cv::imshow("With Box", image);
    cv::waitKey();

    return 0;
}
