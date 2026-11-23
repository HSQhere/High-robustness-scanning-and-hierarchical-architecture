# 光照不均数据集批量结果

数据：`F:\Kiruno's Perfect Arithmetic Classroom\perception\database\qr\qr_samples`，匹配 `illum_*.png`，共 40 张。
期望文本：`156+123+516+231`。

| 类别 | 数量 | 原图解码 | 均衡图解码 | 二值图解码 |
|---|---:|---:|---:|---:|
| illum_shadow | 8 | 8/8 | 8/8 | 8/8 |
| illum_glare | 8 | 8/8 | 8/8 | 8/8 |
| illum_glare_hard | 8 | 5/8 | 7/8 | 7/8 |
| illum_grad | 8 | 8/8 | 8/8 | 8/8 |
| illum_mix | 8 | 8/8 | 8/8 | 8/8 |
| **合计** | 40 | 37/40 | 39/40 | 39/40 |

耗时（本机 Python + OpenCV，不含解码）：尺度估计 p50 128.97 ms，均衡+二值化 p50 0.643 ms。

说明：原图、均衡图、二值图各自独立用 `cv2.QRCodeDetector` 解码；三者失败集合不完全包含，实际使用应保留原图解码作为第一分支。
