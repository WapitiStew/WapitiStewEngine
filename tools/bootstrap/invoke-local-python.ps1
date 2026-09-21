<#
.SYNOPSIS
Runs a WSE Python entry point with WSE's pinned local CPython runtime.

.DESCRIPTION
This launcher is self-contained for a standalone WSE checkout. It reads the
CPython Windows pin from bootstrap-manifest.json, verifies the downloaded
archive, and runs the requested script without consulting py.exe or PATH.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$ScriptPath,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ScriptArguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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
    return $true
}

$wseRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$manifestPath = Join-Path $wseRoot "bootstrap-manifest.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$cpython = @($manifest.toolchains | Where-Object { $_.name -eq "cpython" })
if ($cpython.Count -ne 1) {
    throw "WSE bootstrap manifest must contain exactly one cpython toolchain."
}

$architecture = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { "x86" }
$runtime = $cpython[0].platforms.windows.architectures.$architecture
if ($null -eq $runtime) {
    throw "The pinned CPython runtime does not support Windows architecture: $architecture."
}

$cacheRoot = Join-Path $wseRoot ".bootstrap-cache"
$archivePath = Join-Path $cacheRoot $runtime.archive_name
$runtimeRoot = Join-Path $cacheRoot "toolchains\cpython\$($runtime.extracted_root)"
if (-not (Test-Runtime -RuntimeRoot $runtimeRoot -Runtime $runtime)) {
    New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        Write-Host "[wse-bootstrap] Downloading pinned CPython $($cpython[0].version)."
        Invoke-WebRequest -Uri $runtime.url -OutFile $archivePath
    }
    if ((Get-Sha512Hex -Path $archivePath) -ne $runtime.sha512.ToLowerInvariant()) {
        Remove-Item -LiteralPath $archivePath -Force -ErrorAction SilentlyContinue
        throw "Pinned CPython archive SHA-512 verification failed: $archivePath"
    }

    $stagingRoot = Join-Path $cacheRoot "toolchains\cpython.staging.$([guid]::NewGuid().ToString('N'))"
    try {
        New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($archivePath, $stagingRoot)
        $stagedRuntimeRoot = Join-Path $stagingRoot $runtime.extracted_root
        if (-not (Test-Runtime -RuntimeRoot $stagedRuntimeRoot -Runtime $runtime)) {
            throw "Pinned CPython archive is missing required runtime files."
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $runtimeRoot) | Out-Null
        if (Test-Path -LiteralPath $runtimeRoot) {
            Remove-Item -LiteralPath $runtimeRoot -Recurse -Force
        }
        Move-Item -LiteralPath $stagedRuntimeRoot -Destination $runtimeRoot
    }
    finally {
        if (Test-Path -LiteralPath $stagingRoot) {
            Remove-Item -LiteralPath $stagingRoot -Recurse -Force
        }
    }
}

& (Join-Path $runtimeRoot "python.exe") $ScriptPath @ScriptArguments
exit $LASTEXITCODE