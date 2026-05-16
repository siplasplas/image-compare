cmake_minimum_required(VERSION 3.16)
project(image-compare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(image-compare
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

target_include_directories(image-compare PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(image-compare PRIVATE Qt6::Widgets ${OpenCV_LIBS})