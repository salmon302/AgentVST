@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files\CMake\bin\cmake.exe" --build build --config Debug --target agentvst_tests
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files\CMake\bin\ctest.exe" --test-dir build -C Debug --output-on-failure
if errorlevel 1 exit /b %errorlevel%
