# Changelog (April 2026)

## [1.0] - 2026-04-06
### Added
- **版本定义**: 在 `april_calib_test.cpp` 中通过宏定义 `MAJOR_VERSION 1` 和 `MINOR_VERSION 0` 正式确定了项目版本号。
- **启动预警**: 启动时自动打印标定工具当前版本信息。
- **高性能预处理优化**: 将 `initUndistortRectifyMap` 移出循环，只计算一次映射表，大幅减少重复运算。
- **OpenMP 并行处理**: 对图像去畸变 (`remap`) 及文件保存 (`imwrite`) 引入多核并行加速，大幅缩减存图时间。
- **自动目录建立**: 在标定完成后，自动创建 `result/` 和 `undistorted/` 文件夹。
- **SSH 兼容性**: 注释掉了所有 `cv::imshow` 弹窗调用，防止在无显示器的 SSH 环境下出现 GTK 后端崩溃。
- **标定可视化**: 引入红绿点比对（Observed vs Reprojected）及物理去畸变验证功能。

### Changed
- **Config 全站相对路径**: 配置文件 (`config.yaml`) 支持全站相对路径，解决标定程序跨平台路径配置繁琐的问题。
- **Ceres 优化参数**: 开放了 Ceres Solver 内部线程限制，根据硬件核心数动态开启多核优化。
- **修复 TagSpacing 比率问题**: 修正了 `aslam` 标定板对象对 `tagSpacing` 单位的误判（应该是比率而不是绝对米数），解决了标定误差异常大的 Bug。
- **Python 脚本更新**: 将原始仿真脚本搬迁至 `scripts/` 目录，并适配项目目录结构。

### Fixed
- **标定 `nan` 崩溃问题**: 通过修复角点对应关系及初始内参，解决标定发散导致的 `nan` 错误。
- **路径校验机制**: 完善了对 `input_dir`、`result_images_save_folder` 的启动检查逻辑。
- **内存泄漏**: 修复了底层库 `CameraCalibration.cc` 中一处潜在的 `delete` 指针报错 lint。
