cmake_minimum_required(VERSION 3.25)
project(tilepeek VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Test)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
