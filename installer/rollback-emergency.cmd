@echo off
chcp 65001 >nul
setlocal

net session >nul 2>&1
if not "%errorlevel%"=="0" (
  echo Solicitando permissão de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo ImagePhysicalSizeShell - rollback de emergência
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0rollback.ps1"
echo.
echo Finalizado. Reinicie o Explorer ou o Windows se algum arquivo continuar carregado.
pause
