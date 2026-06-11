param(
    [string]$BaseApk = '',
    [string]$ModApk = 'app\build\outputs\apk\release\app-release-unsigned.apk',
    [string]$AssetsDir = 'assets_decompiled',
    [string]$ConfigDir = 'config_decompiled',
    [string]$OutputDir = 'apk_final',
    [string]$WorkDir = 'build_work',
    [switch]$SkipGradle,
    [switch]$AllowMissingSplits,
    [switch]$BuildStandaloneApk,
    [switch]$SkipInstall,
    [switch]$UninstallBeforeInstall,
    [string]$AdbPath = ''
)

$ErrorActionPreference = 'Stop'

function Write-Utf8NoBom([string]$path, [string]$value) {
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, $value, $utf8NoBom)
}

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
    if ($LASTEXITCODE -ne 0) { throw "apktool falhou (exit code: $LASTEXITCODE)" }
}

function Read-SdkDirFromLocalProperties([string]$root) {
    $lp = Join-Path $root 'local.properties'
    if (-not (Test-Path $lp)) { return '' }
    $line = (Get-Content $lp | Where-Object { $_ -match '^sdk\.dir=' } | Select-Object -First 1)
    if (-not $line) { return '' }
    $sdkDir = $line.Substring('sdk.dir='.Length)
    return $sdkDir.Replace('\:', ':').Replace('\\', '\')
}

function Get-AndroidSdkDir([string]$root) {
    $fromLocalProps = Read-SdkDirFromLocalProperties $root
    if ($fromLocalProps -and (Test-Path $fromLocalProps)) { return $fromLocalProps }

    if ($env:ANDROID_SDK_ROOT) {
        $p = $env:ANDROID_SDK_ROOT
        if (Test-Path $p) { return (Resolve-Path $p).Path }
    }
    if ($env:ANDROID_HOME) {
        $p = $env:ANDROID_HOME
        if (Test-Path $p) { return (Resolve-Path $p).Path }
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Android\Sdk'),
        (Join-Path $env:USERPROFILE 'AppData\Local\Android\Sdk')
    ) | Where-Object { $_ -and $_.Trim().Length -gt 0 } | Select-Object -Unique

    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }

    $lp = Join-Path $root 'local.properties'
    throw "Android SDK não encontrado. Crie $lp com sdk.dir=... ou defina ANDROID_SDK_ROOT/ANDROID_HOME."
}

function Get-LatestBuildToolsDir([string]$sdkDir) {
    $btRoot = Join-Path $sdkDir 'build-tools'
    if (-not (Test-Path $btRoot)) { throw "build-tools não encontrado: $btRoot" }
    $dirs = Get-ChildItem -Path $btRoot -Directory | Select-Object -ExpandProperty Name
    if (-not $dirs) { throw "nenhuma versão em build-tools: $btRoot" }
    $best = $dirs | Sort-Object { [version]($_ -replace '[^0-9\.]','') } -Descending | Select-Object -First 1
    return (Join-Path $btRoot $best)
}

function Resolve-Adb([string]$adbPath) {
    if ($adbPath) {
        if (Test-Path $adbPath) { return (Resolve-Path $adbPath).Path }
        $fromRoot = Join-Path (Get-ProjectRoot) $adbPath
        if (Test-Path $fromRoot) { return (Resolve-Path $fromRoot).Path }
        throw "adb não encontrado em: $adbPath"
    }
    $cmd = Get-Command adb -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $cmd = Get-Command adb.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "adb não encontrado no PATH. Instale platform-tools ou informe -AdbPath."
}

function Invoke-AdbCmd([string]$adb, [string]$argsLine) {
    $cmdLine = "`"$adb`" $argsLine 2>&1"
    $out = & cmd /c $cmdLine
    return @{
        Output = $out
        ExitCode = $LASTEXITCODE
    }
}

function Test-AdbDevice([string]$adb) {
    $null = Invoke-AdbCmd -adb $adb -argsLine 'start-server'
    $r = Invoke-AdbCmd -adb $adb -argsLine 'devices'
    if ($r.ExitCode -ne 0 -or -not $r.Output) { return $false }
    $out = $r.Output
    foreach ($l in $out) {
        if ($l -match '^\s*List of devices attached') { continue }
        if ($l -match '^\s*$') { continue }
        if ($l -match '\tdevice\s*$') { return $true }
    }
    return $false
}

function Install-ApkViaAdb([string]$adb, [string]$apkPath) {
    if (-not (Test-Path $apkPath)) { throw "APK não encontrado: $apkPath" }
    $r = Invoke-AdbCmd -adb $adb -argsLine ("install -r `"$apkPath`"")
    if ($r.ExitCode -ne 0) { throw ("adb install falhou (exit code: {0}): {1}`r`n{2}" -f $r.ExitCode, $apkPath, ($r.Output -join "`r`n")) }
}

function Install-MultipleApksViaAdb([string]$adb, [string[]]$apkPaths) {
    if (-not $apkPaths -or $apkPaths.Count -le 0) { throw "Nenhum APK informado para adb install-multiple" }
    foreach ($p in $apkPaths) {
        if (-not (Test-Path $p)) { throw "APK não encontrado: $p" }
    }
    $args = ($apkPaths | ForEach-Object { "`"$_`"" }) -join ' '
    $r = Invoke-AdbCmd -adb $adb -argsLine ("install-multiple -r " + $args)
    if ($r.ExitCode -ne 0) { throw ("adb install-multiple falhou (exit code: {0})`r`n{1}" -f $r.ExitCode, ($r.Output -join "`r`n")) }
}

function Uninstall-PackageViaAdb([string]$adb, [string]$packageName) {
    if (-not $packageName) { return }
    $r = Invoke-AdbCmd -adb $adb -argsLine ("uninstall `"$packageName`"")
    if ($r.ExitCode -ne 0) {
        $text = ($r.Output -join "`r`n")
        if ($text -notmatch 'Unknown package') { throw ("adb uninstall falhou (exit code: {0})`r`n{1}" -f $r.ExitCode, $text) }
    }
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

function Test-BaseRequiresSplits([string]$manifestPath) {
    if (-not (Test-Path $manifestPath)) { return $false }
    $xml = Get-Content -Raw -Encoding UTF8 $manifestPath
    return ($xml -match 'requiredSplitTypes="' -or $xml -match 'com\.android\.vending\.splits\.required' -or $xml -match '<uses-split\b' -or $xml -match 'isSplitRequired="true"')
}

function Patch-ManifestRemoveSplitRequirement([string]$manifestPath) {
    if (-not (Test-Path $manifestPath)) { return }
    $xml = Get-Content -Raw -Encoding UTF8 $manifestPath
    $xml = [regex]::Replace($xml, '\sandroid:requiredSplitTypes="[^"]*"', '')
    $xml = [regex]::Replace($xml, '\sandroid:splitTypes="[^"]*"', '')
    $xml = [regex]::Replace($xml, '\sandroid:isSplitRequired="true"', '')
    $xml = [regex]::Replace($xml, '\s*<uses-split\b[^>]*/>\s*', "`r`n")
    $xml = [regex]::Replace($xml, '\s*<meta-data[^>]*android:name="com\.android\.vending\.splits\.required"[^>]*/>\s*', "`r`n")
    $xml = [regex]::Replace($xml, '\s*<meta-data[^>]*android:name="com\.android\.vending\.splits"[^>]*/>\s*', "`r`n")
    $xml = [regex]::Replace($xml, '\s*<meta-data[^>]*android:name="com\.android\.vending\.derived\.apk\.id"[^>]*/>\s*', "`r`n")
    Write-Utf8NoBom -path $manifestPath -value $xml
}

function Get-OriginalSplitApks([string]$root) {
    $dir = Join-Path $root 'original_splits'
    if (-not (Test-Path $dir)) { return @() }
    return @(Get-ChildItem -Path $dir -File -Filter '*.apk' -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'base.apk' } | Sort-Object Name)
}

function Zipalign-And-SignApk(
    [string]$buildToolsDir,
    [string]$inApk,
    [string]$outApk,
    [string]$tmpDir
) {
    $zipalign = Join-Path $buildToolsDir 'zipalign.exe'
    $apksigner = Join-Path $buildToolsDir 'apksigner.bat'
    if (-not (Test-Path $zipalign)) { throw "zipalign não encontrado: $zipalign" }
    if (-not (Test-Path $apksigner)) { throw "apksigner não encontrado: $apksigner" }
    if (-not (Test-Path $inApk)) { throw "APK não encontrado: $inApk" }
    if (-not (Test-Path $tmpDir)) { New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null }

    $aligned = Join-Path $tmpDir ((Split-Path $outApk -Leaf) -replace '\.apk$', '.aligned.apk')
    & $zipalign -f 4 $inApk $aligned | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "zipalign falhou (exit code: $LASTEXITCODE): $inApk" }

    $debugKs = Join-Path $env:USERPROFILE '.android\debug.keystore'
    if (-not (Test-Path $debugKs)) { throw "debug.keystore não encontrado: $debugKs" }
    & $apksigner sign --ks $debugKs --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android --out $outApk $aligned | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "apksigner sign falhou (exit code: $LASTEXITCODE): $inApk" }
    & $apksigner verify --verbose $outApk | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "apksigner verify falhou (exit code: $LASTEXITCODE): $outApk" }

    if (Test-Path $aligned) { Remove-Item -Force $aligned }
}

function Resolve-BaseApk([string]$root, [string]$outDir, [string]$baseApk) {
    if ($baseApk) {
        $p = (Join-Path $root $baseApk)
        if (Test-Path $p) { return $p }
        if (Test-Path $baseApk) { return (Resolve-Path $baseApk).Path }
        throw "BaseApk não encontrado: $baseApk"
    }

    $fallback = Join-Path $root 'original_splits\base.apk'
    if (Test-Path $fallback) { return $fallback }

    $candidates = Get-ChildItem -Path $outDir -File -Filter 'ApexCombat-Mod-V*-Final.apk' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
    if ($candidates -and $candidates.Count -gt 0) { return $candidates[0].FullName }

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
    Write-Utf8NoBom -path $manifestPath -value $xml
}

function Patch-ManifestEnsureMod([string]$manifestPath) {
    $xml = Get-Content -Raw -Encoding UTF8 $manifestPath

    if ($xml -notmatch 'android\.permission\.SYSTEM_ALERT_WINDOW') {
        $xml = $xml -replace '<manifest\b', '<manifest'
        $xml = $xml -replace '(<manifest[^>]*>)', ('$1' + "`r`n    <uses-permission android:name=`"android.permission.SYSTEM_ALERT_WINDOW`" />")
    }
    if ($xml -notmatch 'android\.permission\.FOREGROUND_SERVICE\b') {
        $xml = $xml -replace '(<manifest[^>]*>)', ('$1' + "`r`n    <uses-permission android:name=`"android.permission.FOREGROUND_SERVICE`" />")
    }
    if ($xml -notmatch 'android\.permission\.POST_NOTIFICATIONS\b') {
        $xml = $xml -replace '(<manifest[^>]*>)', ('$1' + "`r`n    <uses-permission android:name=`"android.permission.POST_NOTIFICATIONS`" />")
    }

    if ($xml -notmatch 'com\.on00dev\.apexcombatmod\.FloatingModMenuService') {
        $serviceNode = "        <service android:name=`"com.on00dev.apexcombatmod.FloatingModMenuService`" android:enabled=`"true`" android:exported=`"false`" android:foregroundServiceType=`"dataSync`" />"
        if ($xml -match '</application>') {
            $xml = $xml -replace '</application>', ($serviceNode + "`r`n    </application>")
        }
    }

    Write-Utf8NoBom -path $manifestPath -value $xml
}

function Patch-ApplicationAttachBaseContext([string]$baseDecDir) {
    $smaliPath = Join-Path $baseDecDir 'smali\com\pairip\application\Application.smali'
    if (-not (Test-Path $smaliPath)) {
        $smaliPath = Join-Path $baseDecDir 'smali_classes2\com\pairip\application\Application.smali'
    }
    if (-not (Test-Path $smaliPath)) { return $false }

    $text = Get-Content -Raw -Encoding UTF8 $smaliPath
    $re = [regex]'(?ms)^\.method[^\r\n]*\battachBaseContext\(Landroid/content/Context;\)V\r?\n.*?^\.end\s+method\r?\n'
    if (-not $re.IsMatch($text)) { return $false }

    $replacement = @"
.method public attachBaseContext(Landroid/content/Context;)V
    .locals 0

    invoke-super {p0, p1}, Lcom/volvapps/onesdk/OneApplication;->attachBaseContext(Landroid/content/Context;)V

    invoke-static {p0}, Lcom/on00dev/apexcombatmod/ModLoader;->load(Landroid/content/Context;)V

    return-void
.end method

"@

    $newText = $re.Replace($text, $replacement, 1)
    if ($newText -ne $text) {
        Write-Utf8NoBom -path $smaliPath -value $newText
        return $true
    }
    return $false
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
    Write-Utf8NoBom -path $attrsPath -value $xml
}

function Patch-SignatureChecksSmali([string]$baseDecDir) {
    $smaliRoots = Get-ChildItem -Path $baseDecDir -Directory -Filter 'smali*' -ErrorAction SilentlyContinue
    if (-not $smaliRoots) { return 0 }

    $patchedMethods = 0
    $methodRe = [regex]'(?ms)^(?<head>\.method[^\r\n]*\)[^\r\n]*\r?\n)(?<mid>.*?)(?<tail>^\.end method\b.*?$)'

    foreach ($sr in $smaliRoots) {
        $files = Get-ChildItem -Path $sr.FullName -Recurse -File -Filter '*.smali' -ErrorAction SilentlyContinue
        foreach ($f in $files) {
            $text = Get-Content -Raw -Encoding UTF8 $f.FullName
            $hasQuickHit =
                ($text.IndexOf('->checkSignatures(') -ge 0) -or
                ($text.IndexOf('->getPackageInfo(') -ge 0) -or
                ($text.IndexOf('PackageInfo;->signatures') -ge 0) -or
                ($text.IndexOf('Landroid/content/pm/SigningInfo;') -ge 0) -or
                ($text.IndexOf('Signature;->toByteArray(') -ge 0)
            if (-not $hasQuickHit) { continue }

            $matches = $methodRe.Matches($text)
            if ($matches.Count -le 0) { continue }

            $sb = New-Object System.Text.StringBuilder
            $last = 0
            $changed = $false

            foreach ($m in $matches) {
                [void]$sb.Append($text.Substring($last, $m.Index - $last))

                $head = $m.Groups['head'].Value
                $mid = $m.Groups['mid'].Value
                $tail = $m.Groups['tail'].Value

                $hasCheckSignatures = ($mid -match '->checkSignatures\(')
                $hasGetPkg = ($mid -match '->getPackageInfo\(')
                $hasSignatureApis = ($mid -match 'Landroid/content/pm/Signature;|Landroid/content/pm/SigningInfo;|GET_SIGNATURES|GET_SIGNING_CERTIFICATES|PackageInfo;->signatures|Signature;->toByteArray\(')
                $hasCrypto = ($mid -match 'Ljava/security/MessageDigest;|Ljava/security/cert/CertificateFactory;|Ljava/security/cert/X509Certificate;|Ljava/util/Arrays;->equals\(')
                $shouldPatch = $hasCheckSignatures -or ($hasGetPkg -and $hasSignatureApis) -or $hasSignatureApis -or $hasCrypto

                if ($shouldPatch) {
                    $ret = [regex]::Match($head, '\)(?<ret>.)').Groups['ret'].Value
                    if ($ret -eq 'Z') {
                        $patchedMethods++
                        $changed = $true
                        [void]$sb.Append($head + "`r`n    .locals 1`r`n    const/4 v0, 0x1`r`n    return v0`r`n" + $tail)
                    }
                    elseif ($ret -eq 'V') {
                        $patchedMethods++
                        $changed = $true
                        [void]$sb.Append($head + "`r`n    .locals 0`r`n    return-void`r`n" + $tail)
                    }
                    else {
                        [void]$sb.Append($m.Value)
                    }
                }
                else {
                    [void]$sb.Append($m.Value)
                }

                $last = $m.Index + $m.Length
            }

            [void]$sb.Append($text.Substring($last))
            if ($changed) {
                Write-Utf8NoBom -path $f.FullName -value $sb.ToString()
            }
        }
    }

    return $patchedMethods
}

function Merge-And-SignFinalApk(
    [string]$root,
    [string]$buildToolsDir,
    [string]$baseUnsignedApk,
    [string]$originalSplitsDir,
    [string]$assetsDir,
    [string]$configDir,
    [string]$outUnsignedApk,
    [string]$outFinalApk
) {
    $zipalign = Join-Path $buildToolsDir 'zipalign.exe'
    $apksigner = Join-Path $buildToolsDir 'apksigner.bat'
    if (-not (Test-Path $zipalign)) { throw "zipalign não encontrado: $zipalign" }
    if (-not (Test-Path $apksigner)) { throw "apksigner não encontrado: $apksigner" }
    if (-not (Test-Path $baseUnsignedApk)) { throw "APK rebuildado (unsigned) não encontrado: $baseUnsignedApk" }

    $py = @"
import os, zipfile, pathlib, sys
root = pathlib.Path(r'''$root''')
base_apk = pathlib.Path(r'''$baseUnsignedApk''')
splits_dir = root / r'''$originalSplitsDir'''
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

split_overrides = {}
if splits_dir.exists():
    for split_apk in sorted(splits_dir.glob('*.apk')):
        if split_apk.name.lower() == 'base.apk':
            continue
        try:
            with zipfile.ZipFile(split_apk, 'r') as zs:
                for info in zs.infolist():
                    name = info.filename
                    if name.endswith('/') or name.startswith('META-INF/'):
                        continue
                    if not (name.startswith('lib/') or name.startswith('assets/')):
                        continue
                    split_overrides[name] = (split_apk, name)
        except Exception:
            pass

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
            if name in split_overrides:
                continue
            data = zb.read(name)
            zi = zipfile.ZipInfo(name, date_time=info.date_time)
            zi.create_system = info.create_system
            zi.external_attr = info.external_attr
            zi.compress_type = zipfile.ZIP_STORED if name == 'resources.arsc' else info.compress_type
            zo.writestr(zi, data)
    for name, (apk_path, inner_name) in split_overrides.items():
        with zipfile.ZipFile(apk_path, 'r') as zs:
            data = zs.read(inner_name)
        zi = zipfile.ZipInfo(name)
        zi.compress_type = zipfile.ZIP_DEFLATED
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
    if ($LASTEXITCODE -ne 0) { throw "python falhou (exit code: $LASTEXITCODE)" }

    $aligned = [System.IO.Path]::ChangeExtension($outUnsignedApk, '.aligned.apk')
    & $zipalign -f 4 $outUnsignedApk $aligned | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "zipalign falhou (exit code: $LASTEXITCODE)" }
    $debugKs = Join-Path $env:USERPROFILE '.android\debug.keystore'
    if (-not (Test-Path $debugKs)) { throw "debug.keystore não encontrado: $debugKs" }
    & $apksigner sign --ks $debugKs --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android --out $outFinalApk $aligned | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "apksigner sign falhou (exit code: $LASTEXITCODE)" }
    & $apksigner verify --verbose $outFinalApk | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "apksigner verify falhou (exit code: $LASTEXITCODE)" }
}

$root = Get-ProjectRoot
$sdkDir = Get-AndroidSdkDir $root
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

$manifestPath = Join-Path $baseDec 'AndroidManifest.xml'
$wasSplitsRequired = Test-BaseRequiresSplits $manifestPath

Patch-ManifestRemoveLicense (Join-Path $baseDec 'AndroidManifest.xml')
Patch-ManifestEnsureMod (Join-Path $baseDec 'AndroidManifest.xml')
Ensure-ConstraintAttrs (Join-Path $baseDec 'res\\values\\attrs.xml')
$splitApksPre = Get-OriginalSplitApks $root
$shouldBuildStandalone =
    $BuildStandaloneApk -or
    ($wasSplitsRequired -and $splitApksPre.Count -gt 0)
if ($shouldBuildStandalone -and $wasSplitsRequired) {
    Patch-ManifestRemoveSplitRequirement (Join-Path $baseDec 'AndroidManifest.xml')
}

$patchedApp = Patch-ApplicationAttachBaseContext $baseDec
if (-not $patchedApp) { throw "Não consegui patchar Application.attachBaseContext para carregar o mod (com/pairip/application/Application.smali)." }
Write-Host "Patch assinatura (smali)..."
$sigPatched = Patch-SignatureChecksSmali -baseDecDir $baseDec
Write-Host "Assinatura: metodos patched = $sigPatched"

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
Merge-And-SignFinalApk -root $root -buildToolsDir $buildTools -baseUnsignedApk $baseUnsigned -originalSplitsDir 'original_splits' -assetsDir $AssetsDir -configDir $ConfigDir -outUnsignedApk $finalUnsigned -outFinalApk $finalApk

if (-not (Test-Path $finalApk)) { throw "APK final não encontrado: $finalApk" }

$installMode = 'single'
$installApkArgs = @($finalApk)

if ($wasSplitsRequired) {
    $splitApks = $splitApksPre
    if ($splitApks.Count -le 0) {
        if (-not $AllowMissingSplits) {
            throw "Este base.apk exige splits, mas original_splits só tem base.apk. Baixe/extraia o bundle completo (.apks) com os splits (ex: split_config.arm64_v8a.apk) e rode prepare_base.ps1 apontando para ele."
        }
        Write-Host "Aviso: base exige splits, mas nenhum split foi encontrado em original_splits. Instalar apenas $finalApk vai falhar com INSTALL_FAILED_MISSING_SPLIT."
    }
    else {
        $splitOutDir = Join-Path $outDirAbs ("ApexCombat-Mod-V{0}-Splits" -f $v)
        if (Test-Path $splitOutDir) { Remove-Item -Recurse -Force $splitOutDir }
        New-Item -ItemType Directory -Force -Path $splitOutDir | Out-Null

        Copy-Item -Force $finalApk -Destination (Join-Path $splitOutDir 'base.apk')

        foreach ($s in $splitApks) {
            $outSplit = Join-Path $splitOutDir $s.Name
            Zipalign-And-SignApk -buildToolsDir $buildTools -inApk $s.FullName -outApk $outSplit -tmpDir $workDirAbs
        }

        Write-Host "Splits prontos em: $splitOutDir"
        $apkArgs = @((Join-Path $splitOutDir 'base.apk')) + ($splitApks | ForEach-Object { Join-Path $splitOutDir $_.Name })
        $apkArgsQuoted = $apkArgs | ForEach-Object { '"' + $_ + '"' }
        Write-Host ("Instalar (adb): adb install-multiple -r " + ($apkArgsQuoted -join ' '))
        if ($shouldBuildStandalone) {
            Write-Host "Standalone: tente instalar o APK unico (adb): adb install -r `"$finalApk`""
        }

        $apksBundleZip = Join-Path $outDirAbs ("ApexCombat-Mod-V{0}.zip" -f $v)
        $apksBundle = Join-Path $outDirAbs ("ApexCombat-Mod-V{0}.apks" -f $v)
        if (Test-Path $apksBundleZip) { Remove-Item -Force $apksBundleZip }
        if (Test-Path $apksBundle) { Remove-Item -Force $apksBundle }
        Compress-Archive -Path (Join-Path $splitOutDir '*.apk') -DestinationPath $apksBundleZip -Force | Out-Null
        Move-Item -Force -LiteralPath $apksBundleZip -Destination $apksBundle
        Write-Host "Bundle (.apks) pronto em: $apksBundle"

        $installMode = 'multiple'
        $installApkArgs = $apkArgs
    }
}

if (-not $SkipInstall) {
    $adb = Resolve-Adb $AdbPath
    if (-not (Test-AdbDevice $adb)) {
        throw "Nenhum device encontrado via adb. Conecte o celular/emulador e confirme 'adb devices'."
    }

    Write-Host "Instalando via adb..."
    if ($UninstallBeforeInstall) {
        Uninstall-PackageViaAdb -adb $adb -packageName 'com.vector.apexcombat.google'
    }
    if ($installMode -eq 'multiple') {
        Install-MultipleApksViaAdb -adb $adb -apkPaths $installApkArgs
    }
    else {
        Install-ApkViaAdb -adb $adb -apkPath $installApkArgs[0]
    }
    Write-Host "Instalacao OK"
}

Write-Host "OK: $finalApk"
