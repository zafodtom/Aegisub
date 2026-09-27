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

function Wait-FileReadable([string]$Path, [int]$TimeoutSeconds = 30) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $stream = [System.IO.File]::Open(
                $Path,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite
            )
            $stream.Dispose()
            return
        }
        catch {
            Start-Sleep -Milliseconds 500
        }
    }

    throw "Soubor je stále zamčený a nelze jej zabalit: $Path"
}

function New-PortableZip([string]$SourceDirectory, [string]$DestinationZip) {
    $files = Get-ChildItem -Path $SourceDirectory -File -Recurse
    foreach ($file in $files) {
        Wait-FileReadable -Path $file.FullName -TimeoutSeconds 30
    }

    if (Test-Path $DestinationZip) {
        Remove-Item -Force $DestinationZip
    }

    $lastError = $null
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        try {
            Compress-Archive -Path (Join-Path $SourceDirectory "*") -DestinationPath $DestinationZip -CompressionLevel Optimal -ErrorAction Stop

            if (-not (Test-Path $DestinationZip)) {
                throw "ZIP soubor po kompresi nevznikl."
            }

            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $archive = [System.IO.Compression.ZipFile]::OpenRead($DestinationZip)
            try {
                if ($archive.Entries.Count -eq 0) {
                    throw "ZIP archiv je prázdný."
                }
            }
            finally {
                $archive.Dispose()
            }

            return
        }
        catch {
            $lastError = $_
            if (Test-Path $DestinationZip) {
                Remove-Item -Force $DestinationZip -ErrorAction SilentlyContinue
            }
            if ($attempt -lt 5) {
                Write-Warning ("ZIP je dočasně blokovaný, opakuji pokus {0}/5..." -f ($attempt + 1))
                Start-Sleep -Seconds 2
            }
        }
    }

    throw "Portable ZIP se nepodařilo vytvořit ani po opakování: $lastError"
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

New-PortableZip -SourceDirectory $Stage -DestinationZip $Portable

Write-Host ""
Write-Host "[SRTune] Portable package:"
Write-Host "  $Portable"

if (-not $SkipInstaller) {
    $Iscc = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if (-not $Iscc) {
        $ProgramFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
        $Candidates = @(
            (Join-Path $ProgramFilesX86 "Inno Setup 6\ISCC.exe"),
            (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
            (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
            (Join-Path $env:LOCALAPPDATA "Inno Setup 6\ISCC.exe")
        )

        foreach ($Candidate in $Candidates) {
            if ($Candidate -and (Test-Path $Candidate)) {
                $Iscc = Get-Item $Candidate
                break
            }
        }
    }

    if (-not $Iscc) {
        $UninstallRoots = @(
            "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
            "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
            "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
        )

        foreach ($RootKey in $UninstallRoots) {
            $Entries = Get-ItemProperty $RootKey -ErrorAction SilentlyContinue |
                Where-Object { $_.DisplayName -like "Inno Setup*" }

            foreach ($Entry in $Entries) {
                if ($Entry.InstallLocation) {
                    $Candidate = Join-Path $Entry.InstallLocation "ISCC.exe"
                    if (Test-Path $Candidate) {
                        $Iscc = Get-Item $Candidate
                        break
                    }
                }

                if ($Entry.UninstallString) {
                    $UninstallExe = $Entry.UninstallString.Trim('"').Split('"')[0]
                    $InstallDir = Split-Path -Parent $UninstallExe
                    if ($InstallDir) {
                        $Candidate = Join-Path $InstallDir "ISCC.exe"
                        if (Test-Path $Candidate) {
                            $Iscc = Get-Item $Candidate
                            break
                        }
                    }
                }
            }

            if ($Iscc) { break }
        }
    }

    if ($Iscc) {
        $InstallerScript = Join-Path $Root "installer\SRTune.iss"
        $InstallerFinalBaseName = "SRTune-" + $Version + "-Setup-x64"
        $InstallerFinalExe = Join-Path $Dist ($InstallerFinalBaseName + ".exe")
        $InstallerBuiltExe = $null
        $InstallerBuildError = $null

        for ($buildAttempt = 1; $buildAttempt -le 5; $buildAttempt++) {
            $AttemptId = [Guid]::NewGuid().ToString("N")
            $InstallerTempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("srtune-installer-" + $AttemptId)
            $InstallerTempBaseName = "setup-" + $AttemptId
            $InstallerTempExe = Join-Path $InstallerTempDir ($InstallerTempBaseName + ".exe")

            New-Item -ItemType Directory -Force -Path $InstallerTempDir | Out-Null

            try {
                Write-Host ("[SRTune] Inno Setup pokus {0}/5..." -f $buildAttempt)

                $IsccArgs = @(
                    "/DMyAppVersion=$Version",
                    "/DSourceDir=$Stage",
                    "/DOutputDir=$InstallerTempDir",
                    "/DOutputBaseFilename=$InstallerTempBaseName",
                    $InstallerScript
                )

                & $Iscc.FullName $IsccArgs
                $InnoExitCode = $LASTEXITCODE

                if ($InnoExitCode -eq 0 -and (Test-Path $InstallerTempExe)) {
                    Wait-FileReadable -Path $InstallerTempExe -TimeoutSeconds 30
                    $InstallerBuiltExe = $InstallerTempExe
                    break
                }

                $InstallerBuildError = "Inno Setup failed with exit code $InnoExitCode."
            }
            catch {
                $InstallerBuildError = $_.Exception.Message
            }

            if ($buildAttempt -lt 5) {
                Write-Warning ("Inno Setup selhal, opakuji celý build s novým názvem za 3 s. Důvod: {0}" -f $InstallerBuildError)
                Start-Sleep -Seconds 3
            }

            if (Test-Path $InstallerTempDir) {
                Remove-Item -Recurse -Force $InstallerTempDir -ErrorAction SilentlyContinue
            }
        }

        if (-not $InstallerBuiltExe) {
            throw "Inno Setup selhal ve všech 5 pokusech. Poslední chyba: $InstallerBuildError"
        }

        if (Test-Path $InstallerFinalExe) {
            $removed = $false
            for ($attempt = 1; $attempt -le 10; $attempt++) {
                try {
                    Remove-Item -Force $InstallerFinalExe -ErrorAction Stop
                    $removed = $true
                    break
                }
                catch {
                    if ($attempt -lt 10) {
                        Start-Sleep -Milliseconds 500
                    }
                }
            }

            if (-not $removed -and (Test-Path $InstallerFinalExe)) {
                throw "Starý instalátor je stále používán jiným procesem: $InstallerFinalExe"
            }
        }

        Copy-Item -Force $InstallerBuiltExe $InstallerFinalExe

        $InstallerBuiltDir = Split-Path -Parent $InstallerBuiltExe
        if ($InstallerBuiltDir -and (Test-Path $InstallerBuiltDir)) {
            Remove-Item -Recurse -Force $InstallerBuiltDir -ErrorAction SilentlyContinue
        }

        Write-Host ""
        Write-Host "[SRTune] Installer:"
        Write-Host "  $InstallerFinalExe"
    } else {
        Write-Warning "Inno Setup was not found. Portable ZIP was created; installer was skipped."
    }
}

Write-Host ""
Write-Host "[SRTune] Release build completed."
