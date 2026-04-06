# Camera Calibration Tool (AprilGrid + Kannala-Brandt)

这是一个高性能的相机标定工具，专门针对广角/鱼眼相机的 **Kannala-Brandt (Equidistant)** 模型进行标定。

## 核心特性
- **高性能并行化**：底层 Ceres Solver 开启了多线程优化，上层图像预处理（Remap/Undistort）及 I/O（imwrite）均采用了 **OpenMP** 并行处理。在标定验证环节，比传统单线程快了 **10-20 倍**。
- **自动验证**：标定后自动生成 `result/` 和 `undistorted/` 图片，直观对比标定误差（红绿点）并验证去畸变拉直效果。
- **便携性**：全站支持 **YAML 相对路径**，只需在 `camera_calib/build` 目录下运行，无需修改任何绝对路径即可移植。
- **配套仿真**：内置 Python 仿真脚本，可根据指定的内参生成带畸变、带噪声的 3D 实境仿真标定图片。

## 环境依赖
- **OpenCV** (>= 4.0)
- **Ceres Solver**
- **Eigen 3**
- **Boost** (Filesystem, Program-options)
- **YAML-cpp**
- **OpenMP** (通常随编译器自带)

## 快速上手
### 1. 编译
```bash
mkdir build && cd build
cmake ..
make -j$(n_proc)
```

### 2. 生成仿真数据 (可选)
```bash
cd scripts
python3 generate_sim_intrinsics_aprilgrid.py
```
生成的 30 张仿真图片将存放在 `camera_calib/images/`。

### 3. 运行标定
```bash
cd build
./camera_calib
```

## 路径说明
- `config/`: 存放 `config.yaml` 配置文件。
- `images/`: 存放输入待标定的图片。
- `output/`: 存放标定结果（YAML、重投影分布图、去畸变后的修正图）。
- `scripts/`: 仿真数据生成工具。
