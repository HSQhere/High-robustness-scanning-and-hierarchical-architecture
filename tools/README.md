# tools：采集、标注与可视化工具

## 计划

| 工具 | 用途 |
|---|---|
| `capture/` | 相机参数预设、单帧/连拍/包围曝光采集、参数快照 |
| `roi_label/` | ROI 四角标注与复核 |
| `qr_generate/` | 生成合成二维码样本（尺寸、旋转、透视、光照、模糊） |
| `dataset_manifest/` | 生成/校验数据集清单与哈希 |
| `visualize/` | 中间图拼图、失败案例浏览 |

## 规则

- 工具可以依赖 Python/OpenCV，但产出必须可被 C++ 链路复现。
- 合成样本生成器必须记录参数与随机种子，保证可重建。
- 采集工具必须把相机参数、曝光、白平衡、时间戳写入 sidecar 文件。
- 不把临时调试脚本提交到 `src/`；一次性脚本放 `experiments/` 或不入库。

## 当前状态

目录为空。旧 `perception/tools/` 中的 `qr_eval`、`material_capture` 可选择性迁移。
