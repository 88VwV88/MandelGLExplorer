cmake_minimum_required(VERSION 3.10)
project(MandelGL)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

include_directories("include/")

add_executable(mandel-main 
  "src/main.cpp"
  "src/resource-manager.cpp"
  "include/utils.hpp"
  "include/resource-manager.hpp"
)
target_link_libraries(mandel-main PRIVATE
  GL glfw GLEW glm::glm)