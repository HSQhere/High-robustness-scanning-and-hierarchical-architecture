# 05_decode：解码与三帧校验

## 职责

- 对步骤 4 的三路图像（原图 / 均衡图 / 二值图）执行解码。
- 主后端 WeChat QR；备选 ZXing / Quirc；后端不可用时明确报告。
- 对相邻 3 帧做一致性校验；文本非空不等于任务码合法，必须注入任务码校验器。
- 输出 `DecodeResult`，包括文本、后端、置信度、帧号、耗时、失败原因。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| 三路 `cv::Mat` | `DecodeResult`（每路/每后端） |
| 相邻 3 帧 | 接受/歧义/重试建议 |
| `ITaskCodeValidator` | 合法任务码或 `kInvalidTaskCode` |

## 对外接口（草案）

```cpp
class IDecoder {
 public:
  virtual ~IDecoder() = default;
  virtual DecodeResult Decode(const cv::Mat& image) const = 0;
};

class ITaskCodeValidator {
 public:
  virtual ~ITaskCodeValidator() = default;
  virtual Status Validate(const std::string& text, std::string* task_code) const = 0;
};
```

- 解码器只负责“图 → 文本”，不依赖赛规；任务码校验由流水线注入。
- 空文本、非法文本、后端不可用、超时分别返回不同状态。
- 三帧规则：
  - 至少 2 帧相同合法码 → 接受；
  - 只有 1 帧合法 → 默认接受但标记 `single_frame_accept`；
  - 两帧不同合法码 → `kAmbiguous`，不静默选择；
  - 文本非空但校验失败 → 不产生 `TaskCode`，继续尝试备选后端/下一帧。
- 不在解码器内做多码投票（现场一帧最多一个合法码）。

## 数据与实验

- `data/input/`：三路图像冒烟样本。
- `data/output/`：解码结果 JSON、失败帧可视化（默认不入 Git）。
- 实验：WeChat vs ZXing vs Quirc（成功率、p50/p95、失败集合）、三帧收益、文本非空但非法任务码的比例、后端不可用降级。

## 当前状态

骨架。旧 `perception` 工程已有 WeChat/ZXing 实验代码，可迁移；新仓库尚未接入。

## 待确认

- [ ] WeChat 模型文件路径与许可；是否随仓库分发。
- [ ] ZXing-C++ 的构建方式（同编译器自建，避免 ABI 问题）。
- [ ] 是否引入 Quirc 作为第三后端。
- [ ] 三帧是“连续采集”还是“同一帧的三种预处理”；两者语义不同，当前按连续采集设计。
