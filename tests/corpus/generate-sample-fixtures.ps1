param()

$samplesDir = Join-Path $PSScriptRoot "samples"

function Write-Le16 {
  param(
    [byte[]]$Bytes,
    [int]$Offset,
    [UInt16]$Value
  )

  $Bytes[$Offset] = [byte]($Value -band 0xFF)
  $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Write-Le32 {
  param(
    [byte[]]$Bytes,
    [int]$Offset,
    [UInt32]$Value
  )

  Write-Le16 -Bytes $Bytes -Offset $Offset -Value ([UInt16]($Value -band 0xFFFF))
  Write-Le16 -Bytes $Bytes -Offset ($Offset + 2) -Value ([UInt16](($Value -shr 16) -band 0xFFFF))
}

function Write-Le64 {
  param(
    [byte[]]$Bytes,
    [int]$Offset,
    [UInt64]$Value
  )

  Write-Le32 -Bytes $Bytes -Offset $Offset -Value ([UInt32]($Value -band 0xFFFFFFFF))
  Write-Le32 -Bytes $Bytes -Offset ($Offset + 4) -Value ([UInt32](($Value -shr 32) -band 0xFFFFFFFF))
}

function Write-Ascii {
  param(
    [byte[]]$Bytes,
    [int]$Offset,
    [string]$Text
  )

  $textBytes = [System.Text.Encoding]::ASCII.GetBytes($Text)
  [Array]::Copy($textBytes, 0, $Bytes, $Offset, $textBytes.Length)
}

function Write-Utf16Le {
  param(
    [byte[]]$Bytes,
    [int]$Offset,
    [string]$Text
  )

  $textBytes = [System.Text.Encoding]::Unicode.GetBytes($Text)
  [Array]::Copy($textBytes, 0, $Bytes, $Offset, $textBytes.Length)
}

function Write-Bytes {
  param(
    [byte[]]$Target,
    [int]$Offset,
    [byte[]]$Source
  )

  [Array]::Copy($Source, 0, $Target, $Offset, $Source.Length)
}

function Write-NxSuperblock {
  param(
    [byte[]]$Bytes,
    [int]$BaseOffset,
    [UInt64]$Xid,
    [UInt64]$BlockCount,
    [UInt64]$VolumeObjectId
  )

  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x08) -Value 1
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x10) -Value $Xid
  Write-Ascii -Bytes $Bytes -Offset ($BaseOffset + 0x20) -Text "NXSB"
  Write-Le32 -Bytes $Bytes -Offset ($BaseOffset + 0x24) -Value 4096
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x28) -Value $BlockCount
  Write-Bytes -Target $Bytes -Offset ($BaseOffset + 0x48) -Source ([byte[]](0x10,0x11,0x12,0x13,0x20,0x21,0x22,0x23,0x30,0x31,0x32,0x33,0x40,0x41,0x42,0x43))
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x60) -Value ($Xid + 1)
  Write-Le32 -Bytes $Bytes -Offset ($BaseOffset + 0x68) -Value 1
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x70) -Value 1
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x98) -Value 5
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0xA0) -Value 6
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0xA8) -Value 7
  Write-Le32 -Bytes $Bytes -Offset ($BaseOffset + 0xB4) -Value 100
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0xB8) -Value $VolumeObjectId
}

function Write-VolumeSuperblock {
  param(
    [byte[]]$Bytes,
    [int]$BaseOffset,
    [UInt64]$ObjectId,
    [UInt64]$IncompatibleFeatures,
    [UInt16]$Role,
    [string]$Name
  )

  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x08) -Value $ObjectId
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x10) -Value 42
  Write-Ascii -Bytes $Bytes -Offset ($BaseOffset + 0x20) -Text "APSB"
  Write-Le64 -Bytes $Bytes -Offset ($BaseOffset + 0x38) -Value $IncompatibleFeatures
  Write-Bytes -Target $Bytes -Offset ($BaseOffset + 0xF0) -Source ([byte[]](0x50,0x51,0x52,0x53,0x60,0x61,0x62,0x63,0x70,0x71,0x72,0x73,0x80,0x81,0x82,0x83))
  Write-Ascii -Bytes $Bytes -Offset ($BaseOffset + 0x2C0) -Text $Name
  Write-Le16 -Bytes $Bytes -Offset ($BaseOffset + 0x3C4) -Value $Role
}

function New-DirectFixture {
  param(
    [string]$Path,
    [string]$VolumeName,
    [UInt64]$IncompatibleFeatures,
    [UInt16]$Role
  )

  $bytes = New-Object byte[] (4096 * 8)
  Write-NxSuperblock -Bytes $bytes -BaseOffset 0 -Xid 1 -BlockCount 8 -VolumeObjectId 77
  Write-NxSuperblock -Bytes $bytes -BaseOffset 4096 -Xid 42 -BlockCount 8 -VolumeObjectId 77
  Write-VolumeSuperblock -Bytes $bytes -BaseOffset (4096 * 2) -ObjectId 77 -IncompatibleFeatures $IncompatibleFeatures -Role $Role -Name $VolumeName
  [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function New-GptFixture {
  param([string]$Path)

  $logicalBlockSize = 512
  $bytes = New-Object byte[] ($logicalBlockSize * 256)
  $firstLba = [UInt64]40
  $lastLba = [UInt64]103

  Write-Ascii -Bytes $bytes -Offset $logicalBlockSize -Text "EFI PART"
  Write-Le32 -Bytes $bytes -Offset ($logicalBlockSize + 8) -Value 0x00010000
  Write-Le32 -Bytes $bytes -Offset ($logicalBlockSize + 12) -Value 92
  Write-Le64 -Bytes $bytes -Offset ($logicalBlockSize + 24) -Value 1
  Write-Le64 -Bytes $bytes -Offset ($logicalBlockSize + 32) -Value 255
  Write-Le64 -Bytes $bytes -Offset ($logicalBlockSize + 40) -Value 34
  Write-Le64 -Bytes $bytes -Offset ($logicalBlockSize + 48) -Value 200
  Write-Le64 -Bytes $bytes -Offset ($logicalBlockSize + 72) -Value 2
  Write-Le32 -Bytes $bytes -Offset ($logicalBlockSize + 80) -Value 1
  Write-Le32 -Bytes $bytes -Offset ($logicalBlockSize + 84) -Value 128

  $partitionOffset = $logicalBlockSize * 2
  Write-Bytes -Target $bytes -Offset $partitionOffset -Source ([byte[]](0xEF,0x57,0x34,0x7C,0x00,0x00,0xAA,0x11,0xAA,0x11,0x00,0x30,0x65,0x43,0xEC,0xAC))
  Write-Bytes -Target $bytes -Offset ($partitionOffset + 16) -Source ([byte[]](0x91,0x92,0x93,0x94,0xA0,0xA1,0xA2,0xA3,0xB0,0xB1,0xB2,0xB3,0xC0,0xC1,0xC2,0xC3))
  Write-Le64 -Bytes $bytes -Offset ($partitionOffset + 32) -Value $firstLba
  Write-Le64 -Bytes $bytes -Offset ($partitionOffset + 40) -Value $lastLba
  Write-Utf16Le -Bytes $bytes -Offset ($partitionOffset + 56) -Text "Orchard GPT"

  $containerOffset = [int]($firstLba * $logicalBlockSize)
  Write-NxSuperblock -Bytes $bytes -BaseOffset $containerOffset -Xid 1 -BlockCount 8 -VolumeObjectId 77
  Write-NxSuperblock -Bytes $bytes -BaseOffset ($containerOffset + 4096) -Xid 42 -BlockCount 8 -VolumeObjectId 77
  Write-VolumeSuperblock -Bytes $bytes -BaseOffset ($containerOffset + (4096 * 2)) -ObjectId 77 -IncompatibleFeatures 1 -Role 0x0040 -Name "GPT Data"

  [System.IO.File]::WriteAllBytes($Path, $bytes)
}

New-Item -ItemType Directory -Force -Path $samplesDir | Out-Null

New-DirectFixture -Path (Join-Path $samplesDir "plain-user-data.img") -VolumeName "Orchard Data" -IncompatibleFeatures 1 -Role 0x0040
New-GptFixture -Path (Join-Path $samplesDir "gpt-user-data.img")
New-DirectFixture -Path (Join-Path $samplesDir "snapshot-volume.img") -VolumeName "Snapshot Data" -IncompatibleFeatures 3 -Role 0x0040
New-DirectFixture -Path (Join-Path $samplesDir "sealed-system.img") -VolumeName "System" -IncompatibleFeatures 0x21 -Role 0x0001
