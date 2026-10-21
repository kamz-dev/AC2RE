@echo off
rem Builds ac1lab.exe (launcher) and ac1lab.dll (the mod), 32-bit like the game, into this folder.
setlocal
for %%e in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars32.bat" set "VS=%ProgramFiles%\Microsoft Visual Studio\2022\%%e"
)
if not defined VS (echo Visual Studio 2022 with C++ tools not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /O2 /W3 /MT /Foobj\ /LD src\main.c src\game.c src\expr.c src\probes.c src\trace.c src\checks.c src\control.c src\input.c src\overlay.c src\bridge.c src\abilities.c /Fe:ac1lab.dll /link user32.lib gdi32.lib ws2_32.lib || exit /b 1
cl /nologo /O2 /W3 /MT /Foobj\ src\launcher.c /Fe:ac1lab.exe || exit /b 1
del /q ac1lab.exp ac1lab.lib 2>nul
echo Built ac1lab.exe + ac1lab.dll
