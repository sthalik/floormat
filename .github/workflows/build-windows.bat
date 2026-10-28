set path=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files\CMake\bin;C:\Program Files\Git\cmd;%GITHUB_WORKSPACE%/ninja-build
set vcvars=vcvars64.bat
if "%RUNNER_ARCH%"=="ARM64" set vcvars=vcvarsarm64.bat
"C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Auxiliary/Build/%vcvars%" -vcvars_ver=preview >nul && %*
exit /b %ERRORLEVEL%
