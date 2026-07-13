@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo Solicitando permissao de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo Executando rollback de emergencia...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\rollback.ps1"
echo.
echo Rollback finalizado. Se o Explorer estiver aberto, reinicie o Explorer ou o Windows.
pause
