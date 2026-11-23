# third_party：第三方依赖

## 预期依赖

| 依赖 | 用途 | 引入方式 |
|---|---|---|
| OpenCV 4.x | 图像处理、解码后端 | 系统包 / vcpkg / 自建 |
| ZXing-C++ | 解码备选 | 同编译器自建，避免 ABI 问题 |
| ONNX Runtime | 模型推理（非 Jetson 备选） | 预编译包 |
| TensorRT | Jetson 推理 | JetPack 自带 |
| nlohmann/json 或等价 | 配置读写 | header-only / 子模块 |
| pybind11 | Python 绑定（离线） | 子模块 / pip |
| GoogleTest 或 Catch2 | 单元测试 | 子模块 / 包管理 |

## 规则

- 不在 `third_party/` 里直接提交大体积二进制；用子模块、包管理器或构建脚本。
- 每个依赖记录：版本、许可、构建选项、已知 ABI 限制。
- 依赖升级必须跑黄金测试；OpenCV 版本变化可能改变插值/阈值边界，需在实验报告中说明。
- 目标平台（Windows 开发机 / Jetson Orin NX）分别记录构建方式。

## 当前状态

目录为空。构建系统（CMake）与依赖获取方式待 M1 确定。
