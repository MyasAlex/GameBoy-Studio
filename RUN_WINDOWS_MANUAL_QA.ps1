# Records human observations separately from the automated RUN_WINDOWS_QA.ps1.
# Without -Interactive, writes an unexecuted checklist; it never infers PASS.
param(
    [string]$ProgramFolder='', [string]$OutputFolder='', [string]$Tester='',
    [ValidateSet('Unverified','NativeWindows','Wine')][string]$ExecutionEnvironment='Unverified',
    [switch]$Interactive
)
$ErrorActionPreference='Stop'
if(-not $ProgramFolder){$ProgramFolder=if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'GBStudio.exe')){$PSScriptRoot}else{Join-Path $PSScriptRoot 'dist'}}
$ProgramFolder=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ProgramFolder)
if(-not $OutputFolder){$OutputFolder=Join-Path $env:LOCALAPPDATA 'GameBoy-Studio\QA'}
$reportRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputFolder)
$reportFolder=Join-Path $reportRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-manual-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $reportFolder -Force | Out-Null

# Keep steps and expected results in the JSON so an archived report remains
# understandable even if a future release changes the checklist.
$specifications=@(
    @('WIN-01','Installation and persistence','Install and launch as a standard user. Open a ROM in a Unicode path with spaces. Save battery/state, close, reopen. Repeat from a read-only application folder.','Application starts without missing DLLs. Settings use the LocalAppData fallback when needed. ROM, battery save and state survive without corruption.'),
    @('WIN-02','Menus, language and DPI','Check English and Russian menus/help. Resize from minimum to maximized. Move between monitors at 100%, 150% and 200% scaling. Restart with a secondary monitor disconnected.','Text and controls remain usable; menus map to the indicated actions; the restored window remains visible.'),
    @('WIN-03','Keyboard and focus','Use all eight buttons; remap two keys and cancel remapping. Hold A, Tab and Backspace while switching applications and opening a dialog. Return to the game.','No stuck input, turbo or rewind. Game auto-pauses/resumes when allowed. A manually paused game stays paused.'),
    @('WIN-04','Virtual controls and mouse capture','Enable the virtual console. Press each button, drag outside and release. Switch applications while holding a button. Use a scrollbar or drag in another Studio window.','Each button releases on mouse-up, capture transfer and focus loss. Other windows retain their own mouse capture.'),
    @('WIN-05','Two physical XInput controllers','Connect two physical pads. Test each button and stick in single-console and local Link modes. Assign device indexes, reconnect a pad and load/save profiles.','Players remain independent; mappings and deadzones persist; unplugging releases held buttons; reconnecting restores control.'),
    @('WIN-06','Physical tilt and rumble','Use a tilt-capable game and a rumble cartridge/game. Check numpad/right stick, pause, focus loss, controller reconnect and exit, including a mode that continues running in the background.','Tilt follows only focused input and returns to neutral. Rumble responds to cartridge events and stops on pause, focus loss and exit.'),
    @('WIN-07','CPU graphics and capture','Run DMG and CGB games. Check all palettes, LCD/CRT, ghosting, raw comparison, Scale2x/3x, HQ2x and xBRZ at several window sizes. Save a raw BMP.','No clipped or corrupted frames. Effects change only presentation. Raw capture is 160x144 and matches the unprocessed framebuffer.'),
    @('WIN-08','Direct3D rendering and recovery','Repeat graphics checks with Direct3D enabled on the actual GPU. Toggle fullscreen repeatedly, minimize/restore, resize, move monitors and suspend/resume the computer.','Rendering recovers without stale frames or crashes. Device loss gives a usable CPU fallback. Record GPU/driver and hardware versus WARP.'),
    @('WIN-09','Audible output at 44.1/48/96 kHz','Listen through actual speakers/headphones to pulse, wave and noise-heavy gameplay at all three rates and latency settings. Test volume/mute, channel mute and EQ.','Correct pitch/stereo, no persistent clicks, buzzing, clipping or desynchronization. Volume and mute controls take effect promptly.'),
    @('WIN-10','Audio transitions and devices','While listening, pause/resume, rewind, use turbo, change speed, load a state and switch games. Unplug/reconnect the output device and change the Windows default endpoint.','No stale audio after transitions. Audio is muted at non-1x speeds. Device failure is visible and recovery behavior is recorded; no crash or hang.'),
    @('WIN-11','Long session and A/V synchronization','Play for at least 60 minutes at 1x with actual audio output; include background/foreground changes. Record timing, underrun/overflow counters and a short end-of-run A/V recording.','No sustained speed drift, accumulating A/V offset, growing memory use, repeated underruns, freeze or crash.'),
    @('WIN-12','Save, rewind and session continuity','Play, save battery data, use all quick slots, named states, rewind, reset and session restore. Try an incompatible state on a copy of the save.','Expected progress returns; incorrect states are rejected safely; original saves remain intact.'),
    @('WIN-13','Game Boy Camera and webcam','Connect a physical webcam, grant/deny OS permission, capture into a camera game, then disconnect and reconnect the camera. Test image-file input separately.','Actual camera frames arrive with usable orientation/contrast. Denial/disconnect is handled without a freeze. Record the device model and permission behavior.'),
    @('WIN-14','Printer and infrared','Use supported printer/IR games. Print and export an image. Exchange an IR signal between the supported endpoints. State explicitly whether endpoints are emulated or physical.','Printer image and exported file match the game output. IR events reach the intended peer. Physical-device claims require actual hardware evidence.'),
    @('WIN-15','Local and network Link continuity','Use two independent saves. Complete a local Link exchange and a network exchange on two computers. For Pokemon, complete a trade, save, close and reopen both games.','Both peers remain synchronized. Traded data and progress survive restart. Record both build hashes, game hashes and network conditions.'),
    @('WIN-16','Recording and external state exchange','Record and replay input; export WAV/FLAC/AVI and inspect them in independent players. Exchange a BESS state with a named external emulator in both directions.','Replay matches the same ROM; media plays correctly and remains synchronized; both BESS directions preserve the expected state.'),
    @('WIN-17','RetroAchievements account integration','With a real test account, log in/out, unlock a test achievement, reconnect the network and verify Hardcore restrictions against the server response.','Credentials are not exposed in reports. Server confirms the expected account/unlock/Hardcore behavior; local UI alone is insufficient evidence.'),
    @('WIN-18','Release package and signature','On clean Windows 10 and 11 machines, extract/install the final package, run automated QA, verify every EXE/DLL signature, then uninstall while keeping saves.','No build tools are required. Automated checks pass for these exact binaries. Signatures validate if a signed release is claimed. Uninstall preserves user data.')
)
$cases=@($specifications | ForEach-Object {[pscustomobject][ordered]@{id=$_[0];area=$_[1];steps=$_[2];expected=$_[3];status='NOT_RUN';observed='';evidence=@();recorded_at=$null}})
$hashes=[ordered]@{}
foreach($name in @('GBStudio.exe','gb_accuracy.dll','gb_pixel_filters.exe','tools\gb_headless.exe')){
    $path=Join-Path $ProgramFolder $name
    if(Test-Path -LiteralPath $path){$hashes[$name]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
}
$hostData=[ordered]@{os=[Environment]::OSVersion.VersionString;powershell=$PSVersionTable.PSVersion.ToString();execution_environment=$ExecutionEnvironment;gpu=@();sound_devices=@();inventory_error=''}
try {
    $hostData.gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    $hostData.sound_devices=@(Get-CimInstance Win32_SoundDevice | Select-Object Name,Status)
} catch {$hostData.inventory_error=$_.Exception.Message}
$report=[ordered]@{schema_version=1;created_at=(Get-Date).ToString('o');tester=$Tester;host=$hostData;binary_sha256=$hashes;cases=$cases;status='NOT_RUN';native_manual_validation=$false;scope='Human observations only. NOT_RUN and BLOCKED are open work, never PASS. Wine is not native Windows validation. Physical-device and audible claims require a human using the named actual devices.'}
$file=Join-Path $reportFolder 'windows-manual-qa.json'
function Save-ManualQA {$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $file -Encoding UTF8}
Save-ManualQA
if($Interactive){
    if(-not(Test-Path -LiteralPath (Join-Path $ProgramFolder 'GBStudio.exe'))){throw "GBStudio.exe missing. The unexecuted checklist was saved to $file"}
    if(-not $Tester){$report.tester=Read-Host 'Tester name or identifier'}
    foreach($case in $cases){
        Write-Host "`n$($case.id) $($case.area)`nSteps: $($case.steps)`nExpected: $($case.expected)"
        do {$answer=(Read-Host 'Result: PASS / FAIL / BLOCKED / NOT_RUN (Enter = NOT_RUN)').ToUpperInvariant();if(-not $answer){$answer='NOT_RUN'}}while($answer -notin @('PASS','FAIL','BLOCKED','NOT_RUN'))
        $case.status=$answer
        if($answer -ne 'NOT_RUN'){
            do {$case.observed=Read-Host 'Actual observations, device names, duration and limitations (required)'}while(-not $case.observed.Trim())
            $paths=Read-Host 'Evidence paths/URLs, separated by | (no account credentials)'
            $case.evidence=@($paths.Split('|') | ForEach-Object {$_.Trim()} | Where-Object {$_})
            $case.recorded_at=(Get-Date).ToString('o')
        }
        Save-ManualQA
    }
    $report.status=if(@($cases | Where-Object {$_.status -eq 'FAIL'}).Count){'FAIL'}elseif(@($cases | Where-Object {$_.status -ne 'PASS'}).Count){'INCOMPLETE'}else{'PASS'}
    $report.native_manual_validation=$report.status -eq 'PASS' -and $ExecutionEnvironment -eq 'NativeWindows'
    Save-ManualQA
}
Write-Host "Manual report: $file"
if($report.status -eq 'PASS'){exit 0}
if($report.status -eq 'FAIL'){exit 1}
exit 2
