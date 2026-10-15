param([int]$Scene=985)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskResults=@()
$taskCases=@(
    @{Name='hole';Options=@{Hole=$true}},
    @{Name='slab';Options=@{Shape='slab'}},
    @{Name='fence';Options=@{Shape='fence'}},
    @{Name='swim';Options=@{Swim=$true}},
    @{Name='boat';Options=@{Boat=$true}},
    @{Name='flight';Options=@{Flight=$true}},
    @{Name='craft';Options=@{Craft=$true}},
    @{Name='health';Options=@{Health=$true}},
    @{Name='special';Options=@{Special=$true}},
    @{Name='stock';Options=@{Stock=$true}}
)
foreach($taskCase in $taskCases) {
    $taskOptions=$taskCase.Options;$taskOptions.Scene=$Scene++
    Write-Output "START $($taskCase.Name) scene=$($taskOptions.Scene)"
    try {
        & "$PSScriptRoot/test-native.ps1" @taskOptions > "$taskRoot/build/steve/recheck-$($taskCase.Name).log" 2>&1
        $taskResults+=@{name=$taskCase.Name;passed=$true;scene=$taskOptions.Scene}
        Write-Output "PASS $($taskCase.Name)"
    } catch {
        $taskResults+=@{name=$taskCase.Name;passed=$false;scene=$taskOptions.Scene;error="$($_.Exception.Message)"}
        Write-Output "FAIL $($taskCase.Name): $($_.Exception.Message)"
    }
}
$taskResults | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$taskRoot/build/steve/native-regression.json"
if($taskResults | Where-Object {-not $_.passed}) {throw 'Native regression failures; inspect build/steve/native-regression.json'}
