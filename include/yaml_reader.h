#pragma once

#include <string>

// 定义一个配置结构体，容纳所有需要的参数
struct CalibConfig {
    // Board
    int board_width = 6;
    int board_height = 6;
    float square_size = 55.0f;
    float spacing = 16.5f;

    // IO
    std::string input_dir;
    std::string prefix;
    std::string file_extension;
    bool save_result = true;
    std::string result_images_save_folder;
    std::string point_file;

    // Camera
    std::string camera_model;
    std::string camera_name;

    // Processing
    bool use_opencv = true;
    bool view_results = true;
    bool verbose = true;
    float resize_scale = 1.0f;
    int cropper_width = 0;
    int cropper_height = 0;
    int center_x = 0;
    int center_y = 0;
};

class YamlReader {
public:
    YamlReader() = default;
    ~YamlReader() = default;

    // 加载并解析 YAML 文件，成功返回 true，失败返回 false
    bool loadConfig(const std::string& filepath);

    // 获取解析后的配置对象
    CalibConfig getConfig() const;

private:
    CalibConfig config_;
};