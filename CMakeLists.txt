cmake_minimum_required(VERSION 3.12)
project(camera_calib)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "CMake setup with YAML config and Camera Model")

# =========================================================
# 1. 修复报错：定义第三方库生成的 .so 动态库的存放路径
# =========================================================
set(SHARED_LIB_PATH ${CMAKE_BINARY_DIR}/lib)
message(STATUS "SHARED_LIB_PATH set to: ${SHARED_LIB_PATH}")

# =========================================================
# 2. 编译第三方库 (高文良博士的 camera_model)
# =========================================================
message(STATUS "--> Building thirdparty: camera_model")
add_subdirectory(thirdparty/camera_model)

# =========================================================
# 3. 寻找我们主程序需要的其他依赖
# =========================================================
find_package(yaml-cpp REQUIRED)
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")

# =========================================================
# 4. 编译主程序可执行文件
# =========================================================
add_executable(camera_calib 
    src/april_calib_test.cpp
    src/yaml_reader.cpp
)

target_include_directories(camera_calib 
    PRIVATE 
    include
    ${OpenCV_INCLUDE_DIRS}
)

# =========================================================
# 5. 链接所有库 (将 yaml-cpp 和 camera_model 缝合到程序中)
# =========================================================
target_link_libraries(camera_calib 
    PRIVATE 
    yaml-cpp
    camera_model
    ${OpenCV_LIBS}
)