param([int]$Level=40,[int]$Actor=279,[int]$Frames=900,[switch]$Enemy)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskNames=@('SM64DS_STEVE','SM64DS_STEVE_TEST_SCENE','SM64DS_STEVE_TEST_COMBAT','SM64DS_LEVEL','SM64DS_SKIP_INTRO','SM64DS_SKIP_MENU','SM64DS_WINDOW_SELFTEST','SM64DS_SELFTEST_IDLE','SM64DS_NO_DIALOG','SM64DS_NO_PLAYLOG','SM64DS_VOLUME','SM64DS_PROBE_INPUT','SM64DS_SAVE_PATH')
$taskBackup=@{};foreach($name in $taskNames){$taskBackup[$name]=[Environment]::GetEnvironmentVariable($name)}
$taskStem=Join-Path $taskRoot "build/steve/combat-$Level-$Actor"
if(-not ('SteveCombatPower' -as [type])) {
    Add-Type 'using System.Runtime.InteropServices; public static class SteveCombatPower { [DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint flags); }'
}
try {
    [SteveCombatPower]::SetThreadExecutionState([uint32]2147483649) | Out-Null
    $env:SM64DS_STEVE='1';$env:SM64DS_STEVE_TEST_SCENE="$(960+$Level)";$env:SM64DS_STEVE_TEST_COMBAT="$Actor"
    $env:SM64DS_LEVEL="$Level";$env:SM64DS_SKIP_INTRO='1';$env:SM64DS_SKIP_MENU='1';$env:SM64DS_WINDOW_SELFTEST="$Frames"
    $env:SM64DS_SELFTEST_IDLE='1';$env:SM64DS_NO_DIALOG='1';$env:SM64DS_NO_PLAYLOG='1';$env:SM64DS_VOLUME='0'
    $env:SM64DS_PROBE_INPUT=((30..$Frames | Where-Object {$_ % 12 -eq 0} | ForEach-Object {"${_}:A"}) -join ',')
    # A disposable save prevents the proof from changing the user's progress.
    $env:SM64DS_SAVE_PATH="$taskStem-save.sav"
    Remove-Item -LiteralPath $env:SM64DS_SAVE_PATH -ErrorAction SilentlyContinue
    & python "$PSScriptRoot/server/prepare_combat.py" (960+$Level)
    if($LASTEXITCODE -ne 0){throw 'Could not initialize the QA player'}
    & python "$PSScriptRoot/check_budget.py"
    if($LASTEXITCODE -ne 0){throw 'Workspace cap exceeded before combat verification'}
    $taskProcess=Start-Process "$taskRoot/build/port/walk_window.exe" -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput "$taskStem.log" -RedirectStandardError "$taskStem-errors.log"
    $taskHandle=$taskProcess.Handle # Keep the process handle open so ExitCode survives exit.
    $taskDeadline=(Get-Date).AddSeconds(180)
    while(-not $taskProcess.WaitForExit(500)) {
        if((Get-Date) -gt $taskDeadline){$taskProcess.Kill();throw 'Combat verification timed out'}
    }
    $taskProcess.WaitForExit()
    if($taskProcess.ExitCode -ne 0){throw "Game exited with $($taskProcess.ExitCode)"}
    $taskTrace=[IO.File]::ReadAllText("$taskStem-errors.log")
    if($taskTrace -match '\[quarantine\]|FATAL:|UNHOSTED:'){throw 'A native actor fault invalidated the combat run'}
    if($taskTrace -notmatch '\[steve-physics\]'){throw 'Minecraft IPC did not synchronize in-game'}
    if($Enemy) {
        if($taskTrace -notmatch "native-enemy-hit id=$Actor " -or $taskTrace -notmatch "native-enemy-defeated id=$Actor "){throw 'Native enemy hit/defeat was not verified'}
    } else {
        if($taskTrace -notmatch "\[steve-boss\] punch id=$Actor "){throw 'No punch reached the requested boss'}
        if($taskTrace -notmatch "\[steve-boss\] defeat id=$Actor .* reward=[1-9]"){throw 'No real boss reward was spawned'}
        if($taskTrace -notmatch '\[steve-boss-test\] reward-collected actor='){throw 'The native reward collection path was not verified'}
    }
    Copy-Item -LiteralPath "$taskRoot/walk_window_selftest.bmp" -Destination "$taskStem.bmp"
    $taskEvidence=@{level=$Level;actor=$Actor;frames=$Frames;nativeEnemy=[bool]$Enemy;exitCode=$taskProcess.ExitCode;faults=0;evidence=@(($taskTrace -split '\r?\n') | Where-Object {$_ -match '\[steve-boss\]|reward-collected|native-enemy-'})}
    $taskEvidence | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$taskStem.json"
    $taskEvidence.evidence
} finally {
    [SteveCombatPower]::SetThreadExecutionState([uint32]2147483648) | Out-Null
    foreach($name in $taskNames){[Environment]::SetEnvironmentVariable($name,$taskBackup[$name])}
}
