param(
    [Parameter(Mandatory = $true)]
    [string] $InputPath,

    [Parameter(Mandatory = $true)]
    [string] $OutputPath,

    [string[]] $ExcludeRange = @("0xAF400000-0xAF405FFF")
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = "Stop"

function Convert-HexInt {
    param([Parameter(Mandatory = $true)][string] $Value)

    $text = $Value.Trim()
    if ($text.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
        return [uint64][Convert]::ToUInt64($text.Substring(2), 16)
    }

    return [uint64][Convert]::ToUInt64($text, 16)
}

function Convert-HexBytes {
    param([Parameter(Mandatory = $true)][string] $Text)

    if (($Text.Length % 2) -ne 0) {
        throw "Invalid Intel HEX record length."
    }

    $bytes = New-Object byte[] ($Text.Length / 2)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [Convert]::ToByte($Text.Substring($i * 2, 2), 16)
    }

    return ,$bytes
}

function ConvertTo-HexBytes {
    param([Parameter(Mandatory = $true)][byte[]] $Bytes)

    $builder = [System.Text.StringBuilder]::new($Bytes.Length * 2)
    foreach ($byte in $Bytes) {
        [void]$builder.AppendFormat("{0:X2}", $byte)
    }

    return $builder.ToString()
}

function New-HexRecord {
    param(
        [Parameter(Mandatory = $true)][uint16] $Offset,
        [Parameter(Mandatory = $true)][byte] $RecordType,
        [Parameter(Mandatory = $true)][byte[]] $Data
    )

    if ($Data.Length -gt 255) {
        throw "Intel HEX data record is too large: $($Data.Length) bytes."
    }

    $sum = $Data.Length + (($Offset -shr 8) -band 0xFF) + ($Offset -band 0xFF) + $RecordType
    foreach ($byte in $Data) {
        $sum += $byte
    }

    $checksum = ((- $sum) -band 0xFF)
    return ":{0:X2}{1:X4}{2:X2}{3}{4:X2}" -f $Data.Length, $Offset, $RecordType, (ConvertTo-HexBytes $Data), $checksum
}

function Read-ExcludeRanges {
    param([Parameter(Mandatory = $true)][string[]] $Ranges)

    $parsed = @()
    foreach ($range in $Ranges) {
        $parts = $range -split "-", 2
        if ($parts.Count -ne 2) {
            throw "Invalid exclude range '$range'. Expected START-END."
        }

        $start = Convert-HexInt $parts[0]
        $endInclusive = Convert-HexInt $parts[1]
        if ($endInclusive -lt $start) {
            throw "Invalid exclude range '$range'. End is lower than start."
        }

        $parsed += [pscustomobject]@{
            Start = $start
            End = $endInclusive + 1
            Text = ("0x{0:X8}-0x{1:X8}" -f $start, $endInclusive)
        }
    }

    return $parsed | Sort-Object Start
}

function Get-KeptSlices {
    param(
        [Parameter(Mandatory = $true)][uint64] $RecordStart,
        [Parameter(Mandatory = $true)][int] $Length,
        [Parameter(Mandatory = $true)] $Ranges
    )

    $slices = @([pscustomobject]@{ Offset = 0; Length = $Length })

    foreach ($range in $Ranges) {
        $nextSlices = @()
        foreach ($slice in $slices) {
            $sliceStart = $RecordStart + [uint64]$slice.Offset
            $sliceEnd = $sliceStart + [uint64]$slice.Length

            if ($sliceEnd -le $range.Start -or $sliceStart -ge $range.End) {
                $nextSlices += $slice
                continue
            }

            if ($sliceStart -lt $range.Start) {
                $nextSlices += [pscustomobject]@{
                    Offset = $slice.Offset
                    Length = [int]($range.Start - $sliceStart)
                }
            }

            if ($sliceEnd -gt $range.End) {
                $newOffset = $slice.Offset + [int]($range.End - $sliceStart)
                $nextSlices += [pscustomobject]@{
                    Offset = $newOffset
                    Length = [int]($sliceEnd - $range.End)
                }
            }
        }

        $slices = $nextSlices
    }

    return $slices | Where-Object { $_.Length -gt 0 } | Sort-Object Offset
}

function Test-RangeOverlap {
    param(
        [Parameter(Mandatory = $true)][uint64] $Start,
        [Parameter(Mandatory = $true)][uint64] $End,
        [Parameter(Mandatory = $true)] $Ranges
    )

    foreach ($range in $Ranges) {
        if ($End -le $range.Start) {
            return $false
        }
        if ($Start -lt $range.End -and $End -gt $range.Start) {
            return $true
        }
    }

    return $false
}

$inputFile = (Resolve-Path -LiteralPath $InputPath).ProviderPath
$outputFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$excludeRanges = Read-ExcludeRanges $ExcludeRange

$baseAddress = [uint64]0
$outputLines = [System.Collections.Generic.List[string]]::new()
$removedBytes = 0

foreach ($line in [System.IO.File]::ReadLines($inputFile)) {
    $trimmed = $line.Trim()
    if ($trimmed.Length -eq 0) {
        continue
    }

    if (-not $trimmed.StartsWith(":", [System.StringComparison]::Ordinal)) {
        throw "$InputPath contains a non-Intel-HEX line: '$trimmed'"
    }

    if ($trimmed.Length -lt 11) {
        throw "$InputPath contains a short Intel HEX record: '$trimmed'"
    }

    $count = [Convert]::ToInt32($trimmed.Substring(1, 2), 16)
    $offset = [Convert]::ToInt32($trimmed.Substring(3, 4), 16)
    $recordType = [byte][Convert]::ToInt32($trimmed.Substring(7, 2), 16)

    if ($trimmed.Length -ne (11 + ($count * 2))) {
        throw "$InputPath contains a byte-count mismatch: '$trimmed'"
    }

    if ($recordType -eq 0x00) {
        $absoluteStart = $baseAddress + [uint64]$offset
        $absoluteEnd = $absoluteStart + [uint64]$count

        if (-not (Test-RangeOverlap -Start $absoluteStart -End $absoluteEnd -Ranges $excludeRanges)) {
            $outputLines.Add($trimmed)
            continue
        }

        [byte[]]$raw = Convert-HexBytes $trimmed.Substring(1)
        $sum = 0
        foreach ($byte in $raw) {
            $sum = ($sum + $byte) -band 0xFF
        }
        if ($sum -ne 0) {
            throw "$InputPath contains a checksum mismatch: '$trimmed'"
        }

        [byte[]]$data = @()
        if ($count -gt 0) {
            $data = New-Object byte[] $count
            [Array]::Copy($raw, 4, $data, 0, $count)
        }

        $keptSlices = @(Get-KeptSlices -RecordStart $absoluteStart -Length $data.Length -Ranges $excludeRanges)
        $keptBytes = 0

        foreach ($slice in $keptSlices) {
            $chunkOffset = $offset + $slice.Offset
            if (($chunkOffset + $slice.Length) -gt 0x10000) {
                throw "$InputPath contains a data record crossing a 64 KiB Intel HEX boundary."
            }

            $remaining = $slice.Length
            $sourceOffset = $slice.Offset
            while ($remaining -gt 0) {
                $chunkLength = [Math]::Min(16, $remaining)
                $chunk = New-Object byte[] $chunkLength
                [Array]::Copy($data, $sourceOffset, $chunk, 0, $chunkLength)
                $outputLines.Add((New-HexRecord -Offset ([uint16]($offset + $sourceOffset)) -RecordType 0x00 -Data $chunk))
                $sourceOffset += $chunkLength
                $remaining -= $chunkLength
                $keptBytes += $chunkLength
            }
        }

        $removedBytes += ($data.Length - $keptBytes)
        continue
    }

    if ($recordType -eq 0x02) {
        if ($count -ne 2) {
            throw "$InputPath contains an invalid extended segment address record."
        }
        $baseValue = [uint64][Convert]::ToUInt64($trimmed.Substring(9, 4), 16)
        $baseAddress = $baseValue -shl 4
    }
    elseif ($recordType -eq 0x04) {
        if ($count -ne 2) {
            throw "$InputPath contains an invalid extended linear address record."
        }
        $baseValue = [uint64][Convert]::ToUInt64($trimmed.Substring(9, 4), 16)
        $baseAddress = $baseValue -shl 16
    }

    $outputLines.Add($trimmed)
}

$outputDirectory = Split-Path -Parent $outputFile
if ($outputDirectory) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$tempFile = "$outputFile.tmp"
[System.IO.File]::WriteAllLines($tempFile, $outputLines, [System.Text.Encoding]::ASCII)
Move-Item -LiteralPath $tempFile -Destination $outputFile -Force

$rangeText = ($excludeRanges | ForEach-Object { $_.Text }) -join ", "
Write-Host ("Filtered {0} -> {1}; removed {2} byte(s) from {3}." -f $InputPath, $OutputPath, $removedBytes, $rangeText)
