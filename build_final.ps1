param(
    [string]$BaseApk = '',
    [string]$ModApk = 'app\build\outputs\apk\release\app-release-unsigned.apk',
    [string]$AssetsDir = 'assets_decompiled',
    [string]$ConfigDir = 'config_decompiled',
    [string]$OutputDir = 'apk_final',
    [string]$WorkDir = 'build_work',
    [switch]$SkipGradle
)

$ErrorActionPreference = 'Stop'

function Get-ProjectRoot {
    return (Get-Location).Path
}

function Get-ApktoolJarPath {
    $cmd = Get-Command apktool -ErrorAction Stop
    $src = $cmd.Source
    $dir = Split-Path $src -Parent
    $plain = Join-Path $dir 'apktool.jar'
    if (Test-Path $plain) { return $plain }
    $jars = Get-ChildItem -Path $dir -File -Filter 'apktool*.jar' -ErrorAction SilentlyContinue
    if (-not $jars) { throw "apktool*.jar não encontrado em $dir" }
    $best = $null
    $bestVer = [version]'0.0.0'
    foreach ($j in $jars) {
        $m = [regex]::Match($j.BaseName, '(\d+\.\d+\.\d+)')
        if ($m.Success) {
            $v = [version]$m.Groups[1].Value
            if ($v -gt $bestVer) { $bestVer = $v; $best = $j.FullName }
        }
    }
    if ($best) { return $best }
    return ($jars | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
}

function Invoke-Apktool {
    param([Parameter(Mandatory = $true)][string[]]$ApktoolArgs)
    $jar = Get-ApktoolJarPath
    $java = 'java.exe'
    if ($env:JAVA_HOME) {
        $candidate = Join-Path $env:JAVA_HOME 'bin\\java.exe'
        if (Test-Path $candidate) { $java = $candidate }
    }
    & $java -jar "-Xmx1024M" "-Dfile.encoding=UTF8" "-Duser.language=en" $jar @ApktoolArgs | Out-Null
}

function Read-SdkDirFromLocalProperties([string]$root) {
    $lp = Join-Path $root 'local.properties'
    if (-not (Test-Path $lp)) { throw "local.properties não encontrado: $lp" }
    $line = (Get-Content $lp | Where-Object { $_ -match '^sdk\.dir=' } | Select-Object -First 1)
    if (-not $line) { throw "sdk.dir não encontrado em local.properties" }
    $sdkDir = $line.Substring('sdk.dir='.Length)
    return $sdkDir.Replace('\:', ':').Replace('\\', '\')
}

function Get-LatestBuildToolsDir([string]$sdkDir) {
    $btRoot = Join-Path $sdkDir 'build-tools'
    if (-not (Test-Path $btRoot)) { throw "build-tools não encontrado: $btRoot" }
    $dirs = Get-ChildItem -Path $btRoot -Directory | Select-Object -ExpandProperty Name
    if (-not $dirs) { throw "nenhuma versão em build-tools: $btRoot" }
    $best = $dirs | Sort-Object { [version]($_ -replace '[^0-9\.]','') } -Descending | Select-Object -First 1
    return (Join-Path $btRoot $best)
}

function Get-NextVersion([string]$outDir) {
    if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }
    $re = [regex]'ApexCombat-Mod-V(\d+)-Final\.apk$'
    $max = 0
    Get-ChildItem -Path $outDir -File -Filter 'ApexCombat-Mod-V*-Final.apk' -ErrorAction SilentlyContinue | ForEach-Object {
        $m = $re.Match($_.Name)
        if ($m.Success) {
            $v = [int]$m.Groups[1].Value
            if ($v -gt $max) { $max = $v }
        }
    }
    if ($max -le 0) { return 48 }
    return ($max + 1)
}

function Resolve-BaseApk([string]$root, [string]$outDir, [string]$baseApk) {
    if ($baseApk) {
        $p = (Join-Path $root $baseApk)
        if (Test-Path $p) { return $p }
        if (Test-Path $baseApk) { return (Resolve-Path $baseApk).Path }
        throw "BaseApk não encontrado: $baseApk"
    }

    $candidates = Get-ChildItem -Path $outDir -File -Filter 'ApexCombat-Mod-V*-Final.apk' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
    if ($candidates -and $candidates.Count -gt 0) { return $candidates[0].FullName }

    $fallback = Join-Path $root 'original_splits\base.apk'
    if (Test-Path $fallback) { return $fallback }

    throw "Nenhuma base encontrada. Informe -BaseApk ou coloque base.apk em original_splits\\base.apk"
}

function Find-ModSmaliDir([string]$modDecompiledDir) {
    $smaliRoots = Get-ChildItem -Path $modDecompiledDir -Directory -Filter 'smali*' | Select-Object -ExpandProperty FullName
    foreach ($sr in $smaliRoots) {
        $candidate = Join-Path $sr 'com\on00dev\apexcombatmod'
        if (Test-Path $candidate) { return $candidate }
    }
    throw "Não achei smali do mod em $modDecompiledDir"
}

function Patch-ManifestRemoveLicense([string]$manifestPath) {
    $xml = Get-Content -Raw -Encoding UTF8 $manifestPath
    $xml = [regex]::Replace($xml, '\s*<activity[^>]*android:name="com\.pairip\.licensecheck\.LicenseActivity"[^>]*/>\s*', "`r`n")
    $xml = [regex]::Replace($xml, '\s*<provider[^>]*android:name="com\.pairip\.licensecheck\.LicenseContentProvider"[^>]*/>\s*', "`r`n")
    $xml = [regex]::Replace($xml, '\s*<uses-permission[^>]*android:name="com\.android\.vending\.CHECK_LICENSE"[^>]*/>\s*', "`r`n")
    Set-Content -Path $manifestPath -Value $xml -Encoding UTF8
}

function Ensure-ConstraintAttrs([string]$attrsPath) {
    $need = @(
        'layout_constraintBottom_toBottomOf',
        'layout_constraintEnd_toEndOf',
        'layout_constraintStart_toStartOf',
        'layout_constraintTop_toTopOf'
    )
    $xml = Get-Content -Raw -Encoding UTF8 $attrsPath
    $missing = @()
    foreach ($n in $need) {
        if ($xml -notmatch [regex]::Escape("name=`"$n`"")) { $missing += $n }
    }
    if ($missing.Count -eq 0) { return }
    $insert = ($missing | ForEach-Object { "    <attr name=`"$_`" format=`"reference|string`" />" }) -join "`r`n"
    $xml = $xml -replace '</resources>\s*$', ($insert + "`r`n</resources>")
    Set-Content -Path $attrsPath -Value $xml -Encoding UTF8
}

function Merge-And-SignFinalApk(
    [string]$root,
    [string]$buildToolsDir,
    [string]$baseUnsignedApk,
    [string]$assetsDir,
    [string]$configDir,
    [string]$outUnsignedApk,
    [string]$outFinalApk
) {
    $zipalign = Join-Path $buildToolsDir 'zipalign.exe'
    $apksigner = Join-Path $buildToolsDir 'apksigner.bat'
    if (-not (Test-Path $zipalign)) { throw "zipalign não encontrado: $zipalign" }
    if (-not (Test-Path $apksigner)) { throw "apksigner não encontrado: $apksigner" }

    $py = @"
import os, zipfile, pathlib, sys
root = pathlib.Path(r'''$root''')
base_apk = pathlib.Path(r'''$baseUnsignedApk''')
assets_dir = root / r'''$assetsDir'''
config_dir = root / r'''$configDir'''
out_u = pathlib.Path(r'''$outUnsignedApk''')
out_u.parent.mkdir(parents=True, exist_ok=True)

def iter_files(p: pathlib.Path, prefix: str):
    if not p.exists():
        return []
    files=[]
    for f in p.rglob('*'):
        if f.is_file():
            rel = f.relative_to(p).as_posix()
            files.append((prefix + rel, f))
    return files

asset_overrides = dict(iter_files(assets_dir / 'assets', 'assets/'))
lib_overrides = dict(iter_files(config_dir / 'lib', 'lib/'))
lib_overrides.pop('lib/arm64-v8a/libapexcombatmod.so', None)

with zipfile.ZipFile(base_apk,'r') as zb:
    base_names = zb.namelist()

with zipfile.ZipFile(out_u,'w') as zo:
    with zipfile.ZipFile(base_apk,'r') as zb:
        for info in zb.infolist():
            name = info.filename
            if name.endswith('/') or name.startswith('META-INF/'):
                continue
            if name in asset_overrides:
                continue
            if name in lib_overrides:
                continue
            data = zb.read(name)
            zi = zipfile.ZipInfo(name, date_time=info.date_time)
            zi.create_system = info.create_system
            zi.external_attr = info.external_attr
            zi.compress_type = zipfile.ZIP_STORED if name == 'resources.arsc' else info.compress_type
            zo.writestr(zi, data)
    for name, path in lib_overrides.items():
        data = path.read_bytes()
        zi = zipfile.ZipInfo(name)
        zi.compress_type = zipfile.ZIP_DEFLATED
        zo.writestr(zi, data)
    for name, path in asset_overrides.items():
        data = path.read_bytes()
        zi = zipfile.ZipInfo(name)
        zi.compress_type = zipfile.ZIP_DEFLATED
        zo.writestr(zi, data)

print('OK', str(out_u))
"@

    & python -c $py

    $aligned = [System.IO.Path]::ChangeExtension($outUnsignedApk, '.aligned.apk')
    & $zipalign -f 4 $outUnsignedApk $aligned | Out-Null
    $debugKs = Join-Path $env:USERPROFILE '.android\debug.keystore'
    if (-not (Test-Path $debugKs)) { throw "debug.keystore não encontrado: $debugKs" }
    & $apksigner sign --ks $debugKs --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android --out $outFinalApk $aligned | Out-Null
    & $apksigner verify --verbose $outFinalApk | Out-Null
}

$root = Get-ProjectRoot
$sdkDir = Read-SdkDirFromLocalProperties $root
$buildTools = Get-LatestBuildToolsDir $sdkDir

$outDirAbs = Join-Path $root $OutputDir
$workDirAbs = Join-Path $root $WorkDir
$distDirAbs = Join-Path $root 'dist'

New-Item -ItemType Directory -Force -Path $outDirAbs | Out-Null
New-Item -ItemType Directory -Force -Path $workDirAbs | Out-Null
New-Item -ItemType Directory -Force -Path $distDirAbs | Out-Null

$v = Get-NextVersion $outDirAbs
$baseApkAbs = Resolve-BaseApk $root $outDirAbs $BaseApk

Write-Host "Base: $baseApkAbs"
Write-Host "Versão: V$v"

if (-not $SkipGradle) {
    Write-Host "Compilando mod (release)..."
    & "$root\\gradlew.bat" :app:assembleRelease --no-daemon | Out-Null
}

$modApkAbs = Join-Path $root $ModApk
if (-not (Test-Path $modApkAbs)) { throw "ModApk não encontrado: $modApkAbs" }

$modDec = Join-Path $workDirAbs 'mod_decompiled'
$baseDec = Join-Path $workDirAbs 'base_decompiled'

if (Test-Path $modDec) { Remove-Item -Recurse -Force $modDec }
if (Test-Path $baseDec) { Remove-Item -Recurse -Force $baseDec }

Write-Host "Descompilando mod..."
Invoke-Apktool -ApktoolArgs @('d', '-f', $modApkAbs, '-o', $modDec)

Write-Host "Descompilando base..."
Invoke-Apktool -ApktoolArgs @('d', '-f', $baseApkAbs, '-o', $baseDec)

Patch-ManifestRemoveLicense (Join-Path $baseDec 'AndroidManifest.xml')
Ensure-ConstraintAttrs (Join-Path $baseDec 'res\\values\\attrs.xml')

$srcSmali = Find-ModSmaliDir $modDec
$dstSmali = Join-Path $baseDec 'smali\\com\\on00dev\\apexcombatmod'
if (Test-Path $dstSmali) { Remove-Item -Recurse -Force $dstSmali }
New-Item -ItemType Directory -Force -Path (Split-Path $dstSmali -Parent) | Out-Null
Copy-Item -Recurse -Force $srcSmali -Destination (Split-Path $dstSmali -Parent)

$srcSo = Join-Path $modDec 'lib\\arm64-v8a\\libapexcombatmod.so'
$dstSo = Join-Path $baseDec 'lib\\arm64-v8a\\libapexcombatmod.so'
if (-not (Test-Path $srcSo)) { throw "libapexcombatmod.so do mod não encontrado: $srcSo" }
New-Item -ItemType Directory -Force -Path (Split-Path $dstSo -Parent) | Out-Null
Copy-Item -Force $srcSo -Destination $dstSo

$baseUnsigned = Join-Path $distDirAbs 'base_mod-unsigned.apk'
Write-Host "Build da base (apktool)..."
Invoke-Apktool -ApktoolArgs @('b', '-f', $baseDec, '-o', $baseUnsigned)

$finalUnsigned = Join-Path $outDirAbs ("ApexCombat-Mod-V{0}-Final-unsigned.apk" -f $v)
$finalApk = Join-Path $outDirAbs ("ApexCombat-Mod-V{0}-Final.apk" -f $v)

Write-Host "Merge + assinatura..."
Merge-And-SignFinalApk -root $root -buildToolsDir $buildTools -baseUnsignedApk $baseUnsigned -assetsDir $AssetsDir -configDir $ConfigDir -outUnsignedApk $finalUnsigned -outFinalApk $finalApk

Write-Host "OK: $finalApk"
