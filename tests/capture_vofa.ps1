param(
    [string]$PortName = "COM8",
    [double]$ServoKp = -1,
    [double]$ServoKd = -1,
    [int]$Seconds = 20
)

$port = New-Object System.IO.Ports.SerialPort $PortName, 115200, 'None', 8, 'One'
$port.ReadTimeout = 200
$samples = New-Object System.Collections.Generic.List[object]

try {
    $port.Open()
    $port.DiscardInBuffer()

    if ($ServoKp -ge 0 -and $ServoKd -ge 0) {
        $port.Write(("SERVO,{0},{1}`n" -f $ServoKp, $ServoKd))
    }
    $port.Write("STREAM,1`n")
    Start-Sleep -Milliseconds 300

    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $parts = $port.ReadLine().Trim().Split(',')
            if ($parts.Count -ne 8) {
                continue
            }

            $values = @()
            $valid = $true
            foreach ($part in $parts) {
                $number = 0
                if ([int]::TryParse($part.Trim(), [ref]$number)) {
                    $values += $number
                }
                else {
                    $valid = $false
                    break
                }
            }

            if ($valid) {
                $samples.Add([pscustomobject]@{
                    LT = $values[0]
                    LA = $values[1]
                    LPWM = $values[2]
                    RT = $values[3]
                    RA = $values[4]
                    RPWM = $values[5]
                    ERR = $values[6]
                    SERVO = $values[7]
                })
            }
        }
        catch [System.TimeoutException] {
        }
    }

    $running = @($samples | Where-Object { $_.LT -ne 0 -or $_.RT -ne 0 })
    Write-Output "VALID=$($samples.Count) RUNNING=$($running.Count) SERVO=$ServoKp,$ServoKd"
    if ($running.Count -eq 0) {
        Write-Output "NO_RUNNING_DATA"
        exit 0
    }

    $leftMae = (($running | ForEach-Object { [math]::Abs($_.LT - $_.LA) }) |
        Measure-Object -Average).Average / 10
    $rightMae = (($running | ForEach-Object { [math]::Abs($_.RT - $_.RA) }) |
        Measure-Object -Average).Average / 10
    $absoluteErrors = @($running | ForEach-Object { [math]::Abs($_.ERR) })
    $errorMean = ($absoluteErrors | Measure-Object -Average).Average
    $errorMax = ($absoluteErrors | Measure-Object -Maximum).Maximum
    $straight = @($running | Where-Object { [math]::Abs($_.ERR) -lt 10 }).Count
    $medium = @($running | Where-Object {
        [math]::Abs($_.ERR) -ge 10 -and [math]::Abs($_.ERR) -lt 20
    }).Count
    $hard = @($running | Where-Object { [math]::Abs($_.ERR) -ge 20 }).Count
    $servoMin = ($running | Measure-Object SERVO -Minimum).Minimum
    $servoMax = ($running | Measure-Object SERVO -Maximum).Maximum
    $servoLimit = @($running | Where-Object {
        $_.SERVO -le 672 -or $_.SERVO -ge 828
    }).Count
    $leftSaturation = @($running | Where-Object {
        [math]::Abs($_.LPWM) -ge 2450
    }).Count
    $rightSaturation = @($running | Where-Object {
        [math]::Abs($_.RPWM) -ge 2450
    }).Count

    $crossings = 0
    $lastSign = 0
    foreach ($sample in $running) {
        if ($sample.ERR -gt 3) {
            $sign = 1
        }
        elseif ($sample.ERR -lt -3) {
            $sign = -1
        }
        else {
            $sign = 0
        }
        if ($sign -ne 0 -and $lastSign -ne 0 -and $sign -ne $lastSign) {
            $crossings++
        }
        if ($sign -ne 0) {
            $lastSign = $sign
        }
    }

    Write-Output ("SPEED_MAE left={0:N2} right={1:N2} PWM_SAT left={2} right={3}" -f
        $leftMae, $rightMae, $leftSaturation, $rightSaturation)
    Write-Output ("LINE_ERROR mean_abs={0:N2} max_abs={1} straight={2} medium={3} hard={4} sign_crossings={5}" -f
        $errorMean, $errorMax, $straight, $medium, $hard, $crossings)
    Write-Output "SERVO min=$servoMin max=$servoMax near_limit=$servoLimit"
}
finally {
    if ($port.IsOpen) {
        $port.Close()
    }
    $port.Dispose()
}
