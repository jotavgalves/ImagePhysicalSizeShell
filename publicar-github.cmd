@echo off
setlocal
cd /d "%~dp0"

where gh >nul 2>&1
if not "%errorlevel%"=="0" (
  echo GitHub CLI nao encontrado. Instale em: https://cli.github.com/
  pause
  exit /b 1
)

gh auth status >nul 2>&1
if not "%errorlevel%"=="0" (
  echo Voce ainda nao esta logado no GitHub CLI.
  echo Rode:
  echo.
  echo   gh auth login
  echo.
  pause
  exit /b 1
)

git remote get-url origin >nul 2>&1
if "%errorlevel%"=="0" (
  echo Remote origin ja existe. Fazendo push...
  git push -u origin main
  pause
  exit /b %errorlevel%
)

echo Criando repositorio publico ImagePhysicalSizeShell e enviando main...
gh repo create ImagePhysicalSizeShell --public --source "%~dp0" --remote origin --push --description "Windows Explorer shell property handler for physical image sizes in centimeters"
pause
