@echo off
setlocal enabledelayedexpansion

:: May change deppending on your config
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist "nob.exe" (
    echo Building nob.exe...
    cl /nologo nob.c

    if errorlevel 1 (
        echo Failed to build nob.exe.
        exit /b 1
    )
)

nob.exe -target inspector -platform windows -mode debug

endlocal