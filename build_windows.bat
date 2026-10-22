@echo off
REM ============================================================================
REM EATHON NATIVE COMPILER & DATABASE ENGINE - WINDOWS BUILD SCRIPT
REM Supports MinGW-w64 (gcc) or LLVM (clang) on Windows 10/11 (x86_64)
REM ============================================================================

if not exist bin mkdir bin

where gcc >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [Eathon] Building eathon.exe and eatdb.exe using GCC...
    gcc -O2 -Wall -Wextra -std=c99 -Icompiler/include compiler/src/eathon.c -o bin/eathon.exe
    gcc -O2 -Wall -Wextra -std=c99 -Icompiler/include compiler/src/eatdb.c -o bin/eatdb.exe
    goto :done
)

where clang >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [Eathon] Building eathon.exe and eatdb.exe using LLVM Clang...
    clang -O2 -Wall -Wextra -std=c99 -Icompiler/include compiler/src/eathon.c -o bin/eathon.exe
    clang -O2 -Wall -Wextra -std=c99 -Icompiler/include compiler/src/eatdb.c -o bin/eatdb.exe
    goto :done
)

echo [Error] Neither 'gcc' nor 'clang' was found in your Windows PATH.
echo Please install MinGW-w64 (w64devkit / MSYS2) or LLVM for Windows.
exit /b 1

:done
echo [Eathon] Build complete: bin\eathon.exe and bin\eatdb.exe
echo [Eathon] Compiling and running example 01...
bin\eathon.exe examples\01_variables_and_arithmetic.eat -o program.exe --cc gcc
program.exe
