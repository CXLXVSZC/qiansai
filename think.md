# 代码分析报告

## 项目概述

这是一个基于 **RA8P1 开发板** 的 **NPU 加速 AI 人脸检测** 项目，使用 ETHOS-U55 NPU 加速器运行 YOLOv2-tiny 模型进行实时人脸检测。

## 目录结构

```
src/
├── hal_entry.c           # 主程序入口
├── models/               # NPU模型相关
│   ├── model.c          # 模型调用封装
│   ├── model.h          # 模型接口声明
│   ├── sub_0000_tensors.h    # NPU张量地址定义
│   ├── sub_0000_invoke.h     # NPU调用接口
│   └── sub_0000_*.c          # 生成的模型数据/命令流
└── yolo/                 # YOLO检测算法
    ├── yolo_rtthread.c  # YOLO解码、NMS、预处理
    └── yolo_rtthread.h  # 参数和函数声明
```

## 代码功能详解

### 1. hal_entry.c - 主程序入口

**主要功能：**
- 初始化摄像头 (640x480 RGB565)
- 初始化 ETHOS-U55 NPU
- 循环执行：采集 → 预处理 → NPU推理 → 后处理 → LCD显示

**工作流程：**
1. `sensor_init()` / `sensor_reset()` - 初始化摄像头
2. `RM_ETHOSU_Open()` - 启动 NPU
3. 循环检测：
   - `sensor_snapshot()` - 拍摄照片
   - `rgb565_to_gray_resize_192_and_quantization()` - 图像预处理
   - `RunModel(false)` - NPU 推理
   - `decode_output_layer()` - 解码两个输出层
   - `nms_filter()` - NMS 非极大值抑制
   - `lcd_draw_jpg_with_frame()` - 绘制检测框

### 2. models/ - NPU模型接口

**model.c/model.h:**
- `RunModel()` - 运行NPU模型
- `GetModelInputPtr_serving_default_image_input_0()` - 获取输入指针
- `GetModelOutputPtr_StatefulPartitionedCall_*()` - 获取输出指针

**模型信息：**
- 输入：192x192 灰度图，int8_t量化
- 输出：两个YOLO层 (6x6 和 12x12网格)
- 内存 arena 大小：442368 字节

### 3. yolo/ - YOLO检测算法

**yolo_rtthread.c 主要函数：**

| 函数 | 功能 |
|------|------|
| `decode_output_layer()` | 解码YOLO输出层，生成检测框 |
| `rgb565_to_gray_resize_192_and_quantization()` | RGB565→灰度→192x192缩放→量化 |
| `dequantize_int8()` | int8_t反量化到float |
| `nms_filter()` | NMS非极大值抑制 (支持IoU/DIoU) |
| `iou_rect()` | 计算IoU |
| `diou_rect()` | 计算DIoU |

**关键参数：**
```c
INPUT_W = 192, INPUT_H = 192
GRID_SIZE_1 = 6, GRID_SIZE_2 = 12
CONF_THRESH = 0.8f    // 置信度阈值
NMS_THRESH = 0.45f   // NMS阈值
CLASS_NUM = 1        // 检测类别(人脸)
MAX_BOXES = 16       // 最大检测框数
```

## 数据流

```
摄像头 (640x480 RGB565)
    ↓ rgb565_to_gray_resize_192
192x192 灰度图
    ↓ 量化 (scale=0.0078, zp=-1)
int8_t 输入张量
    ↓ RunModel()
NPU 推理 (ETHOS-U55)
    ↓
int8_t 输出张量 (output1: 6x6, output2: 12x12)
    ↓ dequantize_float
float 输出
    ↓ decode_output_layer (x2)
检测框列表
    ↓ nms_filter
最终检测框 → LCD显示
```

## 技术要点

1. **NPU加速**：使用ETHOS-U55运行TensorFlow Lite模型
2. **量化推理**：int8_t量化减少内存和加速推理
3. **双层YOLO**：6x6和12x12双网格检测不同尺度人脸
4. **NMS优化**：支持标准IoU和DIoU两种模式
5. **实时显示**：LCD直接绘制检测框

## 依赖

- RT-Thread 操作系统
- Renesas FSP (Flexible Software Package)
- ETHOS-U55 NPU 驱动
- 摄像头驱动 (GC032A sensor)
- 2D图形加速 (d2)
