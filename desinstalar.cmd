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

echo Desinstalando ImagePhysicalSizeShell...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\uninstall.ps1" -Uninstall -Force -NoRestartExplorer
echo.
echo Desinstalação concluída. Reinicie o Explorer ou o Windows para descarregar DLLs presas, se necessário.
pause
