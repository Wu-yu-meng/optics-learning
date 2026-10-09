cmake_minimum_required(VERSION 3.22.0)
set(project_name AutomaticModeLocked)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Eigen3 REQUIRED)

file(GLOB_RECURSE AutomaticModeLocked_src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_library(${project_name}_lib SHARED ${AutomaticModeLocked_src})

target_include_directories(${project_name}_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${project_name}_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(${project_name}_lib PUBLIC Eigen3::Eigen)
target_link_libraries(${project_name}_lib PRIVATE config_loader_lib)

add_executable(${project_name} main.cpp)
target_link_libraries(${project_name} PRIVATE
    ${project_name}_lib
)
