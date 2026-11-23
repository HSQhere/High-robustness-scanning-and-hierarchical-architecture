# common：公共契约

## 职责

- 定义跨阶段的数据结构：`Frame`、`Quad`、`RoiProposal`、`RoiImage`、`Status`。
- 定义步骤 4 的配置/结果类型：`EstimatorKind`、`ScaleEstimate`、`BinarizeConfig`、`BinarizeResult`。
- 提供与业务无关的工具：图像格式检查、奇数核夹取、配置 JSON 读写、指标统计、时间戳。
- 维护 `api_version` 与兼容性说明。

## 不做什么

- 不包含任何具体算法（二值化、解码、修补）。
- 不依赖任何 `layers/*` 目录。
- 不做文件 I/O 之外的业务逻辑；数据集遍历放 `apps/`/`experiments/`。

## 目录

```text
common/
├── include/qrscan/common/   # 公开头文件
│   ├── types.hpp            # Frame / Quad / RoiProposal / RoiImage / Status
│   ├── binarize_types.hpp   # 步骤 4 类型
│   ├── config.hpp           # 配置结构与 JSON 读写声明
│   └── metrics.hpp          # 指标统计
└── src/                     # 非头文件实现
```

## 接口冻结顺序

1. `Status` 与 `Frame`/`RoiImage`（所有阶段都依赖）。
2. `BinarizeConfig`/`BinarizeResult`（步骤 4 先落地）。
3. 解码/质量/流水线类型（M3 前冻结）。

## 当前状态

仅有目录与设计草案，见 [`docs/03-interfaces.md`](../docs/03-interfaces.md)。尚未生成头文件。

## 待确认

- [ ] JSON 库选型：nlohmann/json、OpenCV FileStorage 或自写最小解析器。
- [ ] 是否需要稳定的 C ABI（当前只承诺 C++ 接口）。
- [ ] 配置版本迁移策略。
