param(
    [Parameter(Mandatory = $true)][string] $InputPath,
    [Parameter(Mandatory = $true)][string] $OutputPath
)

$ErrorActionPreference = 'Stop'
$image = [IO.File]::ReadAllBytes($InputPath)
if ($image.Length -lt 32 -or [BitConverter]::ToUInt32($image, 0) -ne 0x3153454E -or
    [BitConverter]::ToUInt32($image, 4) -ne 1 -or
    [BitConverter]::ToUInt32($image, 8) -ne $image.Length -or
    [BitConverter]::ToUInt32($image, 20) -ne 0) {
    throw 'Invalid InfoNES image header'
}

$bssOffset = [BitConverter]::ToUInt32($image, 12)
$bssSize = [BitConverter]::ToUInt32($image, 16)
if ($bssOffset -lt $image.Length -or $bssOffset + $bssSize -gt 65536) {
    throw 'InfoNES state exceeds its SRAM region'
}
foreach ($entryOffset in 24, 28) {
    $entry = [BitConverter]::ToUInt32($image, $entryOffset)
    if (($entry -band 1) -eq 0 -or $entry -lt 0x20010020 -or $entry -ge (0x20010000 + $image.Length)) {
        throw 'InfoNES entry point is outside its program'
    }
}

$crc = [uint32]::MaxValue
$polynomial = [uint32]3988292384
foreach ($value in $image) {
    $crc = $crc -bxor $value
    for ($bit = 0; $bit -lt 8; ++$bit) {
        if ($crc -band 1) {
            $crc = ($crc -shr 1) -bxor $polynomial
        } else {
            $crc = $crc -shr 1
        }
    }
}
[BitConverter]::GetBytes([uint32]($crc -bxor [uint32]::MaxValue)).CopyTo($image, 20)
[IO.File]::WriteAllBytes($OutputPath, $image)
