# Distribuição

Esta pasta contém o pacote pronto para instalação em Windows 11 x64.

Arquivo principal:

- `ImagePhysicalSizeShell-setup-windows-x64.zip`

Comando de instalação via CMD, depois que o ZIP estiver acessível por URL:

```cmd
powershell -NoProfile -ExecutionPolicy Bypass -Command "$u='COLE_A_URL_DO_ZIP_AQUI'; $z=\"$env:TEMP\ImagePhysicalSizeShell.zip\"; $d=\"$env:TEMP\ImagePhysicalSizeShell\"; Invoke-WebRequest $u -OutFile $z; if(Test-Path $d){Remove-Item $d -Recurse -Force}; Expand-Archive $z $d -Force; Start-Process -FilePath (Join-Path $d 'ImagePhysicalSizeShell\instalar.cmd') -Verb RunAs"
```

