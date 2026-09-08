@echo off
chcp 65001 >nul
echo Compiling AutoClicker with clang-cl...

:: Check if clang-cl is available
where clang-cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: clang-cl not found. Please install LLVM or use Developer Command Prompt for VS with Clang support.
    exit /b 1
)

:: Compile with optimizations for small size
clang-cl /EHsc /O1 /GL main.cpp /Fe:AutoClicker.exe /link /LTCG user32.lib comctl3.lib shell32.lib

if %ERRORLEVEL% equ 0 (
    echo Compilation successful! Output: AutoClicker.exe
) else (
    echo Compilation failed.
    exit /b 1
)
