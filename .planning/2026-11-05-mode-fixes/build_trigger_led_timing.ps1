$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$taskBuildDir = Join-Path $PSScriptRoot 'trigger-led-timing-build'
$taskProject = Join-Path $taskRoot 'KEIL_PRJ\trigger_led_timing_validation.uvprojx'
$taskLog = Join-Path $taskBuildDir 'rebuild.log'
$taskUv4 = 'D:\Users\mym02\AppData\Local\Keil_v5\UV4\UV4.exe'
New-Item -ItemType Directory -Path $taskBuildDir -Force | Out-Null
$taskProjectText = Get-Content -LiteralPath (Join-Path $taskRoot 'KEIL_PRJ\MM32SPIN0230_Demo.uvprojx') -Raw
$taskProjectText = $taskProjectText.Replace('<OutputDirectory>.\Objects\</OutputDirectory>', '<OutputDirectory>..\.planning\2026-11-05-mode-fixes\trigger-led-timing-build\Objects\</OutputDirectory>')
$taskProjectText = $taskProjectText.Replace('<ListingPath>.\Listings\</ListingPath>', '<ListingPath>..\.planning\2026-11-05-mode-fixes\trigger-led-timing-build\Listings\</ListingPath>')
$taskProjectText = $taskProjectText.Replace('--output=@L.bin', '--output=..\.planning\2026-11-05-mode-fixes\trigger-led-timing-build\SPIN0230_DEMO.bin')
[System.IO.File]::WriteAllText($taskProject, $taskProjectText, [System.Text.UTF8Encoding]::new($false))
$taskArguments = '-r "' + $taskProject + '" -t MM32SPIN0230 -o "' + $taskLog + '"'
$taskBuildStarted = Get-Date
$taskProcess = Start-Process -FilePath $taskUv4 -ArgumentList $taskArguments -WorkingDirectory (Join-Path $taskRoot 'KEIL_PRJ') -Wait -PassThru -WindowStyle Hidden
Write-Output "UV4 exit code: $($taskProcess.ExitCode)"
if(!(Test-Path -LiteralPath $taskLog)) { throw 'Missing Keil rebuild log.' }
$taskLogText = Get-Content -LiteralPath $taskLog -Raw
Write-Output $taskLogText
if($taskProcess.ExitCode -ne 0 -or $taskLogText -notmatch '0 Error\(s\), 0 Warning\(s\)') { throw 'Keil rebuild did not finish with zero errors and warnings.' }
foreach($taskRelative in @('Objects\SPIN0230_DEMO.hex', 'Objects\SPIN0230_DEMO.axf', 'SPIN0230_DEMO.bin'))
{
    $taskOutput = Get-Item -LiteralPath (Join-Path $taskBuildDir $taskRelative)
    if($taskOutput.Length -eq 0 -or $taskOutput.LastWriteTime -lt $taskBuildStarted) { throw "Missing or stale build artifact: $taskRelative" }
    $taskOutput | Select-Object FullName, Length, LastWriteTime
}
