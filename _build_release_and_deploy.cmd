@echo off
REM Build and deploy AgentVST plugins to Ableton VST3 folder (Release mode)
REM
REM This is the recommended way to build plugins for DAW testing.
REM Always use Release builds (smaller, faster, optimized).
REM
REM Requires:
REM   - Visual Studio Build Tools or Visual Studio
REM   - CMake
REM   - JUCE (fetched automatically)

setlocal
cls

echo.
echo ==================================================================
echo  AgentVST: Build and Deploy (Release Mode)
echo ==================================================================
echo.

REM Call vcvars to set up build environment
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo [ERROR] Failed to initialize Visual Studio environment.
    exit /b 1
)

REM Configure CMake for deployment
echo [1] Configuring CMake...
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -DAGENTVST_VST3_DEPLOY_DIR="C:\Users\salmo\Documents\VST3\AgentVST-Dev"
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

REM Build Release configuration
echo.
echo [2] Building Release plugins...
"C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --parallel --target SimpleGain AgentCompressor AgentEQ Escher_Pitch_Shifter
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM Verify deployment
echo.
echo [3] Verifying deployment...
set DEPLOY_DIR=C:\Users\salmo\Documents\VST3\AgentVST-Dev

if not exist "%DEPLOY_DIR%" (
    echo [WARNING] Deployment directory does not exist: %DEPLOY_DIR%
    exit /b 1
)

REM Check deployed binaries
echo.
echo Deployment Status:
echo.
for %%F in ("%DEPLOY_DIR%\*.vst3\Contents\x86_64-win\*.vst3") do (
    for /F "tokens=*" %%A in ('dir /-C "%%F" ^| find "%%~nF"') do (
        echo   %%~nF: %%A
    )
)

echo.
echo ==================================================================
echo  SUCCESS: Plugins built and deployed to:
echo  %DEPLOY_DIR%
echo.
echo  Next steps:
echo    1. Close any open DAWs (Ableton, etc.)
echo    2. Reopen your DAW
echo    3. Rescan VST3 plugins in settings
echo    4. Load a plugin and test (e.g., SimpleGain - set gain to +6dB)
echo.
echo  Troubleshooting: Run 'powershell -ExecutionPolicy Bypass -File scripts/verify_deployment.ps1'
echo ==================================================================
echo.

exit /b 0
