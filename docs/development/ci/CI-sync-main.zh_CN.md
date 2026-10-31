<p align="right"><strong>简体中文</strong> · <a href="CI-sync-main.md">English</a></p>

# 可选上游同步

Passport OS 的 `main` 维护 OS 产品，不是上游硬件 demo 分支。
`.github/workflows/sync-main.yml` 没有定时任务，仅在 GitHub fork 上、
`PASSPORT_UPSTREAM_SYNC=true` 时允许手动运行。

默认上游为 `leiyin321/passportOS`，fork 维护者可显式设置
`PASSPORT_UPSTREAM_REPOSITORY`。启用前审核变更及冲突，尤其自己的信任根、分区和默认服务。
当前独立公共 OS 仓库不是 GitHub fork，会跳过此任务。
官方 `FoloToy/ai-passport` BSP 更新单独审核/挑选，不自动用出厂 demo 替换 OS 桌面。

检出明确选择 `main`，不持久保存凭据。固定 SHA 的同步 Action 仅使用自动生成、
具备 `contents: write` 的 `GITHUB_TOKEN`，无需个人令牌。
fork 所有者需启用 Actions 并主动开启同步，失败后检查原因，不强制重置或丢弃本地产品变更。
