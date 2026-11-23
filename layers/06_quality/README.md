# 06_quality：质量判别与代价函数

## 职责

- 汇总步骤 4/5 的诊断，估计“继续当前路径还能不能救”。
- 计算缺失面积、饱和比例、暗区比例、模块尺度置信度、清晰度、CNN 不确定性等。
- 用代价函数在动作集合中选择下一步：换路、换估计器、回几何、多重曝光、修补、超分、失败。
- 只给建议，不自己执行动作；执行由 `08_pipeline` 负责。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| `BinarizeResult`（含质量指标） | `QualityReport` |
| `DecodeResult`（空/非法/歧义） | `Action recommended` + `reason` |
| 可选 CNN 延迟/不确定性 | `estimated_failure_probability` |

## 对外接口（草案）

```cpp
class IQualityJudge {
 public:
  virtual ~IQualityJudge() = default;
  virtual QualityReport Evaluate(const BinarizeResult& binarize,
                                 const DecodeResult& decode) const = 0;
};
```

- 动作集合：`kDecodeOtherRoute`、`kRetryEstimator`、`kBackToGeometry`、`kMultiExposure`、`kInpaint`、`kSuperRes`、`kFail`。
- 先过滤不可用动作（模型缺失、相机不支持、超预算），再按代价排序。
- 默认不推荐生成式动作；只有“局部损坏且未饱和”才推荐修补。
- 信息不可逆丢失（饱和/欠曝）优先多重曝光，不承诺修补。

## 数据与实验

- `data/input/`：带质量标注的失败帧（可救/不可救）。
- `data/output/`：质量指标 CSV、动作选择混淆矩阵（默认不入 Git）。
- 实验：
  - 阈值标定：`saturation_ratio`/`missing_area_ratio` 与“能否解码”的 ROC；
  - 代价函数权重标定：用历史帧回放，比较推荐动作与实际最优动作；
  - 误判代价：把“可救”判成“失败” vs 把“不可救”判成“修补”的代价不同。

## 当前状态

骨架。质量指标来自步骤 4 的占位定义；判别器尚未实现。

## 待确认

- [ ] `missing_area_ratio` 的估计方法（掩膜/定位图案缺失/解码器反馈）。
- [ ] 代价函数权重与 `C_fail` 阈值。
- [ ] 是否引入轻量 CNN 判别器，还是先用规则。
- [ ] “可救/不可救”的标注规范。
