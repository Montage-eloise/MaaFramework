#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "MaaFramework/MaaAPI.h"
#include "MaaToolkit/MaaToolkitAPI.h"

MaaController* create_win32_controller();

int main()
{
    std::string user_path = "./";
    MaaToolkitConfigInitOption(user_path.c_str(), "{}");

    auto controller_handle = create_win32_controller();
    auto ctrl_id = MaaControllerPostConnection(controller_handle);

    auto resource_handle = MaaResourceCreate(nullptr, nullptr);
    std::string resource_dir = R"(D:\Work\MaaFramework\sample\test\resource)";
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

    auto task_id = MaaTaskerPostTask(tasker_handle, "MyTask", R"({
            "MyTask": {
                "recognition": "OCR",
                "expected": ["配置"],
                "roi": [0, 53, 437, 311],
                "action": "Click"
            }
        })");
    MaaTaskerWait(tasker_handle, task_id);

    destroy();

    return 0;
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

        if (window_name == "Clash for Windows") {
            hwnd = MaaToolkitDesktopWindowGetHandle(window_handle);
            break;
        }
    }

    // create controller by hwnd
    auto controller_handle =
        MaaWin32ControllerCreate(hwnd, MaaWin32ScreencapMethod_GDI, MaaWin32InputMethod_SendMessage, nullptr, nullptr);

    destroy();
    return controller_handle;
}