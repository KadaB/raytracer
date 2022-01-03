cmake_minimum_required(VERSION 2.8)

project(raytracing)

set(CMAKE_BUILD_TYPE Release)

find_package(OpenCV REQUIRED) # video capture
find_package(OpenMP REQUIRED) 

# Sources
file (GLOB source_files "src/*.cpp") # autoload all the cpps in src folder
add_executable(${PROJECT_NAME} ${source_files})

# Linking 
include_directories(${OpenCV_INCLUDES})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} OpenMP::OpenMP_CXX)

# resources
# copy all resource files in res into build dir
#file(COPY "${PROJECT_SOURCE_DIR}/res" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
