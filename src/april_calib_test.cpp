#include <iostream>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "../include/yaml_reader.h"

#include "camera_model/camera_models/Camera.h"
#include "camera_model/calib/CameraCalibration.h"
#include "camera_model/apriltag_frontend/GridCalibrationTargetAprilgrid.hpp"

#define MAJOR_VERSION 1
#define MINOR_VERSION 0


int main(int argc, char** argv)
{
    std::cout << "# INFO: Camera calibration version: " << MAJOR_VERSION << "." << MINOR_VERSION << std::endl;
    std::string config_path = "../config/config.yaml";

    //加载配置文件
    YamlReader reader;
    if (!reader.loadConfig(config_path)) {
        std::cerr << "# ERROR: Failed to load config file from: " << config_path << std::endl;
        std::cerr << "Please make sure the file exists and the format is correct." << std::endl;
        return -1;
    }

    //解析配置文件
    CalibConfig cfg = reader.getConfig();

    if (!std::filesystem::exists(cfg.input_dir) || 
        !std::filesystem::is_directory(cfg.input_dir)) {
        std::cerr << "# ERROR: Cannot find input directory or it is not a directory: " 
                  << cfg.input_dir << std::endl;
        return -1;
    }

    if (!std::filesystem::exists(cfg.result_images_save_folder) || 
        !std::filesystem::is_directory(cfg.result_images_save_folder)) {
        std::cerr << "# ERROR: Cannot find result directory or it is not a directory: " 
                  << cfg.result_images_save_folder << std::endl;
        return -1;
    }

    if (!std::filesystem::exists(cfg.point_file) || 
        !std::filesystem::is_directory(cfg.point_file)) {
        std::cerr << "# ERROR: Cannot find point file directory or it is not a directory: " 
                  << cfg.point_file << std::endl;
        return -1;
    }

    //获取镜头模型类型
    camera_model::Camera::ModelType model_type;

    if (cfg.camera_model == "kannala-brandt") {
        model_type = camera_model::Camera::ModelType::KANNALA_BRANDT;
    }
    else if (cfg.camera_model == "pinhole") {
        model_type = camera_model::Camera::ModelType::PINHOLE;
    }
    else {
        std::cerr << "# ERROR: Unknown camera model: " << cfg.camera_model << std::endl;
        return 1;
    }

    switch (model_type) {
    case camera_model::Camera::ModelType::KANNALA_BRANDT:
        std::cout << "# INFO: Camera model: Kannala-Brandt" << std::endl;
        break;
    case camera_model::Camera::ModelType::PINHOLE:
        std::cout << "# INFO: Camera model: Kannala-Brandt" << std::endl;
        break;
    }

    //获取拍摄照片路径
    std::vector<std::string> image_file_names;

    for (const auto& entry : std::filesystem::directory_iterator(cfg.input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string filename = entry.path().filename().string();

        if (!cfg.prefix.empty()) {
            if (filename.find(cfg.prefix) != 0) {
                continue;
            }
        }

        if (entry.path().extension().string() != cfg.file_extension) {
            continue;
        }

        image_file_names.push_back(entry.path().string());

        if (cfg.verbose) {
            std::cout << "# INFO: Adding " << image_file_names.back() << std::endl;
        }
    }

    if (image_file_names.empty()) {
        std::cerr << "# ERROR: No images found in " << cfg.input_dir 
                  << " with extension " << cfg.file_extension << std::endl;
        return -1;
    }

    if (cfg.verbose) {
        std::cerr << "# INFO: # images: " << image_file_names.size() << std::endl;
    }

    cv::Size frame_size = cv::imread(image_file_names.front(), -1).size();
    std::cout << "frame size: " << frame_size << std::endl;

    cv::Size board_size;
    board_size.width = cfg.board_width;
    board_size.height = cfg.board_height;

    camera_model::CameraCalibration calibration(model_type, cfg.camera_name, 
        frame_size, board_size, cfg.square_size);
    calibration.setVerbose(cfg.verbose);

    std::vector<bool> chess_board_found(image_file_names.size(), false);
    
    for (size_t image_index = 0; image_index < image_file_names.size(); ++image_index) {
        std::string image_name = image_file_names.at(image_index);

        cv::Mat image_src = cv::imread(image_name, -1);
        if (image_src.channels() == 3) {
            cv::cvtColor(image_src, image_src, cv::ColorConversionCodes::COLOR_BGR2GRAY);
        }

        cv::Mat image = image_src;
        aslam::cameras::GridCalibrationTargetAprilgrid aprilgrid(cfg.board_height, 
            cfg.board_width, cfg.square_size / 1000.0f, cfg.spacing / cfg.square_size);

        std::vector<cv::Point2f> points_2ds;
        std::vector<cv::Point3f> points_3ds;
        std::vector<bool> out_corner_observed;
        bool is_ok = aprilgrid.computeObservation(image, points_2ds, out_corner_observed);
        points_3ds = aprilgrid.points3d();
        if (is_ok) {
            std::cout << "# INFO: Detected chessboard in image " << image_index + 1 << ", "
                      << image_file_names.at(image_index) << std::endl;

            std::vector<cv::Point2f> points2_fin;
            std::vector<cv::Point3f> points3_fin;
            for (int index = 0; index < out_corner_observed.size(); ++index) {
                if (out_corner_observed.at(index)) {
                    points2_fin.push_back(points_2ds.at(index));
                    points3_fin.push_back(points_3ds.at(index));
                }
            }

            if (points2_fin.size() >= cfg.board_width * cfg.board_height) {
                calibration.addChessboardData(points2_fin, points3_fin);
                calibration.addImage(image, image_name);
            }
            else {
                std::cerr << "# valid points less than 30, skip this image" << std::endl;
                is_ok = false;
            }
        }
        else {
            std::cerr << "\033[31;47;1m"
                      << "# INFO: Did not detect chessboard in image: " 
                      << image_file_names.at(image_index) << "\033[0m"
                      << std::endl;
        }

        chess_board_found.at(image_index) = is_ok;
    }

    if (calibration.sampleCount() < 1) {
        std::cerr << "# ERROR: Insufficient number of detected chessboards." << std::endl;
        return 1;
    }

    std::cerr << "# INFO: Calibrating..." << std::endl;

    std::cout << "Calibrate start." << std::endl;
    calibration.calibrate();

    std::cout << "Calibrate done." << std::endl;

    std::string calib_result_path = cfg.input_dir + cfg.camera_name + "_camera_calib.yaml";

    try {
        calibration.writeParams(calib_result_path);
        std::cout << "# INFO: Calibration result saved to: " << calib_result_path << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "# ERROR: Failed to save calibration result to " << calib_result_path << std::endl;
        std::cerr << "# OpenCV Error Info: " << e.what() << std::endl;
        return -1;
    }

    if (cfg.view_results && cfg.verbose) {
        std::cout << "\033[32;40;1m"
                  << "# INFO: Used image num: " << calibration.m_ImagesShow.size() << "\033[0m" << std::endl;
        std::cout << "details shown in the images," << std::endl
                  << "\033[32;40;1m"
                  << "green points is observed points, " << std::endl
                  << "\033[31;47;1m"
                  << "red points is estimated points. "
                  << "\033[0m" << std::endl;

        if (calibration.m_ImagesShow.size() > 0) {
            cv::Mat pointDistributedImage =
                cv::Mat(calibration.m_ImagesShow[0].rows, 
                    calibration.m_ImagesShow[0].cols, CV_8UC3, cv::Scalar(0));
            cv::Mat pointDistributedImageGood =
                cv::Mat(calibration.m_ImagesShow[0].rows, 
                    calibration.m_ImagesShow[0].cols, CV_8UC3, cv::Scalar(0));

            // visualize observed and reprojected points
            calibration.drawResultsInitial(calibration.m_ImagesShow, 
                calibration.m_ImageNames, pointDistributedImage);
            calibration.drawResultsFiltered(calibration.m_ImagesGoodShow, 
                calibration.m_ImageNames, pointDistributedImageGood);

            std::string point_file = cfg.point_file + cfg.camera_name + "_point2d.txt";
            calibration.save2D(point_file);

            bool is_save_images = true;

            cv::Mat map1, map2;
            camera_model::CameraConstPtr camera = calibration.camera();
            if (is_save_images && camera) {
                std::filesystem::create_directories(cfg.result_images_save_folder + "result/");
                std::filesystem::create_directories(cfg.result_images_save_folder + "undistorted/");

                cv::Mat rmat = cv::Mat::eye(3, 3, CV_32F);
                camera->initUndistortRectifyMap(map1, map2, -1, -1, 
                    calibration.m_ImagesShow.at(0).size(), -1, -1, rmat);
            }

            std::cout << "# INFO: Saving results to " << cfg.result_images_save_folder << " ..." << std::endl;

            #pragma omp parallel for
            for (int i = 0; i < (int)calibration.m_ImagesShow.size(); ++i) {
                if (is_save_images) {
                    #pragma omp critical
                    {
                        std::cout << "  -> Saving results for image: " << i << " [" 
                                  << i + 1 << "/" << calibration.m_ImagesShow.size() << "]" << std::endl;
                    }

                    std::ostringstream ss;
                    ss << i;
                    cv::imwrite(cfg.result_images_save_folder + "result/" + "calib_result_" + ss.str() + ".jpg",
                                calibration.m_ImagesGoodShow.at(i));

                    // 应用预先计算好的去畸变映射
                    if (!map1.empty()) {
                        cv::Mat undistorted_image;
                        cv::remap(calibration.m_ImagesShow.at(i), undistorted_image, map1, map2, cv::INTER_LINEAR);
                        
                        std::ostringstream undistorted_ss;
                        undistorted_ss << i;
                        cv::imwrite(cfg.result_images_save_folder + "undistorted/" + "undistorted_" + 
                            undistorted_ss.str() + ".jpg", undistorted_image);
                    }
                }
            }
            std::cout << "# INFO: Results saved as JPEGs in: " << cfg.result_images_save_folder << std::endl;
        }
    }

    return 0;
}