param(
    [string]$Version = "1.0.0",
    [switch]$SkipBridge,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $Root "winui\Aegisub.WinUI\Aegisub.WinUI\Aegisub.WinUI.vcxproj"
$BridgeBuild = Join-Path $Root "build-srtune-release"
$BridgeExe = Join-Path $BridgeBuild "src\aegisub-winui-bridge.exe"
$Dist = Join-Path $Root "dist"
$Stage = Join-Path $Dist ("SRTune-" + $Version + "-x64")
$Portable = Join-Path $Dist ("SRTune-" + $Version + "-portable-x64.zip")

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found in PATH."
    }
}

Require-Command "msbuild"

if (-not $SkipBridge) {
    Require-Command "meson"
    Require-Command "ninja"

    if (-not (Test-Path (Join-Path $BridgeBuild "meson-private\coredata.dat"))) {
        meson setup $BridgeBuild $Root --buildtype=release -Ddefault_library=static
    }

    meson compile -C $BridgeBuild aegisub-winui-bridge
}

if (-not (Test-Path $BridgeExe)) {
    $FallbackBridge = Join-Path $Root "build-x64\src\aegisub-winui-bridge.exe"
    if (Test-Path $FallbackBridge) {
        $BridgeExe = $FallbackBridge
    } else {
        throw "aegisub-winui-bridge.exe was not found. Build the bridge first or omit -SkipBridge."
    }
}

New-Item -ItemType Directory -Force -Path $Dist | Out-Null
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

$OutDir = $Stage.TrimEnd("\") + "\"

msbuild $Project /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:WindowsPackageType=None /p:WindowsAppSDKSelfContained=true /p:AppxPackage=false /p:OutDir="$OutDir" /p:WinUiBridgePath="$BridgeExe" /v:minimal

$AppExe = Join-Path $Stage "SRTune.exe"
if (-not (Test-Path $AppExe)) {
    throw "Release build finished without SRTune.exe in $Stage"
}

Copy-Item -Force (Join-Path $Root "LICENCE") (Join-Path $Stage "LICENCE.txt")
Copy-Item -Force (Join-Path $Root "README.md") (Join-Path $Stage "README.md")

$FfmpegNotice = Join-Path $Root "winui\Aegisub.WinUI\Aegisub.WinUI\External\FFMPEG-NOTICE.txt"
if (Test-Path $FfmpegNotice) {
    Copy-Item -Force $FfmpegNotice (Join-Path $Stage "FFMPEG-NOTICE.txt")
}

if (Test-Path $Portable) { Remove-Item -Force $Portable }
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $Portable -CompressionLevel Optimal

Write-Host ""
Write-Host "[SRTune] Portable package:"
Write-Host "  $Portable"

if (-not $SkipInstaller) {
    $Iscc = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if (-not $Iscc) {
        $ProgramFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
        $Candidates = @(
            (Join-Path $ProgramFilesX86 "Inno Setup 6\ISCC.exe"),
            (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
        )
        foreach ($Candidate in $Candidates) {
            if ($Candidate -and (Test-Path $Candidate)) {
                $Iscc = Get-Item $Candidate
                break
            }
        }
    }

    if ($Iscc) {
        $InstallerScript = Join-Path $Root "installer\SRTune.iss"
        & $Iscc.FullName "/DMyAppVersion=$Version" "/DSourceDir=$Stage" $InstallerScript
        if ($LASTEXITCODE -ne 0) {
            throw "Inno Setup failed with exit code $LASTEXITCODE."
        }
    } else {
        Write-Warning "Inno Setup was not found. Portable ZIP was created; installer was skipped."
    }
}

Write-Host ""
Write-Host "[SRTune] Release build completed."
