set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(hp_detector detect_hp.cpp)
target_link_libraries(hp_detector ${OpenCV_LIBS})

add_executable(roi roi.cpp)
target_link_libraries(roi ${OpenCV_LIBS})