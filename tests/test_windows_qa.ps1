param([string]$WorkFolder='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'RUN_WINDOWS_QA.ps1')
if(-not $WorkFolder){$WorkFolder=Join-Path $root ('build\windows-qa-tests-'+[Guid]::NewGuid().ToString('N'))}
New-Item -ItemType Directory -Path $WorkFolder -Force | Out-Null
$unicodeName='process '+[char]0x0416+[char]0x4E2D
$executable=Join-Path $WorkFolder ($unicodeName+'.exe')
$source=@'
using System;
using System.Text;
using System.Threading;
public static class QAProcessFixture {
    public static int Main(string[] args) {
        Console.OutputEncoding = new UTF8Encoding(false);
        if (args.Length > 0 && args[0] == "sleep") { Thread.Sleep(30000); return 0; }
        if (args.Length > 0 && args[0] == "fail") { Console.Error.Write("fixture error"); return 7; }
        if (args.Length > 0 && args[0] == "pipes") {
            string block = new string('x', 8192);
            for (int i = 0; i < 64; ++i) { Console.Out.Write(block); Console.Error.Write(block); }
            return 0;
        }
        foreach (string arg in args) Console.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(arg)));
        return 0;
    }
}
'@
Add-Type -TypeDefinition $source -OutputAssembly $executable -OutputType ConsoleApplication
function Assert-QA([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$arguments=@('','plain','with spaces',('Unicode '+[char]0x0416+[char]0x4E2D),'C:\directory with spaces\','C:\two slashes\\','embedded "quote"','slash\"quote','& % $ ; ( )')
$result=Invoke-QANativeProcess -Program $executable -Arguments $arguments -TimeoutSeconds 10
Assert-QA ($result.status -eq 'PASS' -and $result.exit_code -eq 0 -and $result.capture_complete) 'Successful exit must be numeric zero, with completed output capture.'
$received=@($result.stdout.Replace("`r`n","`n").TrimEnd([char]10).Split([char]10) | ForEach-Object {[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($_))})
Assert-QA ($received.Count -eq $arguments.Count) 'Native argument count changed.'
for($n=0;$n -lt $arguments.Count;$n++){Assert-QA ($received[$n] -ceq $arguments[$n]) "Native argument $n did not round-trip."}

$result=Invoke-QANativeProcess -Program $executable -Arguments @('fail') -TimeoutSeconds 10
Assert-QA ($result.status -eq 'FAIL' -and $result.exit_code -eq 7 -and $result.stderr -eq 'fixture error') 'Nonzero exit code and stderr must survive capture.'
$result=Invoke-QANativeProcess -Program $executable -Arguments @('pipes') -TimeoutSeconds 10
Assert-QA ($result.status -eq 'PASS' -and $result.stdout.Length -eq 524288 -and $result.stderr.Length -eq 524288) 'Large concurrent stdout and stderr must not deadlock or truncate.'
$result=Invoke-QANativeProcess -Program $executable -Arguments @('sleep') -TimeoutSeconds 1
Assert-QA ($result.status -eq 'TIMEOUT' -and $null -ne $result.exit_code -and $result.seconds -lt 10) 'Timeout must terminate and report the process within a bounded time.'
$result=Invoke-QANativeProcess -Program (Join-Path $WorkFolder 'missing.exe') -Arguments @() -TimeoutSeconds 1
Assert-QA ($result.status -eq 'ERROR' -and $null -eq $result.exit_code -and $result.stderr) 'Launch failure must be reported as an error.'

$header=[Text.Encoding]::ASCII.GetBytes("P6`n160 144`n255`n")
$pixels=New-Object byte[] (160*144*3)
# Uniform colored frames must count as blank, even when R, G and B differ.
for($n=0;$n -lt $pixels.Length;$n+=3){$pixels[$n]=20;$pixels[$n+1]=40;$pixels[$n+2]=60}
$image=Join-Path $WorkFolder 'frame.ppm'
[IO.File]::WriteAllBytes($image,[byte[]]($header+$pixels))
$frame=Test-QADemoFrame -Path $image
Assert-QA ($frame.status -eq 'FAIL' -and $frame.unique_colors -eq 1) 'Uniform colored screenshot must fail the visible-graphics check.'
$frame=Test-QADemoFrame -Path $image -RequireNonblank $false
Assert-QA ($frame.status -eq 'PASS') 'Short runs may skip the nonblank check while validating dimensions and payload.'
$pixels[0]=21
[IO.File]::WriteAllBytes($image,[byte[]]($header+$pixels))
$frame=Test-QADemoFrame -Path $image
Assert-QA ($frame.status -eq 'PASS' -and $frame.width -eq 160 -and $frame.height -eq 144 -and $frame.unique_colors -eq 2) 'A complete original-size frame with visible graphics must pass.'
[IO.File]::WriteAllBytes($image,[byte[]]($header+$pixels[0..($pixels.Length-2)]))
Assert-QA ((Test-QADemoFrame -Path $image).status -eq 'FAIL') 'Truncated screenshot must fail.'
[IO.File]::WriteAllBytes($image,[Text.Encoding]::ASCII.GetBytes("P6`n80 72`n255`n"))
Assert-QA ((Test-QADemoFrame -Path $image).status -eq 'FAIL') 'Incorrect screenshot dimensions must fail.'
Write-Host 'PASS: native exit codes, argument quoting, Unicode paths, pipe draining, timeout, launch failure and demo framebuffer checks.'
