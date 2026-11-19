@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo MSVC Build Tools not found at "%VCVARS%"
    exit /b 1
)
call "%VCVARS%" >nul
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /EHsc /O2 /std:c++17 /W3 /Fo:build\ /Fe:chess.exe main.cpp /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
echo Built %~dp0chess.exe
