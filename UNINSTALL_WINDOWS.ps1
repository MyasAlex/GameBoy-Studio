$ErrorActionPreference = 'Stop'
$program='HKCU:\Software\Classes\GameBoyStudio.GameBoy'
if (Test-Path $program) { Remove-Item -LiteralPath $program -Recurse }
foreach ($extension in @('.gb','.gbc','.zip','.gz')) {
    $key="HKCU:\Software\Classes\$extension\OpenWithProgids"
    if (Test-Path $key) { Remove-ItemProperty -LiteralPath $key -Name 'GameBoyStudio.GameBoy' -ErrorAction SilentlyContinue }
}
foreach ($folder in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))) {
    $shortcut=Join-Path $folder 'GameBoy Studio.lnk'
    if (Test-Path -LiteralPath $shortcut) { Remove-Item -LiteralPath $shortcut }
}
Write-Host 'Shortcuts and Open With registration removed. Application files and saves are retained in %LOCALAPPDATA%\GameBoy-Studio.'
