param(
  [string]$ServiceName = ("OrchardSmoke-" + [Guid]::NewGuid().ToString("N").Substring(0, 8)),
  [string]$DisplayName
)

$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot "OrchardWinFspTestCommon.ps1")

$repoRoot = Get-OrchardRepoRoot -ScriptRoot $PSScriptRoot
$serviceHostExecutable = Join-Path $repoRoot "build\\default\\src\\mount-service\\orchard-service-host.exe"
if (-not (Test-Path $serviceHostExecutable)) {
  throw "Missing orchard-service-host executable at '$serviceHostExecutable'. Build the repo first."
}

if (-not $DisplayName) {
  $DisplayName = $ServiceName
}

function Wait-OrchardServiceState {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedStatus,

    [int]$TimeoutMs = 15000
  )

  $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
  do {
    $service = Get-Service -Name $Name -ErrorAction Stop
    if ($service.Status.ToString() -eq $ExpectedStatus) {
      return
    }
    Start-Sleep -Milliseconds 250
  } while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs)

  throw "Timed out waiting for service '$Name' to reach state '$ExpectedStatus'."
}

$installed = $false
try {
  & $serviceHostExecutable --install --service-name $ServiceName --display-name $DisplayName
  $installed = $true

  Start-Service -Name $ServiceName
  Wait-OrchardServiceState -Name $ServiceName -ExpectedStatus Running

  Stop-Service -Name $ServiceName
  Wait-OrchardServiceState -Name $ServiceName -ExpectedStatus Stopped

  [pscustomobject]@{
    service_name = $ServiceName
    display_name = $DisplayName
    status = "stopped_after_smoke"
  } | ConvertTo-Json -Depth 3
}
finally {
  if ($installed) {
    try {
      & $serviceHostExecutable --uninstall --service-name $ServiceName | Out-Null
    }
    catch {
      Write-Warning "Failed to uninstall smoke service '$ServiceName': $($_.Exception.Message)"
    }
  }
}
