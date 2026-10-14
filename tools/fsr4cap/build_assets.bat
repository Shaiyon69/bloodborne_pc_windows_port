@echo off
rem tools\fsr4cap\build_assets.bat <amd_fidelityfx_upscaler_dx12.dll 4.1.x> [amd_fidelityfx_loader_dx12.dll 2.3.x]
rem
rem Builds the FSR 4.1.1 asset set (fsr4_411\: SPIR-V of every pass, model weights) from your own
rem AMD upscaler DLL, for upscaler=fsr411 (gpu\...\fsr411). Nothing of AMD's is shipped in this
rem repository: the DLL is yours (OptiScaler's FSR4_LATEST folder has one, as do many games).
rem   1. builds dxil-spirv (pinned commit + dxil-spirv-class-bindings.patch) and fsr4cap.exe;
rem   2. sparse-clones AMD's FidelityFX SDK 2.3.0 for the API headers, and for the loader DLL
rem      when you did not pass one (Kits\FidelityFX\signedbin\);
rem   3. runs capture_all.py: every output size class and quality ratio, recording the D3D12
rem      frames natively (the Linux original needed umu-run and Proton for this);
rem   4. extract.py translates them, checks the replay rules and writes fsr4_411\.
rem
rem Needs: git, cmake, ninja, a MinGW gcc (w64devkit or msys64), Python 3.10+, spirv-dis and
rem spirv-as from the Vulkan SDK, and network access for the two pinned git repositories.
rem
rem AMD's own 4.1.1 file limits the INT8 model to some graphics cards; a DLL whose check has been
rem lifted is needed to capture on the others. The assets this writes require
rem VK_VALVE_shader_mixed_float_dot_product at run time (extract.py translates the passes exactly
rem as vkd3d-proton does), which ships in RADV and Proton but not in the Windows drivers: check
rem yours with out\bb-gpu-capabilities.exe --upscalers.
setlocal
cd /d "%~dp0..\.."

set "UPSCALER_DLL=%~f1"
if "%UPSCALER_DLL%"=="" (
    echo [ERROR] Usage: tools\fsr4cap\build_assets.bat ^<amd_fidelityfx_upscaler_dx12.dll^> [amd_fidelityfx_loader_dx12.dll]
    exit /b 1
)
if not exist "%UPSCALER_DLL%" (
    echo [ERROR] No such file: %UPSCALER_DLL%
    exit /b 1
)
set "LOADER_DLL=%~f2"

if defined W64DEVKIT_DIR set "PATH=%W64DEVKIT_DIR%\bin;%PATH%"
if exist "C:\DevTools\w64devkit\bin" set "PATH=C:\DevTools\w64devkit\bin;%PATH%"
if defined MINGW_DIR set "PATH=%MINGW_DIR%\bin;%PATH%"
if exist "C:\msys64\mingw64\bin" set "PATH=C:\msys64\mingw64\bin;%PATH%"
if exist "C:\Program Files\CMake\bin" set "PATH=C:\Program Files\CMake\bin;%PATH%"
if defined VULKAN_SDK set "PATH=%VULKAN_SDK%\Bin;%PATH%"

if defined PYTHON goto found_python
for %%P in (
    "C:\Program Files\Python314\python.exe"
    "C:\Program Files\Python313\python.exe"
    "C:\Program Files\Python312\python.exe"
    "C:\Program Files\Python311\python.exe"
    "C:\Program Files\Python310\python.exe"
    "%LocalAppData%\Programs\Python\Python314\python.exe"
    "%LocalAppData%\Programs\Python\Python313\python.exe"
    "%LocalAppData%\Programs\Python\Python312\python.exe"
    "%LocalAppData%\Programs\Python\Python311\python.exe"
    "%LocalAppData%\Programs\Python\Python310\python.exe"
) do if not defined PYTHON if exist %%P set "PYTHON=%%~P"
if not defined PYTHON for /f "delims=" %%I in ('where python.exe 2^>nul') do if not defined PYTHON set "PYTHON=%%~I"
if not defined PYTHON (
    echo [ERROR] Python 3.10+ not found.
    exit /b 1
)
:found_python

set "WORK=%CD%\out\fsr4cap"
if not exist "%WORK%" mkdir "%WORK%"

rem 1. dxil-spirv: DXIL to SPIR-V exactly as vkd3d-proton translates it.
set "DX=%WORK%\dxil-spirv"
if exist "%DX%\build\dxil-spirv.exe" goto have_dxil
if exist "%DX%" rmdir /s /q "%DX%"
echo [1/5] Building dxil-spirv...
git clone -q https://github.com/HansKristian-Work/dxil-spirv.git "%DX%" || exit /b 1
git -C "%DX%" checkout -q 7dc52786cdb1f53c54dd5c5c2698dff00ea5f0a3 || exit /b 1
git -C "%DX%" submodule update -q --init --recursive || exit /b 1
git -C "%DX%" apply "%CD%\tools\fsr4cap\dxil-spirv-class-bindings.patch" || exit /b 1
cmake -S "%DX%" -B "%DX%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release > "%WORK%\cmake.log" 2>&1 || exit /b 1
ninja -C "%DX%\build" dxil-spirv > "%WORK%\ninja.log" 2>&1 || exit /b 1
:have_dxil

rem 2. AMD's FidelityFX API headers (MIT), and its loader DLL when none was given.
set "SDK=%WORK%\ffxsdk"
if exist "%SDK%\Kits\FidelityFX\api\include\ffx_api.h" goto have_sdk
if exist "%SDK%" rmdir /s /q "%SDK%"
echo [2/5] Fetching the FidelityFX SDK 2.3.0 headers...
git clone -q --filter=blob:none --sparse https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git "%SDK%" || exit /b 1
git -C "%SDK%" checkout -q 60f4ea81909200d8542eca14dccb2628b763a9a3 || exit /b 1
git -C "%SDK%" sparse-checkout set --no-cone /Kits/FidelityFX/api/include/ /Kits/FidelityFX/upscalers/include/ /Kits/FidelityFX/signedbin/ || exit /b 1
:have_sdk
if not defined LOADER_DLL set "LOADER_DLL=%SDK%\Kits\FidelityFX\signedbin\amd_fidelityfx_loader_dx12.dll"
if not exist "%LOADER_DLL%" (
    echo [ERROR] No loader DLL: %LOADER_DLL%
    exit /b 1
)

rem 3. fsr4cap.exe, next to both DLLs: LoadLibraryA finds them in the exe's own folder.
echo [3/5] Building fsr4cap.exe...
set "K=%SDK%\Kits\FidelityFX"
gcc -std=c11 -O1 -Wall -I"%K%\api\include" -I"%K%\upscalers\include" tools\fsr4cap\fsr4cap.c tools\fsr4cap\capture.c tools\fsr4cap\rootsig.c -o "%WORK%\fsr4cap.exe" -ld3d12 -ldxguid -static || exit /b 1
copy /y "%UPSCALER_DLL%" "%WORK%\amd_fidelityfx_upscaler_dx12.dll" >nul || exit /b 1
copy /y "%LOADER_DLL%" "%WORK%\amd_fidelityfx_loader_dx12.dll" >nul || exit /b 1

echo [4/5] Recording the captures...
"%PYTHON%" tools\fsr4cap\capture_all.py "%WORK%" || exit /b 1

echo [5/5] Translating them into fsr4_411...
"%PYTHON%" tools\fsr4cap\extract.py "%DX%\build\dxil-spirv.exe" "%WORK%" fsr4_411 || exit /b 1

echo Done: fsr4_411\ is ready. Select FSR 4.1.1 in the launcher's Graphics tab.
endlocal
