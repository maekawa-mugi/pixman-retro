# Import only the audited candidate sources into the ZIP's v5 ALL-PR tree.
# Run from the pixman-retro repository after fetching the review branch.
# Does not touch .git, build artifacts, or the original Pixman kernel files.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$src = Join-Path $repo 'build-ps2-mmi\all-pr\source\test\ps2'
if (-not (Test-Path -LiteralPath (Join-Path $src 'progress.c'))) {
    throw "v5 integrated test/ps2/progress.c not found at $src"
}
$names = @(
    'latency-variants-asm.S',
    'latency-variants-test.c',
    'latency-variants-host.c',
    'upgrade-v5-harness.py',
    'throughput-core-asm.S',
    'throughput-blend-asm.S',
    'throughput-variants-test.c',
    'throughput-variants-host.c',
    'upgrade-v5-throughput.py'
)
foreach ($name in $names) {
    $from = Join-Path $PSScriptRoot $name
    if (-not (Test-Path -LiteralPath $from)) {
        throw "Missing $from. Fetch the review branch and git restore the files first."
    }
    Copy-Item -LiteralPath $from -Destination (Join-Path $src $name) -Force
    Write-Output "STAGED,$name"
}
$py = Get-Command python3 -ErrorAction SilentlyContinue
if ($null -eq $py) {
    $py = Get-Command python -ErrorAction Stop
}
& $py.Source (Join-Path $src 'upgrade-v5-harness.py')
if ($LASTEXITCODE -ne 0) {
    throw "v5 latency upgrade failed with exit code $LASTEXITCODE"
}
& $py.Source (Join-Path $src 'upgrade-v5-throughput.py')
if ($LASTEXITCODE -ne 0) {
    throw "v5 throughput upgrade failed with exit code $LASTEXITCODE"
}
Write-Output 'V5_LATENCY_AND_THROUGHPUT_READY_FOR_BUILD'
