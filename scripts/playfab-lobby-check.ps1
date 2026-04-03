#requires -Version 5.1
<#
.SYNOPSIS
  Check how many PlayFab Multiplayer lobbies match the Win64 client (MC_WIN64 + net version).

.DESCRIPTION
  Uses the same APIs as the game: LoginWithCustomID -> GetEntityToken -> Lobby/FindLobbies.
  CustomId must match the client (16 hex chars from the same identity the game uses for PlayFab).

.PARAMETER TitleId
  PlayFab Title ID (same as MINECRAFT_PLAYFAB_TITLE_ID).

.PARAMETER CustomId
  LoginWithCustomId value, e.g. 16 uppercase hex digits from your Win64 profile (see game logs / uid.dat mapping).

.PARAMETER NetVersion
  MINECRAFT_NET_VERSION / VER_NETWORK (default 560 — override if your build differs).

.PARAMETER Filter
  Optional full OData filter string. If omitted, uses: string_key1 eq 'MC_WIN64' and number_key1 eq <NetVersion>

.PARAMETER LeaveAll
  After listing, call LeaveLobby for each lobby as the logged-in entity (must be a member — same CustomId as host).

.PARAMETER Force
  Required with -LeaveAll so stale lobbies are not removed by mistake.

.EXAMPLE
  .\playfab-lobby-check.ps1 -TitleId C7923 -CustomId A1B2C3D4E5F67890

.EXAMPLE
  .\playfab-lobby-check.ps1 -TitleId C7923 -CustomId F9F46710E8C2EFB4 -LeaveAll -Force

.EXAMPLE
  .\playfab-lobby-check.ps1 -TitleId C7923 -CustomId A1B2C3D4E5F67890 -NetVersion 560
#>
param(
	[Parameter(Mandatory = $true)]
	[string] $TitleId,

	[Parameter(Mandatory = $true)]
	[string] $CustomId,

	[int] $NetVersion = 560,

	[string] $Filter = $null,

	[switch] $LeaveAll,

	[switch] $Force
)

$ErrorActionPreference = 'Stop'
$base = "https://$TitleId.playfabapi.com"

function Get-JsonProperty {
	param($Obj, [string[]]$Names)
	foreach ($n in $Names) {
		if ($null -ne $Obj -and $Obj.PSObject.Properties.Name -contains $n) {
			return $Obj.$n
		}
	}
	return $null
}

if ($LeaveAll -and -not $Force) {
	Write-Error "LeaveAll removes every matching lobby for this login. Re-run with -Force to confirm."
	exit 1
}

# Match game client: CustomId is 16 hex digits, no 0x prefix (strip if pasted from debugger).
$CustomId = $CustomId.Trim()
if ($CustomId.StartsWith('0x', [System.StringComparison]::OrdinalIgnoreCase)) {
	$CustomId = $CustomId.Substring(2)
}
$CustomId = $CustomId.ToUpperInvariant()

try {
	$loginBody = @{
		TitleId        = $TitleId
		CustomId       = $CustomId
		CreateAccount  = $true
	} | ConvertTo-Json

	$login = Invoke-RestMethod -Uri "$base/Client/LoginWithCustomID" -Method Post -Body $loginBody -ContentType 'application/json'
	$ticket = Get-JsonProperty $login.data @('SessionTicket', 'sessionTicket')
	if ([string]::IsNullOrEmpty($ticket)) {
		throw "LoginWithCustomID: no SessionTicket in response."
	}

	$etHeaders = @{ 'X-Authorization' = $ticket }
	$et = Invoke-RestMethod -Uri "$base/Authentication/GetEntityToken" -Method Post -Body '{}' -ContentType 'application/json' -Headers $etHeaders
	$entityToken = Get-JsonProperty $et.data @('EntityToken', 'entityToken')
	if ([string]::IsNullOrEmpty($entityToken)) {
		throw "GetEntityToken: no EntityToken in response."
	}

	if ([string]::IsNullOrWhiteSpace($Filter)) {
		$Filter = "string_key1 eq 'MC_WIN64' and number_key1 eq $NetVersion"
	}

	$findBody = @{
		Filter     = $Filter
		Pagination = @{ PageSizeRequested = 50 }
	} | ConvertTo-Json -Depth 5

	$findHeaders = @{ 'X-EntityToken' = $entityToken }
	$find = Invoke-RestMethod -Uri "$base/Lobby/FindLobbies" -Method Post -Body $findBody -ContentType 'application/json' -Headers $findHeaders

	$data = Get-JsonProperty $find @('data', 'Data')
	$lobbies = $null
	if ($null -ne $data) {
		$lobbies = Get-JsonProperty $data @('Lobbies', 'lobbies')
	}

	$list = @()
	if ($null -ne $lobbies) {
		$list = @($lobbies)
	}

	$pagination = $null
	if ($null -ne $find.data) {
		$pagination = Get-JsonProperty $find.data @('Pagination', 'pagination')
	}
	$totalMatched = Get-JsonProperty $pagination @('TotalMatchedLobbyCount', 'totalMatchedLobbyCount')

	Write-Host "PlayFab TitleId: $TitleId"
	Write-Host "Filter: $Filter"
	Write-Host "Lobbies in this page: $($list.Count)"
	if ($null -ne $totalMatched) {
		Write-Host "TotalMatchedLobbyCount (API): $totalMatched"
	}

	if ($list.Count -eq 0) {
		Write-Host "No active lobbies matching this filter."
		exit 0
	}

	Write-Host "---"
	$i = 0
	foreach ($lob in $list) {
		$i++
		$lid = Get-JsonProperty $lob @('LobbyId', 'lobbyId')
		$sd = Get-JsonProperty $lob @('SearchData', 'searchData')
		$ip = $null
		$port = $null
		if ($null -ne $sd) {
			$ip = Get-JsonProperty $sd @('string_key2', 'String_key2')
			$nk2 = Get-JsonProperty $sd @('number_key2', 'Number_key2')
			if ($null -ne $nk2) {
			try { $port = [int][double]$nk2 } catch { $port = $nk2 }
		}
		}
		Write-Host ("#{0} LobbyId={1} announce={2}:{3}" -f $i, $lid, $ip, $port)
	}

	if ($LeaveAll) {
		$entity = Get-JsonProperty $et.data @('Entity', 'entity')
		$eid = Get-JsonProperty $entity @('Id', 'id')
		$etype = Get-JsonProperty $entity @('Type', 'type')
		if ([string]::IsNullOrEmpty($eid) -or [string]::IsNullOrEmpty($etype)) {
			throw "GetEntityToken: missing Entity.Id / Entity.Type (needed for LeaveLobby)."
		}

		Write-Host "---"
		Write-Host "LeaveLobby (as $etype $eid)..."
		$left = 0
		foreach ($lob in $list) {
			$lid = Get-JsonProperty $lob @('LobbyId', 'lobbyId')
			if ([string]::IsNullOrEmpty($lid)) { continue }
			$leaveBody = @{
				LobbyId      = $lid
				MemberEntity = @{ Id = $eid; Type = $etype }
			} | ConvertTo-Json -Depth 5
			try {
				Invoke-RestMethod -Uri "$base/Lobby/LeaveLobby" -Method Post -Body $leaveBody -ContentType 'application/json' -Headers $findHeaders | Out-Null
				Write-Host "Left lobby: $lid"
				$left++
			}
			catch {
				Write-Warning "LeaveLobby failed for $lid : $_"
			}
		}
		Write-Host "Done. Successfully left $left / $($list.Count) lobbies."
	}

	exit 0
}
catch {
	Write-Error $_
	exit 1
}
