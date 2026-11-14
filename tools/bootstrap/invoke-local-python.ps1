<#
.SYNOPSIS
Runs a WSE Python entry point with an installed or verified cached CPython runtime.

.DESCRIPTION
Detects an installed host-native Python first. When one is unavailable, the
bootstrap manifest supplies a verified local fallback. Bootstrap offline and
read-only options apply before provisioning that fallback too.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$ScriptPath,

    [switch]$WseBatchArguments,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ScriptArguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Windows PowerShell otherwise decodes native output with the active OEM code page.
# Keep both Python's redirected streams and PowerShell's native bridge in UTF-8.
$utf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"

function Get-BatchScriptArguments {
    # -File receives C-runtime-parsed argv before this script starts. That parser
    # treats the final backslash in a quoted cmd directory as an escaped quote,
    # potentially swallowing --offline. Recover the original batch tail instead.
    # cmd quoting uses ordinary backslashes and doubled quotes within a quoted
    # argument; no shell is started to interpret the recovered text.
    $line = [Environment]::CommandLine
    $tokens = New-Object 'System.Collections.Generic.List[string]'
    $index = 0
    while ($index -lt $line.Length) {
        while ($index -lt $line.Length -and [char]::IsWhiteSpace($line[$index])) { $index++ }
        if ($index -ge $line.Length) { break }
        $token = New-Object System.Text.StringBuilder
        $quoted = $false
        while ($index -lt $line.Length) {
            $character = $line[$index]
            if ($character -eq '"') {
                if ($quoted -and $index + 1 -lt $line.Length -and $line[$index + 1] -eq '"') {
                    $null = $token.Append('"')
                    $index += 2
                    continue
                }
                $quoted = -not $quoted
            }
            elseif (-not $quoted -and [char]::IsWhiteSpace($character)) { break }
            else { $null = $token.Append($character) }
            $index++
        }
        if ($quoted) { throw "Unterminated quote in batch arguments. Double an embedded literal quote inside a quoted argument." }
        $tokens.Add($token.ToString())
    }
    $marker = $tokens.IndexOf("-WseBatchArguments")
    if ($marker -lt 0) { throw "The batch argument marker is missing from the original command line." }
    return ,$tokens.GetRange($marker + 1, $tokens.Count - $marker - 1).ToArray()
}

function Get-HostArchitecture {
    $native = $env:PROCESSOR_ARCHITEW6432
    if (-not $native) { $native = $env:PROCESSOR_ARCHITECTURE }
    switch ($native) {
        "AMD64" { return "x86_64" }
        "ARM64" { return "arm64" }
        default { throw "Unsupported Windows host architecture: $native. A 64-bit host is required." }
    }
}

function Test-PythonExecutable {
    param([string]$Executable, [string[]]$PrefixArguments = @())
    # ASCII JSON transports Unicode executable paths without depending on the probe's code page.
    $probe = 'import json,sys,sysconfig; print(json.dumps(dict(executable=sys.executable,version=list(sys.version_info[:3]),platform=sysconfig.get_platform(),implementation=sys.implementation.name)))'
    try {
        $output = & $Executable @PrefixArguments -c $probe 2>$null
        if ($LASTEXITCODE -ne 0) { return $null }
        $identity = ($output -join "`n") | ConvertFrom-Json
        $expectedPlatform = if ($architecture -eq "arm64") { "win-arm64" } else { "win-amd64" }
        if ($identity.implementation -ne "cpython" -or $identity.platform -ne $expectedPlatform -or
            $identity.version[0] -ne 3 -or $identity.version[1] -lt 8 -or
            -not (Test-Path -LiteralPath $identity.executable -PathType Leaf)) {
            return $null
        }
        return $identity
    }
    catch { return $null }
}

function Find-InstalledPython {
    if ($env:WSE_PYTHON_HOME) {
        $identity = Test-PythonExecutable (Join-Path $env:WSE_PYTHON_HOME "python.exe")
        if ($null -ne $identity) { return $identity.executable }
    }
    foreach ($name in @("python.exe", "python3.exe", "py.exe")) {
        foreach ($command in @(Get-Command $name -CommandType Application -All -ErrorAction SilentlyContinue)) {
            # An App Execution Alias can open the Store instead of starting an interpreter.
            if ($command.Source -match '[\\/]Microsoft[\\/]WindowsApps[\\/]') { continue }
            $prefix = if ($name -eq "py.exe") { @("-3") } else { @() }
            $identity = Test-PythonExecutable $command.Source $prefix
            if ($null -ne $identity) { return $identity.executable }
        }
    }
    return $null
}

function Invoke-PythonScript {
    param([string]$Executable)
    # PowerShell 5.1's native argument binder loses empty arguments and quotes,
    # including the trailing slash in a quoted Windows directory. Quote each
    # argument using the Windows C runtime rules and start the process directly.
    $quoted = foreach ($value in (@($ScriptPath) + $ScriptArguments)) {
        $escaped = [regex]::Replace($value, '(\\*)"', '$1$1\"')
        $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
        '"' + $escaped + '"'
    }
    $process = New-Object System.Diagnostics.Process
    try {
        $process.StartInfo.FileName = $Executable
        $process.StartInfo.Arguments = $quoted -join ' '
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        # Drain both pipes concurrently. Copy bytes to preserve UTF-8 without
        # turning native stderr into a PowerShell error record or buffering a build.
        $null = $process.Start()
        $outputTask = $process.StandardOutput.BaseStream.CopyToAsync([Console]::OpenStandardOutput())
        $errorTask = $process.StandardError.BaseStream.CopyToAsync([Console]::OpenStandardError())
        $process.WaitForExit()
        $null = $outputTask.GetAwaiter().GetResult()
        $null = $errorTask.GetAwaiter().GetResult()
        return $process.ExitCode
    }
    finally { $process.Dispose() }
}

function Assert-CacheChild {
    param([string]$Path)
    $prefix = [IO.Path]::GetFullPath($cacheRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not [IO.Path]::GetFullPath($Path).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime cache path escapes the selected cache: $Path"
    }
}

function Get-Sha512Hex {
    param([Parameter(Mandatory = $true)][string]$Path)
    # Hash with .NET directly: Get-FileHash can be unresolvable when the caller's PSModulePath
    # omits the Windows PowerShell module directories, which a CI runner's environment does.
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha512 = [System.Security.Cryptography.SHA512]::Create()
        try {
            return ([System.BitConverter]::ToString($sha512.ComputeHash($stream)) -replace '-', '').ToLowerInvariant()
        }
        finally {
            $sha512.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Test-Runtime {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeRoot,
        [Parameter(Mandatory = $true)][object]$Runtime
    )
    foreach ($requiredFile in @($Runtime.required_files)) {
        if (-not (Test-Path -LiteralPath (Join-Path $RuntimeRoot $requiredFile) -PathType Leaf)) {
            return $false
        }
    }
    $identity = Test-PythonExecutable (Join-Path $RuntimeRoot "python.exe")
    return $null -ne $identity -and ($identity.version -join ".") -eq $cpython.version
}

function Expand-PythonArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$StagingRoot,
        [int]$Attempts = 5
    )
    Assert-CacheChild $StagingRoot
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        try {
            if (Test-Path -LiteralPath $StagingRoot) {
                Remove-Item -LiteralPath $StagingRoot -Recurse -Force -ErrorAction Stop
            }
            New-Item -ItemType Directory -Force -Path $StagingRoot | Out-Null
            [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $StagingRoot)
            return
        }
        catch {
            $isIoError = $_.Exception -is [System.IO.IOException] -or
                $_.Exception.InnerException -is [System.IO.IOException]
            if (-not $isIoError) {
                throw
            }
            if ($attempt -eq $Attempts) {
                throw
            }
            Start-Sleep -Milliseconds (500 * $attempt)
        }
    }
}

$lock = $null
$stagingRoot = $null
$downloadPath = $null
try {
    if ($WseBatchArguments) { $ScriptArguments = Get-BatchScriptArguments }
    $wseRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
    $architecture = Get-HostArchitecture
    $python = Find-InstalledPython
    if ($python) {
        exit (Invoke-PythonScript $python)
    }

    $manifestPath = Join-Path $wseRoot "bootstrap-manifest.json"
    $cacheRoot = Join-Path $wseRoot ".bootstrap-cache"
    $allowFetch = $true
    $allowInstall = $true
    # Other entry points (e.g. Doxygen --check) own their own flags. Interpret
    # bootstrap options only when starting the repository's bootstrap.py.
    if ([IO.Path]::GetFullPath($ScriptPath) -eq (Join-Path $wseRoot "bootstrap.py")) {
        for ($index = 0; $index -lt $ScriptArguments.Count; $index++) {
            $argument = $ScriptArguments[$index]
            if ($argument -eq "--") { break }
            switch -Regex ($argument) {
                '^--(offline|no-fetch-toolchain)$' { $allowFetch = $false; break }
                '^--(check|verify-cache)$' { $allowFetch = $false; $allowInstall = $false; break }
                '^--(cache-dir|manifest)(=(.*))?$' {
                    $option = $Matches[1]
                    if ($Matches.ContainsKey(3)) { $value = $Matches[3] }
                    else {
                        $index++
                        if ($index -ge $ScriptArguments.Count) { throw "Missing value for --$option." }
                        $value = $ScriptArguments[$index]
                    }
                    if ($option -eq "cache-dir") { $cacheRoot = [IO.Path]::GetFullPath($value) }
                    else { $manifestPath = [IO.Path]::GetFullPath($value) }
                    break
                }
            }
        }
    }

    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $cpythonPins = @()
    if ($manifest.PSObject.Properties["toolchains"]) {
        $cpythonPins = @($manifest.toolchains | Where-Object { $_.name -eq "cpython" })
    }
    # Package-only custom manifests predate verification toolchains. Keep their
    # dependency selection while using the repository's launch-runtime pin.
    if ($cpythonPins.Count -eq 0 -and $manifestPath -ne (Join-Path $wseRoot "bootstrap-manifest.json")) {
        $launchManifest = Get-Content -LiteralPath (Join-Path $wseRoot "bootstrap-manifest.json") -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($launchManifest.PSObject.Properties["toolchains"]) {
            $cpythonPins = @($launchManifest.toolchains | Where-Object { $_.name -eq "cpython" })
        }
    }
    if ($cpythonPins.Count -ne 1) {
        throw "No usable Python 3.8+ was found; the selected manifest must pin one cpython toolchain. Set WSE_PYTHON_HOME to an installed Python."
    }
    $cpython = $cpythonPins[0]
    $runtime = $cpython.platforms.windows.architectures.$architecture
    if ($null -eq $runtime) { throw "The selected CPython pin has no Windows $architecture archive." }
    if ($cpython.version -notmatch '^\d+\.\d+\.\d+$' -or
        $runtime.sha512 -notmatch '^[0-9a-fA-F]{128}$' -or
        [IO.Path]::GetFileName($runtime.archive_name) -ne $runtime.archive_name) {
        throw "The selected CPython pin must have a version, archive filename and published SHA-512 digest."
    }
    $archivePath = Join-Path $cacheRoot $runtime.archive_name
    $runtimeRoot = Join-Path $cacheRoot "toolchains\cpython\windows-$architecture\$($cpython.version)"
    Assert-CacheChild $archivePath
    Assert-CacheChild $runtimeRoot
    # Reuse complete older cache layouts, but check their actual version/host identity first.
    foreach ($candidate in @($runtimeRoot, (Join-Path $cacheRoot "toolchains\cpython\tools"), (Join-Path $cacheRoot "toolchains\cpython"))) {
        if (Test-Runtime -RuntimeRoot $candidate -Runtime $runtime) {
            exit (Invoke-PythonScript (Join-Path $candidate "python.exe"))
        }
    }
    if (-not $allowInstall -or (-not $allowFetch -and -not (Test-Path -LiteralPath $archivePath -PathType Leaf))) {
        throw "No usable Python 3.8+ or prepared CPython $($cpython.version) runtime was found. --offline/--no-fetch-toolchain/--check/--verify-cache prohibit the required provisioning. Set WSE_PYTHON_HOME, or prepare the selected cache while online: $cacheRoot"
    }

    New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
    $lockPath = Join-Path $cacheRoot "cpython-windows-$architecture.lock"
    for ($attempt = 0; $attempt -lt 120; $attempt++) {
        try {
            $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
            break
        }
        catch [IO.IOException] { Start-Sleep -Milliseconds 250 }
    }
    if ($null -eq $lock) { throw "Another launcher is preparing CPython in $cacheRoot. Retry after it finishes." }

    # Another launcher may have completed while this one waited for the cache lock.
    if (-not (Test-Runtime -RuntimeRoot $runtimeRoot -Runtime $runtime)) {
        if (Test-Path -LiteralPath $runtimeRoot) {
            throw "Cached CPython has an incomplete or incompatible identity: $runtimeRoot. Remove only that runtime directory and retry."
        }
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
            $downloadPath = "$archivePath.$([guid]::NewGuid().ToString('N')).download.tmp"
            Write-Host "[wse-bootstrap] Downloading pinned CPython $($cpython.version) for Windows $architecture."
            # TLS 1.2 is required by NuGet on older Windows PowerShell installations.
            [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -UseBasicParsing -Uri $runtime.url -OutFile $downloadPath -TimeoutSec 120
            if ((Get-Sha512Hex $downloadPath) -ne $runtime.sha512.ToLowerInvariant()) {
                throw "Pinned CPython archive SHA-512 verification failed: $archivePath"
            }
            Move-Item -LiteralPath $downloadPath -Destination $archivePath
        }
        if ((Get-Sha512Hex $archivePath) -ne $runtime.sha512.ToLowerInvariant()) {
            throw "Pinned CPython archive SHA-512 verification failed: $archivePath. Replace only this archive with the verified pin."
        }
        $stagingRoot = Join-Path $cacheRoot "toolchains\cpython.staging.$([guid]::NewGuid().ToString('N'))"
        Expand-PythonArchive -ArchivePath $archivePath -StagingRoot $stagingRoot
        $stagedRuntimeRoot = [IO.Path]::GetFullPath((Join-Path $stagingRoot $runtime.extracted_root))
        Assert-CacheChild $stagedRuntimeRoot
        if (-not $stagedRuntimeRoot.StartsWith($stagingRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "The pinned CPython extraction root escapes its staging directory."
        }
        if (-not (Test-Runtime -RuntimeRoot $stagedRuntimeRoot -Runtime $runtime)) {
            throw "Pinned CPython archive is incomplete or has the wrong version/host architecture."
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $runtimeRoot) | Out-Null
        Move-Item -LiteralPath $stagedRuntimeRoot -Destination $runtimeRoot
    }
    $lock.Dispose()
    $lock = $null
    exit (Invoke-PythonScript (Join-Path $runtimeRoot "python.exe"))
}
catch {
    [Console]::Error.WriteLine("[wse-bootstrap] " + $_.Exception.Message)
    exit 1
}
finally {
    if ($null -ne $lock) { $lock.Dispose() }
    if ($downloadPath -and (Test-Path -LiteralPath $downloadPath)) {
        Assert-CacheChild $downloadPath
        Remove-Item -LiteralPath $downloadPath -Force
    }
    if ($stagingRoot -and (Test-Path -LiteralPath $stagingRoot)) {
        Assert-CacheChild $stagingRoot
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
