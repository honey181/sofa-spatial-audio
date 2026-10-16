# Build script for HrtfDspCpu.dll proxy and tools using LLVM-MinGW (UCRT)
$ErrorActionPreference = "Stop"

Write-Host "Searching for Clang/Clang++ compiler..." -ForegroundColor Cyan

# Locate clang++ installed via winget
$clangPath = (Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "clang++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)
$clangCPath = (Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "clang.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)
$arPath = (Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "llvm-ar.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName)

if (-not $clangPath -or -not (Test-Path $clangPath)) {
    $clangPath = (Get-Command "clang++" -ErrorAction SilentlyContinue).Source
    $clangCPath = (Get-Command "clang" -ErrorAction SilentlyContinue).Source
    $arPath = (Get-Command "llvm-ar" -ErrorAction SilentlyContinue).Source
}

if (-not $clangPath) {
    Write-Error "Clang++ not found. Please install MartinStorsjo.LLVM-MinGW.UCRT via winget."
    exit 1
}

Write-Host "Using compiler: $clangPath" -ForegroundColor Green

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Join-Path $projectRoot "src"
$includeDir = Join-Path $projectRoot "include"
$mysofaInc = Join-Path $projectRoot "libmysofa-src\src\hrtf"
$outDir = Join-Path $projectRoot "bin"
$mysofaLib = Join-Path $projectRoot "libmysofa.a"

if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

# 1. Check if libmysofa.a needs building
if (-not (Test-Path $mysofaLib)) {
    Write-Host "Building libmysofa.a (including pure zlib)..." -ForegroundColor Cyan
    $incHrtf = (Resolve-Path "libmysofa-src\src\hrtf").Path
    $incHdf = (Resolve-Path "libmysofa-src\src\hdf").Path
    $incSrc = (Resolve-Path "libmysofa-src\src").Path
    $incResample = (Resolve-Path "libmysofa-src\src\resampler").Path
    $incZlib = (Resolve-Path "zlib-src").Path

    if (-not (Test-Path "zlib-src\zconf.h")) {
        Copy-Item "zlib-src\zconf.h.in" "zlib-src\zconf.h" -Force
    }

    $objDir = Join-Path $projectRoot "build_all_objs"
    if (-not (Test-Path $objDir)) { New-Item -ItemType Directory -Path $objDir | Out-Null }

    $allFiles = Get-ChildItem "zlib-src\*.c", "libmysofa-src\src\hrtf\*.c", "libmysofa-src\src\hdf\*.c", "libmysofa-src\src\resampler\*.c"
    $allObjs = @()

    foreach ($f in $allFiles) {
        $objFile = Join-Path $objDir "$($f.BaseName).o"
        $allObjs += $objFile
        $args = @("-c", $f.FullName, "-o", $objFile, "-I$incHrtf", "-I$incHdf", "-I$incSrc", "-I$incResample", "-I$incZlib", "-O3")
        & $clangCPath $args
    }
    & $arPath rcs $mysofaLib $allObjs
    Write-Host "Built libmysofa.a" -ForegroundColor Green
}

# 2. Build HrtfDspCpu.dll
$outputDll = Join-Path $outDir "HrtfDspCpu.dll"
$sources = @(
    (Join-Path $srcDir "main_proxy.cpp"),
    (Join-Path $srcDir "hrtf_dsp_engine.cpp"),
    (Join-Path $srcDir "sofa_loader.cpp"),
    (Join-Path $srcDir "headphone_eq.cpp"),
    (Join-Path $srcDir "spatial_logger.cpp")
)

$compilerFlags = @(
    "-shared",
    "-static",
    "-O3",
    "-std=c++17",
    "-I$includeDir",
    "-I$srcDir",
    "-I$mysofaInc",
    $mysofaLib,
    "-lole32",
    "-luuid",
    "-Wl,--out-implib,$outDir\libHrtfDspCpu.a",
    "-o", $outputDll
)

Write-Host "Compiling HrtfDspCpu.dll with real SOFA engine & telemetry..." -ForegroundColor Cyan
& $clangPath $compilerFlags $sources

if ($LASTEXITCODE -eq 0) {
    Write-Host "Successfully built: $outputDll" -ForegroundColor Green
    Get-Item $outputDll | Select-Object Name, Length, LastWriteTime
} else {
    Write-Error "Compilation of HrtfDspCpu.dll failed."
    exit 1
}

# 3. Build test_spatial.exe
Write-Host "Compiling bin/test_spatial.exe..." -ForegroundColor Cyan
$testSources = @(
    (Join-Path $srcDir "test_spatial.cpp"),
    (Join-Path $srcDir "hrtf_dsp_engine.cpp"),
    (Join-Path $srcDir "sofa_loader.cpp"),
    (Join-Path $srcDir "headphone_eq.cpp"),
    (Join-Path $srcDir "spatial_logger.cpp")
)
$testExe = Join-Path $outDir "test_spatial.exe"
$testFlags = @(
    "-static",
    "-O3",
    "-std=c++17",
    "-I$includeDir",
    "-I$srcDir",
    "-I$mysofaInc",
    $mysofaLib,
    "-lole32",
    "-luuid",
    "-lwinmm",
    "-o", $testExe
)
& $clangPath $testFlags $testSources
if ($LASTEXITCODE -eq 0) {
    Write-Host "Successfully built: $testExe" -ForegroundColor Green
}

# 4. Build SpatialAudioTester.exe (Interactive 3D Sound Tracker GUI)
Write-Host "Compiling bin/SpatialAudioTester.exe (Interactive 3D GUI)..." -ForegroundColor Cyan
$guiSource = Join-Path $srcDir "spatial_audio_tester_gui.cpp"
$guiExe = Join-Path $outDir "SpatialAudioTester.exe"
$guiFlags = @(
    "-O3",
    "-std=c++17",
    "-I$mysofaInc",
    $mysofaLib,
    "-lgdi32",
    "-lole32",
    "-luuid",
    "-mwindows",
    "-o", $guiExe
)
& $clangPath $guiFlags $guiSource
if ($LASTEXITCODE -eq 0) {
    Write-Host "Successfully built: $guiExe" -ForegroundColor Green
}

# 5. Synchronize ProgramData configuration
$progDataDir = "C:\ProgramData\SofaSpatialAudio"
if (-not (Test-Path $progDataDir)) {
    New-Item -ItemType Directory -Path $progDataDir | Out-Null
}
Copy-Item "$projectRoot\config\*" $progDataDir -Recurse -Force
Write-Host "Synchronized configuration to $progDataDir" -ForegroundColor Green

