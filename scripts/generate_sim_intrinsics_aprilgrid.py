import cv2
import numpy as np
import math
import os
import yaml

print("Starting to generate synthetic AprilGrid calibration data with a known camera intrinsic matrix...")

# 1. 模拟一个已知的相机内参矩阵 (K) 和图像分辨率
tgt_w, tgt_h = 1920, 1536
fx, fy = 1200.0, 1200.0
cx, cy = tgt_w / 2.0, tgt_h / 2.0
K = np.array([
    [fx,  0, cx],
    [ 0, fy, cy],
    [ 0,  0,  1]
], dtype=np.float64)

# 使用 Kannala-Brandt (equidistant/fisheye) 模型
# dist_coeffs = [k1, k2, k3, k4]
# 真实现实中的广角/鱼眼畸变曲线通常贴近纯等距投影(全 0 参数)或带一点点负数。
# 之前为了刻意搞畸变放了正数，导致生成了物理罕见的“枕形/放大镜”畸变，从而让 C++ 算法直接蒙圈。
dist_coeffs = np.array([0.0, 0.0, 0.0, 0.0], dtype=np.float64)

print("---------------------------------")
print("Known Camera Intrinsics Matrix K:")
print(K)
print(f"Resolution: {tgt_w}x{tgt_h}")
print("---------------------------------")

# 2. 生成 AprilGrid 标准图片模板 (跟之前的逻辑类似，但是我们要赋予它现实的物理尺寸)
bw = 6
bh = 6
tag_size_px = 200
spacing_px = int(tag_size_px * 0.3)
quiet_zone_px = tag_size_px

pattern_w = bw * (tag_size_px + spacing_px) + spacing_px
pattern_h = bh * (tag_size_px + spacing_px) + spacing_px

board_w = pattern_w + 2 * quiet_zone_px
board_h = pattern_h + 2 * quiet_zone_px

board_img = np.ones((board_h, board_w), dtype=np.uint8) * 255

try:
    aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_APRILTAG_36h11)
except Exception:
    aruco_dict = cv2.aruco.Dictionary_get(cv2.aruco.DICT_APRILTAG_36h11)

# 绘制符合 Kalibr 标准的小黑块
for r in range(bh + 1):
    for c in range(bw + 1):
        x = quiet_zone_px + c * (tag_size_px + spacing_px)
        y = quiet_zone_px + r * (tag_size_px + spacing_px)
        board_img[y:y+spacing_px, x:x+spacing_px] = 0

tag_id = 0
for r_k in range(bh):
    for c_k in range(bw):
        # 恢复正确的 Kalibr 布局：Tag 0 在物理尺寸的左下角
        r_image = (bh - 1) - r_k
        c_image = c_k
        
        x = quiet_zone_px + spacing_px + c_image * (tag_size_px + spacing_px)
        y = quiet_zone_px + spacing_px + r_image * (tag_size_px + spacing_px)
        
        try:
            tag_8x8 = np.zeros((8, 8), dtype=np.uint8)
            cv2.aruco.generateImageMarker(aruco_dict, tag_id, 8, tag_8x8, 1)
        except Exception:
            tag_8x8 = cv2.aruco.drawMarker(aruco_dict, tag_id, 8, 1)
            
        tag_10x10 = np.zeros((10, 10), dtype=np.uint8)
        tag_10x10[1:9, 1:9] = tag_8x8
        # 【关键修正】由于两者的正向定义差了180度，导致中心对上了，但角点全歪了
        tag_10x10 = np.rot90(tag_10x10, 2)
        tag_img = cv2.resize(tag_10x10, (tag_size_px, tag_size_px), interpolation=cv2.INTER_NEAREST)
        board_img[y:y+tag_size_px, x:x+tag_size_px] = tag_img
        tag_id += 1

# 3. 建立 3D 物理坐标
# 根据您 C++ 的 config.yaml 设置：square_size: 55.0 mm, spacing: 16.5 mm
tag_size_m = 0.055
spacing_m = 0.0165
quiet_zone_m = tag_size_m

board_w_m = bw * tag_size_m + (bw + 1) * spacing_m + 2 * quiet_zone_m
board_h_m = bh * tag_size_m + (bh + 1) * spacing_m + 2 * quiet_zone_m

# 以标定板中心为 3D 世界坐标系的原点 (Z=0 平面)
corners_3d = np.float32([
    [-board_w_m/2, -board_h_m/2, 0],
    [ board_w_m/2, -board_h_m/2, 0],
    [ board_w_m/2,  board_h_m/2, 0],
    [-board_w_m/2,  board_h_m/2, 0]
])

# 对应的模板图像的四个角点 (此处保留作注)
corners_2d_img = np.float32([[0, 0], [board_w, 0], [board_w, board_h], [0, board_h]])

# 预先计算用于 Kannala-Brandt 畸变模型(fisheye) 的全图射线方向
print("Pre-computing fisheye rays for full image remap...")
U, V = np.meshgrid(np.arange(tgt_w), np.arange(tgt_h))
pts_dst = np.stack([U, V], axis=-1).astype(np.float32).reshape(-1, 1, 2)
# 使用 cv2.fisheye.undistortPoints 从像素映射到 normalized 平面
pts_norm = cv2.fisheye.undistortPoints(pts_dst, K, dist_coeffs)
pts_norm = pts_norm.reshape(-1, 2)
# 组织成 N×3 的相机系下射线方向数组
d_c_flat = np.stack([pts_norm[:, 0], pts_norm[:, 1], np.ones(tgt_h * tgt_w)], axis=-1)

# 4. 基于真实的内参，生成多张视角的投影照片
np.random.seed(42)  # 固定种子以复现
num_images = 30
output_dir = '../images'
os.makedirs(output_dir, exist_ok=True)

ground_truth = {
    'camera_models': {
        'cam0': {
            'camera_model': 'pinhole',
            'intrinsics': [float(fx), float(fy), float(cx), float(cy)],
            'distortion_model': 'equidistant',
            'distortion_coeffs': [float(x) for x in dist_coeffs.tolist()],
            'resolution': [tgt_w, tgt_h]
        }
    },
    'poses': []
}

for i in range(num_images):
    # 构建随机但这又合理的观察视角：
    # 标定板在相机坐标系下的平移
    # 物理标定板缩小到了 55mm，为了让它在画面中仍然保持刚刚放大的视觉比例，需要把 Z 轴进一步拉近！
    tx = np.random.uniform(-0.28, 0.28)
    ty = np.random.uniform(-0.18, 0.18)
    tz = np.random.uniform(0.56, 1.34)
    tvec = np.array([[tx], [ty], [tz]], dtype=np.float64)
    
    # 标定板在相机坐标系下的旋转 (绕 X/Y 轴的倾角，和绕 Z 轴的旋转)
    # 【惊天大Bug修复】必须让板子翻转 180 度面向相机！
    # 如果 rx 在 0 附近，Z_board 背对相机，相当于我们在看一块透明毛玻璃的背面，这会生成镜像二维码，导致永远无法被识别！
    rx = np.random.uniform(145, 215) # 翻转 180度，并带有正负 35 度的倾角
    ry = np.random.uniform(-35, 35)
    rz = np.random.uniform(-180, 180) # 板子自己的朝向
    
    rot_x = cv2.Rodrigues(np.array([math.radians(rx), 0, 0], dtype=np.float64))[0]
    rot_y = cv2.Rodrigues(np.array([0, math.radians(ry), 0], dtype=np.float64))[0]
    rot_z = cv2.Rodrigues(np.array([0, 0, math.radians(rz)], dtype=np.float64))[0]
    
    R = rot_x @ rot_y @ rot_z
    rvec, _ = cv2.Rodrigues(R)
    
    # 获取从板子到相机的逆变换（即相机在板子坐标系下的位姿）
    R_inv = R.T
    O_b = -R_inv @ tvec.reshape(3) # C在B坐标系下的位置
    
    # 将相机发出的射线全部转到板子坐标系之下: D_b = R_inv * d_c
    D_b = (R_inv @ d_c_flat.T).T # shape: (N, 3)
    
    # 计算射线跟板子平面 (Z=0) 的交点
    # Z_intersect = O_b.z + lambda * D_b.z = 0  =>  lambda = -O_b.z / D_b.z
    D_b_z = D_b[:, 2]
    valid_mask = np.abs(D_b_z) > 1e-6
    lmbda = np.zeros(D_b.shape[0], dtype=np.float32)
    lmbda[valid_mask] = -O_b[2] / D_b_z[valid_mask]
    
    # 我们只对向着板子前方发出的射线有效 (lambda > 0)
    valid_mask = valid_mask & (lmbda > 0)
    
    # 计算交点的 X_b, Y_b 物理坐标
    P_b_x = O_b[0] + lmbda * D_b[:, 0]
    P_b_y = O_b[1] + lmbda * D_b[:, 1]
    
    # 然后将物理坐标反算回 2D 模板图像的像素位置 (u_tmpl, v_tmpl)
    u_tmpl = (P_b_x - (-board_w_m / 2.0)) * (board_w / board_w_m)
    # 【修复镜像错误】Kalibr Y轴向上，所以 P_b_y = +H/2 是板子顶部。将其映射到模板图 v=0 (顶部)
    v_tmpl = (board_h_m / 2.0 - P_b_y) * (board_h / board_h_m)
    
    # 越界的射线赋值为一个非法的像素位置
    u_tmpl[~valid_mask] = -1.0
    v_tmpl[~valid_mask] = -1.0
    
    map_x = u_tmpl.reshape(tgt_h, tgt_w).astype(np.float32)
    map_y = v_tmpl.reshape(tgt_h, tgt_w).astype(np.float32)
    
    # 使用 remap 完成包含畸变的图像绘制！
    bg_color = int(np.random.uniform(150, 220))
    warped = cv2.remap(board_img, map_x, map_y, cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=bg_color)
    
    # 添加高斯噪声以模拟真实传感器
    noise = np.random.normal(0, 3.5, (tgt_h, tgt_w)).astype(np.float32)
    warped = np.clip(warped.astype(np.float32) + noise, 0, 255).astype(np.uint8)
    # 添加深度的透镜模拟模糊，修正纯生成图片的亚像素级锯齿错误！这个非常关键，Kalibr是根据连续阶梯梯度拟合亚像素角点的，如果没有平滑的梯度边界反而会拟合到错误的一边。
    warped = cv2.GaussianBlur(warped, (5, 5), 1.5)
        
    out_path = os.path.join(output_dir, f'calib_sim_{i}.jpg')
    cv2.imwrite(out_path, warped)
    print(f"Generated {out_path} (Depth ~ {tz:.2f}m)")
    
    # 保存真值位姿信息
    ground_truth['poses'].append({
        'image_name': f'calib_sim_{i}.jpg',
        'rvec': rvec.flatten().tolist(),
        'tvec': tvec.flatten().tolist()
    })

gt_path = os.path.join(output_dir, 'simulated_ground_truth.yaml')
with open(gt_path, 'w') as f:
    yaml.dump(ground_truth, f, default_flow_style=False)

print(f"\nAll Done! Generated {num_images} simulated images.")
print(f"Ground truth saved to {gt_path}")
