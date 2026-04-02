#Requires -Version 5.1
<#
.SYNOPSIS
  Creates Minecraft Win64 PlayFab player statistics via the Admin API (no manual typing in Game Manager).

.DESCRIPTION
  Uses your title's Developer Secret Key (server-side only - never ship this in the game client).

  Flow per statistic: CreatePlayerStatisticDefinition; on any failure except 429 exhaustion, calls
  UpdatePlayerStatisticDefinition (covers "already exists" and PlayFab error wording you cannot match).

  Throttles with DelaySeconds and retries 429 using retryAfterSeconds from the response body.

.PARAMETER DelaySeconds
  Pause between API calls (default 1.1). Raise if you still see 429.

.PARAMETER Max429Retries
  Max 429 retries per single Create or Update call (default 20).

.EXAMPLE
  .\playfab-create-leaderboard-statistics.ps1 -TitleId C7923 -SecretKey 'YOUR_SECRET'

.NOTES
  Docs: https://learn.microsoft.com/en-us/rest/api/playfab/admin/player-data-management/
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)]
    [string] $TitleId,

    [Parameter(Mandatory = $true)]
    [string] $SecretKey,

    [switch] $IncludeKillsPeaceful,

    [double] $DelaySeconds = 1.1,

    [int] $Max429Retries = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# TLS 1.2 for older Windows PowerShell defaults
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
}
catch { }

function Get-AllStatisticNames {
    param([bool] $IncludeKillsD0)
    $names = New-Object 'System.Collections.Generic.List[string]'

    for ($d = 0; $d -le 3; $d++) {
        $names.Add("MC_Travel_D${d}_Sum")
        for ($c = 0; $c -le 3; $c++) { $names.Add("MC_Travel_D${d}_C$c") }

        $names.Add("MC_Mine_D${d}_Sum")
        for ($c = 0; $c -le 6; $c++) { $names.Add("MC_Mine_D${d}_C$c") }

        $names.Add("MC_Farm_D${d}_Sum")
        for ($c = 0; $c -le 5; $c++) { $names.Add("MC_Farm_D${d}_C$c") }
    }

    for ($d = 1; $d -le 3; $d++) {
        $names.Add("MC_Kills_D${d}_Sum")
        for ($c = 0; $c -le 6; $c++) { $names.Add("MC_Kills_D${d}_C$c") }
    }

    if ($IncludeKillsD0) {
        $names.Add('MC_Kills_D0_Sum')
        for ($c = 0; $c -le 6; $c++) { $names.Add("MC_Kills_D0_C$c") }
    }

    return $names
}

function Get-RateLimitWaitSeconds {
    param([string] $RawJson)
    if (-not $RawJson) { return 0 }
    try {
        $o = $RawJson | ConvertFrom-Json
        if ($o.errorCode -eq 1199 -or $o.error -eq 'APIClientRequestRateLimitExceeded' -or $o.code -eq 429) {
            $sec = 3
            if ($null -ne $o.retryAfterSeconds) { $sec = [int]$o.retryAfterSeconds }
            if ($sec -lt 1) { $sec = 2 }
            return $sec + 1
        }
    }
    catch { }
    return 0
}

function Test-PlayFabSuccessBody {
    param([string] $JsonText)
    if (-not $JsonText) { return $false }
    try {
        $o = $JsonText | ConvertFrom-Json
        return ($o.code -eq 200)
    }
    catch {
        return $false
    }
}

function Invoke-PlayFabPostOnce {
    param(
        [string] $Uri,
        [hashtable] $Headers,
        [string] $JsonBody
    )
    try {
        $r = Invoke-WebRequest -Uri $Uri -Method Post -Headers $Headers -Body $JsonBody `
            -ContentType 'application/json; charset=utf-8' -UseBasicParsing -ErrorAction Stop
        return @{ Ok = $true; Http = [int]$r.StatusCode; Text = $r.Content; Err = $null }
    }
    catch {
        $text = $null
        $http = 0
        try {
            $resp = $_.Exception.Response
            if ($null -ne $resp) {
                $http = [int]$resp.StatusCode
                $stream = $resp.GetResponseStream()
                if ($null -ne $stream) {
                    $reader = New-Object System.IO.StreamReader($stream)
                    $text = $reader.ReadToEnd()
                }
            }
        }
        catch { }

        if (-not $text) {
            $text = $_.Exception.Message
        }
        return @{ Ok = $false; Http = $http; Text = $text; Err = $_.Exception.Message }
    }
}

function Invoke-PlayFabAdminPostWithRetry {
    param(
        [string] $TitleId,
        [string] $SecretKey,
        [string] $Path,
        [hashtable] $Body,
        [int] $Max429Retries
    )
    $uri = "https://${TitleId}.playfabapi.com${Path}"
    $json = ($Body | ConvertTo-Json -Compress -Depth 6)
    $headers = @{
        'X-SecretKey' = $SecretKey
    }

    $attempt429 = 0
    while ($true) {
        $once = Invoke-PlayFabPostOnce -Uri $uri -Headers $headers -JsonBody $json
        $text = $once.Text

        if ($once.Ok -and (Test-PlayFabSuccessBody -JsonText $text)) {
            return @{ Ok = $true; Raw = $null; RateLimitExceeded = $false }
        }

        # HTTP 200 but PlayFab business error in JSON body
        if ($once.Ok -and $text) {
            $wait = Get-RateLimitWaitSeconds -RawJson $text
            if ($wait -gt 0) {
                $attempt429++
                if ($attempt429 -gt $Max429Retries) {
                    return @{ Ok = $false; Raw = $text; RateLimitExceeded = $true }
                }
                Write-Host "    rate limit (body), sleep ${wait}s ($attempt429/$Max429Retries)" -ForegroundColor DarkYellow
                Start-Sleep -Seconds $wait
                continue
            }
            return @{ Ok = $false; Raw = $text; RateLimitExceeded = $false }
        }

        # HTTP error (4xx/5xx)
        if (-not $once.Ok) {
            $wait = Get-RateLimitWaitSeconds -RawJson $text
            if ($wait -gt 0 -or $once.Http -eq 429) {
                if ($wait -le 0) { $wait = 4 }
                $attempt429++
                if ($attempt429 -gt $Max429Retries) {
                    return @{ Ok = $false; Raw = $text; RateLimitExceeded = $true }
                }
                Write-Host "    rate limit (HTTP $($once.Http)), sleep ${wait}s ($attempt429/$Max429Retries)" -ForegroundColor DarkYellow
                Start-Sleep -Seconds $wait
                continue
            }
            return @{ Ok = $false; Raw = $text; RateLimitExceeded = $false }
        }

        return @{ Ok = $false; Raw = $text; RateLimitExceeded = $false }
    }
}

$statNames = Get-AllStatisticNames -IncludeKillsD0:$IncludeKillsPeaceful
$created = 0
$updated = 0
$failed = 0

Write-Host "Title: $TitleId - $($statNames.Count) statistics; ${DelaySeconds}s between calls; up to $Max429Retries 429 retries per Create/Update." -ForegroundColor Cyan

$definitionBody = @{
    VersionChangeInterval = 'Never'
    AggregationMethod     = 'Last'
}

$first = $true
foreach ($name in $statNames) {
    if (-not $PSCmdlet.ShouldProcess($name, 'Create/update PlayFab statistic')) {
        continue
    }

    if (-not $first) {
        Start-Sleep -Seconds $DelaySeconds
    }
    $first = $false

    $createPayload = $definitionBody.Clone()
    $createPayload['StatisticName'] = $name

    $cr = Invoke-PlayFabAdminPostWithRetry -TitleId $TitleId -SecretKey $SecretKey `
        -Path '/Admin/CreatePlayerStatisticDefinition' -Body $createPayload -Max429Retries $Max429Retries

    if ($cr.Ok) {
        $created++
        Write-Host "  create OK: $name" -ForegroundColor Green
        continue
    }

    if ($cr.RateLimitExceeded) {
        Write-Host "  FAIL (429 max retries) create: $name" -ForegroundColor Red
        if ($cr.Raw) { Write-Host "    $($cr.Raw)" -ForegroundColor DarkRed }
        $failed++
        continue
    }

    # Create failed for any other reason (e.g. already exists) -> Update (idempotent for existing)
    Start-Sleep -Seconds $DelaySeconds
    $updatePayload = $definitionBody.Clone()
    $updatePayload['StatisticName'] = $name
    $ur = Invoke-PlayFabAdminPostWithRetry -TitleId $TitleId -SecretKey $SecretKey `
        -Path '/Admin/UpdatePlayerStatisticDefinition' -Body $updatePayload -Max429Retries $Max429Retries

    if ($ur.Ok) {
        $updated++
        Write-Host "  update OK: $name (create skipped or failed - stat present)" -ForegroundColor Yellow
        continue
    }

    if ($ur.RateLimitExceeded) {
        Write-Host "  FAIL (429 max retries) update: $name" -ForegroundColor Red
        if ($ur.Raw) { Write-Host "    $($ur.Raw)" -ForegroundColor DarkRed }
        $failed++
        continue
    }

    # Neither create nor update worked: show both bodies
    Write-Host "  FAIL: $name" -ForegroundColor Red
    Write-Host "    create: $($cr.Raw)" -ForegroundColor DarkRed
    Write-Host "    update: $($ur.Raw)" -ForegroundColor DarkRed
    $failed++
}

Write-Host ""
Write-Host "Done. created=$created updated=$updated failed=$failed" -ForegroundColor Cyan
if ($failed -gt 0) {
    Write-Host "Fix failures, or wait a few minutes if still throttled, then re-run (safe to repeat)." -ForegroundColor Yellow
    exit 1
}

Write-Host @"

Next: In Game Manager, ensure each MC_*_Sum statistic is used as a leaderboard if your UI requires it.
Client reads GetLeaderboard only on *_Sum statistic names.
"@ -ForegroundColor DarkGray
