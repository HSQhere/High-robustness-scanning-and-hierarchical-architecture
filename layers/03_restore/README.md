# 03_restore：边界修补与透视校正

## 职责

- 对 ROI 做传统 CV 边界修补（形态学、inpaint、边缘延拓），处理裁切损伤、遮挡边缘、局部缺失。
- 做透视校正/精校正：优先用二维码定位图案/定时图案，退化时用 ROI 四角。
- 输出修补后的 `RoiImage` + 损伤掩膜 + 质量标记；不承诺“修复到可读”，只负责把图像交给步骤 4。
- 生成式修补不在这里，见 `07b_inpaint`。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| `RoiImage` | 修补/校正后的 `RoiImage` |
| `RestoreConfig` | `damage_mask`、`Status`、诊断 |
| 可选定位图案 | 精校正单应矩阵 |

## 对外接口（草案）

```cpp
class IRestorer {
 public:
  virtual ~IRestorer() = default;
  virtual Status Restore(const RoiImage& input,
                         const RestoreConfig& config,
                         RoiImage* output,
                         cv::Mat* damage_mask) = 0;
};
```

- 未检测到损伤时原样透传，`status=kOk` 且 `damage_mask` 为空，不能伪造掩膜。
- 修补区域必须有掩膜；下游据此判断“观测”还是“生成”。
- 透视校正失败时返回原图 + `kLowConfidence`，不能返回黑图。
- 不做二值化、不做解码。

## 数据与实验

- `data/input/`：带遮挡/裁切损伤的 ROI 小样本。
- `data/output/`：修补前后对比、掩膜可视化（默认不入 Git）。
- 实验：修补方法（inpaint / 形态学 / 边缘延拓）、掩膜膨胀、透视精校正的角点误差、对下游解码的影响。

## 当前状态

骨架。传统修补的具体算法与参数 `[待确认]`。

## 待确认

- [ ] 透视校正是用 ROI 四角还是定位图案拟合；两者失败时如何回退。
- [ ] 损伤掩膜从哪里来（分割/阈值/人工）。
- [ ] 修补是否可能引入假模块；如何在下游门控。
