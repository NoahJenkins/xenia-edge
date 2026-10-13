# Prepared Dell Linux CI migration

This change is prepared for `NoahJenkins/xenia-edge`, based on `origin/edge`
(`9d8210b32ddd3f0c8ab7d5df18798ec9e513aa65`). Live acceptance is pending.
Do not enable `OPTIPLEX_CI_ENABLED` until infrastructure acceptance.

## Scheduling

| Job / check name | Original runner | Opted-in, trusted caller | Disabled or fork PR |
| --- | --- | --- | --- |
| Lint | ubuntu-24.04 | optiplex-ci-ubuntu24 | ubuntu-24.04 |
| Build Linux | ubuntu-22.04 | optiplex-ci-ubuntu22 | ubuntu-22.04 |
| Build macOS ARM64 | macos-26 | macos-26 | macos-26 |
| Build macOS x86_64 (cross from arm64) | macos-26 | macos-26 | macos-26 |
| Build Windows | windows-2025 | windows-2025 | windows-2025 |
| Create Release | macos-26 | macos-26 | macos-26, subject to existing guard |

Only the two Linux `runs-on` selectors change. Names, required check contexts,
triggers, dependencies, caller permissions (`contents: read`, `actions: write`),
action versions, caching, artifacts and implicit timeouts/cancellation remain
unchanged. No project concurrency setting is added. `CI.yml` is unchanged.
The release job retains `contents: write` and its exact upstream repository,
`push`, and `refs/heads/edge` guard. It cannot publish from NoahJenkins's fork.

The repository variable must contain the exact lowercase value `true` to route
trusted jobs to Dell. Unset, empty, `false`, and unrelated values use hosted
runners. GitHub's string equality ignores case, so a second `fromJSON` check
rejects `TRUE`/`True`: these malformed opt-ins fail before assigning a runner on
trusted events. Use unset or `false` to disable; do not use uppercase booleans.
This deliberate fail-closed configuration validation avoids silently enabling
Dell with uppercase values without introducing a new check/job.

PR trust is determined from `github.event.pull_request.head.repo.full_name`
versus `github.repository`, including Dependabot fork PRs. Missing head-repository
information stays hosted. The fork check short-circuits before variable parsing.
Reusable workflows retain the caller's `github` context, so the original event
and PR head are available without additional caller inputs. See GitHub's
[reusable workflow context reference](https://docs.github.com/en/actions/reference/workflows-and-actions/reusing-workflow-configurations#github-context)
and [expression semantics](https://docs.github.com/en/actions/reference/workflows-and-actions/expressions).

## Infrastructure image and package requirements

These exact custom labels must identify new repository-scoped, ephemeral x64
VM workers on `optiplex-7040`. Never reuse or relabel `optiplex-tailscale-policy`
or `tailscale-policy`.

- `optiplex-ci-ubuntu24`: reviewed Ubuntu 24.04 **x86_64 VM** image, Python 3,
  Git, CA certificates and pipx. The existing Setup step installs
  `clang-format==21.1.8` and places `$HOME/.local/bin` on PATH. Run the unchanged
  `./xenia-build.py lint --all`.
- `optiplex-ci-ubuntu22`: reviewed Ubuntu 22.04 **x86_64 VM** image; glibc 2.35
  is the build compatibility boundary. Never build on the Ubuntu 24.04 host or
  a 24.04 VM. Provide the GitHub-hosted image prerequisites: passwordless guest
  sudo, Bash, Git, Python 3, wget/curl, CA certificates, GnuPG, lsb-release,
  software-properties-common, GNU grep/coreutils, tar/gzip/xz/unzip, pkg-config,
  build-essential and liblz4-dev. CMake must satisfy the source's >=3.20
  requirement; Ninja and CMake are also installed by the unchanged Setup step.
  linuxdeploy and its plugin need guest FUSE support (`libfuse2`, guest
  `/dev/fuse`); provide binutils, file and squashfs-tools for acceptance checks.
  All package operations and package caches are inside the disposable guest.

The Linux Setup step continues to configure apt.llvm.org's Jammy LLVM 21 repo
and `ppa:ubuntu-toolchain-r/test`, then install exactly the existing packages:
`clang-21`, `lld-21`, `libclang-rt-21-dev`, `llvm-21`, `g++-14`, `libgtk-3-dev`,
`libx11-xcb-dev`, `ninja-build`, `cmake`, `libfontconfig1-dev`, `libxtst-dev`,
`libasound2-dev`, `libpulse-dev`, `libudev-dev`, `libdbus-1-dev`, `libx11-dev`,
`libxext-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`,
`libxss-dev`, `libxkbcommon-dev`, and `libxfixes-dev`.

Keep Slang **2026.8**, the pinned submodule commits and existing shallow update
(including the Linux DirectXShaderCompiler exclusion), and `fetchdata` downloads.
Keep the existing continuous linuxdeploy/plugin downloads and cache key,
Jammy apt cache key, and `mozilla-actions/sccache-action@v0.0.10` with
`SCCACHE_GHA_ENABLED=true` and both CMake compiler launchers. GHA cache reads
and writes remain remote; fork tokens continue to self-demote to read-only.
The reviewed image must support the current Actions runner and unchanged
checkout/cache/upload action versions. Allow outbound GitHub/Actions artifact
and cache services, PyPI, Slang/linuxdeploy releases, apt repositories and data
repositories referenced by the source; no access to the home network.

The infrastructure controller owns the pool-wide **one active job**, **<=4
vCPUs**, **<=16 GiB RAM** contract. Other jobs queue normally. Use one fresh VM
per job with an independent runner home/workspace, no shared writable disks,
no host package-cache mounts, Docker socket, SSH credentials, home directories
or production data. Any Docker execution is inside the guest. Destroy writable
disks on success, failure, cancellation and timeout. This repository does not
implement or claim that lifecycle guarantee.

The native macOS/Windows jobs and release publication remain hosted. There are
no production deployment/migration, signing/attestation or privileged provider
repair workflows in these YAML files. GitHub-managed Dependabot, CodeQL default
setup and Copilot workflows are outside this migration and unchanged.

## Validation and acceptance

Run focused semantic coverage (Node >=20):

```sh
npm --prefix tools/ci ci --ignore-scripts
npm --prefix tools/ci test
# actionlint 1.7.12; custom scalar labels are valid GitHub syntax.
actionlint -shellcheck='' .github/workflows/CI.yml .github/workflows/build.yml
git diff --check
```

The tests parse the actual workflows and evaluate their expressions with
GitHub's `@actions/expressions` implementation. They cover enabled/disabled/
missing variables, push/manual/same-repository/fork PRs, owner-authored and
Dependabot-authored forks, missing head metadata, malformed opt-ins, draft
suppression, the release guard, and artifact upload/download names. A structural
comparison with the reviewed Git base checks that everything except the two
selectors is unchanged; it does not compare a copied YAML fixture. For a new
base, set `CI_BASE_REF` to the reviewed commit when running the suite.

Preparation observations: GitHub reports default branch `edge`, **zero**
registered repository runners, and no opt-in variable. There is no usable local
Linux VM (the local Docker daemon is unavailable). Local lint on macOS with the
pinned formatter is only a preparation check. Hosted Ubuntu CI, if successful,
is compatibility evidence; it is not evidence of Dell execution or cleanup.

After infrastructure acceptance, record all of the following at the exact PR
head SHA before declaring migration accepted:

1. A complete caller `CI` run with the unchanged lint command on
   `optiplex-ci-ubuntu24` and the entire Linux Setup, Release build,
   `Prepare artifacts`, and upload on `optiplex-ci-ubuntu22`. Record run URL,
   head SHA, runner names/labels, reviewed image IDs, guest `/etc/os-release`,
   `uname -m`, `ldd --version`, compiler/package versions and sccache stats.
   A draft PR intentionally skips Build on its PR event; an authorized branch
   push exercises the normal CI caller without changing draft behavior.
2. Capture wall time, peak guest/pool memory, CPU allocation, disk use and cold/
   warm cache behavior across the full build and packaging. If runtime or
   resources exceed the infrastructure contract, report measurements and
   request an infrastructure adjustment; do not raise host limits or drop jobs.
   No build-duration limit beyond the original workflow's implicit timeout is
   specified here; confirm any controller deadline with the infrastructure agent.
3. Download `xenia_edge_linux` from the **caller run** with the ordinary artifact
   API (`gh run download RUN_ID -R NoahJenkins/xenia-edge -n xenia_edge_linux`).
   Inspect the downloaded archive's complete file list and hashes: its top level
   must contain exactly `xenia_edge.AppImage` and `LICENSE`; LICENSE must match
   the repository. Preserve all existing AppImage content, icons, desktop file,
   executable and bundled libraries. The caller's existing download-artifact
   step still names `xenia_edge_linux`; Create Release stays skipped on this fork.
   Do not run or weaken the publishing job to test downloads.
4. In a disposable Ubuntu 22.04 verification guest, inspect `file` and `readelf
   -h` for the AppImage and extracted `usr/bin/xenia_edge` (x86-64 ELF).
   Extract using `--appimage-extract` or squashfs-tools, inventory every bundled
   ELF and run `readelf --version-info` on each. Record **required**, not merely
   defined, GLIBC and GLIBCXX/CXXABI versions; required GLIBC must be <=2.35.
   Check `readelf -d` NEEDED/RPATH/RUNPATH and resolve dependencies in the
   extracted AppDir so bundled libstdc++ satisfies GCC 14's requirements.
   Compare contents against hosted packaging at the same source/head; never
   infer the baseline solely from an Ubuntu label or a green lint job.
5. Exercise a normal fork PR through CI.yml (including a Dependabot fork case):
   confirm both Linux jobs run hosted, retain expected permissions, and upload
   artifacts. Confirm both native macOS jobs and Windows stay on their original
   runners and that release is skipped on NoahJenkins/xenia-edge.
6. Obtain controller evidence for separate lint/build VMs, pool-wide serialization,
   no host mounts/home-network access, and writable-disk destruction following
   success, failure, cancellation and timeout. Runner unregistration alone is
   not disk-cleanup proof.

At preparation time no Dell build, packaging/resource measurements, runtime
baseline, downloaded artifact inspection, real fork run or cleanup proof is
claimed. These are live acceptance requirements, not reasons to activate early.

## Rollback

An authorized infrastructure operator can unset `OPTIPLEX_CI_ENABLED` or set it
to lowercase `false`; subsequent Linux jobs use the original hosted selectors.
Already queued/running jobs keep their assigned runner: cancel them through
normal Actions controls if needed and verify controller cleanup. Do not change
runner labels or reuse a writable guest. Revert the migration commit through
normal review to remove the routing change; `CI.yml`, release guard and native
runner selectors require no repair. No settings or variables were changed by
this preparation task.
