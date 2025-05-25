#include "MaaFramework/MaaAPI.h"
#include "Utils/Buffer/ImageBuffer.hpp"
#include <cmath>
#include <iostream>
#include <meojson/json.hpp>
#include <opencv2/opencv.hpp>
#include <random>
#include <windows.h>
#ifdef _MSC_VER
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4189) // local variable is initialized but not referenced
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

MaaBool my_action(
    MaaContext* context,
    MaaTaskId task_id,
    const char* node_name,
    const char* custom_action_name,
    const char* custom_action_param,
    MaaRecoId reco_id,
    const MaaRect* box,
    void* trans_arg);

struct Point
{
    double x;
    double y;
};

// 读取血条百分比
int DetectHPBarPercent(const cv::Mat& image, cv::Rect roi)
{
    cv::Mat hpBar = image(roi);
    cv::Mat hsv;
    cv::cvtColor(hpBar, hsv, cv::COLOR_BGR2HSV);

    cv::Mat mask1, mask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 70, 50), cv::Scalar(180, 255, 255), mask2);
    redMask = mask1 | mask2;

    int filledWidth = 0;
    for (int x = 0; x < redMask.cols; ++x) {
        int colNonZero = cv::countNonZero(redMask.col(x));
        if (colNonZero > redMask.rows / 2) {
            filledWidth++;
        }
        else {
            break;
        }
    }

    int percent = (int)((double)filledWidth / redMask.cols * 100);
    return std::clamp(percent, 0, 100);
}

// 检测小地图红点数量
int DetectRedDots(const cv::Mat& minimap)
{
    cv::Mat hsv, mask1, mask2;
    cv::cvtColor(minimap, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 100, 100), cv::Scalar(180, 255, 255), mask2);
    cv::Mat redMask = mask1 | mask2;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    return contours.size();
}

static std::mt19937 rng(std::random_device {}()); // 静态随机数生成器，初始化一次

// 模拟返回起点点击
Point ReturnToStart(const Point& current, const Point& start, const Point& screenCenter, double maxOffsetThreshold = 1000.0)
{
    double dx = start.x - current.x;
    double dy = start.y - current.y;
    double len = std::sqrt(dx * dx + dy * dy);
    // 阈值判断：距离太大，认为异常，直接返回空
    if (len == 0 || len > maxOffsetThreshold) {
        return screenCenter;
    }
    double nx = dx / len;
    double ny = dy / len;

    double screenDiagonal = std::sqrt(screenCenter.x * 2 * screenCenter.x * 2 + screenCenter.y * 2 * screenCenter.y * 2);

    const double minDistance = 20.0;
    const double maxDistance = 120.0;

    double baseDistance = maxDistance;
    if (len < screenDiagonal) {
        baseDistance = minDistance + (maxDistance - minDistance) * (len / screenDiagonal);
    }

    // 用 baseDistance ± 10 生成随机点击距离
    std::uniform_real_distribution<double> dist(baseDistance - 10.0, baseDistance + 10.0);
    double clickDistance = dist(rng);

    Point clickPoint = { screenCenter.x + nx * clickDistance, screenCenter.y + ny * clickDistance };

    return clickPoint;
}

MaaBool my_action(
    MaaContext* context,
    MaaTaskId task_id,
    const char* node_name,
    const char* custom_action_name,
    const char* custom_action_param,
    MaaRecoId reco_id,
    const MaaRect* box,
    void* trans_arg)
{
    auto image_buffer = MaaImageBufferCreate();
    auto tasker = MaaContextGetTasker(context);
    auto controller = MaaTaskerGetController(tasker);
    MaaControllerCachedImage(controller, image_buffer);

    auto out_box = MaaRectCreate();
    auto out_detail = MaaStringBufferCreate();

    const Point screenCenter = { 400, 300 }; // 假设分辨率 800x600
    const cv::Mat image = image_buffer->get();
    cv::Rect hpBarRect(74, 24, 112, 10);
    hpBarRect &= cv::Rect(0, 0, image.cols, image.rows);
    int hp = DetectHPBarPercent(image, hpBarRect);
    if (hp <= 60) {
        MaaControllerPostPressKey(controller, 114); // 按F3吃蛋糕
    }

    // 识别是否正在攻击

    // 如果在休息则按F2起身，寻找目标攻击

    // 范围内没有目标,尝试回原坐标
    json::value pp_override { { "寻找坐标", json::object { { "recognition", "OCR" }, { "roi", json::array { 659, 155, 53, 17 } } } } };
    std::string pp_override_str = pp_override.to_string();

    MaaRecoId my_reco_id = MaaContextRunRecognition(context, "MyColorMatching", pp_override_str.c_str(), image_buffer);
    MaaTaskerGetRecognitionDetail(tasker, my_reco_id, nullptr, nullptr, nullptr, out_box, out_detail, nullptr, nullptr);

    auto detail_string = MaaStringBufferGet(out_detail);
    std::ignore = detail_string;
    // 如果距离原坐标很近，按INSERT键休息

    MaaImageBufferDestroy(image_buffer);
    MaaRectDestroy(out_box);
    MaaStringBufferDestroy(out_detail);

    return true;
}
