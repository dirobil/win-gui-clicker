@echo off
setlocal

REM Проверка наличия clang-cl
where clang-cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: clang-cl not found. Please ensure LLVM is installed and in PATH.
    echo Or run this from 'Developer Command Prompt for VS'.
    exit /b 1
)

echo Compiling AutoClicker with clang-cl...

REM Флаги компиляции:
REM /O2 - Максимальная оптимизация скорости
REM /W4 - Высокий уровень предупреждений
REM /EHsc - Обработка исключений C++
REM /FeAutoClicker.exe - Имя выходного файла
REM /link user32.lib comctl32.lib - Подключаемые библиотеки

clang-cl /nologo /O2 /W4 /EHsc /FeAutoClicker.exe main.cpp /link user32.lib comctl32.lib

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Output: AutoClicker.exe
) else (
    echo Compilation failed.
    exit /b 1
)

endlocal
