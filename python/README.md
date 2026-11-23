# python：离线训练、导出与实验

## 定位

Python **只做离线工作**，不进入实时扫码链路：

- 模型训练、微调、量化、导出（ONNX/TensorRT）。
- 批量实验编排、指标统计、绘图、报告生成。
- 与论文参考实现逐像素对照。
- 通过 pybind11 调用 C++ 阶段，验证 C++ 移植是否一致。

## 建议目录

```text
python/
├── training/       # 训练脚本与配置
├── export/         # ONNX/TensorRT 导出
├── experiments/    # 跨阶段实验编排
├── bindings/       # pybind11 绑定
└── viz/            # 可视化与报告
```

## 规则

- 不把 Python 作为线上运行时依赖；C++ 缺失的能力不能靠 Python 进程补。
- 实验脚本必须记录 `run_meta.json`，与 C++ 实验同一规范。
- 参考实现（如 Chen 2023 光照均衡）保留为黄金对照，不直接复制进 C++ 而不做验证。
- 训练数据路径从项目内 `layers/*/data/input/` 或配置传入，不写死绝对路径。

## 当前状态

目录为空，待建立。旧 `perception` 工程的 Python 参考实现可迁移到 `python/reference/` 或各阶段 `experiments/`。
