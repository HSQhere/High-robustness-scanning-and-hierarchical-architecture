# 07a_multiexposure：多重曝光包围

## 职责

- 在过曝/欠曝导致信息不可逆丢失时，用预设值、阈值、下一值各拍一帧（3 帧）。
- 按光照/饱和度加权融合为一张，保留暗部与亮部信息，避免新增饱和。
- 融合后回到步骤 5 解码；不在这里做二值化或生成式修补。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| 当前相机参数 + 包围步长 | 3 帧 `Frame` |
| 3 帧图像 | 融合帧（BGR 或灰度）+ 融合质量报告 |

## 对外接口（草案）

```cpp
class IExposureController {
 public:
  virtual ~IExposureController() = default;
  virtual Status CaptureBracket(const BracketRequest& request,
                                std::vector<Frame>* frames) = 0;
  virtual Status Fuse(const std::vector<Frame>& frames,
                      const FuseConfig& config,
                      Frame* fused) = 0;
};
```

- 包围参数：基准值、步长、最小/最大界；必须检查相机界，越界时夹取并记录。
- 融合只做真实观测的加权平均/曝光融合，不做生成式补全。
- 已经饱和的像素无法恢复；融合只能利用其他曝光帧中未饱和的对应像素。
- 相机不支持硬件 bracketing 时，软件改曝光重拍；必须报告额外耗时。

## 数据与实验

- `data/input/`：不同曝光序列（暗光、过曝、混合光照）。
- `data/output/`：融合前后对比、饱和比例变化（默认不入 Git）。
- 实验：
  - 曝光步长与帧数（3 帧是否足够）；
  - 融合方法（加权平均 / Mertens / 简单饱和掩膜）；
  - 3 帧采集+融合的总耗时；
  - 融合后解码成功率与“新增伪影”检查。

## 当前状态

骨架。相机是否支持硬件包围曝光 `[待确认]`。

## 待确认

- [ ] 曝光步长是固定值还是按当前亮度自适应。
- [ ] 融合在 BGR 还是灰度域；是否需要先做增益归一化。
- [ ] 3 帧采集是否计入总时间预算。
