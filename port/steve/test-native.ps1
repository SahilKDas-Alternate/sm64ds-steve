param([switch]$Hole,[switch]$Stock,[switch]$Fixture,[switch]$Swim,[switch]$SwimCoin,[switch]$Boat,[switch]$Flight,[switch]$Craft,[switch]$Health,[switch]$Special,[switch]$Animated,[switch]$Conduit,[string]$Shape,[int]$Scene=900)
$taskEnvNames=@('SM64DS_STEVE','SM64DS_STEVE_TEST_SCENE','SM64DS_STEVE_TEST','SM64DS_STEVE_TEST_HOLE','SM64DS_STEVE_TEST_SWIM','SM64DS_STEVE_TEST_BOAT','SM64DS_STEVE_TEST_FLIGHT','SM64DS_STEVE_TEST_SHAPE','SM64DS_STEVE_TEST_CRAFT','SM64DS_STEVE_TEST_HEALTH','SM64DS_LEVEL','SM64DS_WINDOW_SELFTEST','SM64DS_SELFTEST_IDLE','SM64DS_NO_DIALOG','SM64DS_NO_PLAYLOG','SM64DS_VOLUME')
$taskEnvBackup=@{}
$taskEnvNames+=@('SM64DS_SKIP_INTRO','SM64DS_SKIP_MENU')
$taskEnvNames+='SM64DS_STEVE_TEST_SWIM_COIN'
$taskEnvNames+=@('SM64DS_STEVE_TEST_SPECIAL','SM64DS_STEVE_TEST_ANIM','SM64DS_STEVE_TEST_CONDUIT')
foreach($taskName in $taskEnvNames){$taskEnvBackup[$taskName]=[Environment]::GetEnvironmentVariable($taskName)}
try {
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
foreach($taskFlag in @('SM64DS_STEVE_TEST','SM64DS_STEVE_TEST_HOLE','SM64DS_STEVE_TEST_SWIM','SM64DS_STEVE_TEST_BOAT','SM64DS_STEVE_TEST_FLIGHT','SM64DS_STEVE_TEST_SHAPE','SM64DS_STEVE_TEST_CRAFT','SM64DS_STEVE_TEST_HEALTH')) {Remove-Item "Env:$taskFlag" -ErrorAction SilentlyContinue}
if(-not $Stock){$env:SM64DS_STEVE='1';$env:SM64DS_STEVE_TEST_SCENE="$Scene"}else{Remove-Item Env:SM64DS_STEVE -ErrorAction SilentlyContinue}
if($Fixture){$env:SM64DS_STEVE_TEST='1'}
Remove-Item Env:SM64DS_STEVE_TEST_SPECIAL -ErrorAction SilentlyContinue
if($Special){$env:SM64DS_STEVE_TEST_SPECIAL='1'}
Remove-Item Env:SM64DS_STEVE_TEST_ANIM -ErrorAction SilentlyContinue
Remove-Item Env:SM64DS_STEVE_TEST_CONDUIT -ErrorAction SilentlyContinue
if($Conduit){$env:SM64DS_STEVE_TEST_CONDUIT='1'}
if($Animated){$env:SM64DS_STEVE_TEST_ANIM='1';$env:SM64DS_STEVE_TEST_SPECIAL='1'}
if($Hole){$env:SM64DS_STEVE_TEST_HOLE='1'}
Remove-Item Env:SM64DS_STEVE_TEST_SWIM_COIN -ErrorAction SilentlyContinue
if($SwimCoin){$Swim=$true;$env:SM64DS_STEVE_TEST_SWIM_COIN='1'}
if($Swim){$env:SM64DS_STEVE_TEST_SWIM='1'}
if($Boat){$env:SM64DS_STEVE_TEST_BOAT='1'}
if($Flight){$env:SM64DS_STEVE_TEST_FLIGHT='1'}
if($Health){$env:SM64DS_STEVE_TEST_HEALTH='1'}
if($Craft){$env:SM64DS_STEVE_TEST_CRAFT='1'}
if($Shape){$env:SM64DS_STEVE_TEST_SHAPE=$Shape}
$env:SM64DS_LEVEL='1';$env:SM64DS_WINDOW_SELFTEST=if($Hole -or $Conduit){'300'}elseif($Animated){'240'}else{'180'};$env:SM64DS_SELFTEST_IDLE='1';$env:SM64DS_NO_DIALOG='1';$env:SM64DS_NO_PLAYLOG='1';$env:SM64DS_VOLUME='0'
$env:SM64DS_SKIP_INTRO='1';$env:SM64DS_SKIP_MENU='1'
$taskName=if($Stock){'stock'}elseif($Hole){'hole'}elseif($Swim){'swim'}elseif($Boat){'boat'}elseif($Flight){'flight'}elseif($Health){'health'}elseif($Craft){'craft'}elseif($Conduit){'conduit'}elseif($Animated){'animated'}elseif($Special){'special'}elseif($Shape){$Shape}else{'native'}
$taskOut="$taskRoot/build/steve/$taskName-test.log";$taskErr="$taskRoot/build/steve/$taskName-test-errors.log"
$taskGame=Start-Process -FilePath "$taskRoot/build/port/walk_window.exe" -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $taskOut -RedirectStandardError $taskErr
if($taskGame.ExitCode -ne 0){throw "Native test exited with $($taskGame.ExitCode)"}
Copy-Item -LiteralPath "$taskRoot/walk_window_selftest.bmp" -Destination "$taskRoot/build/steve/$taskName-test.bmp"
Select-String -LiteralPath $taskOut -Pattern '^selftest:'
Select-String -LiteralPath $taskErr -Pattern '\[steve'
$taskTrace=Get-Content -LiteralPath $taskErr -Raw
if($taskTrace -match '\[quarantine\]|FATAL:|UNHOSTED:'){throw 'A native actor fault invalidated the run'}
if($Conduit -and $taskTrace -notmatch '\[steve-conduit\] full-frame active=1 hunting=1'){throw 'Conduit never activated in-game'}
if($Animated){
    foreach($taskPattern in @('frame=105 kind=chest progress=1.00','frame=140 kind=chest progress=0.00','frame=170 kind=shulker progress=1.00','kind=sign progress=0.00 text=[1-9]','patterns=base:.*cross:.*border:.*circle:')){if($taskTrace -notmatch $taskPattern){throw "Block entity verification missing: $taskPattern"}}
}
if($Craft -and $taskTrace -notmatch '\[steve-craft\] width=3 output=oak_door count=3'){throw 'Native crafting result missing'}
if($Flight -and $taskTrace -notmatch 'flags=(2|3) native='){throw 'Native flight never became authoritative'}
if($Boat -and $taskTrace -notmatch 'flags=(4|5) native='){throw 'Native boat passenger never synchronized'}
if($Swim -and $taskTrace -notmatch 'flags=(16|17) native='){throw 'Native swimming never became authoritative'}
if($SwimCoin){
    foreach($taskPattern in @('\[steve-air-test\].*underwater=1','\[steve-air\] underwater coins=1 refill=300 bubbles=6 before=([0-9]+)','\[steve-air\] acknowledged air=29[5-9]|\[steve-air\] acknowledged air=300')){
        if($taskTrace -notmatch $taskPattern){throw "Underwater coin verification missing: $taskPattern"}
    }
    $taskAirBefore=[int]([regex]::Match($taskTrace,'bubbles=6 before=([0-9]+)').Groups[1].Value)
    if($taskAirBefore -ge 295){throw 'Coin test did not first deplete oxygen'}
    $taskAirAfter=[int]([regex]::Match($taskTrace,'acknowledged air=([0-9]+)').Groups[1].Value)
    @{nativeSwimming=$true;nativeCoinPickup=$true;airBefore=$taskAirBefore;airAcknowledged=$taskAirAfter;bubbles=6;faults=0;frames=180;scene=$Scene} | ConvertTo-Json | Set-Content -LiteralPath "$taskRoot/build/steve/swim-coin-test.json"
}
if($Health){
    $taskHealthSamples=[regex]::Matches($taskTrace,'health=([0-9.]+) hp=') | ForEach-Object {[double]$_.Groups[1].Value}
    if(-not ($taskHealthSamples | Where-Object {$_ -ge 17.4 -and $_ -lt 19.5}) -or $taskTrace -notmatch 'health=20.00 hp=8'){throw 'Native damage/healing did not synchronize'}
}
if($Shape -and $taskTrace -notmatch 'shapes=1'){throw 'Official collision shapes not loaded'}
if($Shape -or $Hole){
    $taskPosition=[regex]::Match((Get-Content -LiteralPath $taskOut -Raw),'selftest: .*pos=\((-?[0-9]+), (-?[0-9]+), (-?[0-9]+)\)')
    $taskY=[double]$taskPosition.Groups[2].Value/4096
    $taskExpectedY=if($Hole){200}elseif($Shape -eq 'fence'){350}else{250}
    if([Math]::Abs($taskY-$taskExpectedY) -gt 1){throw "Native collision height $taskY, expected $taskExpectedY"}
    if($Hole -and $taskTrace -notmatch '\[steve-after\] y=200\.00 previous=200\.00 ground=200\.00 fall=0\.00'){throw 'Carved-floor landing did not remain stable'}
}
& python "$PSScriptRoot/check_budget.py"

} finally {
    foreach($taskName in $taskEnvNames){[Environment]::SetEnvironmentVariable($taskName,$taskEnvBackup[$taskName])}
}
