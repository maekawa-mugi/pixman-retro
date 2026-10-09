#!/usr/bin/env bash
# Clone-only v5 PS2SDK build, with mandatory integrated host verification.
# Output: build-ps2-mmi/target/pixman_mmi_test.elf
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"

if [[ -z "${PS2DEV:-}" ]]; then
    for candidate in /usr/local/ps2dev "$HOME/ps2dev" /opt/ps2dev; do
        if [[ -x "$candidate/ee/bin/mips64r5900el-ps2-elf-gcc" ]]; then
            PS2DEV="$candidate"
            break
        fi
    done
fi
: "${PS2DEV:?Set PS2DEV to the installed PS2DEV prefix}"
PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
export PS2DEV PS2SDK
export PATH="$PS2DEV/ee/bin:$PATH"

for tool in python3 meson ninja cc; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing host tool: $tool" >&2
        echo 'Ubuntu: sudo apt install -y build-essential ninja-build python3 python3-pip' >&2
        echo 'Install Meson in a virtualenv if not already present.' >&2
        exit 2
    fi
done

if [[ ! -x "$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc" ]]; then
    echo "Missing PS2SDK EE cross compiler under $PS2DEV" >&2
    exit 2
fi
if [[ ! -f "$PS2SDK/ee/startup/linkfile" ]]; then
    echo "Missing $PS2SDK/ee/startup/linkfile" >&2
    exit 2
fi

echo "PS2DEV=$PS2DEV"
echo "PS2SDK=$PS2SDK"
echo "HOST PREFLIGHT: test/ps2/verify-host.sh"
bash test/ps2/verify-host.sh

echo "PS2 ELF BUILD: test/ps2/build.sh"
bash test/ps2/build.sh "$root/build-ps2-mmi/target"

target="$root/build-ps2-mmi/target/pixman_mmi_test.elf"
test -s "$target"
(cd "$(dirname "$target")" && sha256sum "$(basename "$target")" > SHA256SUMS.txt)
echo
echo "PS2_ELF_READY=$target"
ls -lh "$target"
