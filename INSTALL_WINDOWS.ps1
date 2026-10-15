param([switch]$DesktopShortcut, [switch]$RegisterOpenWith)
$ErrorActionPreference = 'Stop'
$source = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'GBStudio.exe')) { $PSScriptRoot } else { Join-Path $PSScriptRoot 'dist' }
if (-not (Test-Path -LiteralPath (Join-Path $source 'GBStudio.exe'))) { throw 'GBStudio.exe is missing. Extract the complete archive first.' }
$destination = Join-Path $env:LOCALAPPDATA 'GameBoy-Studio'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
# A per-user copy; existing save/config files are retained. An active emulator
# must be closed before replacing its executable; no process is terminated.
if ([IO.Path]::GetFullPath($source) -ne [IO.Path]::GetFullPath($destination)) {
    Get-ChildItem -LiteralPath $source | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force }
}
$executable = Join-Path $destination 'GBStudio.exe'
$shell = New-Object -ComObject WScript.Shell
function Make-Shortcut([string]$folder) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'GameBoy Studio.lnk'))
    $shortcut.TargetPath = $executable
    $shortcut.WorkingDirectory = $destination
    $shortcut.IconLocation = "$executable,0"
    $shortcut.Save()
}
Make-Shortcut ([Environment]::GetFolderPath('Programs'))
if ($DesktopShortcut) { Make-Shortcut ([Environment]::GetFolderPath('Desktop')) }
if ($RegisterOpenWith) {
    $classes = 'HKCU:\Software\Classes'
    $program = Join-Path $classes 'GameBoyStudio.GameBoy'
    New-Item -Path "$program\shell\open\command" -Force | Out-Null
    Set-Item -Path $program -Value 'Game Boy / Game Boy Color ROM'
    Set-Item -Path "$program\shell\open\command" -Value ('"{0}" "%1"' -f $executable)
    foreach ($extension in @('.gb','.gbc','.zip','.gz')) {
        $openWith = Join-Path $classes "$extension\OpenWithProgids"
        New-Item -Path $openWith -Force | Out-Null
        New-ItemProperty -Path $openWith -Name 'GameBoyStudio.GameBoy' -PropertyType None -Value ([byte[]]@()) -Force | Out-Null
    }
    Write-Host 'GameBoy Studio was added to Open With. Choose a default app in Windows if desired.'
}
Write-Host "Installed: $destination"

