#include "./include/forward.h"

import maa.core;
import napi;
import stdmock;

import maa.nodejs.info;
import maa.nodejs.utils;
import maa.nodejs.wrapper;
import maa.nodejs.instance.context;
import maa.nodejs.instance.controller;
import maa.nodejs.instance.resource;
import maa.nodejs.instance.tasker;
import maa.nodejs.utility.utility;
import maa.nodejs.toolkit.config;
import maa.nodejs.toolkit.find;
import maa.nodejs.toolkit.pi;
import maa.nodejs.agent;

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    auto extCtx = Napi::External<ExtContextInfo>::New(env, new ExtContextInfo, &DeleteFinalizer<ExtContextInfo*>);
    exports["__context"] = extCtx;

    load_instance_context(env, exports, extCtx);
    load_instance_controller(env, exports, extCtx);
    load_instance_resource(env, exports, extCtx);
    load_instance_tasker(env, exports, extCtx);
    load_utility_utility(env, exports, extCtx);

    load_toolkit_config(env, exports, extCtx);
    load_toolkit_find(env, exports, extCtx);
    load_toolkit_pi(env, exports, extCtx);

    load_agent(env, exports, extCtx);

#define DE(prefix, key) prefix##_obj[#key] = JSConvert<prefix>::to_value(env, prefix##Enum::prefix##_##key)
#define DEM(prefix, key) prefix##_obj[#key] = JSConvert<prefix>::to_value(env, _##prefix##_##key)

    auto MaaStatus_obj = Napi::Object::New(env);
    DE(MaaStatus, Invalid);
    DE(MaaStatus, Pending);
    DE(MaaStatus, Running);
    DE(MaaStatus, Succeeded);
    DE(MaaStatus, Failed);
    exports["Status"] = MaaStatus_obj;

    auto MaaLoggingLevel_obj = Napi::Object::New(env);
    DE(MaaLoggingLevel, Off);
    DE(MaaLoggingLevel, Fatal);
    DE(MaaLoggingLevel, Error);
    DE(MaaLoggingLevel, Warn);
    DE(MaaLoggingLevel, Info);
    DE(MaaLoggingLevel, Debug);
    DE(MaaLoggingLevel, Trace);
    DE(MaaLoggingLevel, All);
    exports["LoggingLevel"] = MaaLoggingLevel_obj;

    auto MaaAdbScreencapMethod_obj = Napi::Object::New(env);
    DEM(MaaAdbScreencapMethod, EncodeToFileAndPull);
    DEM(MaaAdbScreencapMethod, Encode);
    DEM(MaaAdbScreencapMethod, RawWithGzip);
    DEM(MaaAdbScreencapMethod, RawByNetcat);
    DEM(MaaAdbScreencapMethod, MinicapDirect);
    DEM(MaaAdbScreencapMethod, MinicapStream);
    DEM(MaaAdbScreencapMethod, EmulatorExtras);
    DEM(MaaAdbScreencapMethod, All);
    DEM(MaaAdbScreencapMethod, Default);
    exports["AdbScreencapMethod"] = MaaAdbScreencapMethod_obj;

    auto MaaAdbInputMethod_obj = Napi::Object::New(env);
    DEM(MaaAdbInputMethod, AdbShell);
    DEM(MaaAdbInputMethod, MinitouchAndAdbKey);
    DEM(MaaAdbInputMethod, Maatouch);
    DEM(MaaAdbInputMethod, EmulatorExtras);
    DEM(MaaAdbInputMethod, All);
    DEM(MaaAdbInputMethod, Default);
    exports["AdbInputMethod"] = MaaAdbInputMethod_obj;

    auto MaaWin32ScreencapMethod_obj = Napi::Object::New(env);
    DEM(MaaWin32ScreencapMethod, GDI);
    DEM(MaaWin32ScreencapMethod, FramePool);
    DEM(MaaWin32ScreencapMethod, DXGI_DesktopDup);
    exports["Win32ScreencapMethod"] = MaaWin32ScreencapMethod_obj;

    auto MaaWin32InputMethod_obj = Napi::Object::New(env);
    DEM(MaaWin32InputMethod, Seize);
    DEM(MaaWin32InputMethod, SendMessage);
    exports["Win32InputMethod"] = MaaWin32InputMethod_obj;

    auto MaaDbgControllerType_obj = Napi::Object::New(env);
    DEM(MaaDbgControllerType, CarouselImage);
    DEM(MaaDbgControllerType, ReplayRecording);
    exports["DbgControllerType"] = MaaDbgControllerType_obj;

    auto MaaInferenceDevice_obj = Napi::Object::New(env);
    DE(MaaInferenceDevice, CPU);
    DE(MaaInferenceDevice, Auto);
    exports["InferenceDevice"] = MaaInferenceDevice_obj;

    auto MaaInferenceExecutionProvider_obj = Napi::Object::New(env);
    DE(MaaInferenceExecutionProvider, Auto);
    DE(MaaInferenceExecutionProvider, CPU);
    DE(MaaInferenceExecutionProvider, DirectML);
    DE(MaaInferenceExecutionProvider, CoreML);
    DE(MaaInferenceExecutionProvider, CUDA);
    exports["InferenceExecutionProvider"] = MaaInferenceExecutionProvider_obj;

    return exports;
}
