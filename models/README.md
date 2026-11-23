# models：模型权重与部署

## 内容

| 子目录（建议） | 模型 | 用途 | 备注 |
|---|---|---|---|
| `roi/` | YOLOv8n / QRDet | ROI 检测 | ONNX 或 TensorRT engine |
| `superres/` | 超分/QR 重构 | 小模块增强 | 可选 |
| `inpaint/` | 受损修补 | 步骤 7-2 | 可选，高风险 |
| `decode/wechat_qrcode/` | WeChat QR 模型 | 解码 | 检查许可后再分发 |

## 规则

- 权重**不进普通 Git**；用 Git LFS、发布包或现场部署目录。
- 每个模型必须有 `MODEL_CARD.md`：来源、许可、输入尺寸、预处理、输出、精度、延迟、目标设备。
- TensorRT engine 与 GPU/驱动/版本绑定，不能跨机器直接复制；引擎由脚本在本机生成。
- 模型路径通过配置或环境变量指定，不写死在代码里。
- 模型缺失时阶段返回 `kModelUnavailable`，流水线可降级。

## 待确认

- [ ] 各模型的最终选型与许可。
- [ ] 是否统一走 ONNX Runtime，还是 Jetson 上用 TensorRT。
- [ ] 模型版本与仓库 commit 的对应关系。
