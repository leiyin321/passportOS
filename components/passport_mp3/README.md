[简体中文](README.zh_CN.md)

# Passport MP3 adapter

Firmware uses the pinned Espressif `esp_audio_codec` 2.5.0 MP3 decoder (`esp_mp3_dec_*`, PacketVideo integer implementation). `decoder.c` adapts its bounded frame interface to `passport_mp3.h`; it checks output length, sample width and channel count. The device reports actual codec heap cost and decode timing. Opaque vendor memory is not inferred from the wrapper size. This replaces scalar floating-point decoding on C3 starting with OS 1.0.52.

Host tests use a portable reference decoder because the Espressif component supplies target-specific binaries. That reference is `minimp3.h` and its CC0 license from [lieff/minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633), commit `ea99364f61c14656440e8d77e9c233ccf3124633`. One local adaptation passes heap-owned scratch to `mp3dec_decode_frame`. Its 22,928-byte allocation and host throughput are **not** device decoder measurements. The reference is excluded from ESP firmware by preprocessing.

The OS assembler rejects free-bitrate and Layer I/II frames. Host tests exercise synthetic sine audio, arbitrary chunks, repeated open/close, ID3 limits and malformed streams. Threaded service tests cover bounded prebuffering, stalls, EOF, cancellation and joining before resource release. Actual vendor decoding, timing and audio quality are tested on the board; see the [City Radio report](../../docs/test-reports/city-radio.md).
