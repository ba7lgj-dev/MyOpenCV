cmake_minimum_required(VERSION 3.16)
project(DualCameraInspector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(OpenCV REQUIRED)
find_package(Qt6 COMPONENTS Widgets Gui Network SerialPort REQUIRED)

add_executable(dual_camera_inspector
    src/main.cpp
    src/MainWindow.cpp
    src/CameraWorker.cpp
    src/CameraProcessor.cpp
    src/ConfigManager.cpp
    src/WeChatNotifier.cpp
    src/SerialPumpController.cpp
)

target_include_directories(dual_camera_inspector PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dual_camera_inspector PRIVATE
    ${OpenCV_LIBS}
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    Qt6::SerialPort
)

