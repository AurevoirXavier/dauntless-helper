cmake_minimum_required(VERSION 3.14)
project(dauntless-helper)

set(CMAKE_CXX_STANDARD 17)

include_directories(../detours)
include_directories(helper)
include_directories(imgui)

add_library(hack SHARED dllmain.cpp)
add_library(helper helper/helper.cpp)
#add_library(imgui imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_impl_dx11.cpp imgui/imgui_impl_win32.cpp)
add_library(imgui imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_impl_dx11.cpp imgui/imgui_impl_win32.cpp)
target_link_libraries(hack helper)
target_link_libraries(hack imgui)
target_link_libraries(hack C:/Users/cestl/Desktop/dauntless-helper/cpp-ver/detours/detours.lib)

#add_executable(dauntless-helper injector.cpp)
