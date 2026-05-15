param(
    [int]$KeepFinalCount = 1,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Write-Action([string]$message) {
    Write-Host $message -ForegroundColor Cyan
}

function Remove-PathSafe([string]$path) {
    if (-not (Test-Path $path)) {
        return
    }

    if ($DryRun) {
        Write-Action "[DryRun] Remover: $path"
        return
    }

    Remove-Item -Recurse -Force $path
    Write-Action "Removido: $path"
}

function Remove-FileSafe([string]$path) {
    if (-not (Test-Path $path)) {
        return
    }

    if ($DryRun) {
        Write-Action "[DryRun] Remover arquivo: $path"
        return
    }

    Remove-Item -Force $path
    Write-Action "Removido arquivo: $path"
}

function Get-ProjectRoot {
    return (Get-Location).Path
}

function Cleanup-GeneratedDirectories([string]$root) {
    $dirs = @(
        '.gradle',
        '.idea',
        'build',
        'build_work',
        'dist',
        'working_decompiled',
        'mod_decompiled',
        'temp_debug',
        'temp_decompiled',
        'temp_smali',
        'temp_split_assets',
        'temp_split_lib',
        'app\build',
        'app\.cxx',
        'extracted\medal_bundle',
        'app\src\test',
        'app\src\androidTest'
    )

    foreach ($dir in $dirs) {
        Remove-PathSafe (Join-Path $root $dir)
    }
}

function Cleanup-UnusedFiles([string]$root) {
    $files = @(
        'temp.keystore',
        'mod.keystore',
        'Relatorio_Precos_ApexCombatMod.csv'
    )

    foreach ($file in $files) {
        Remove-FileSafe (Join-Path $root $file)
    }
}

function Cleanup-ApkFinal([string]$root, [int]$keepCount) {
    $apkFinalDir = Join-Path $root 'apk_final'
    if (-not (Test-Path $apkFinalDir)) {
        return
    }

    $finalApks = Get-ChildItem -Path $apkFinalDir -File -Filter 'ApexCombat-Mod-V*-Final.apk' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending

    $keep = @()
    if ($keepCount -gt 0) {
        $keep = $finalApks | Select-Object -First $keepCount
    }

    $keepNames = @{}
    foreach ($item in $keep) {
        $keepNames[$item.Name] = $true
    }

    foreach ($file in Get-ChildItem -Path $apkFinalDir -File -ErrorAction SilentlyContinue) {
        $name = $file.Name
        $isFinalApk = $name -match '^ApexCombat-Mod-V\d+-Final\.apk$'

        if ($isFinalApk -and $keepNames.ContainsKey($name)) {
            continue
        }

        Remove-FileSafe $file.FullName
    }
}

function Show-Summary([string]$root) {
    Write-Host ''
    Write-Host 'Itens principais preservados:' -ForegroundColor Green

    $kept = @(
        'original_splits',
        'base_decompiled',
        'assets_decompiled',
        'config_decompiled',
        'app\src\main',
        'extracted\extracted_lua',
        'apk_final',
        'dump.cs',
        'build_final.ps1',
        'cleanup_workspace.ps1',
        'gradle',
        'gradlew',
        'gradlew.bat',
        'build.gradle.kts',
        'settings.gradle.kts',
        'gradle.properties',
        'local.properties',
        'release-v3.keystore'
    )

    foreach ($item in $kept) {
        $fullPath = Join-Path $root $item
        if (Test-Path $fullPath) {
            Write-Host " - $item"
        }
    }
}

$root = Get-ProjectRoot

Write-Host "Limpando workspace em: $root" -ForegroundColor Yellow
Write-Host "KeepFinalCount = $KeepFinalCount" -ForegroundColor Yellow
if ($DryRun) {
    Write-Host 'Modo DryRun ativado. Nada sera removido.' -ForegroundColor Yellow
}

Cleanup-GeneratedDirectories $root
Cleanup-UnusedFiles $root
Cleanup-ApkFinal $root $KeepFinalCount
Show-Summary $root

Write-Host ''
Write-Host 'Limpeza concluida.' -ForegroundColor Green
