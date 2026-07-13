@echo off
setlocal

net session >nul 2>&1
if errorlevel 1 (
  echo Solicitando permissao de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo ImagePhysicalSizeShell - rollback de emergencia
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0rollback.ps1"
echo.
echo Finalizado. Reinicie o Explorer ou o Windows se algum arquivo continuar carregado.
pause
