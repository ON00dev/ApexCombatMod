param(
    [string]$InputApkOrApks = 'base.apk',
    [string]$PackageName = 'com.vector.apexcombat.google',
    [string]$AdbPath = '',
    [string]$AssetsDir = 'assets_decompiled',
    [string]$OriginalSplitsDir = 'original_splits',
    [string]$BaseDecompiledDir = 'base_decompiled',
    [switch]$SkipAssets,
    [switch]$SkipBaseDecompile,
    [switch]$AllowMissingSplits
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

function Resolve-InputPath([string]$root, [string]$p) {
    if (-not $p) { throw "InputApkOrApks não informado" }
    $candidate = Join-Path $root $p
    if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    if (Test-Path $p) { return (Resolve-Path $p).Path }
    if ($p -eq 'base.apk') {
        $fallback = Join-Path $root 'original_splits\base.apk'
        if (Test-Path $fallback) { return (Resolve-Path $fallback).Path }
    }
    throw "Arquivo não encontrado: $p"
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

function Reset-Dir([string]$path) {
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}

function Get-FullPathSafe([string]$p) {
    try { return [System.IO.Path]::GetFullPath($p) } catch { return $p }
}

function Select-BaseApkCandidate($apks) {
    if (-not $apks -or $apks.Count -le 0) { return $null }
    $c = $apks | Where-Object { $_.Name -ieq 'base.apk' } | Select-Object -First 1
    if ($c) { return $c }
    $c = $apks | Where-Object { $_.Name -ieq 'base-master.apk' } | Select-Object -First 1
    if ($c) { return $c }
    $c = $apks | Where-Object { $_.Name -like 'base-master*.apk' } | Sort-Object Name | Select-Object -First 1
    if ($c) { return $c }
    $c = $apks | Where-Object { $_.Name -like 'base-*.apk' } | Sort-Object Name | Select-Object -First 1
    if ($c) { return $c }
    $c = $apks | Where-Object { $_.Name -like 'base*.apk' } | Sort-Object Name | Select-Object -First 1
    if ($c) { return $c }
    return ($apks | Sort-Object Name | Select-Object -First 1)
}

function Flatten-Apks([string]$sourceDir, [string]$destDir) {
    $apkCandidates = Get-ChildItem -Path $sourceDir -Recurse -File -Filter '*.apk' -ErrorAction SilentlyContinue
    foreach ($a in $apkCandidates) {
        $dst = Join-Path $destDir $a.Name
        if (Test-Path $dst) { continue }
        Copy-Item -LiteralPath $a.FullName -Destination $dst -Force
    }
}

function Prepare-OriginalSplits([string]$inputPath, [string]$outDir) {
    $tmpInput = $null
    if (Test-Path $inputPath) {
        $inFull = Get-FullPathSafe $inputPath
        $outFull = Get-FullPathSafe $outDir
        if ($inFull.StartsWith($outFull, [System.StringComparison]::OrdinalIgnoreCase)) {
            $tmpInput = Join-Path $env:TEMP ("prepare_base_" + [guid]::NewGuid().ToString() + [System.IO.Path]::GetExtension($inputPath))
            Copy-Item -LiteralPath $inputPath -Destination $tmpInput -Force
            $inputPath = $tmpInput
        }
    }

    Reset-Dir $outDir
    if (Test-Path $inputPath -PathType Container) {
        $apks = Get-ChildItem -Path $inputPath -File -Filter '*.apk' -ErrorAction SilentlyContinue
        if (-not $apks -or $apks.Count -le 0) { throw "Nenhum *.apk encontrado em: $inputPath" }
        foreach ($a in $apks) {
            Copy-Item -LiteralPath $a.FullName -Destination (Join-Path $outDir $a.Name) -Force
        }
        $baseCandidate = Select-BaseApkCandidate $apks
        if (-not $baseCandidate) { throw "Não foi possível identificar base.apk em: $inputPath" }
        Copy-Item -LiteralPath $baseCandidate.FullName -Destination (Join-Path $outDir 'base.apk') -Force
        $res = (Resolve-Path (Join-Path $outDir 'base.apk')).Path
        if ($tmpInput -and (Test-Path $tmpInput)) { Remove-Item -Force $tmpInput }
        return $res
    }

    $ext = [System.IO.Path]::GetExtension($inputPath).ToLowerInvariant()
    if ($ext -in @('.apks', '.xapk', '.zip')) {
        $tmp = Join-Path $outDir '_archive'
        New-Item -ItemType Directory -Force -Path $tmp | Out-Null
        Expand-Archive -LiteralPath $inputPath -DestinationPath $tmp -Force
        Flatten-Apks -sourceDir $tmp -destDir $outDir
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }

        $flatApks = Get-ChildItem -Path $outDir -File -Filter '*.apk' -ErrorAction SilentlyContinue
        $baseCandidate = Select-BaseApkCandidate $flatApks
        if (-not $baseCandidate) { throw "nenhum APK encontrado dentro de $inputPath" }
        Copy-Item -LiteralPath $baseCandidate.FullName -Destination (Join-Path $outDir 'base.apk') -Force
        $res = (Resolve-Path (Join-Path $outDir 'base.apk')).Path
        if ($tmpInput -and (Test-Path $tmpInput)) { Remove-Item -Force $tmpInput }
        return $res
    }

    $dstBase = Join-Path $outDir 'base.apk'
    Copy-Item -LiteralPath $inputPath -Destination $dstBase -Force
    $leaf = Split-Path $inputPath -Leaf
    if ($leaf -and ($leaf -ne 'base.apk')) {
        Copy-Item -LiteralPath $inputPath -Destination (Join-Path $outDir $leaf) -Force
    }
    $parent = Split-Path $inputPath -Parent
    $apks = Get-ChildItem -Path $parent -File -Filter '*.apk' -ErrorAction SilentlyContinue
    foreach ($a in $apks) {
        $dst = Join-Path $outDir $a.Name
        if ($a.FullName -ne $inputPath -and -not (Test-Path $dst)) {
            Copy-Item -LiteralPath $a.FullName -Destination $dst -Force
        }
    }
    $res = (Resolve-Path $dstBase).Path
    if ($tmpInput -and (Test-Path $tmpInput)) { Remove-Item -Force $tmpInput }
    return $res
}

function Export-SplitsFromDevice([string]$packageName, [string]$outDir, [string]$adbPath) {
    if (-not $packageName) { throw "PackageName não informado" }
    $adb = Resolve-Adb $adbPath
    Reset-Dir $outDir

    $lines = & $adb shell pm path $packageName 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $lines) { throw "Falha ao consultar APKs instalados. Verifique adb devices e se o app está instalado: $packageName" }

    $remotePaths = @()
    foreach ($l in $lines) {
        $m = [regex]::Match($l, '^package:(.+)$')
        if ($m.Success) { $remotePaths += $m.Groups[1].Value.Trim() }
    }
    if ($remotePaths.Count -le 0) { throw "Nenhum caminho retornado por pm path para: $packageName" }

    $baseRemote = $remotePaths | Where-Object { $_.EndsWith('/base.apk') } | Select-Object -First 1
    if (-not $baseRemote) { $baseRemote = $remotePaths | Select-Object -First 1 }

    foreach ($rp in $remotePaths) {
        $name = Split-Path $rp -Leaf
        $dst = Join-Path $outDir $name
        & $adb pull $rp $dst | Out-Null
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dst)) {
            throw "Falha ao fazer pull de $rp. Se der permissão negada, exporte os splits via app (ex: SAI) e use o .apks/.xapk como entrada."
        }
    }

    Copy-Item -Force (Join-Path $outDir (Split-Path $baseRemote -Leaf)) -Destination (Join-Path $outDir 'base.apk')
    return (Resolve-Path (Join-Path $outDir 'base.apk')).Path
}

function Extract-AssetsFromApk([string]$apkPath, [string]$outAssetsDir) {
    Reset-Dir $outAssetsDir
    Add-Type -AssemblyName System.IO.Compression.FileSystem | Out-Null
    $zip = [System.IO.Compression.ZipFile]::OpenRead($apkPath)
    try {
        foreach ($e in $zip.Entries) {
            $name = $e.FullName
            if (-not $name.StartsWith('assets/')) { continue }
            if ($name.EndsWith('/')) { continue }
            $dest = Join-Path $outAssetsDir ($name -replace '/', '\')
            $destDir = Split-Path $dest -Parent
            if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Force -Path $destDir | Out-Null }
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest, $true)
        }
    }
    finally {
        $zip.Dispose()
    }
}

function Decompile-BaseApk([string]$apkPath, [string]$outDir) {
    Reset-Dir $outDir
    Invoke-Apktool -ApktoolArgs @('d', '-f', $apkPath, '-o', $outDir)
}

function Test-BaseRequiresSplits([string]$baseDecDir) {
    $manifestPath = Join-Path $baseDecDir 'AndroidManifest.xml'
    if (-not (Test-Path $manifestPath)) { return $false }
    $xml = Get-Content -Raw -Encoding UTF8 $manifestPath
    return ($xml -match 'requiredSplitTypes="' -or $xml -match 'com\.android\.vending\.splits\.required' -or $xml -match '<uses-split\b' -or $xml -match 'isSplitRequired="true"')
}

function Count-OriginalSplitApks([string]$originalSplitsDir) {
    if (-not (Test-Path $originalSplitsDir)) { return 0 }
    $apks = Get-ChildItem -Path $originalSplitsDir -File -Filter '*.apk' -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'base.apk' }
    if (-not $apks) { return 0 }
    return $apks.Count
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

$root = Get-ProjectRoot
$originalSplitsAbs = Join-Path $root $OriginalSplitsDir
$assetsAbs = Join-Path $root $AssetsDir
$baseDecAbs = Join-Path $root $BaseDecompiledDir

Write-Host "Preparando splits originais em: $originalSplitsAbs"
$usedAdb = $false
try {
    $inputAbs = Resolve-InputPath $root $InputApkOrApks
    Write-Host "Input: $inputAbs"
    $baseApkAbs = Prepare-OriginalSplits -inputPath $inputAbs -outDir $originalSplitsAbs
}
catch {
    if (-not $PackageName) { throw }
    Write-Host "Input falhou. Tentando via ADB (package = $PackageName)..."
    $baseApkAbs = Export-SplitsFromDevice -packageName $PackageName -outDir $originalSplitsAbs -adbPath $AdbPath
    $usedAdb = $true
}
Write-Host "Base usado: $baseApkAbs"

if (-not $SkipAssets) {
    Write-Host "Extraindo assets para: $assetsAbs"
    Extract-AssetsFromApk -apkPath $baseApkAbs -outAssetsDir $assetsAbs
}

if (-not $SkipBaseDecompile) {
    Write-Host "Descompilando base.apk para: $baseDecAbs"
    Decompile-BaseApk -apkPath $baseApkAbs -outDir $baseDecAbs
    Write-Host "Patch assinatura (smali)..."
    $sigPatched = Patch-SignatureChecksSmali -baseDecDir $baseDecAbs
    Write-Host "Assinatura: metodos patched = $sigPatched"

    $splitsRequired = Test-BaseRequiresSplits $baseDecAbs
    if ($splitsRequired) {
        $splitCount = Count-OriginalSplitApks $originalSplitsAbs
        if ($splitCount -le 0 -and -not $usedAdb -and $PackageName) {
            Write-Host "Splits ausentes. Tentando obter splits via ADB (package = $PackageName)..."
            $baseApkAbs = Export-SplitsFromDevice -packageName $PackageName -outDir $originalSplitsAbs -adbPath $AdbPath
            $usedAdb = $true
            Write-Host "Base usado: $baseApkAbs"

            if (-not $SkipAssets) {
                Write-Host "Extraindo assets para: $assetsAbs"
                Extract-AssetsFromApk -apkPath $baseApkAbs -outAssetsDir $assetsAbs
            }

            Write-Host "Descompilando base.apk para: $baseDecAbs"
            Decompile-BaseApk -apkPath $baseApkAbs -outDir $baseDecAbs
            Write-Host "Patch assinatura (smali)..."
            $sigPatched = Patch-SignatureChecksSmali -baseDecDir $baseDecAbs
            Write-Host "Assinatura: metodos patched = $sigPatched"

            $splitCount = Count-OriginalSplitApks $originalSplitsAbs
        }

        if ($splitCount -le 0 -and -not $AllowMissingSplits) {
            throw "Este base.apk exige splits, mas nenhum split foi encontrado em $originalSplitsAbs. Use como entrada um .apks/.xapk (bundle completo), uma pasta com base.apk + split_config.*.apk, ou ADB (PackageName)."
        }
    }
}

Write-Host "OK"
