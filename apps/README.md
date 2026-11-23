# apps：命令行与评估入口

## 计划中的可执行文件

### `qrscan_cli`

单图/单帧调试：

```text
qrscan_cli --image <path> [--estimator scanline|canny|hybrid] --out <dir>
```

输出：

- `01_input_gray.png`
- `02_illumination.png`
- `03_equalized.png`
- `04_binary.png`
- `05_montage.png`
- `result.json`（模块尺度、参数、耗时、质量指标、解码尝试）

### `qrscan_eval`

批量评估：

```text
qrscan_eval --config <json> --dataset <registry-key> --out <run_dir>
```

输出：

- `run_meta.json`
- `metrics.json`
- `per_image.csv`
- `report.md`

## 规则

- `apps/` 负责文件 I/O、参数解析、数据集遍历；阶段模块保持无文件 I/O。
- 所有入口必须支持 `--config`，保证实验可复现。
- 退出码：0 成功；非 0 表示运行失败（不是“没解出码”）。单图未解出码仍算运行成功，结果写 JSON。
- 不在这里实现算法；只编排阶段与输出。

## 当前状态

目录为空，待实现。优先实现 `qrscan_cli` 以服务步骤 4 的 C++ 重构与黄金对照。
