<p align="right"><strong>简体中文</strong> · <a href="ota-1.0.17-rollback.md">English</a></p>

# OTA 1.0.17 回滚与 1.0.18 修复

## 设备证据

用户通过“设置 → 系统更新”下载并安装了 1.0.17。候选版本从 `ota_1` 启动，
ESP-IDF 状态为 `PENDING_VERIFY`。存储初始化通过后，候选版本确认自身之前，
`passport_http` 任务触发栈保护异常。ESP-IDF 自动回滚到 `ota_0`；设备以
1.0.16 重启，并成功进入 `Passport OS ready`，Wi-Fi 也已连接。

符号化前已验证与设备匹配的归档 ELF：

- 版本/build：`1.0.17` / `2026102520`
- ELF SHA-256：`030e1405510005777d424d5c2f8aa7676179440f03194df1e24500dcbbe6a172`
- 故障任务：`passport_http`
- 栈范围：`0x3fcdc8b4`–`0x3fcde8b0`；采集到的 SP 为 `0x3fcdbd10`，低于栈底 2,980 字节
- PC `0x42012a52` 映射到 `os/services/network/network.c` 的 `worker`；MEPC `0x420131f2` 映射到该 worker 中的 `xQueueReceive`。

目标 ABI 下网络 `job` 描述符大小为 9,308 字节。原实现将其作为自动局部变量，
而 FreeRTOS worker 栈只有 8,192 字节；worker 还包含其他局部缓冲区。这直接
解释了观察到的栈越界。故障不是签名拒绝或下载镜像损坏：候选镜像成功启动并
完成了存储初始化。

## 1.0.18 修复

网络 job 现在只在堆上分配一次，单项 FreeRTOS 队列改为存储指针，不再复制整个
job。worker 和所有请求入口均不再把大型描述符放入任务栈。提交失败和 worker
处理完成后都会释放该分配。新增宿主回归测试检查所有权与栈分配约束。

指针队列占 4 字节，而旧队列项占 9,308 字节，空闲时节省 9,304 字节。网络请求
执行期间会使用一个 9,308 字节的堆 job。固件 `.dram0.bss` 为 64,608 字节，
与 1.0.17 相同。

## 验证与发布

- `python3 tests/passport/test_network_worker_stack.py`：2 项通过。
- `PASSPORT_OS_VERSION=1.0.18 ./tools/validate.sh`：完整宿主测试及 ESP-IDF 5.5.3 门禁通过。
- 应用镜像：2,278,416 / 2,621,440 字节；剩余 343,024 字节（13.08%）。
- 合并镜像：2,409,488 字节；SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`。
- 签名更新：version `1.0.18`，build `2026102521`，应用 SHA-256 `ecd9e121907b68b187deb1eeda97d5d28d5c7f602272793ef3fcbbb64d4cf99e`。
- 本机更新服务返回了完整镜像，大小及 SHA-256 匹配；生成发布包时验签通过。
- 后续真机验收：用户从 Settings 安装 1.0.18。候选版本从 `ota_1` 启动，版本为 1.0.18，ELF SHA-256 为 `c317ec41cb3630a0c95e3cfaccb1c278449b4d55ace3c14c98e3e885d6052027`。存储初始化通过，日志出现 `OTA self-test valid rc=0`，随后 Wi-Fi 到达 `WIFI online`；用户确认 About 显示 1.0.18。候选版本启动后的完整日志没有 panic。
- 候选启动资源指标：空闲堆 85,320 字节，历史最低空闲堆 85,264 字节，最大连续块 73,728 字节；`passport_http` 任务栈高水位 3,884 字节。这是启动时数据，不代表网络传输峰值。

原始 panic 采集保存在被 Git 忽略的 `local/`，未提交，以免传播设备日志数据。
