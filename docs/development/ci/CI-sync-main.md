<p align="right"><a href="CI-sync-main.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Optional upstream synchronization

Passport OS `main` is the OS product, not the upstream hardware demo branch.
`.github/workflows/sync-main.yml` has no scheduled trigger. It runs only by manual
dispatch on a GitHub fork with `PASSPORT_UPSTREAM_SYNC=true`.

The default upstream is `leiyin321/passportOS`; a fork maintainer may explicitly set
`PASSPORT_UPSTREAM_REPOSITORY`. Review upstream changes and any conflicts before
opting in, especially custom trust roots, partitions and service defaults. The
standalone public OS repository is not a GitHub fork and skips this job.
Official `FoloToy/ai-passport` BSP updates are reviewed/cherry-picked separately;
do not automatically replace the OS Launcher with the factory demo.

The checkout selects `main` without persisted credentials. The pinned sync Action
uses the generated `GITHUB_TOKEN` with `contents: write`, not a personal token.
Fork owners must enable Actions and explicitly opt in. Inspect failures rather
than forcing a reset or discarding local product changes.
