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

本层可选构建单视图 C++ `WeChatDecoder`。它输出候选文本、输入视图坐标四角、源帧号、路线、状态与耗时，不计算未标定的置信度。`kSingle` 不表示合法任务码；非法候选不应阻止上层尝试其他路线。同一帧的三种视图只有一个源帧号。三帧放行、赛事校验与备选后端仍为草案，未改动 04 或默认流水线。

```sh
cmake -S . -B build-decode -DQRSCAN_ENABLE_WECHAT_DECODE=ON -DQRSCAN_WECHAT_MODEL_DIR=/path/to/wechat_models
cmake --build build-decode
ctest --test-dir build-decode --output-on-failure
```

应用构造 `qrscan::decode::WeChatDecoder(model_dir)` 时提供外置 `detect_2021nov`、`sr_2021nov` 的 `.prototxt` 与 `.caffemodel`，每个工作线程一个实例。CMake 的模型目录仅供可选真实模型冒烟测试使用，不写入库。没有模型或构建时缺少 `wechat_qrcode` 时有效图返回 `kBackendUnavailable`；空图/非 8 位单通道图返回 `kInvalidInput`。可用 `-DQRSCAN_USE_OPENCV_WECHAT=OFF` 测试无后端构建。未打包权重，不需要联网。

## 待确认

- [ ] WeChat 模型文件路径与许可；是否随仓库分发。
- [ ] ZXing-C++ 的构建方式（同编译器自建，避免 ABI 问题）。
- [ ] 是否引入 Quirc 作为第三后端。
- [ ] 三帧是“连续采集”还是“同一帧的三种预处理”；两者语义不同，当前按连续采集设计。
