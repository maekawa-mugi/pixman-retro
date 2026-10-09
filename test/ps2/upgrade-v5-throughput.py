#!/usr/bin/env python3
"""Apply the throughput experiment after upgrade-v5-harness.py."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]

def swap(name, before, after):
    path = root / name
    content = path.read_text(encoding="utf-8")
    if after in content:
        print("ALREADY", name)
        return
    if content.count(before) != 1:
        raise RuntimeError("%s: expected one anchor, found %d" % (name, content.count(before)))
    path.write_text(content.replace(before, after), encoding="utf-8")
    print("UPDATED", name)

swap("test/ps2/main.c",
     "extern int ps2_latency_run(void);",
     "extern int ps2_latency_run(void);\nextern int ps2_throughput_run(void);")

swap("test/ps2/main.c",
     "    failures += ps2_latency_run();",
     "    failures += ps2_latency_run();\n    failures += ps2_throughput_run();")

# Accept both the compact original build.sh suite_objects line and the
# folded version from a previously patched checkout.
import re

def patch_target_build():
    path = root / "test/ps2/build.sh"
    content = path.read_text(encoding="utf-8")
    if '"$output_dir/throughput-variants-test.o"' in content:
        print("ALREADY", path.name, "throughput")
        return
    pattern = r'(?m)^suite_objects=\([^)]*latency-variants-asm\.o"\)'
    hits = list(re.finditer(pattern, content))
    if len(hits) != 1:
        raise RuntimeError("build.sh: expected one latency suite_objects block")
    start, end = hits[0].span()
    existing = hits[0].group(0)
    extras = (' "$output_dir/throughput-variants-test.o"'
              ' "$output_dir/throughput-core-asm.o"'
              ' "$output_dir/throughput-blend-asm.o")')
    flags = "$" + "{flags[@]}"
    prefix = (
        '"$cc" "' + flags + '" -std=gnu99 -c "$source_dir/test/ps2/throughput-variants-test.c" -o "$output_dir/throughput-variants-test.o"\n'
        '"$cc" "' + flags + '" -c "$source_dir/test/ps2/throughput-core-asm.S" -o "$output_dir/throughput-core-asm.o"\n'
        '"$cc" "' + flags + '" -c "$source_dir/test/ps2/throughput-blend-asm.S" -o "$output_dir/throughput-blend-asm.o"\n'
    )
    updated = content[:start] + prefix + existing[:-1] + extras + content[end:]
    path.write_text(updated, encoding="utf-8")
    print("UPDATED", path.name, "throughput")

patch_target_build()

swap("test/ps2/verify-host.sh",
     '"$root/test/ps2/latency-variants-test.c" "$root/test/ps2/latency-variants-host.c" \\',
     '"$root/test/ps2/latency-variants-test.c" "$root/test/ps2/latency-variants-host.c" \\\n    "$root/test/ps2/throughput-variants-test.c" "$root/test/ps2/throughput-variants-host.c" \\')

swap("test/ps2/verify-host.sh",
     "echo 'HOST_NEGATIVE_CONTROLS_PASS,latency,group-status'",
     '''if PS2_THROUGHPUT_TEST_FAULT=1 "$out/runner" > "$out/throughput-fault.log" 2>&1; then
    echo 'ERROR: throughput mismatch escaped the harness' >&2; exit 1
fi
grep -q 'THROUGHPUT_FAIL,ADD,u8,blocks=7' "$out/throughput-fault.log" || {
    echo 'ERROR: throughput injected mismatch not identified' >&2; exit 1;
}
echo 'HOST_NEGATIVE_CONTROLS_PASS,latency,group-status,throughput' '''.rstrip())

print("V5_THROUGHPUT_PATCH_PASS")
