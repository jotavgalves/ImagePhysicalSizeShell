@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if not "%errorlevel%"=="0" (
  echo Solicitando permissão de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo Executando rollback de emergência...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\rollback.ps1"
echo.
echo Rollback finalizado. Se o Explorer estiver aberto, reinicie o Explorer ou o Windows.
pause
