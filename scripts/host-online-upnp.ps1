# Dot-sourced by host-online.ps1: IPv4 address checks, router discovery (traceroute hops, SSDP) and the
# UPnP IGD calls (description, SOAP, port mappings). Uses the constants host-online.ps1 defines first.

$script:UnresponsiveIgds = @{}
$script:LastWanFailure = ''
$script:HostErrors = @{}

# The code travels keyed by the thrown message, so a tolerated failure caught earlier cannot label a later error.
function Set-HostError([string]$Code, [string]$Router, [string]$Message) {
    $script:HostErrors[$Message] = [pscustomobject]@{ Code = $Code; Router = $Router }
    $Message
}

function Get-HostError([string]$Message) {
    if ($script:HostErrors.ContainsKey($Message)) { return $script:HostErrors[$Message] }
    [pscustomobject]@{ Code = 'other'; Router = '' }
}

function Test-DottedQuad([string]$Address) { $Address -match $DottedQuadPattern }

function Get-IPv4Bytes([string]$Address) {
    if (-not (Test-DottedQuad $Address)) { throw "'$Address' is not an IPv4 address in a.b.c.d form." }
    [Net.IPAddress]::Parse($Address).GetAddressBytes()
}

function Get-PrivateBlock([string]$Address) {
    $b = Get-IPv4Bytes $Address
    if ($b[0] -eq 10) { return '10.0.0.0/8' }
    if ($b[0] -eq 172 -and $b[1] -ge 16 -and $b[1] -le 31) { return '172.16.0.0/12' }
    if ($b[0] -eq 192 -and $b[1] -eq 168) { return '192.168.0.0/16' }
    $null
}

function Test-PrivateIPv4([string]$Address) { [bool](Get-PrivateBlock $Address) }

function Test-SharedIPv4([string]$Address) {
    $b = Get-IPv4Bytes $Address
    $b[0] -eq 100 -and $b[1] -ge 64 -and $b[1] -le 127
}

function Test-UnusableIPv4([string]$Address) {
    $b = Get-IPv4Bytes $Address
    ($b[0] -eq 0) -or ($b[0] -eq 127) -or ($b[0] -ge 224) -or ($b[0] -eq 169 -and $b[1] -eq 254)
}

function Test-PublicIPv4([string]$Address) {
    -not (Test-PrivateIPv4 $Address) -and -not (Test-SharedIPv4 $Address) -and -not (Test-UnusableIPv4 $Address)
}

function New-LanHttpClient {
    Add-Type -AssemblyName System.Net.Http
    $handler = [Net.Http.HttpClientHandler]::new()
    $handler.UseProxy = $false
    $handler.AllowAutoRedirect = $false
    $client = [Net.Http.HttpClient]::new($handler)
    $client.Timeout = [TimeSpan]::FromSeconds(6)
    $client.MaxResponseContentBufferSize = $MaxResponseBytes
    $client.DefaultRequestHeaders.ExpectContinue = $false
    $client
}

function Get-LocalIPv4 {
    $socket = [Net.Sockets.Socket]::new([Net.Sockets.AddressFamily]::InterNetwork, [Net.Sockets.SocketType]::Dgram, [Net.Sockets.ProtocolType]::Udp)
    try {
        $socket.Connect($TraceTarget, 53)  # a UDP connect sends nothing; it only picks the default route
        $socket.LocalEndPoint.Address.ToString()
    } finally { $socket.Dispose() }
}

function Get-HopAddress([int]$Hop) {
    $ping = [Net.NetworkInformation.Ping]::new()
    $options = [Net.NetworkInformation.PingOptions]::new($Hop, $true)
    $payload = New-Object byte[] 32
    try {
        foreach ($attempt in 1..3) {
            $reply = $ping.Send($TraceTarget, 1000, $payload, $options)
            if ($reply.Status -eq 'TtlExpired') { return $reply.Address.ToString() }
        }
    } finally { $ping.Dispose() }
    $null
}

function New-SsdpRequest([string]$HostHeader, [string]$SearchTarget) {
    $lines = @('M-SEARCH * HTTP/1.1', "HOST: $HostHeader", 'MAN: "ssdp:discover"', 'MX: 2', "ST: $SearchTarget", '', '')
    [Text.Encoding]::ASCII.GetBytes($lines -join "`r`n")
}

function New-SsdpClient([string]$LocalAddress) {
    $local = [Net.IPAddress]::Parse($LocalAddress)
    $client = [Net.Sockets.UdpClient]::new([Net.IPEndPoint]::new($local, 0))
    $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastInterface, $local.GetAddressBytes())
    $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastTimeToLive, 2)
    $client
}

function Send-IgdSearch($Client, [string]$Target) {
    $endpoint = [Net.IPEndPoint]::new([Net.IPAddress]::Parse($Target), $SsdpPort)
    foreach ($searchTarget in $IgdSearchTargets) {
        $bytes = New-SsdpRequest "${Target}:$SsdpPort" $searchTarget
        [void]$Client.Send($bytes, $bytes.Length, $endpoint)
    }
}

function ConvertFrom-SsdpReply([string]$Text, [string]$Address) {
    $headers = @{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -match '^([A-Za-z-]+):\s*(.*)$') { $headers[$Matches[1].ToUpperInvariant()] = $Matches[2].Trim() }
    }
    if (-not $headers['LOCATION']) { return $null }
    [pscustomobject]@{ Address = $Address; Location = $headers['LOCATION']; Server = $headers['SERVER']; SearchTarget = $headers['ST'] }
}

function Receive-SsdpReply($Client) {
    $remote = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
    try { $bytes = $Client.Receive([ref]$remote) }
    catch { return $null }  # an ICMP port-unreachable from a host without SSDP surfaces here
    ConvertFrom-SsdpReply ([Text.Encoding]::ASCII.GetString($bytes)) $remote.Address.ToString()
}

function Receive-SsdpReplies($Client, [int]$WaitMs) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($WaitMs)
    while ($true) {
        $left = ($deadline - [DateTime]::UtcNow).TotalMilliseconds
        if ($left -le 0) { break }
        if (-not $Client.Client.Poll([int]($left * 1000), [Net.Sockets.SelectMode]::SelectRead)) { break }
        $reply = Receive-SsdpReply $Client
        if ($reply) { $reply }
    }
}

function Search-Igds([string]$LocalAddress, [string]$Target) {
    $client = New-SsdpClient $LocalAddress
    try {
        Send-IgdSearch $client $Target
        @(Receive-SsdpReplies $client $SsdpWaitMs)
    } finally { $client.Dispose() }
}

function Test-UrlOnHost([string]$Url, [string]$Address) {
    $uri = $null
    if (-not [Uri]::TryCreate($Url, [UriKind]::Absolute, [ref]$uri)) { return $false }
    ($uri.Scheme -eq 'http') -and ($uri.Host -eq $Address)
}

function Read-ContentText($Response) {
    # MiniUPnPd sends charset="utf-8" with quotes, which ReadAsStringAsync rejects
    [Text.Encoding]::UTF8.GetString($Response.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult())
}

function Get-HttpText([string]$Url) {
    $response = $script:Http.GetAsync($Url).GetAwaiter().GetResult()
    if (-not $response.IsSuccessStatusCode) { throw "GET $Url answered HTTP $([int]$response.StatusCode)." }
    Read-ContentText $response
}

function ConvertTo-XmlDocument([string]$Text) {
    $settings = [Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
    $settings.XmlResolver = $null
    $reader = [Xml.XmlReader]::Create([IO.StringReader]::new($Text), $settings)
    try {
        $document = [Xml.XmlDocument]::new()
        $document.XmlResolver = $null
        $document.Load($reader)
        $document
    } finally { $reader.Dispose() }
}

function Get-DescriptionBase($Description, [string]$Location, [string]$Address) {
    $node = $Description.SelectSingleNode("//*[local-name()='URLBase']")
    if (-not $node -or -not $node.InnerText.Trim()) { return $Location }
    $base = $node.InnerText.Trim()
    if (Test-UrlOnHost $base $Address) { return $base }
    Write-Warn "Ignored the URLBase $base in the description at ${Location}: it is not on $Address."
    $Location
}

function Find-WanService([string]$Location, [string]$Address) {
    $description = ConvertTo-XmlDocument (Get-HttpText $Location)
    $base = [Uri](Get-DescriptionBase $description $Location $Address)
    foreach ($type in $WanServiceTypes) {
        $node = $description.SelectSingleNode("//*[local-name()='service'][normalize-space(*[local-name()='serviceType'])='$type']")
        if (-not $node) { continue }
        $control = [Uri]::new($base, $node.SelectSingleNode("*[local-name()='controlURL']").InnerText.Trim()).AbsoluteUri
        if (Test-UrlOnHost $control $Address) { return [pscustomobject]@{ ServiceType = $type; ControlUrl = $control } }
        Write-Warn "Ignored $type at ${Location}: its control URL $control is not plain http on $Address."
    }
    $null
}

function Get-ShortFailure([string]$Detail) {
    # HttpClient reports its own timeout as a canceled task
    if ($Detail -match '(?i)time[sd]?\s*out|timeout|task was cancel') { return 'timeout' }
    $first = ([string](($Detail -split "`r?`n")[0])).Trim()
    if ($first.Length -gt 80) { return $first.Substring(0, 80) }
    $first
}

function Find-WanServiceOrWarn([string]$Location, [string]$Address) {
    try { Find-WanService $Location $Address }
    catch {
        $detail = $_.Exception.GetBaseException().Message
        $script:LastWanFailure = Get-ShortFailure $detail
        Write-Warn "Could not read the UPnP description at ${Location}: $detail"
        $null
    }
}

function New-Igd([string]$Address, [string]$Server, [string]$Location, $Service) {
    [pscustomobject]@{ Address = $Address; Server = $Server; Location = $Location; ServiceType = $Service.ServiceType; ControlUrl = $Service.ControlUrl }
}

function Select-PinnedReplies($Replies, [string]$Address) {
    foreach ($reply in @($Replies | Where-Object { $_.Address -eq $Address })) {
        if (Test-UrlOnHost $reply.Location $Address) { $reply; continue }
        Write-Warn "Ignored an SSDP answer from ${Address}: its LOCATION $($reply.Location) is not plain http on $Address."
    }
}

function Find-RouterIgd([string]$Address, [string]$LocalAddress, $LanReplies) {
    $script:UnresponsiveIgds.Remove($Address)
    $replies = @(Select-PinnedReplies $LanReplies $Address)
    if (-not $replies) { $replies = @(Select-PinnedReplies (Search-Igds $LocalAddress $Address) $Address) }
    $byVersion = @($replies | Sort-Object { $_.SearchTarget -notlike '*:2' })
    $script:LastWanFailure = 'no WAN service'
    foreach ($location in @($byVersion | ForEach-Object { $_.Location } | Select-Object -Unique)) {
        $service = Find-WanServiceOrWarn $location $Address
        $reply = $byVersion | Where-Object { $_.Location -eq $location } | Select-Object -First 1
        if ($service) { return New-Igd $Address $reply.Server $location $service }
    }
    if ($replies) { $script:UnresponsiveIgds[$Address] = $script:LastWanFailure }
    $null
}

function New-SoapBody([string]$ServiceType, [string]$Action, $Arguments) {
    $fields = foreach ($name in $Arguments.Keys) { '<{0}>{1}</{0}>' -f $name, [Security.SecurityElement]::Escape([string]$Arguments[$name]) }
    '<?xml version="1.0"?>' +
    '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
    ('<s:Body><u:{0} xmlns:u="{1}">{2}</u:{0}></s:Body></s:Envelope>' -f $Action, $ServiceType, (@($fields) -join ''))
}

function Send-SoapRequest($Igd, [string]$Action, [string]$Body) {
    $request = [Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Post, $Igd.ControlUrl)
    [void]$request.Headers.TryAddWithoutValidation('SOAPAction', ('"{0}#{1}"' -f $Igd.ServiceType, $Action))
    $request.Content = [Net.Http.StringContent]::new($Body, [Text.Encoding]::UTF8, 'text/xml')
    try {
        $response = $script:Http.SendAsync($request).GetAwaiter().GetResult()
        [pscustomobject]@{ Status = [int]$response.StatusCode; Text = (Read-ContentText $response) }
    } catch {
        throw (Set-HostError 'upnp_unresponsive' $Igd.Address "The UPnP service of $($Igd.Address) did not answer $Action ($($Igd.ControlUrl)): $($_.Exception.GetBaseException().Message)")
    }
}

function ConvertTo-XmlOrNull([string]$Text) {
    try { ConvertTo-XmlDocument $Text } catch { $null }
}

function New-UpnpResult([int]$Code, [string]$Description, $Values) {
    [pscustomobject]@{ Code = $Code; Description = $Description; Values = $Values }
}

function Get-ChildText($Node, [string]$Name) {
    $child = $Node.SelectSingleNode(".//*[local-name()='$Name']")
    if ($child) { return $child.InnerText.Trim() }
    ''
}

function ConvertFrom-SoapResponse($Response) {
    $document = ConvertTo-XmlOrNull $Response.Text
    if (-not $document) { return New-UpnpResult $Response.Status "HTTP $($Response.Status) without a SOAP body" @{} }
    $fault = $document.SelectSingleNode("//*[local-name()='UPnPError']")
    if ($fault) { return New-UpnpResult ([int](Get-ChildText $fault 'errorCode')) (Get-ChildText $fault 'errorDescription') @{} }
    if ($Response.Status -ge 300) { return New-UpnpResult $Response.Status "HTTP $($Response.Status)" @{} }
    $values = @{}
    $reply = $document.SelectSingleNode("//*[local-name()='Body']/*")
    if ($reply) { foreach ($node in $reply.SelectNodes('*')) { $values[$node.LocalName] = $node.InnerText } }
    New-UpnpResult 0 '' $values
}

function Invoke-UpnpAction($Igd, [string]$Action, $Arguments) {
    $response = Send-SoapRequest $Igd $Action (New-SoapBody $Igd.ServiceType $Action $Arguments)
    ConvertFrom-SoapResponse $response
}

function Assert-UpnpSuccess($Result, [string]$What, [string]$Router) {
    if ($Result.Code -eq 0) { return }
    if ($Result.Code -eq $UpnpNotAuthorized) {
        throw (Set-HostError 'refused' $Router "$What was refused (UPnP error 606, Action not authorized): the router answers UPnP but does not let this PC change it. Allow UPnP port mapping in its settings.")
    }
    throw "$What failed: UPnP error $($Result.Code) $($Result.Description)."
}

function Get-ExternalAddress($Igd) {
    $result = Invoke-UpnpAction $Igd 'GetExternalIPAddress' ([ordered]@{})
    Assert-UpnpSuccess $result "GetExternalIPAddress on $($Igd.Address)" $Igd.Address
    $address = [string]$result.Values['NewExternalIPAddress']
    if (-not $address -or $address -eq '0.0.0.0') { throw "The router at $($Igd.Address) reports no WAN address: is it connected to the internet?" }
    if (-not (Test-DottedQuad $address) -or (Test-UnusableIPv4 $address)) { throw "The router at $($Igd.Address) reports '$address' as its WAN address, which is not a usable IPv4 address." }
    $address
}

function ConvertTo-RemoteHost([string]$Text) {
    # the measured MiniUPnPd 1.7 modem lists a wildcard remote host as 'none'; the request form is ''
    if (Test-DottedQuad $Text) { return $Text }
    ''
}

function ConvertTo-Mapping($Values, [int]$Port, [string]$Protocol) {
    $externalPort = if ($Values['NewExternalPort']) { [int]$Values['NewExternalPort'] } else { $Port }
    $mappedProtocol = if ($Values['NewProtocol']) { [string]$Values['NewProtocol'] } else { $Protocol }
    [pscustomobject]@{
        Port = $externalPort
        Protocol = $mappedProtocol
        RemoteHost = (ConvertTo-RemoteHost ([string]$Values['NewRemoteHost']))
        InternalClient = [string]$Values['NewInternalClient']
        InternalPort = [int]$Values['NewInternalPort']
        Enabled = [string]$Values['NewEnabled']
        Description = [string]$Values['NewPortMappingDescription']
        LeaseSeconds = [int]$Values['NewLeaseDuration']
    }
}

function Get-PortMapping($Router, [int]$Port, [string]$Protocol = 'UDP', [string]$RemoteHost = '') {
    $arguments = [ordered]@{ NewRemoteHost = $RemoteHost; NewExternalPort = $Port; NewProtocol = $Protocol }
    $result = Invoke-UpnpAction $Router.Igd 'GetSpecificPortMappingEntry' $arguments
    if ($result.Code -eq $UpnpNoSuchEntry) { return $null }
    Assert-UpnpSuccess $result "Reading the $Protocol $Port mapping on $($Router.Address)" $Router.Address
    ConvertTo-Mapping $result.Values $Port $Protocol
}

function Get-GenericMapping($Router, [int]$Index) {
    $result = Invoke-UpnpAction $Router.Igd 'GetGenericPortMappingEntry' ([ordered]@{ NewPortMappingIndex = $Index })
    if ($result.Code -eq $UpnpInvalidArrayIndex -or $result.Code -eq $UpnpNoSuchEntry) { return $null }
    Assert-UpnpSuccess $result "Reading mapping number $Index on $($Router.Address)" $Router.Address
    ConvertTo-Mapping $result.Values 0 ''
}

function Get-AllMappings($Router) {
    for ($index = 0; $index -lt $MaxGenericEntries; $index++) {
        $mapping = Get-GenericMapping $Router $index
        if (-not $mapping) { return }
        $mapping
    }
}

function Get-DescribedMappings($Router) { @(Get-AllMappings $Router | Where-Object { $_.Description -eq $MappingDescription }) }

function Remove-PortMappingEntry($Router, [int]$Port, [string]$Protocol, [string]$RemoteHost) {
    $arguments = [ordered]@{ NewRemoteHost = $RemoteHost; NewExternalPort = $Port; NewProtocol = $Protocol }
    $result = Invoke-UpnpAction $Router.Igd 'DeletePortMapping' $arguments
    Assert-UpnpSuccess $result "Deleting $Protocol $Port on $($Router.Address)" $Router.Address
}

function Remove-PortMapping($Router, $Mapping) {
    Remove-PortMappingEntry $Router $Mapping.Port $Mapping.Protocol $Mapping.RemoteHost
    if (Get-PortMapping $Router $Mapping.Port $Mapping.Protocol $Mapping.RemoteHost) { throw "$($Router.Address) accepted the delete of $($Mapping.Protocol) $($Mapping.Port) but still lists it." }
    Write-Note "deleted  $($Mapping.Protocol) $($Mapping.Port) on $($Router.Address) (was -> $($Mapping.InternalClient):$($Mapping.InternalPort))"
}
