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

// 读取文件到字符串
std::string ReadFileToString(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs) {
        std::cout << "Failed to open file: " << filename << std::endl;
        return "";
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

int main()
{
    std::string user_path = "./";
    MaaToolkitConfigInitOption(user_path.c_str(), "{}");

    auto controller_handle = create_win32_controller();
    auto ctrl_id = MaaControllerPostConnection(controller_handle);

    auto resource_handle = MaaResourceCreate(nullptr, nullptr);
    std::string resource_dir = "./resource";
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
    if (!MaaTaskerInited(tasker_handle)) {
        std::cout << "Failed to init MAA" << std::endl;
        destroy();
        return -1;
    }
    // MaaResourceRegisterCustomRecognition(resource_handle, "MyReco", my_reco, nullptr);
    MaaResourceRegisterCustomAction(resource_handle, "MyAct", &my_action, nullptr);

    //{ "custom_action_param", "442,1594" }
    // json::value task_param { { "MyTask",
    //                            json::object { { "recognition", "DirectHit" },
    //                                           { "action", "Custom" },
    //                                           { "custom_action", "MyAct" },
    //                                           { "custom_action_param", "2158,2782" } } } };

    auto json_text = ReadFileToString("./ocr.json");
    // std::string task_param_str = json_text.to_string();

    auto task_id = MaaTaskerPostTask(tasker_handle, "Battle", json_text.c_str());
    auto status = MaaTaskerWait(tasker_handle, task_id);

    destroy();

    return status == MaaStatus_Succeeded;
}

static bool is_seating = false;
static int try_time = 0;
static int seating_time = 0;
static Point last_click_point = { 0, 0 };

// 读取血条百分比
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
    // std::cout << "Red pixels detected: " << cv::countNonZero(redMask) << std::endl;

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
std::optional<Point> ReturnToStart(const Point& current, const Point& start, const Point& screenCenter, double maxOffsetThreshold = 1000.0)
{
    double dx = start.x - current.x;
    double dy = start.y - current.y;
    double len = std::sqrt(dx * dx + dy * dy);
    constexpr double logicToPixelScale = 38.0;
    // 阈值判断：距离太大，认为异常，直接返回空
    if (len <= 4 || len > maxOffsetThreshold * logicToPixelScale) {
        return std::nullopt; // 返回空，表示不进行点击
    }

    double len_px = len * logicToPixelScale;
    double nx = dx / len;
    double ny = dy / len;

    double screenDiagonal = std::sqrt(screenCenter.x * screenCenter.x + screenCenter.y * screenCenter.y);

    double minDistance = 20;
    double maxDistance = 200;

    double baseDistance = maxDistance;
    if (len_px < screenDiagonal) {
        baseDistance = minDistance + (maxDistance - minDistance) * (len_px / screenDiagonal);
    }
    else {
        baseDistance = maxDistance;
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
    else if (parts.size() == 1) {
        std::string num_str = parts[0];
        if (text.length() == 8) {
            // 尝试按固定宽度切分一个长整数（如12345678 → 1234, 5678）
            pt.x = std::stoi(num_str.substr(0, num_str.length() / 2));
            pt.y = std::stoi(num_str.substr(num_str.length() / 2));
        }
        if (text.length() == 7) {
            // 尝试按固定宽度切分一个长整数（如1234567 → 123, 5678）
            pt.x = std::stoi(num_str.substr(0, 3));
            pt.y = std::stoi(num_str.substr(3));
        }
        else {
            std::cerr << "Not enough digits to split into x and y\n";
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
                if (current.x == 0 && current.y == 0) {
                    std::cerr << "Parsed point is (0, 0), skipping click.\n";
                    if (seating_time < 3) {
                        seating_time++;
                        return Point { 400, 300 };
                    }
                    else {
                        seating_time = 0;        // 重置坐下次数
                        return last_click_point; // 返回上次点击位置
                    }
                }
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

    auto destroy = [&]() {
        MaaImageBufferDestroy(image_buffer);
        MaaRectDestroy(out_box);
        MaaStringBufferDestroy(out_detail);
    };

    const cv::Mat image = image_buffer->get();

    // 1. 检查血条
    {
        cv::Rect hpBarRect(74, 24, 112, 10);
        auto scaledRect = ScaleRect(hpBarRect, { 800, 600 }, { image.cols, image.rows });
        int hp = DetectHPBarPercent(image, scaledRect);
        if (hp <= 60) {
            std::cout << "HP is low: " << hp << "%, eating cake..." << std::endl;
            MaaControllerPostPressKey(controller, 114); // F3 吃蛋糕
        }
    }

    // 2. OCR 识别是否正在攻击
    {
        if (try_time < 3) {
            cv::Rect attackBarRect(340, 1, 120, 20);
            auto scaled = ScaleRect(attackBarRect, { 800, 600 }, { image.cols, image.rows });
            json::array roi_array = { scaled.x, scaled.y, scaled.width, scaled.height };

            json::value override = { { "ATTACKING",
                                       { { "recognition", "OCR" }, { "roi", roi_array }, { "threshold", 0.6 }, { "only_rec", true } } } };

            std::string override_str = override.to_string();
            auto id = MaaContextRunRecognition(context, "ATTACKING", override_str.c_str(), image_buffer);
            if (id != MaaInvalidId) {
                destroy();
                try_time++;
                return true;
            }
            const char* detail = MaaStringBufferGet(out_detail);
            if (check_ocr(detail)) {
                destroy();
                return true;
            }
        }
        else {
            try_time = 0; // 重置尝试次数
        }
    }

    // 3. 使用模型识别寻找目标
    {
        json::value override = { { "BATTLEING",
                                   { { "recognition", "NeuralNetworkDetect" },
                                     { "model", "hunter.onnx" },
                                     { "threshold", 0.4 },
                                     { "order_by", "area" },
                                     { "labels", "BloodHunter" },
                                     { "expected", { 0 } } } } };

        std::string override_str = override.to_string();
        auto id = MaaContextRunRecognition(context, "BATTLEING", override_str.c_str(), image_buffer);
        MaaBool hit = false;

        MaaTaskerGetRecognitionDetail(tasker, id, nullptr, nullptr, &hit, out_box, nullptr, nullptr, nullptr);

        if (hit && id != MaaInvalidId) {
            if (is_seating) {
                MaaControllerPostPressKey(controller, 113); // F2 起身
                is_seating = false;
            }

            MaaControllerPostClick(controller, out_box->x + out_box->width / 2, out_box->y + out_box->height / 2);

            destroy();
            return true;
        }
    }

    // 4. 识别坐标文字进行点击或休息
    {
        cv::Rect posRect(659, 155, 64, 17);
        auto scaled = ScaleRect(posRect, { 800, 600 }, { image.cols, image.rows });
        json::array roi_array = { scaled.x, scaled.y, scaled.width, scaled.height };

        json::value override = { { "POS", { { "recognition", "OCR" }, { "roi", roi_array } } } };

        std::string override_str = override.to_string();
        auto id = MaaContextRunRecognition(context, "POS", override_str.c_str(), image_buffer);

        MaaTaskerGetRecognitionDetail(tasker, id, nullptr, nullptr, nullptr, out_box, out_detail, nullptr, nullptr);
        const char* detail = MaaStringBufferGet(out_detail);

        auto pt = ProcessDetailTextAndClick(detail, custom_action_param, { 400, 300 });
        if (pt) {
            std::cout << "Click point: " << pt->x << ", " << pt->y << std::endl;
            last_click_point = *pt;
            MaaControllerPostClick(controller, pt->x, pt->y);
        }
        else {
            std::cout << "Distance is too close to the original position, resting..." << std::endl;
            MaaControllerPostPressKey(controller, 45); // Insert 键休息
            is_seating = true;
        }
    }

    destroy();
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
