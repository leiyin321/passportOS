[English](README.md)

# Passport MP3 适配器

固件使用固定版本 Espressif `esp_audio_codec` 2.5.0 的 MP3 解码器（`esp_mp3_dec_*`，PacketVideo 整数实现）。`decoder.c` 将有界帧接口适配到 `passport_mp3.h`，检查输出长度、采样位宽和声道数。设备日志记录实际解码器堆占用和耗时，不把包装结构大小当作底层内存。从 OS 1.0.52 起替换 C3 上的标量浮点解码。

Espressif 组件提供目标平台二进制，因此主机测试使用便携参考解码器。参考实现来自 [lieff/minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633) 提交 `ea99364f61c14656440e8d77e9c233ccf3124633` 的 `minimp3.h` 和 CC0 许可。本地适配将堆工作区传给 `mp3dec_decode_frame`。其 22,928 字节分配和主机吞吐**不是**设备解码器测量；ESP 固件通过预处理排除此参考实现。

OS 帧组装器拒绝自由码率及 Layer I/II。主机测试覆盖合成正弦波、任意分块、重复打开／关闭、ID3 上限和畸形输入。多线程服务测试覆盖有界预缓冲、断流、EOF、取消及线程结束后回收。实际厂商解码器、耗时和音质在开发板上验证，见[城市电台报告](../../docs/test-reports/city-radio.zh_CN.md)。
