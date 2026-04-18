@echo off
setlocal

set "ABLETON_VST3_DEV_DIR=I:\Documents\Ableton\VST3\dev"

if not exist "%ABLETON_VST3_DEV_DIR%" (
    mkdir "%ABLETON_VST3_DEV_DIR%"
)

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%

"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -DAGENTVST_VST3_DEPLOY_DIR="%ABLETON_VST3_DEV_DIR%"
if errorlevel 1 exit /b %errorlevel%

echo.
echo ==================================================================
echo AgentVST VST3 Deployment Configured
echo ==================================================================
echo.
echo Deploy Directory: %ABLETON_VST3_DEV_DIR%
echo.
echo NEXT STEP: Build plugins in Release mode
echo.
echo   RECOMMENDED - Use the convenience script:
echo     _build_release_and_deploy.cmd
echo.
echo   OR - Build manually with Release configuration:
echo     cmake --build build --config Release --parallel
echo.
echo IMPORTANT: Always use Release builds for DAW testing.
echo Debug builds are large and unoptimized.
echo.
echo See DEPLOYMENT.md for complete guide.
echo ==================================================================
echo.
