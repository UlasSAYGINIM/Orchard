param(
  [string]$TargetPath,
  [string]$MountPoint,
  [int]$HoldMs = 60000
)

$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot "OrchardWinFspTestCommon.ps1")

$repoRoot = Get-OrchardRepoRoot -ScriptRoot $PSScriptRoot
if (-not $TargetPath) {
  $TargetPath = Join-Path $repoRoot "tests\\corpus\\samples\\plain-user-data.img"
}

$serviceHostExecutable = Join-Path $repoRoot "build\\default\\src\\mount-service\\orchard-service-host.exe"
if (-not (Test-Path $serviceHostExecutable)) {
  throw "Missing orchard-service-host executable at '$serviceHostExecutable'. Build the repo first."
}

if (-not $MountPoint) {
  $freeDriveLetter = Get-OrchardFreeDriveLetter
  if (-not $freeDriveLetter) {
    throw "No free drive letter is available for the Orchard service console smoke run."
  }
  $MountPoint = "$freeDriveLetter`:"
}

$shutdownEventName = "Local\OrchardServiceConsole-" + [Guid]::NewGuid().ToString("N")
$createdNew = $false
$shutdownEvent =
  New-Object System.Threading.EventWaitHandle(
    $false,
    [System.Threading.EventResetMode]::ManualReset,
    $shutdownEventName,
    [ref]$createdNew)

$resolvedTargetPath = (Resolve-Path $TargetPath).Path
$process =
  Start-Process -FilePath $serviceHostExecutable `
    -ArgumentList @(
      "--console",
      "--target", $resolvedTargetPath,
      "--mountpoint", $MountPoint,
      "--hold-ms", $HoldMs,
      "--shutdown-event", $shutdownEventName
    ) `
    -PassThru `
    -WindowStyle Hidden

try {
  $mounted = $false
  for ($attempt = 0; $attempt -lt 60; ++$attempt) {
    Start-Sleep -Milliseconds 250
    if (Test-Path "$MountPoint\\") {
      $mounted = $true
      break
    }
    if ($process.HasExited) {
      throw "orchard-service-host exited early with code $($process.ExitCode)."
    }
  }

  if (-not $mounted) {
    throw "The mount point '$MountPoint' did not appear before timeout."
  }

  $checkResult = Invoke-OrchardPlainReadChecks -MountPoint $MountPoint
  $shutdownEvent.Set() | Out-Null

  for ($attempt = 0; $attempt -lt 60; ++$attempt) {
    if ($process.HasExited) {
      break
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
  }

  if (-not $process.HasExited) {
    throw "orchard-service-host did not exit after the shutdown event was signaled."
  }

  [pscustomobject]@{
    mount_point = $MountPoint
    alpha_text = $checkResult.alpha_text
    note_text = $checkResult.note_text
    root_entry_count = $checkResult.root_entry_count
    exit_code = $process.ExitCode
  } | ConvertTo-Json -Depth 4
}
finally {
  $shutdownEvent.Dispose()
  Start-Sleep -Milliseconds 500
  if (Test-Path "$MountPoint\\") {
    throw "Mount point '$MountPoint' still exists after orchard-service-host exited."
  }
}
