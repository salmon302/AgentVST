@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%

set CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe
set HELPER=build\examples\simple_gain\Debug\juce_vst3_helper.exe

"%CMAKE_EXE%" --build build --config Debug --target SimpleGain_VST3 AgentCompressor_VST3 AgentEQ_VST3
if errorlevel 1 exit /b %errorlevel%

if not exist "%HELPER%" (
  echo [FAIL] Missing helper executable: %HELPER%
  exit /b 1
)

set FAIL=0

call :validate "SimpleGain" "build\examples\simple_gain\SimpleGain_artefacts\Debug\VST3\SimpleGain.vst3\Contents\x86_64-win\SimpleGain.vst3"
call :validate "AgentCompressor" "build\examples\compressor\AgentCompressor_artefacts\Debug\VST3\AgentCompressor.vst3\Contents\x86_64-win\AgentCompressor.vst3"
call :validate "AgentEQ" "build\examples\three_band_eq\AgentEQ_artefacts\Debug\VST3\AgentEQ.vst3\Contents\x86_64-win\AgentEQ.vst3"

if "%FAIL%"=="1" (
  echo [FAIL] One or more VST3 validation checks failed.
  exit /b 1
)

echo [OK] VST3 example builds and validations succeeded.
exit /b 0

:validate
set NAME=%~1
set MODULE=%~2

if not exist "%MODULE%" (
  echo [FAIL] %NAME% module not found: %MODULE%
  set FAIL=1
  goto :eof
)

"%HELPER%" -validate -path "%MODULE%"
if errorlevel 1 (
  echo [FAIL] %NAME% module validation failed.
  set FAIL=1
) else (
  echo [OK] %NAME% module validation passed.
)
goto :eof
