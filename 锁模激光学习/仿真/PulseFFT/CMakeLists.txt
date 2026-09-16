cmake_minimum_required(VERSION 3.22.0)
set(project_name PulseFFT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE PulseFFT_src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(${project_name}_lib SHARED ${PulseFFT_src})
add_executable(${project_name} main.cpp)

target_include_directories(${project_name}_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${project_name}_lib PUBLIC time_lib)

target_link_libraries(${project_name} PRIVATE
    ${project_name}_lib
    img_viz_lib
    foxglove_viz
)
