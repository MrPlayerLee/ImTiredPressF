@echo off
rem Example: build.bat --version 1.2.0 publishes to build\bin\Version_1.2.0
setlocal EnableExtensions DisableDelayedExpansion
set "ROOT=%~dp0"
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules;%PSModulePath%"
if not defined OPENCV_ROOT set "OPENCV_ROOT=E:\OpenCV\opencv\build"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.v143.x86.x64 -property installationPath`) do set "VS=%%I"
if not defined VS for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%I"
if not defined VS exit /b 1
call "%VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
python "%ROOT%tools\build.py" %*
exit /b %ERRORLEVEL%
