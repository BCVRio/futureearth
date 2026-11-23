# Streams FutureEarth from this PC to a phone browser on the same Wi-Fi.
# Starts Epic's signalling server (built copy in ../streaming) and Unreal, then
# prints the address to open on the phone. Close the Unreal window to stop;
# the signalling server is stopped with it.
#
#   powershell -File phone\start_phone_stream.ps1 [-Map RealWorld|TestStreet] [-Year 2126]
param(
    [string]$Engine = "F:\Epic511\Unreal511\UE_5.8",
    [ValidateSet("RealWorld", "TestStreet")][string]$Map = "RealWorld",
    [int]$Year = 2126,
    [int]$HttpsPort = 8443,
    [int]$HttpPort = 8080,
    [int]$Width = 720,
    [int]$Height = 1280
)

$repo = Split-Path -Parent $PSScriptRoot
$server = Join-Path $repo "streaming\SignallingWebServer"
$www = Join-Path $server "www"
$certs = Join-Path $server "certificates"
$proj = Join-Path $repo "unreal\FutureEarth\FutureEarth.uproject"
if (-not (Test-Path (Join-Path $server "dist\index.js"))) { Write-Output "Signalling server is not built. See README, 'Phone window'."; exit 1 }

# This PC's address on the local network.
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -match '^(192\.168|10\.|172\.(1[6-9]|2\d|3[01]))\.' -and $_.PrefixOrigin -ne 'WellKnown' } | Select-Object -First 1).IPAddress
if (-not $ip) { Write-Output "No local network address found."; exit 1 }

# iPhone motion sensors only work on https, so make a self-signed certificate for this address.
$key = Join-Path $certs "futureearth-key.pem"; $crt = Join-Path $certs "futureearth-cert.pem"; $stamp = Join-Path $certs "futureearth-ip.txt"
if (-not (Test-Path $crt) -or (Get-Content $stamp -ErrorAction SilentlyContinue) -ne $ip) {
    New-Item -ItemType Directory -Force $certs | Out-Null
    $openssl = "C:\Program Files\Git\usr\bin\openssl.exe"
    if (-not (Test-Path $openssl)) { $openssl = "openssl" }
    & $openssl req -x509 -newkey rsa:2048 -nodes -days 365 -keyout $key -out $crt -subj "/CN=FutureEarth local" -addext "subjectAltName=IP:$ip,DNS:localhost" 2>$null
    if (-not (Test-Path $crt)) { Write-Output "Could not create a certificate (openssl not found?)."; exit 1 }
    Set-Content -Path $stamp -Value $ip -Encoding ascii
}

Copy-Item (Join-Path $PSScriptRoot "phone.html"), (Join-Path $PSScriptRoot "futureearth-phone.js") $www -Force

# The server resolves certificate paths relative to its own folder, so they are passed relative.
$signalling = Start-Process -FilePath "node" -WorkingDirectory $server -PassThru -WindowStyle Hidden -ArgumentList `
    "dist/index.js", "--serve", "--http_root", "`"$www`"", "--homepage", "phone.html", "--player_port", $HttpPort, `
    "--https", "--https_port", $HttpsPort, "--ssl_key_path", "certificates/futureearth-key.pem", "--ssl_cert_path", "certificates/futureearth-cert.pem", "--console_messages", "basic" `
    -RedirectStandardOutput (Join-Path $env:TEMP "futureearth-signalling.log") -RedirectStandardError (Join-Path $env:TEMP "futureearth-signalling.err.log")

if (-not $env:GOOGLE_MAPS_API_KEY) { $env:GOOGLE_MAPS_API_KEY = [Environment]::GetEnvironmentVariable("GOOGLE_MAPS_API_KEY", "User") }
$unreal = Start-Process -FilePath (Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe") -PassThru -ArgumentList `
    "`"$proj`"", "/Game/FutureEarth/Maps/$Map", "-game", "-windowed", "-ResX=$Width", "-ResY=$Height", "-FutureYear=$Year", `
    "-PixelStreamingConnectionURL=ws://127.0.0.1:8888", "-nosound", "-nosplash", "-abslog=`"$(Join-Path $env:TEMP 'futureearth-stream.log')`""

$query = "?AutoConnect=true&AutoPlayVideo=true&StartVideoMuted=true&MatchViewportRes=true"
Write-Output ""
Write-Output "On the phone (same Wi-Fi) open:   https://${ip}:$HttpsPort/$query"
Write-Output "On this PC, to check the stream:  http://localhost:$HttpPort/$query"
Write-Output ""
Write-Output "The phone will warn that the connection is not private: that is the self-signed"
Write-Output "certificate made on this PC. Choose Show Details, then visit this website."
Write-Output "Then tap 'Enable motion' and allow it. Close the Unreal window to stop."
$unreal.WaitForExit()
if (-not $signalling.HasExited) { $signalling.Kill() }
