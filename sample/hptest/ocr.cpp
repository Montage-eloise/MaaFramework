#include <meojson/json.hpp>

#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "MaaFramework/MaaAPI.h"
#include "MaaToolkit/MaaToolkitAPI.h"
#include "Utils/Buffer/ImageBuffer.hpp"
#include <opencv2/opencv.hpp>
#include <random>

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
MaaController* create_win32_controller();
MaaBool my_action(
    MaaContext* context,
    MaaTaskId task_id,
    const char* node_name,
    const char* custom_action_name,
    const char* custom_action_param,
    MaaRecoId reco_id,
    const MaaRect* box,
    void* trans_arg);

int main()
{
    std::string user_path = "./";
    MaaToolkitConfigInitOption(user_path.c_str(), "{}");

    auto controller_handle = create_win32_controller();
    auto ctrl_id = MaaControllerPostConnection(controller_handle);

    auto resource_handle = MaaResourceCreate(nullptr, nullptr);
    std::string resource_dir = "./resource";

    // auto controller_handle = MaaDbgControllerCreate(
    //     testing_path.string().c_str(),
    //     result_path.string().c_str(),
    //     MaaDbgControllerType_CarouselImage,
    //     "{}",
    //     nullptr,
    //     nullptr);
    auto res_id = MaaResourcePostBundle(resource_handle, resource_dir.c_str());
    MaaControllerWait(controller_handle, ctrl_id);
    MaaResourceWait(resource_handle, res_id);

    auto tasker_handle = MaaTaskerCreate(nullptr, nullptr);
    MaaTaskerBindResource(tasker_handle, resource_handle);
    MaaTaskerBindController(tasker_handle, controller_handle);
    auto destroy = [&]() {
        MaaTaskerDestroy(tasker_handle);
        MaaResourceDestroy(resource_handle);
        MaaControllerDestroy(controller_handle);
    };
    // {
    //     auto failed_id = MaaTaskerPostTask(tasker_handle, "_NotExists_", "{}");
    //     auto failed_status = MaaTaskerWait(tasker_handle, failed_id);
    //     if (failed_id == MaaInvalidId || failed_status != MaaStatus_Failed) {
    //         std::cout << "Failed to detect invalid task" << std::endl;
    //         return false;
    //     }
    // }
    if (!MaaTaskerInited(tasker_handle)) {
        std::cout << "Failed to init MAA" << std::endl;
        destroy();
        return -1;
    }
    MaaResourceRegisterCustomRecognition(resource_handle, "MyReco", my_reco, nullptr);
    MaaResourceRegisterCustomAction(resource_handle, "MyAct", &my_action, nullptr);

    json::value task_param {
        { "MyTask", json::object { { "action", "Custom" }, { "custom_action", "MyAct" }, { "custom_action_param", "2158,2782" } } }
    };
    std::string task_param_str = task_param.to_string();

    auto task_id = MaaTaskerPostTask(tasker_handle, "MyTask", task_param_str.c_str());
    auto status = MaaTaskerWait(tasker_handle, task_id);

    destroy();

    return status == MaaStatus_Succeeded;
}

static cv::Rect ScaleRect(const cv::Rect& rect, cv::Size fromSize, cv::Size toSize)
{
    double scaleX = static_cast<double>(toSize.width) / fromSize.width;
    double scaleY = static_cast<double>(toSize.height) / fromSize.height;
    return cv::Rect(
        static_cast<int>(rect.x * scaleX),
        static_cast<int>(rect.y * scaleY),
        static_cast<int>(rect.width * scaleX),
        static_cast<int>(rect.height * scaleY));
}

struct Point
{
    int x;
    int y;
};

static std::mt19937 rng(std::random_device {}()); // 静态随机数生成器，初始化一次

// 模拟返回起点点击
Point ReturnToStart(const Point& current, const Point& start, const Point& screenCenter, double maxOffsetThreshold = 1000.0)
{
    double dx = start.x - current.x;
    double dy = start.y - current.y;
    double len = std::sqrt(dx * dx + dy * dy);
    double logicToPixelScale = 38.0;
    // 阈值判断：距离太大，认为异常，直接返回空
    if (len == 0 || len > maxOffsetThreshold * logicToPixelScale) {
        return screenCenter;
    }

    double len_px = len * logicToPixelScale;
    double nx = dx / len;
    double ny = dy / len;

    double screenDiagonal = std::sqrt(screenCenter.x * 2 * screenCenter.x * 2 + screenCenter.y * 2 * screenCenter.y * 2);

    const double minDistance = 20.0;
    const double maxDistance = 120.0;

    double baseDistance = maxDistance;
    if (len_px < screenDiagonal) {
        baseDistance = minDistance + (maxDistance - minDistance) * (len_px / screenDiagonal);
    }

    // 用 baseDistance ± 10 生成随机点击距离
    std::uniform_real_distribution<double> dist(baseDistance - 10.0, baseDistance + 10.0);
    double clickDistance = dist(rng);

    Point clickPoint = { (int)(screenCenter.x + nx * clickDistance), int(screenCenter.y + ny * clickDistance) };

    return clickPoint;
}

// 工具函数：从字符串 "x,y" 提取 Point
Point ParseTextToPoint(const std::string& text)
{
    Point pt {};
    std::string number;
    std::vector<std::string> parts;

    for (char c : text) {
        if (isdigit(c)) {
            number += c;
        }
        else if (!number.empty()) {
            parts.push_back(number);
            number.clear();
        }
    }
    if (!number.empty()) {
        parts.push_back(number);
    }

    if (parts.size() >= 2) {
        try {
            pt.x = std::stoi(parts[0]);
            pt.y = std::stoi(parts[1]);
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to parse OCR point: " << e.what() << std::endl;
        }
    }
    else {
        std::cerr << "OCR point parse failed: not enough parts\n";
    }

    return pt;
}

// 主处理函数
std::optional<Point> ProcessDetailTextAndClick(const char* detail_string, const char* start, const Point& screenCenter)
{
    try {
        auto result = json::parse(detail_string);

        if (result && result->contains("best") && !(*result)["best"].is_null()) {
            const auto& best = (*result)["best"];
            if (best.contains("text") && best.at("text").is_string()) {
                std::string text = best.at("text").as_string();
                Point current = ParseTextToPoint(text);
                Point startPoint = ParseTextToPoint(start);
                return ReturnToStart(current, startPoint, screenCenter);
            }
            else {
                std::cerr << "best.text not found or not a string.\n";
            }
        }
        else {
            std::cerr << "No 'best' found or it is null.\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to parse or process JSON: " << e.what() << "\n";
    }
    return std::nullopt; // 返回默认值
}

static bool check_ocr(const char* detail_string)
{
    try {
        auto result = json::parse(detail_string);

        if (result && result->contains("best") && !(*result)["best"].is_null()) {
            const auto& best = (*result)["best"];
            if (best.contains("text") && best.at("text").is_string()) {
                return true;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to parse or process JSON: " << e.what() << "\n";
        return false;
    }
    return false;
}

MaaBool my_reco(
    MaaContext* context,
    MaaTaskId task_id,
    const char* node_name,
    const char* custom_recognition_name,
    const char* custom_recognition_param,
    const MaaImageBuffer* image,
    const MaaRect* roi,
    void* trans_arg,
    /* out */ MaaRect* out_box,
    /* out */ MaaStringBuffer* out_detail)
{
    // 修改为目标ROI
    cv::Rect hpBarRect(659, 155, 64, 17);

    cv::Rect scaled = ScaleRect(hpBarRect, { 800, 600 }, { MaaImageBufferWidth(image), MaaImageBufferHeight(image) });
    json::array roi_array = { scaled.x, scaled.y, scaled.width, scaled.height };

    json::value pp_override { { "BATTLE", json::object { { "recognition", "OCR" }, { "roi", roi_array } } } };
    std::string pp_override_str = pp_override.to_string();

    MaaRecoId my_reco_id = MaaContextRunRecognition(context, "BATTLE", pp_override_str.c_str(), image);

    auto tasker = MaaContextGetTasker(context);
    MaaTaskerGetRecognitionDetail(tasker, my_reco_id, nullptr, nullptr, nullptr, out_box, out_detail, nullptr, nullptr);

    auto detail_string = MaaStringBufferGet(out_detail);
    return check_ocr(detail_string);
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
    cv::Mat image = image_buffer->get();
    // 坐标OCR
    cv::Rect hpBarRect(659, 155, 64, 17);
    cv::Rect scaled = ScaleRect(hpBarRect, { 800, 600 }, { image.cols, image.rows });
    json::array roi_array = { scaled.x, scaled.y, scaled.width, scaled.height };

    json::value pp_override { { "POS", json::object { { "recognition", "OCR" }, { "roi", roi_array } } } };
    std::string pp_override_str = pp_override.to_string();

    MaaRecoId my_reco_id = MaaContextRunRecognition(context, "POS", pp_override_str.c_str(), image_buffer);

    MaaTaskerGetRecognitionDetail(tasker, my_reco_id, nullptr, nullptr, nullptr, out_box, out_detail, nullptr, nullptr);

    auto detail_string = MaaStringBufferGet(out_detail);
    auto start = custom_action_param;
    auto pt = ProcessDetailTextAndClick(detail_string, start, { 400, 300 });

    if (pt) {
        std::cout << "Click point: " << pt->x << ", " << pt->y << std::endl;
        MaaControllerPostClick(controller, pt->x, pt->y);
    }
    // 可视化
    cv::rectangle(image, scaled, cv::Scalar(0, 255, 0), 2);
    cv::putText(
        image,
        std::to_string(1) + "%",
        cv::Point(scaled.x, scaled.y - 5),
        cv::FONT_HERSHEY_SIMPLEX,
        0.5,
        cv::Scalar(255, 255, 255),
        1);

    cv::imshow("HP Detection", image);
    cv::waitKey(0);
    MaaImageBufferDestroy(image_buffer);
    MaaRectDestroy(out_box);
    MaaStringBufferDestroy(out_detail);

    return true;
}

MaaController* create_win32_controller()
{
    void* hwnd = nullptr; // It's a HWND, you can find it by yourself without MaaToolkit API

    auto list_handle = MaaToolkitDesktopWindowListCreate();
    auto destroy = [&]() {
        MaaToolkitDesktopWindowListDestroy(list_handle);
    };

    MaaToolkitDesktopWindowFindAll(list_handle);

    size_t size = MaaToolkitDesktopWindowListSize(list_handle);

    if (size == 0) {
        std::cout << "No window found" << std::endl;

        destroy();
        return nullptr;
    }

    for (size_t i = 0; i < size; ++i) {
        auto window_handle = MaaToolkitDesktopWindowListAt(list_handle, i);
        std::string class_name = MaaToolkitDesktopWindowGetClassName(window_handle);
        std::string window_name = MaaToolkitDesktopWindowGetWindowName(window_handle);

        if (window_name == "Pirate King Online") {
            hwnd = MaaToolkitDesktopWindowGetHandle(window_handle);
            break;
        }
    }

    // create controller by hwnd
    auto controller_handle = MaaWin32ControllerCreate(hwnd, MaaWin32ScreencapMethod_GDI, MaaWin32InputMethod_Seize, nullptr, nullptr);

    destroy();
    return controller_handle;
}
