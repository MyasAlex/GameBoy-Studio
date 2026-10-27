# Reports are written to a unique subdirectory of OutputFolder (or LocalAppData).
param(
    [string]$GameFolder='', [string]$DmgBoot='', [string]$CgbBoot='',
    [ValidateSet('auto','dmg','pocket','light','cgb','cgb0','cgba','cgbb','cgbc','cgbd','cgbe','agb','sgb','sgb2','sgbpal','sgb-pal')][string]$Model='auto',
    [ValidateRange(1,100000)][int]$Frames=600,
    [ValidateRange(10,7200)][int]$TimeoutSeconds=180,
    [string]$OutputFolder=''
)

function ConvertTo-QANativeArgument([AllowEmptyString()][string]$Argument){
    # Windows CRT / CommandLineToArgvW quoting: backslashes before a quote, and
    # before the closing quote, must be doubled. This preserves empty arguments,
    # Unicode paths, embedded quotes and directory paths ending in a backslash.
    $escaped=[regex]::Replace($Argument,'(\\*)"',{param($match) $match.Groups[1].Value+$match.Groups[1].Value+'\"'})
    $escaped=[regex]::Replace($escaped,'(\\+)$',{param($match) $match.Value+$match.Value})
    return '"'+$escaped+'"'
}

function Invoke-QANativeProcess {
    param([string]$Program,[string[]]$Arguments,[ValidateRange(1,7200)][int]$TimeoutSeconds=180)
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $process=New-Object Diagnostics.Process
    $stdoutTask=$null; $stderrTask=$null; $started=$false
    $status='ERROR'; $exitCode=$null; $stdout=''; $stderr=''; $captureComplete=$false
    try {
        $start=New-Object Diagnostics.ProcessStartInfo
        $start.FileName=$Program
        $start.Arguments=($Arguments | ForEach-Object {ConvertTo-QANativeArgument $_}) -join ' '
        $start.UseShellExecute=$false
        $start.CreateNoWindow=$true
        $start.RedirectStandardOutput=$true
        $start.RedirectStandardError=$true
        $start.StandardOutputEncoding=New-Object Text.UTF8Encoding($false)
        $start.StandardErrorEncoding=New-Object Text.UTF8Encoding($false)
        $process.StartInfo=$start
        $started=$process.Start()
        if(-not $started){throw 'Native process could not start.'}
        # Drain both pipes concurrently before waiting. Sequential reads can
        # deadlock when a child fills stderr while the parent waits for stdout.
        $stdoutTask=$process.StandardOutput.ReadToEndAsync()
        $stderrTask=$process.StandardError.ReadToEndAsync()
        $finished=$process.WaitForExit($TimeoutSeconds*1000)
        if(-not $finished){
            try {$process.Kill()} catch {if(-not $process.HasExited){throw}}
            if(-not $process.WaitForExit(5000)){throw 'Timed-out process did not terminate.'}
        }
        $exitCode=$process.ExitCode
        $status=if(-not $finished){'TIMEOUT'}elseif($exitCode -eq 0){'PASS'}else{'FAIL'}
        # Bound pipe completion too: a child might leave inherited pipe handles
        # open in another process. Do not let that defeat the execution timeout.
        $captureComplete=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdoutTask,$stderrTask),5000)
        if($stdoutTask.IsCompleted){$stdout=$stdoutTask.GetAwaiter().GetResult()}
        if($stderrTask.IsCompleted){$stderr=$stderrTask.GetAwaiter().GetResult()}
        if(-not $captureComplete){
            $stderr+="`nOutput capture did not finish within 5 seconds."
            if($status -ne 'TIMEOUT'){$status='ERROR'}
        }
    } catch {
        $stderr+="`n"+$_.Exception.Message
        $status='ERROR'
    } finally {
        # Dispose always, and avoid abandoning a child if setup or output
        # capture raised an exception after it started.
        if($started){
            try {if(-not $process.HasExited){$process.Kill();[void]$process.WaitForExit(5000)}}
            catch {$stderr+="`nProcess cleanup: "+$_.Exception.Message;$status='ERROR'}
        }
        $watch.Stop()
        $process.Dispose()
    }
    [pscustomobject]@{status=$status;exit_code=$exitCode;seconds=$watch.Elapsed.TotalSeconds;stdout=$stdout;stderr=$stderr;capture_complete=$captureComplete}
}

function Test-QADemoFrame {
    param([string]$Path,[bool]$RequireNonblank=$true)
    $frame=[ordered]@{path=$Path;status='FAIL';width=$null;height=$null;unique_colors=$null;nonblank_required=$RequireNonblank;message=''}
    try {
        $bytes=[IO.File]::ReadAllBytes($Path)
        $header=[Text.Encoding]::ASCII.GetString($bytes,0,[Math]::Min(256,$bytes.Length))
        $match=[regex]::Match($header,'\AP6\r?\n(?<width>[0-9]+) (?<height>[0-9]+)\r?\n255\r?\n')
        if(-not $match.Success){throw 'Expected a binary P6 screenshot with 8-bit RGB pixels.'}
        $frame.width=[int]$match.Groups['width'].Value
        $frame.height=[int]$match.Groups['height'].Value
        if($frame.width -ne 160 -or $frame.height -ne 144){throw 'Expected the original 160x144 framebuffer.'}
        if($bytes.Length -ne $match.Length+160*144*3){throw 'Screenshot pixel payload is incomplete or has unexpected trailing data.'}
        $colors=New-Object 'System.Collections.Generic.HashSet[int]'
        for($offset=$match.Length;$offset -lt $bytes.Length;$offset+=3){
            [void]$colors.Add(([int]$bytes[$offset] -shl 16) -bor ([int]$bytes[$offset+1] -shl 8) -bor [int]$bytes[$offset+2])
        }
        $frame.unique_colors=$colors.Count
        if($RequireNonblank -and $colors.Count -lt 2){throw 'Demo screenshot is a uniform frame; expected visible homebrew graphics.'}
        $frame.status='PASS'
        $frame.message=if($RequireNonblank){'Original-size screenshot contains visible graphics.'}else{'Original-size screenshot verified; nonblank check requires at least 60 frames.'}
    } catch {$frame.message=$_.Exception.Message}
    [pscustomobject]$frame
}

function Invoke-WindowsQA {
    $ErrorActionPreference='Stop'
    $folder=if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'GBStudio.exe')){$PSScriptRoot}else{Join-Path $PSScriptRoot 'dist'}
    $headless=Join-Path $folder 'tools\gb_headless.exe'
    if(-not(Test-Path -LiteralPath $headless)){throw 'Extract the full archive first.'}
    $reportRoot=if($OutputFolder){$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputFolder)}else{Join-Path $env:LOCALAPPDATA 'GameBoy-Studio\QA'}
    $reportFolder=Join-Path $reportRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $reportFolder -Force | Out-Null
    $results=New-Object 'System.Collections.Generic.List[object]'
    function Run-Check([string]$name,[string]$program,[string[]]$arguments,[string]$checkType='execution_smoke_test'){
        $id=$results.Count
        $result=Invoke-QANativeProcess -Program $program -Arguments $arguments -TimeoutSeconds $TimeoutSeconds
        $stdout=Join-Path $reportFolder "$id.stdout.txt"; $stderr=Join-Path $reportFolder "$id.stderr.txt"
        [IO.File]::WriteAllText($stdout,$result.stdout,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText($stderr,$result.stderr,(New-Object Text.UTF8Encoding($false)))
        $check=[pscustomobject]@{name=$name;check_type=$checkType;status=$result.status;exit_code=$result.exit_code;seconds=$result.seconds;stdout=$result.stdout;stderr=$result.stderr;stdout_file=$stdout;stderr_file=$stderr;capture_complete=$result.capture_complete;screenshot=$null}
        $results.Add($check)
        return $check
    }
    $null=Run-Check 'Win32 CPU rendering self-test' (Join-Path $folder 'GBStudio.exe') @('--self-test',(Join-Path $folder 'demo.gbc')) 'cpu_rendering_self_test'
    foreach($rom in @((Join-Path $folder 'demo.gb'),(Join-Path $folder 'demo.gbc'))){
        $image=Join-Path $reportFolder ([IO.Path]::GetFileName($rom)+'.ppm')
        # These original demos do not contain the licensed Nintendo boot logo;
        # run them from the post-boot entry point even when real-game boot images
        # were supplied. Hardware model selection is reserved for GameFolder.
        $check=Run-Check ([IO.Path]::GetFileName($rom)) $headless @($rom,'--frames',"$Frames",'--output',$image) 'demo_execution_and_frame'
        if($check.status -eq 'PASS'){
            $check.screenshot=Test-QADemoFrame -Path $image -RequireNonblank ($Frames -ge 60)
            if($check.screenshot.status -ne 'PASS'){$check.status='FAIL'}
        }
    }
    if($GameFolder){
        foreach($rom in Get-ChildItem -LiteralPath $GameFolder -File -Recurse | Where-Object {$_.Extension -in @('.gb','.gbc')}){
            $data=[IO.File]::ReadAllBytes($rom.FullName);if($data.Length -lt 336){continue}
            $color=($Model -like 'cgb*') -or ($Model -eq 'agb') -or ($Model -eq 'auto' -and ($data[323] -band 128))
            $boot=if($color){$CgbBoot}else{$DmgBoot}
            $cliModel=if($Model -eq 'sgbpal'){'sgb-pal'}else{$Model}
            $arguments=@($rom.FullName,'--frames',"$Frames",'--model',$cliModel)
            if($boot){$arguments+=@('--boot-rom',(Resolve-Path -LiteralPath $boot).Path)}
            $null=Run-Check $rom.Name $headless $arguments
        }
    }
    $binaryHashes=@{}
    foreach($name in @('GBStudio.exe','gb_pixel_filters.exe','tools\gb_headless.exe')){$binaryHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $folder $name) -Algorithm SHA256).Hash}
    $report=[ordered]@{schema_version=2;date=(Get-Date).ToString('o');platform=[Environment]::OSVersion.VersionString;binary_sha256=$binaryHashes;frames=$Frames;tests=@($results.ToArray());scope='PASS means only that this automated check succeeded. GUI self-test checks CPU rendering only. Demo checks export original 160x144 frames and require nonuniform graphics at 60 or more frames. Real-ROM runs test startup/continued headless execution without automated gameplay; they do not establish game compatibility. This does not validate audible/device latency, GPU, actual gamepad/webcam/printer/IR behavior, WAN or Pokemon trades.';manual_checks=@('Direct3D + CRT/HQ2x/Scale2x/3x/xBRZ; device loss and resize','48/96 kHz audible quality, latency and long-session underruns','Two physical gamepads, tilt, rumble and virtual controls','Webcam capture, Printer and infrared','Pokemon trade/save continuity and long real-game play','RetroAchievements real account and server-approved Hardcore','Cross-emulator BESS exchange and trusted code signature')}
    $file=Join-Path $reportFolder 'windows-qa.json'
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $file -Encoding UTF8
    Write-Host "Report: $file"
    if($results | Where-Object {$_.status -ne 'PASS'}){return 1}
    return 0
}

# Dot-sourcing exposes the process and frame validators for regression tests.
if($MyInvocation.InvocationName -ne '.') {exit (Invoke-WindowsQA)}
