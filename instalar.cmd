@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo Solicitando permissao de administrador...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo Instalando ImagePhysicalSizeShell...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -Install -Authorization APLICAR -NonInteractive
if errorlevel 1 (
  echo.
  echo Falha na instalacao. Veja os backups em C:\ProgramData\ImagePhysicalSizeShell\backup
  pause
  exit /b 1
)

echo.
echo Criando atalhos...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install-shortcuts.ps1" -AllUsersStartMenu

echo.
echo Instalacao concluida.
echo App: C:\Program Files\ImagePhysicalSizeShell\ipsconfigui.exe
echo Rollback: execute rollback-emergencia.cmd como administrador se precisar desfazer.
echo.

start "" "C:\Program Files\ImagePhysicalSizeShell\ipsconfigui.exe"
exit /b 0
