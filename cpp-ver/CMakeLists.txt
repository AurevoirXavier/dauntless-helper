cmake_minimum_required(VERSION 3.14)
project(dauntless-helper)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(hack SHARED dllmain.cpp)
target_link_libraries(hack "C:/Users/cestl/Desktop/dauntless-helper/cpp-ver/lib/detours.lib")
target_link_libraries(hack "C:/Users/cestl/Desktop/dauntless-helper/cpp-ver/lib/FW1FontWrapper.lib")

add_executable(dauntless-helper injector.cpp)
