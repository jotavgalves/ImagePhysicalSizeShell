@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo Solicitando permissao de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo Desinstalando ImagePhysicalSizeShell...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\uninstall.ps1" -Uninstall -Force -NoRestartExplorer
echo.
echo Desinstalacao concluida. Reinicie o Explorer ou o Windows para descarregar DLLs presas, se necessario.
pause
