set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(hp_detector detect_hp.cpp)
target_link_libraries(hp_detector ${OpenCV_LIBS})

add_executable(roi roi.cpp)
target_link_libraries(roi ${OpenCV_LIBS})

add_executable(ocr ocr.cpp)
target_include_directories(
    ocr
    PRIVATE ${MAA_PRIVATE_INC} ${MAA_PUBLIC_INC})
target_link_libraries(ocr ${OpenCV_LIBS} MaaFramework MaaToolkit)