param([switch]$ServerOnly,[switch]$Test)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
& python "$PSScriptRoot/check_budget.py"
if($LASTEXITCODE -ne 0){throw 'Workspace exceeds 1 GiB'}
$taskRuntime=Join-Path $taskRoot 'runtime/minecraft'
if(-not (Select-String -LiteralPath "$taskRuntime/eula.txt" -Pattern '^eula=true$' -Quiet)){throw 'Minecraft EULA acceptance is required in runtime/minecraft/eula.txt'}
# Avoid truncating a live server's logs or racing its world-file locks.
$taskPidFile=Join-Path $taskRoot 'build/steve/server.pid'
if(Test-Path -LiteralPath $taskPidFile){
    $taskOldId=0
    if([int]::TryParse(([IO.File]::ReadAllText($taskPidFile)).Trim(),[ref]$taskOldId)){
        $taskOld=Get-Process -Id $taskOldId -ErrorAction SilentlyContinue
        if($taskOld -and $taskOld.ProcessName -eq 'java' -and -not $taskOld.WaitForExit(20000)){throw 'The local Minecraft server is already running. Stop it through server/ipc.py --stop before starting another instance.'}
    }
}
$taskJava='C:/Program Files/Microsoft/jdk-17.0.14.7-hotspot/bin/java.exe'
$taskJar=Join-Path $taskRuntime 'server-1.18.jar'
if((Get-FileHash -LiteralPath $taskJar -Algorithm SHA1).Hash -ne '3CF24A8694ACA6267883B17D934EFACC5E44440D'){throw 'Official Minecraft 1.18 server hash mismatch'}
# The vanilla server discards summoned animals when spawn-animals is false.
# Natural spawning is independently disabled by the adapter's doMobSpawning rule.
$taskProperties=Join-Path $taskRuntime 'server.properties'
$taskServerSettings=[IO.File]::ReadAllText($taskProperties)
foreach($taskSetting in @('spawn-animals','spawn-monsters','spawn-npcs')) {
    if($taskServerSettings -match "(?m)^$taskSetting=") {$taskServerSettings=[regex]::Replace($taskServerSettings,"(?m)^$taskSetting=.*$","$taskSetting=true")}
    else {$taskServerSettings+="`n$taskSetting=true`n"}
}
[IO.File]::WriteAllText($taskProperties,$taskServerSettings)
Remove-Item -LiteralPath "$taskRuntime/budget-stop.txt" -ErrorAction SilentlyContinue
$taskServer=Start-Process -FilePath $taskJava -WorkingDirectory $taskRuntime -WindowStyle Hidden -PassThru -ArgumentList @('--add-exports=java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED','-Dlog4j2.formatMsgNoLookups=true','-XX:ActiveProcessorCount=2','-javaagent:steve-agent.jar','-Xms128M','-Xmx512M','-jar','server-1.18.jar','nogui') -RedirectStandardOutput "$taskRuntime/server.log" -RedirectStandardError "$taskRuntime/server-errors.log"
$taskServer.Id | Set-Content -LiteralPath "$taskRoot/build/steve/server.pid"
$taskWatcher=Start-Process -FilePath 'python' -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -ArgumentList @('port/steve/watch_budget.py',"$($taskServer.Id)")
$taskWatcher.Id | Set-Content -LiteralPath "$taskRoot/build/steve/budget-monitor.pid"
$taskDeadline=(Get-Date).AddSeconds(90)
while(-not (Select-String -LiteralPath "$taskRuntime/server.log" -Pattern 'STEVE_BRIDGE_READY' -Quiet)){
    if($taskServer.HasExited){throw "Minecraft server exited: inspect $taskRuntime/server-errors.log"}
    if((Get-Date) -gt $taskDeadline){throw 'Minecraft server readiness timed out'}
    Start-Sleep -Milliseconds 500
}
Write-Output 'Minecraft 1.18 bridge is ready.'
if($ServerOnly){return}
$taskGameEnv=@('SM64DS_STEVE','SM64DS_LEVEL','SM64DS_NO_DIALOG','SM64DS_STEVE_TEST','SM64DS_WINDOW_SELFTEST','SM64DS_SELFTEST_IDLE')
$taskPreviousEnv=@{}
foreach($taskName in $taskGameEnv){$taskPreviousEnv[$taskName]=[Environment]::GetEnvironmentVariable($taskName)}
try{
    $env:SM64DS_STEVE='1'
    if($Test){$env:SM64DS_LEVEL='1';$env:SM64DS_NO_DIALOG='1';$env:SM64DS_STEVE_TEST='1';$env:SM64DS_WINDOW_SELFTEST='180';$env:SM64DS_SELFTEST_IDLE='1'}
    $taskGame=Join-Path $taskRoot 'build/port/walk_window.exe'
    if(-not (Test-Path -LiteralPath $taskGame)){throw 'Build the game with port/steve/build-game.cmd first'}
    $taskProcess=Start-Process -FilePath $taskGame -WorkingDirectory $taskRoot -PassThru -Wait
    if($taskProcess.ExitCode -ne 0){throw "SM64DS exited with code $($taskProcess.ExitCode)"}
}finally{
    & python "$PSScriptRoot/server/ipc.py" --stop
    $taskServer.WaitForExit(15000) | Out-Null
    & python "$PSScriptRoot/check_budget.py"
    foreach($taskName in $taskGameEnv){[Environment]::SetEnvironmentVariable($taskName,$taskPreviousEnv[$taskName])}
}
