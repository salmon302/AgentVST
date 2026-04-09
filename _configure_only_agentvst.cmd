@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -DAGENTVST_VST3_DEPLOY_DIR=""
if errorlevel 1 exit /b %errorlevel%

echo.
echo AgentVST VST3 deploy copy is disabled for this build directory.
