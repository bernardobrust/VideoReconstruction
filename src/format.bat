@echo off
setlocal enabledelayedexpansion

:: Check if clang-format is installed
where clang-format >nul 2>nul
if errorlevel 1 (
    echo Error: clang-format is not installed or not in PATH.
    exit /b 1
)

for /r %%F in (*.c *.h) do (
    echo %%F | findstr /i /r "\\lib\\ \\build\\ \\dbg\\ \\profiling\\" >nul
    if errorlevel 1 (
        clang-format -i "%%F"
    )
)

echo Formatting complete.