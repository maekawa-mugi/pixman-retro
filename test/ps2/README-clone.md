# PS2 v5 — Clone-only WSL build

This branch includes the **complete v5 source tree imported from the
uploaded ZIP**, without generated artifacts or `.git` from the ZIP.
The standalone SDK build and on-screen A/B regression suite are in
`test/ps2/`. The latency and throughput experiments, Solid OVER candidate and 72-case API benchmark are integrated into the test harness. Production Pixman dispatch still uses baseline kernels. Host validation includes injected latency, throughput and unattributed group failures.

## Ubuntu / WSL

Requires a preinstalled PS2DEV + PS2SDK with EE GCC, and Ubuntu host
tools (Python 3, Meson, Ninja, C compiler). Install host prerequisites
once if needed:

```bash
sudo apt-get install -y git build-essential python3 python3-venv ninja-build pkg-config
python3 -m venv "$HOME/.venvs/ps2-meson"
"$HOME/.venvs/ps2-meson/bin/pip" install 'meson>=1.3'
source "$HOME/.venvs/ps2-meson/bin/activate"
```

Then, from any working directory:

```bash
git clone -b codex/ps2-mmi-fixes-squashed-20261009 \
  https://github.com/maekawa-mugi/pixman-retro.git
cd pixman-retro
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
bash test/ps2/build-from-clone.sh
```

The wrapper validates compiler + SDK files, runs the **17-group A/B
host regression**, then builds the static Pixman libraries and links
`build-ps2-mmi/target/pixman_mmi_test.elf` via the genuine
`test/ps2/build.sh` v5 script. It also writes
`build-ps2-mmi/target/SHA256SUMS.txt`.

For an existing `PS2DEV` with a nondefault path, set it before running
the wrapper. The wrapper also detects `$HOME/ps2dev` and
`/opt/ps2dev` when `PS2DEV` is unset.

**Validation:** the ZIP v5 patch was checked and applied to the
same base revision locally; shell syntax passed. The imported v5
source passed native `bash test/ps2/verify-host.sh` with
`INTEGRATED_RESULT,PASS,checks=12878,groups=17,A_bad=0,B_bad=0`.
A fresh PS2SDK cross-build, PCSX2 and real PS2 run have **not**
been performed in the assistant's environment.
