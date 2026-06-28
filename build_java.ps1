# Define o diretório do script como raiz do projeto
Set-Location -Path $PSScriptRoot

Write-Host "=== Limpando build anterior ===" -ForegroundColor Cyan
.\gradlew clean

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERRO: 'gradlew clean' falhou. Código: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n=== Compilando release ===" -ForegroundColor Cyan
.\gradlew :app:assembleRelease

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERRO: 'gradlew :app:assembleRelease' falhou. Código: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`nBuild concluído com sucesso!" -ForegroundColor Green
Write-Host "APK gerado em: .\app\build\outputs\apk\release\" -ForegroundColor Yellow