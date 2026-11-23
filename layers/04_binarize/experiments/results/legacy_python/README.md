# 旧 Python 实验基线（只读参考）

来源：旧 `perception` 工程
`tests/qrv2/qr_dynamic_illumination_demo/`，运行于 2026-11-19 / 2026-11-20。
环境：Python + OpenCV 4.10，Windows 开发机；**不是 C++ / Jetson 数据**。

## 文件

| 文件 | 内容 |
|---|---|
| `finder_compare_report.md` | 扫描线 vs Canny 的完整对照报告（旧仓库原文） |
| `finder_compare_results.json` | 逐图模块估计、候选数、支持度、歧义、耗时、三路解码 |
| `illumination_dataset_summary.json` | 40 张光照样本的逐图参数/耗时/解码汇总 |
| `illumination_dataset_summary.md` | 上述汇总的人读版本 |
| `illum_glare_hard_montage.png` | 强亮斑类别拼图：原图 / 照明估计 / 均衡图 / 二值图 |
| `illum_shadow_montage.png` | 阴影类别拼图 |

## 关键结论（供 C++ 重构对照）

- 40 张光照：三路并集 40/40；单路最高 39/40；`scanline` 模块误差 0，`canny_norm` 0.06 px。
- 11 个基础条件：`scanline` 0 失败、max 0.215 px；`canny_strict` 失败 2；`blur_1` 上所有 Canny 变体估错（3.0–3.4 px，真值 9.6）。
- 2 个透视条件：两种估计器都不可靠，只有 `persp_15` 可解；透视应由几何解决。
- `scanline` Python 耗时 105–153 ms；Canny 变体 15–33 ms。C++ 重写后必须重新测，不能直接引用。

## 使用限制

- 这些结果是**历史基线**，用于检查 C++ 移植是否回归；不能作为最终验收数据。
- 数据集是 280×280 合成图 + 少量 1280×800 实拍，不是 8MP ROI。
- 旧报告中的“0 失败”指解码并集，不等于模块估计全部正确；详见报告 §8。
- 论文参数未公开，旧实现是工程复现，不是官方代码。

## 复现（旧工程）

```bash
cd <old_perception>/tests/qrv2/qr_dynamic_illumination_demo
python experiments/finder_compare.py
python run_dataset.py
```

C++ 复现入口在 M1 完成后由 `apps/qrscan_eval` 提供；结果应写入新的 `run_id` 目录，不覆盖本目录。
