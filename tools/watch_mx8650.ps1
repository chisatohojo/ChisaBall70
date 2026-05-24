param(
  [string]$Port = "auto",
  [int]$Baud = 115200,
  [int]$DurationSeconds = 0,
  [switch]$ShowZero
)

$ErrorActionPreference = "Stop"

function Find-XiaoPort {
  $ports = Get-PnpDevice -PresentOnly -Class Ports -ErrorAction SilentlyContinue |
    Where-Object {
      $_.InstanceId -match "VID_303A&PID_1001|VID_2886&PID_0046|VID_2886&PID_8046" -or
      $_.FriendlyName -match "USB|XIAO|ESP32|Serial"
    } |
    ForEach-Object {
      if ($_.FriendlyName -match "\(COM\d+\)") {
        [pscustomobject]@{
          Port = $Matches[0].Trim("(", ")")
          Name = $_.FriendlyName
          InstanceId = $_.InstanceId
        }
      }
    } |
    Where-Object { $_.Port -notin @("COM1", "COM2") } |
    Sort-Object {
      if ($_.InstanceId -match "VID_303A&PID_1001") { 0 }
      elseif ($_.InstanceId -match "VID_2886") { 1 }
      else { 2 }
    }, Port

  $first = $ports | Select-Object -First 1
  if (-not $first) {
    throw "XIAO/ESP32C3 COM port was not found. Check the USB connection and data-capable cable."
  }

  return $first
}

if ($Port -eq "auto") {
  $detected = Find-XiaoPort
  $Port = $detected.Port
  Write-Host "Using $Port - $($detected.Name)"
} else {
  Write-Host "Using $Port"
}

Write-Host "Baud: $Baud"
Write-Host "DTR=true RTS=false for XIAO ESP32C3 app serial"
Write-Host "Press Ctrl+C to stop."
Write-Host ""

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
$serial.ReadTimeout = 200
$serial.WriteTimeout = 200
$serial.DtrEnable = $true
$serial.RtsEnable = $false

$lineBuffer = ""
$lastZeroPrint = Get-Date
$deadline = $null
if ($DurationSeconds -gt 0) {
  $deadline = (Get-Date).AddSeconds($DurationSeconds)
}

try {
  $serial.Open()
  Start-Sleep -Milliseconds 300

  while ($true) {
    if ($deadline -and (Get-Date) -ge $deadline) {
      break
    }

    $chunk = $serial.ReadExisting()
    if ($chunk.Length -eq 0) {
      Start-Sleep -Milliseconds 30
      continue
    }

    $lineBuffer += $chunk
    while ($lineBuffer.Contains("`n")) {
      $idx = $lineBuffer.IndexOf("`n")
      $line = $lineBuffer.Substring(0, $idx).Trim("`r")
      $lineBuffer = $lineBuffer.Substring($idx + 1)

      if ($line.Length -eq 0) {
        continue
      }

      if ($line -match "^dx=(-?\d+), dy=(-?\d+)(.*)$") {
        $dx = [int]$Matches[1]
        $dy = [int]$Matches[2]
        $extra = $Matches[3]

        if ($ShowZero -or $dx -ne 0 -or $dy -ne 0) {
          $stamp = Get-Date -Format "HH:mm:ss.fff"
          Write-Host "$stamp  dx=$dx, dy=$dy$extra"
        } elseif (((Get-Date) - $lastZeroPrint).TotalSeconds -ge 1) {
          $lastZeroPrint = Get-Date
          $stamp = Get-Date -Format "HH:mm:ss.fff"
          Write-Host "$stamp  dx=0, dy=0"
        }
      } elseif ($line -match "^(debug|pid1=|MX8650A|sdio_pin=|WARN:)") {
        Write-Host $line
      } elseif ($line -match "^ESP-ROM") {
        Write-Warning "ESP32-C3 is in USB bootloader mode. Release BOOT, press RESET, then start the monitor again."
        Write-Host $line
      }
    }
  }
}
finally {
  if ($serial -and $serial.IsOpen) {
    $serial.Close()
  }
}
