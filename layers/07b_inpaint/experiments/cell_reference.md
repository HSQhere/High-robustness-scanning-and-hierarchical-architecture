# 7-2 格点级恢复：离线参考

这是 7-2 的一种二维码专用实现：输入已知格数、已透视校正且已对齐的 RGB 码区（不含静区），输出每格是黑色的分数 `p_dark`。低可见性使主评分向 0.5 收缩；`ambiguity = 1 - abs(2*p_dark - 1)` 只是接近判定边界的程度，**不是校准后的错误概率，也不是受损掩膜**。本实验不生成照片，不识别文本，不投票，也不把已完全丢失的信息当成原始观测。网格定位、判极性/版本与后续解码不在本接口内；网格明显错误时不要调用。

仅用于离线对照，依赖 Python 3、NumPy、OpenCV-Python；不修改 CMake、原 `IInpainter` 草案或比赛运行链路。不能用颜色与参考答案调参后再按同一图评分。

须由贡献者单独提供冻结的 `weights.npz`；本分支不内嵌或下载权重。参考权重 SHA-256：`329cef8f902959271f95ccfe5688871e9f4b560ac00cb4ba55879dea79ccc9f8`。

```sh
cd layers/07b_inpaint/experiments
export QRSCAN_CELL_WEIGHTS=/path/to/weights.npz
python3 test_cell_reference.py -v
python3 cell_reference.py --weights "$QRSCAN_CELL_WEIGHTS" --patch aligned.png --modules 25 --polarity 1 --out scores.npz
```

`--polarity 1` 对应普通黑码白底，`-1` 对应反色。`--out` 不覆盖已有文件。通过离线数值对照不等于通过 Jetson 延迟、现场识别率或像素级修补验收。
