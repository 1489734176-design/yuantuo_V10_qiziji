$ErrorActionPreference = 'Stop'
$BuildFolder = 'build'
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$taskBuildDir = Join-Path $PSScriptRoot $BuildFolder
$taskRelativeBuild = '..\.planning\2026-11-06-scene-current-check\' + $BuildFolder
$taskProject = Join-Path $taskRoot 'KEIL_PRJ\scene_current_validation.uvprojx'
$taskLog = Join-Path $taskBuildDir 'rebuild.log'
$taskUv4 = 'D:\Users\mym02\AppData\Local\Keil_v5\UV4\UV4.exe'
New-Item -ItemType Directory -Path $taskBuildDir -Force | Out-Null
$taskProjectText = Get-Content -LiteralPath (Join-Path $taskRoot 'KEIL_PRJ\MM32SPIN0230_Demo.uvprojx') -Raw
$taskProjectText = $taskProjectText.Replace('<OutputDirectory>.\Objects\</OutputDirectory>', ('<OutputDirectory>' + $taskRelativeBuild + '\Objects\</OutputDirectory>'))
$taskProjectText = $taskProjectText.Replace('<ListingPath>.\Listings\</ListingPath>', ('<ListingPath>' + $taskRelativeBuild + '\Listings\</ListingPath>'))
$taskProjectText = $taskProjectText.Replace('--output=@L.bin', ('--output=' + $taskRelativeBuild + '\SPIN0230_DEMO.bin'))
[System.IO.File]::WriteAllText($taskProject, $taskProjectText, [System.Text.UTF8Encoding]::new($false))
$taskArguments = '-r "' + $taskProject + '" -t MM32SPIN0230 -o "' + $taskLog + '"'
$taskProcess = Start-Process -FilePath $taskUv4 -ArgumentList $taskArguments -WorkingDirectory (Join-Path $taskRoot 'KEIL_PRJ') -Wait -PassThru -WindowStyle Hidden
Write-Output "UV4 exit code: $($taskProcess.ExitCode)"
if(Test-Path -LiteralPath $taskLog) { Get-Content -LiteralPath $taskLog }
if($taskProcess.ExitCode -gt 1) { throw 'Keil clean rebuild failed.' }
foreach($taskArtifact in @('Objects\SPIN0230_DEMO.axf','Objects\SPIN0230_DEMO.hex','SPIN0230_DEMO.bin')) {
    $taskArtifactPath = Join-Path $taskBuildDir $taskArtifact
    if(!(Test-Path -LiteralPath $taskArtifactPath)) { throw "Missing firmware: $taskArtifact" }
    Get-Item -LiteralPath $taskArtifactPath | Select-Object FullName, Length, LastWriteTime
}
Remove-Item -LiteralPath $taskProject
