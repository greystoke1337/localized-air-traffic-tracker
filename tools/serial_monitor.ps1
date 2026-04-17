param(
    [string]$Port    = "COM8",
    [int]   $Seconds = 60
)
$serial = New-Object System.IO.Ports.SerialPort($Port, 115200)
$serial.ReadTimeout = 2000
$serial.Open()
$serial.DtrEnable = $true
$serial.RtsEnable = $true
Start-Sleep -Milliseconds 100
$serial.RtsEnable = $false
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
    try {
        $line = $serial.ReadLine()
        Write-Host $line
    } catch { }
}
$serial.Close()
