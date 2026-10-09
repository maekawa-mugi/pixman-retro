#!/usr/bin/env bash
# Run from Linux/WSL with PS2DEV and PS2SDK set.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/../.." && pwd)
output_dir=${1:-"$source_dir/build-ps2-mmi"}
: "${PS2DEV:?Set PS2DEV to the PS2 toolchain root}"
: "${PS2SDK:?Set PS2SDK to the PS2 SDK root}"
cc=${CC:-"$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc"}
ar=${AR:-"$PS2DEV/ee/bin/mips64r5900el-ps2-elf-ar"}
crt_dir=${PS2EE_CRT_DIR:-"$PS2DEV/ee/mips64r5900el-ps2-elf/lib"}
if [[ ! -f "$crt_dir/crt0.o" ]]; then
    crt_dir="$PS2SDK/../ee/mips64r5900el-ps2-elf/lib"
fi
[[ -f "$crt_dir/crt0.o" && -f "$PS2SDK/ee/startup/linkfile" ]] || {
    echo 'Missing EE crt0.o or PS2SDK linkfile' >&2; exit 1;
}
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
flags=(-O2 -march=r5900 -G0 -D_EE -DPIXMAN_NO_TLS
       "-I$PS2SDK/ee/include" "-I$PS2SDK/common/include" "-L$PS2SDK/ee/lib")
link_flags=(-march=r5900 -G0 "-B$crt_dir/"
            "-T$PS2SDK/ee/startup/linkfile" "-L$PS2SDK/ee/lib"
            -Wl,-zmax-page-size=128,--gc-sections
            -Wl,--start-group -ldebug -lm -lc -lcdvd -lcglue
            -lpthread -lpthreadglue -lkernel -Wl,--end-group)
# Use Meson's normal source list, config probes and generated version header.
python3 - "$output_dir/ps2-cross.ini.new" "$cc" "$ar" "${#flags[@]}" \
    "${flags[@]}" "${link_flags[@]}" <<'PY'
import sys
path, cc, ar, count, *args = sys.argv[1:]
count = int(count)
with open(path, 'w') as f:
    f.write(f"[binaries]\nc = {cc!r}\nar = {ar!r}\nstrip = {cc.replace('gcc', 'strip')!r}\n")
    f.write("[host_machine]\nsystem = 'ps2'\ncpu_family = 'mips64'\ncpu = 'r5900'\nendian = 'little'\n")
    f.write("[properties]\nneeds_exe_wrapper = true\n")
    f.write(f"[built-in options]\nc_args = {args[:count]!r}\nc_link_args = {args[count:]!r}\n")
PY
cross_changed=0
if [[ -f "$output_dir/ps2-cross.ini" ]] &&
   ! cmp -s "$output_dir/ps2-cross.ini.new" "$output_dir/ps2-cross.ini"; then
    cross_changed=1
fi
mv "$output_dir/ps2-cross.ini.new" "$output_dir/ps2-cross.ini"
setup=(meson setup "$output_dir/library" "$source_dir"
       --cross-file "$output_dir/ps2-cross.ini" --default-library=static -Db_staticpic=false
       -Dee-mmi=enabled -Dloongson-mmi=disabled -Dmips-dspr2=disabled
       -Dtests=disabled -Ddemos=disabled -Dtls=disabled -Dopenmp=disabled
       -Dgtk=disabled -Dlibpng=disabled)
if [[ -f "$output_dir/library/meson-private/coredata.dat" ]]; then
    if ((cross_changed)); then
        # Meson caches cross-file built-in options; reread on SDK/flag changes.
        setup+=(--wipe)
    else
        setup+=(--reconfigure)
    fi
fi
"${setup[@]}"
meson compile -C "$output_dir/library"
wrap_flags=()
for name in add copy fill in_a8 over 8888_to_0565 ca_over ca_over_solid over_solid_a8 over_image_a8 over_image_ca over_8888_0565 nearest_opaque bilinear_lerp bilinear_hsum bilinear_vfinish; do
    wrap_flags+=("-Wl,--wrap=ee_mmi_${name}_quads")
done
"$cc" "${flags[@]}" -std=gnu99 -DHAVE_CONFIG_H \
    "-I$output_dir/library/pixman" "-I$source_dir/pixman" \
    -ffunction-sections -fdata-sections -c "$source_dir/test/ps2/main.c" \
    -o "$output_dir/main.o"
"$cc" "${flags[@]}" -std=gnu99 -DHAVE_CONFIG_H \
    "-I$output_dir/library/pixman" "-I$source_dir/pixman" \
    -c "$source_dir/test/ps2/benchmark.c" -o "$output_dir/benchmark.o"
"$cc" "${flags[@]}" -std=gnu99 -c "$source_dir/test/ps2/progress.c" -o "$output_dir/progress.o"
"$cc" "${flags[@]}" -std=gnu99 -c "$source_dir/test/ps2/latency-variants-test.c" -o "$output_dir/latency-variants-test.o"
"$cc" "${flags[@]}" -c "$source_dir/test/ps2/latency-variants-asm.S" -o "$output_dir/latency-variants-asm.o"
"$cc" "${flags[@]}" -std=gnu99 -c "$source_dir/test/ps2/throughput-variants-test.c" -o "$output_dir/throughput-variants-test.o"
"$cc" "${flags[@]}" -c "$source_dir/test/ps2/throughput-core-asm.S" -o "$output_dir/throughput-core-asm.o"
"$cc" "${flags[@]}" -c "$source_dir/test/ps2/throughput-blend-asm.S" -o "$output_dir/throughput-blend-asm.o"
suite_objects=("$output_dir/benchmark.o" "$output_dir/progress.o" "$output_dir/latency-variants-test.o" "$output_dir/latency-variants-asm.o" "$output_dir/throughput-variants-test.o" "$output_dir/throughput-core-asm.o" "$output_dir/throughput-blend-asm.o")
model_flags=()
for name in add copy fill in_a8 over 8888_to_0565 ca_over ca_over_solid over_solid_a8 over_image_a8 over_image_ca over_8888_0565 nearest_opaque bilinear_lerp bilinear_hsum bilinear_vfinish; do
    model_flags+=("-Dee_mmi_${name}_quads=b_ee_mmi_${name}_quads")
done
"$cc" "${flags[@]}" "${model_flags[@]}" -c "$source_dir/test/ee-mmi-host-reference.c" -o "$output_dir/b-models.o"
suite_objects+=("$output_dir/b-models.o")
for spec in 'ee-mmi-suite:ee_kernel_suite_main' 'ee-mmi-ca-fused-test:ee_test_ca' 'ee-mmi-a8-fused-test:ee_test_solid_a8' 'ee-mmi-565-fused-test:ee_test_565' 'ee-mmi-image-a8-fused-test:ee_test_image_a8' 'ee-mmi-image-ca-fused-test:ee_test_image_ca' 'ee-mmi-scale-test:ee_test_scale' 'ee-mmi-bilinear-4tap-test:ee_test_bilinear4'; do
    file=${spec%%:*}
    entry=${spec#*:}
    "$cc" "${flags[@]}" -std=gnu99 -DPS2_TEST_PROGRESS -Dmain="$entry" -c "$source_dir/test/$file.c" -o "$output_dir/$file.o"
    suite_objects+=("$output_dir/$file.o")
done
"$cc" "${flags[@]}" -std=gnu99 -c "$source_dir/test/ee-mmi-bench.c" -o "$output_dir/ee-mmi-bench.o"
suite_objects+=("$output_dir/ee-mmi-bench.o")
"$cc" -o "$output_dir/pixman_mmi_test.elf" "$output_dir/main.o" "${suite_objects[@]}" \
    -Wl,--start-group "$output_dir/library/pixman/libpixman-1.a" \
    "$output_dir/library/pixman/libpixman-ee-mmi.a" -Wl,--end-group \
    "-Wl,-Map,$output_dir/pixman_mmi_test.map" "${wrap_flags[@]}" "${link_flags[@]}"
printf '\nELF: %s/pixman_mmi_test.elf\n' "$output_dir"
