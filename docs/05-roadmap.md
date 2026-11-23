# 05 路线图：里程碑、分工、验收与风险

> 当前阶段只做**静止二维码**。运动扫码、多码同帧、其他码制不排入本期里程碑。

## 1. 里程碑

| 里程碑 | 内容 | 产出 | 状态 |
|---|---|---|---|
| **M0 架构与骨架** | 目录、接口草案、流程/实验/路线图文档、步骤 4 数据冒烟集 | 本仓库文档与目录 | **已完成** |
| **M1 步骤 4 C++ 重构** | 公共类型 + `04_binarize` 实现 + CLI + 单元测试 | 可编译的 `libqrscan_binarize`、`qrscan_cli`、测试 | **已完成** |
| **M2 步骤 4 实验** | 扫描线 vs Canny、去噪消融、三路必要性、53 张输出 | `experiments/results/step4_baseline/` 报告 | **已完成（Python 黄金对照/真实 ROI 待补）** |
| **M3 流水线骨架** | `08_pipeline` 状态机 + mock 阶段 + 与 `03_restore`/`05_decode` 接口联调 | 可跑通单帧静态流程 | 待开始 |
| **M4 视觉前端** | ROI 检测（YOLOv8n/QRDet）、几何重映射、超分评估 | 真实 ROI 上的端到端数据 | 待开始 |
| **M5 恢复与决策** | 质量判别器、代价函数、多重曝光、CNN 修补 | 失败分支的可解释策略 | 待开始 |
| **M6 现场验收** | 真实 8MP/120° 相机、Jetson、正式板与光照 | 冻结验收集 + 验收报告 | 待开始 |

## 2. 近期任务（按依赖排序）

### M1.1 冻结公共契约

- [x] `common/include/qrscan/common/types.hpp`：`Status`、`Frame`、`Quad`、`RoiImage`。
- [x] `common/include/qrscan/common/binarize_types.hpp`：`EstimatorKind`、`ScaleEstimate`、`BinarizeConfig`、`BinarizeResult`。
- [x] JSON 读取：`LoadBinarizeConfig()` 使用 OpenCV FileStorage（未单列 `common/src/config.cpp`）。
- [x] 约定 `api_version=1`；之后改字段必须升版本。

### M1.2 步骤 4 C++ 实现

- [x] 拆分函数：`ApplyDenoise`、`EstimateScanline`、`EstimateCanny`、`EqualizeIllumination`、`AdaptiveBinarize`。
- [x] `IScaleEstimator` 及 `ScanlineEstimator`/`CannyEstimator`/`HybridEstimator` + 工厂。
- [x] `Binarizer::Process` 输出 illumination/equalized/binary + 质量指标。
- [x] 所有参数来自 `BinarizeConfig`，代码里不写第二套默认值。
- [x] 失败路径：空图、非灰度、尺寸过小、尺度失败、窗口越界。

### M1.3 CLI 与测试

- [x] `qrscan_cli --image <path> --estimator scanline|canny|hybrid --out <dir>`：保存三路图 + `result.json`。
- [x] 单元测试：奇数核、窗口夹取、比例筛选、分箱、hybrid 回退、确定性。
- [ ] 黄金测试：Python 参考输出与 C++ 输出逐像素/容差比较。
- [ ] 性能测试：1MP、8MP ROI 的 p50/p95（280×280 已有数据）。

### M2 实验

- [x] 复现 40 张光照 + 11 基础 + 2 透视（53 张唯一图）。
- [x] `hybrid` 双估计一致性门控；`blur_1`/`rot_45` 正确回退扫描线。
- [x] 去噪消融：none / median3 / bilateral / gaussian。
- [x] 三路失败集合分析。
- [x] 输出 `report.md` + `metrics.json` + `per_image.csv` + 逐图三路/二值图。
- [ ] 均值窗/闭运算核/缩放常数/自适应 C 的完整消融。
- [ ] 真实 ROI 抽查（待现场采集）。

### M3 流水线

- [ ] `08_pipeline` 状态机：采集 → ROI → 几何 → 修补 → 二值化 → 解码 → 校验 → 判别。
- [ ] Mock 阶段：固定 ROI、恒等几何、恒等修补、假解码，用于先跑通编排。
- [ ] 超时/预算/回退日志。
- [ ] 与 `05_decode` 约定任务码校验注入点。

## 3. 分工（待认领）

| 模块 | 负责人 | 备注 |
|---|---|---|
| `common` | 待认领 | 接口冻结后其他人才能并行 |
| `00_capture` | 待认领 | 依赖相机 SDK |
| `01_roi_detect` | 待认领 | YOLOv8n/QRDet + TensorRT |
| `02_geometry` | 待认领 | remap/裁切/超分 |
| `03_restore` | 待认领 | 传统边界修补 + 透视校正 |
| `04_binarize` | 当前开发者 | 本次重构重点 |
| `05_decode` | 待认领 | WeChat/ZXing/三帧 |
| `06_quality` | 待认领 | 判别器/代价函数 |
| `07a_multiexposure` | 待认领 | 相机包围曝光 |
| `07b_inpaint` | 待认领 | CNN 修补 |
| `08_pipeline` | 待认领 | 编排/状态机 |
| `apps` / `datasets` | 待认领 | 工具与数据 |

> 认领后在 PR 中更新本表；接口变更必须通知所有依赖方。

## 4. 步骤 4 验收标准（M1/M2）

**功能**

- [ ] 支持 `scanline`、`canny`、`hybrid` 三种模式，配置可切换，结果可复现。
- [ ] 输出 illumination / equalized / binary 三路；原图路径始终保留。
- [ ] 输出 `module_px`、置信度、支持度、拟合误差、歧义标志、耗时。
- [ ] 输出 `saturation_ratio`、`dark_ratio`、`local_contrast` 等质量指标。
- [ ] 失败时返回明确 `Status`，不抛异常、不返回全黑图冒充成功。

**质量**

- [ ] 40 张光照集：模块估计失败 0（hybrid 模式），三路 union 不低于旧基线 40/40。
- [ ] 11 个基础条件：模块估计失败 0，union 不低于旧基线 11/11。
- [ ] `blur_1` 不得被 Canny “自信地错”直接采纳；必须触发回退或低置信度。
- [ ] 二值图与 Python 参考的不一致像素 ≤0.1%，差异可解释。
- [ ] 同一输入 + 同一配置，多次运行逐像素一致。

**性能（暂定，待 Jetson 实测后冻结）**

- [ ] 280×280：步骤 4 全流程 p50 ≤5 ms（C++，不含解码）。
- [ ] 1MP ROI：p50 ≤15 ms。
- [ ] 8MP 全幅不做步骤 4；只处理 ROI。
- [ ] 内存峰值不超过输入图像的 4 倍（不含模型）。

**工程**

- [ ] `-Wall -Wextra -Wpedantic` 无警告（至少本项目代码）。
- [ ] 单元测试覆盖核心分支；黄金测试可在 CI 跑。
- [ ] 配置 JSON 往返一致；默认值只有一处。
- [ ] README 更新：算法、参数、实验命令、已知限制。

## 5. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| Canny 在模糊图上“自信地错” | 二值化窗口错误，解码失败 | hybrid：支持度/窗口/ROI 尺度一致性门控 + 扫描线回退 |
| 扫描线 Python 慢 | 旧实验 100+ ms | C++ 重写、减少多阈值视图、按行/列并行；以 C++ 实测为准 |
| 透视下尺度估计不可靠 | 窗口错误 | 不靠估计器解决透视；先做 ROI/透视校正 |
| 局部饱和高光不可逆 | 任何二值化都救不回 | 输出 `saturation_ratio`；触发多重曝光，不承诺修补 |
| CNN 修补生成错误模块 | 错误接受，比赛风险高 | 不确定性 + 任务码校验 + 第二帧独立确认；默认不启用 |
| 数据集许可/体积 | 无法入库/仓库膨胀 | 小样本入库、完整集外置、LFS/发布包、保留 PROVENANCE |
| 接口漂移 | 并行开发冲突 | `common` 先冻结、`api_version`、PR 影响面说明 |
| 模型/后端版本不一致 | 结果不可复现 | `run_meta.json` 记录版本；黄金测试绑定容器/环境 |
| 8MP 全幅延迟 | 超预算 | 只处理 ROI；不在全幅跑重操作 |
| 任务码“非空但非法”挡住回退 | 漏检 | 解码后显式校验；区分空文本/非法文本；尝试备选后端 |

## 6. 待确认问题

1. 相机是否支持硬件包围曝光？若不支持，软件改曝光重拍的时间预算是多少？
2. ROI 检测用 YOLOv8n 还是 QRDet，还是两者并联？训练数据从哪来？
3. 超分网络是否值得上 Jetson？如果上，用哪一档模型、什么尾延迟可接受？
4. 修补网络只在“局部损坏/遮挡”启用，还是也处理低分辨率？两者的风险不同。
5. 多重曝光与修补同时可用时的默认优先级：先真实观测（多重曝光），还是先生成（修补）？
6. 任务码校验规则是否已在 `common` 中独立实现，避免解码模块依赖赛规？
7. 正式验收集的采集计划与冻结时间。

## 7. 下一步（本次会话之后）

1. 评审本架构与接口草案，确认目录名、命名空间、配置格式。
2. 冻结 `common` 最小类型集（不追求一次完整）。
3. 把旧 Python 参考实现逐函数移植到 `layers/04_binarize/src/`。
4. 先跑通单图 CLI，再做 53 张批量实验。
5. 用 C++ 结果复现旧表；差异逐项解释后再谈优化。
