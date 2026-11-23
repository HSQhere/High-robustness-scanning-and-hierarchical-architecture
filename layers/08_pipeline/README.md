# 08_pipeline：流程编排

## 职责

- 实现完整状态机：采集 → ROI → 几何 → 修补/透视 → 二值化 → 解码 → 三帧校验 → 质量判别 → 回退/接受/失败。
- 管理帧预算、阶段超时、重试次数、回退顺序。
- 注入任务码校验器、质量判别器、可选模型；阶段之间只通过 `common` 类型通信。
- 输出统一 `PipelineResult`：任务码、失败原因、每次尝试、质量报告、总耗时。
- 记录遥测：每阶段耗时、动作选择、回退原因、配置哈希。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| `std::vector<Frame>`（1–3 帧） | `PipelineResult` |
| `PipelineConfig` | 每阶段 `Status`/耗时/诊断 |
| 注入的 `ITaskCodeValidator` 等 | 可回放的 `run_meta` |

## 对外接口（草案）

```cpp
class IPipeline {
 public:
  virtual ~IPipeline() = default;
  virtual PipelineResult Run(const std::vector<Frame>& frames,
                             const PipelineConfig& config) = 0;
};
```

- 流水线是唯一知道完整流程的模块；阶段不反向依赖它。
- 不实现具体算法；只调用阶段接口。
- 超时/预算耗尽时返回最佳诊断，不无限重试。
- 所有回退必须记录“从哪一步回、为什么、代价多少”。
- 支持 mock 阶段，便于在模型/相机缺失时先跑通编排。

## 数据与实验

- `data/input/`：回放帧序列 + 配置。
- `data/output/`：`PipelineResult` JSON、阶段耗时、回退轨迹（默认不入 Git）。
- 实验：
  - 端到端成功率（任务码合法率）；
  - p50/p95 总延迟；
  - 故障注入：ROI 失败、解码空、任务码非法、模型缺失、超时；
  - 回放一致性：同一输入 + 配置多次运行结果一致。

## 当前状态

骨架。状态机图见 [`docs/02-pipeline.md`](../../docs/02-pipeline.md)。

## 待确认

- [ ] 进程/线程模型（单进程多线程 vs 多进程）。
- [ ] 总预算与各阶段预算的默认值。
- [ ] 是否需要支持“只跑单阶段”的调试模式。
- [ ] 遥测输出格式与落盘位置。
