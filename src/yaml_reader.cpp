#include "../include/yaml_reader.h"
#include <yaml-cpp/yaml.h>
#include <iostream>

bool YamlReader::loadConfig(const std::string& filepath)
{
    try {
        YAML::Node yaml_node = YAML::LoadFile(filepath);

        // 解析 board 节点
        if (yaml_node["board"]) {
            config_.board_width = yaml_node["board"]["width"].as<int>(6);
            config_.board_height = yaml_node["board"]["height"].as<int>(6);
            config_.square_size = yaml_node["board"]["square_size"].as<float>(55.0f);
            config_.spacing = yaml_node["board"]["spacing"].as<float>(16.5f);
        }

        // 解析 io 节点
        if (yaml_node["io"]) {
            config_.input_dir = yaml_node["io"]["input_dir"].as<std::string>("");
            config_.prefix = yaml_node["io"]["prefix"].as<std::string>("");
            config_.file_extension = yaml_node["io"]["file_extension"].as<std::string>(".jpg");
            config_.save_result = yaml_node["io"]["save_result"].as<bool>(true);
            config_.result_images_save_folder = yaml_node["io"]["result_images_save_folder"].as<std::string>("");
            config_.point_file = yaml_node["io"]["point_file"].as<std::string>("");
        }

        // 解析 camera 节点
        if (yaml_node["camera"]) {
            config_.camera_model = yaml_node["camera"]["model"].as<std::string>("kannala-brandt");
            config_.camera_name = yaml_node["camera"]["name"].as<std::string>("camera");
        }

        // 解析 processing 节点
        if (yaml_node["processing"]) {
            config_.use_opencv = yaml_node["processing"]["use_opencv"].as<bool>(true);
            config_.view_results = yaml_node["processing"]["view_results"].as<bool>(true);
            config_.verbose = yaml_node["processing"]["verbose"].as<bool>(true);
            config_.resize_scale = yaml_node["processing"]["resize_scale"].as<float>(1.0f);
            
            if (yaml_node["processing"]["cropper"]) {
                config_.cropper_width = yaml_node["processing"]["cropper"]["width"].as<int>(0);
                config_.cropper_height = yaml_node["processing"]["cropper"]["height"].as<int>(0);
                config_.center_x = yaml_node["processing"]["cropper"]["center_x"].as<int>(0);
                config_.center_y = yaml_node["processing"]["cropper"]["center_y"].as<int>(0);
            }
        }
        return true;
    } catch (const YAML::Exception& e) {
        std::cerr << "# ERROR: Failed to parse YAML file: " << filepath << "\n" 
                  << "Reason: " << e.what() << std::endl;
        return false;
    }
}

CalibConfig YamlReader::getConfig() const
{
    return config_;
}