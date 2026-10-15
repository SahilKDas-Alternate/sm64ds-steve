$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$taskJava = 'C:/Program Files/Microsoft/jdk-17.0.14.7-hotspot/bin'
if (-not (Test-Path -LiteralPath "$taskJava/javac.exe")) { throw 'Install a Java 17 JDK before building the Minecraft 1.18 adapter.' }
New-Item -ItemType Directory -Force -Path "$taskRoot/build/steve/java","$taskRoot/runtime/minecraft" | Out-Null
& "$taskJava/javac.exe" --add-exports java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED -source 17 -target 17 -d "$taskRoot/build/steve/java" (Get-ChildItem -LiteralPath "$PSScriptRoot/src/steve" -Filter '*.java').FullName
if ($LASTEXITCODE -ne 0) { throw 'Minecraft adapter compilation failed.' }
@"
Manifest-Version: 1.0
Premain-Class: steve.Agent
Can-Redefine-Classes: false
Can-Retransform-Classes: false

"@ | Set-Content -LiteralPath "$taskRoot/build/steve/agent-manifest.mf" -Encoding ascii
& "$taskJava/jar.exe" "-J-Djava.io.tmpdir=$taskRoot/build/steve" --create --file "$taskRoot/runtime/minecraft/steve-agent.jar" --manifest "$taskRoot/build/steve/agent-manifest.mf" -C "$taskRoot/build/steve/java" .
if ($LASTEXITCODE -ne 0) { throw 'Minecraft adapter packaging failed.' }
New-Item -ItemType Directory -Force -Path "$taskRoot/build/steve/backend/steve" | Out-Null
Get-ChildItem -LiteralPath "$taskRoot/build/steve/java/steve" -Filter '*.class' | Where-Object { $_.Name -notlike 'Agent*' } | Copy-Item -Destination "$taskRoot/build/steve/backend/steve"
& "$taskJava/jar.exe" "-J-Djava.io.tmpdir=$taskRoot/build/steve" --create --file "$taskRoot/runtime/minecraft/steve-backend.jar" -C "$taskRoot/build/steve/backend" .
if ($LASTEXITCODE -ne 0) { throw 'Minecraft backend packaging failed.' }
& python "$taskRoot/port/steve/check_budget.py"
if ($LASTEXITCODE -ne 0) { throw 'Workspace exceeds 1 GiB.' }
