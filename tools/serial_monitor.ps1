$port = New-Object System.IO.Ports.SerialPort('COM7', 115200)
$port.ReadTimeout = 2000
$port.Open()
$port.DtrEnable = $true
$port.RtsEnable = $true
Start-Sleep -Milliseconds 100
$port.RtsEnable = $false
$deadline = (Get-Date).AddSeconds(15)
while ((Get-Date) -lt $deadline) {
    try {
        $line = $port.ReadLine()
        Write-Host $line
    } catch { }
}
$port.Close()
