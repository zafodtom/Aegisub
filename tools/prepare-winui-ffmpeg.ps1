param(
    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

function Find-Ffmpeg {
    $command = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($command -and $command.Source -and (Test-Path $command.Source)) {
        return $command.Source
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Microsoft\\WinGet\\Links\\ffmpeg.exe'),
        (Join-Path $env:ProgramFiles 'ffmpeg\\bin\\ffmpeg.exe'),
        'C:\\ffmpeg\\bin\\ffmpeg.exe'
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

$source = Find-Ffmpeg
$tempRoot = $null

if (-not $source) {
    Write-Host '[Aegisub] FFmpeg nebyl nalezen lokálně. Stahuji release essentials build...'
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('aegisub-ffmpeg-' + [Guid]::NewGuid().ToString('N'))
    $archive = Join-Path $tempRoot 'ffmpeg.zip'
    $extract = Join-Path $tempRoot 'extract'
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

    Invoke-WebRequest -UseBasicParsing -Uri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip' -OutFile $archive
    Expand-Archive -Path $archive -DestinationPath $extract -Force
    $source = Get-ChildItem -Path $extract -Filter ffmpeg.exe -File -Recurse | Select-Object -First 1 -ExpandProperty FullName

    if (-not $source) {
        throw 'Stažený archiv FFmpeg neobsahuje ffmpeg.exe.'
    }
}

$destinationDirectory = Split-Path -Parent $Destination
New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
Copy-Item -Force -Path $source -Destination $Destination

$version = & $Destination -version 2>$null | Select-Object -First 1
Write-Host ('[Aegisub] Přibalen FFmpeg: ' + $version)
Write-Host ('[Aegisub] Cíl: ' + $Destination)

if ($tempRoot -and (Test-Path $tempRoot)) {
    Remove-Item -Recurse -Force $tempRoot
}
