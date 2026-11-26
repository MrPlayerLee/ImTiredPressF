param(
 [Parameter(Mandatory)][ValidateSet('prepare','core','release','verify','offline')][string]$Action,
 [Parameter(Mandatory)][string]$Directory,
 [string]$PublicFile,
 [string]$SmartCardKey
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot
$config=[xml](Get-Content -LiteralPath "$root\BuildConfig.props" -Raw)
$repo=[string]$config.Project.PropertyGroup.RepositoryUrl
if($repo -cnotmatch '^https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$'){throw 'Invalid repository URL'}
$sha=[Security.Cryptography.SHA256]::Create()
function Hex([byte[]]$Bytes){[BitConverter]::ToString($Bytes).Replace('-','').ToLowerInvariant()}
function HashFile([string]$Path){$f=[IO.File]::OpenRead($Path);try {return ,$sha.ComputeHash($f)} finally {$f.Dispose()}}
function AtomicBytes($path,[byte[]]$data){
    if([IO.File]::Exists($path) -and [Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -ceq [Convert]::ToBase64String($data)){return}
    $tmp=$path+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try{[IO.File]::WriteAllBytes($tmp,$data)
        if([IO.File]::Exists($path)){[IO.File]::Replace($tmp,$path,[Management.Automation.Language.NullString]::Value)}else{[IO.File]::Move($tmp,$path)}
    }finally{if([IO.File]::Exists($tmp)){[IO.File]::Delete($tmp)}}
}
function Record($file,$domain){
    [byte[]]$head=[Text.Encoding]::ASCII.GetBytes($domain)+(HashFile $file)
    [byte[]]$signature=$ecdsa.SignHash($sha.ComputeHash($head))
    if($signature.Length -ne 64 -or !$ecdsa.VerifyHash($sha.ComputeHash($head),$signature)){throw 'CNG signature failure'}
    return ,([byte[]]($head+$signature))
}
function Verify($file,$domain){
    [byte[]]$data=[IO.File]::ReadAllBytes($file+'.sig')
    [byte[]]$head=[Text.Encoding]::ASCII.GetBytes($domain)+(HashFile $file)
    if($data.Length -ne 104 -or (Hex $head) -ne (Hex $data[0..39]) -or !$ecdsa.VerifyHash($sha.ComputeHash($head),[byte[]]$data[40..103])){throw "Signature verification failed: $file"}
}
$dir=[IO.Path]::GetFullPath($Directory)
[IO.Directory]::CreateDirectory($dir) | Out-Null
$blobFormat=[Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob
$provider=[Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
$keyName='ImTiredPressF.Dev.'+(Hex $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($repo)))
if($Action -eq 'offline'){
    if(!$PublicFile -or !$SmartCardKey){throw 'Offline signing requires the release public blob and a provisioned hardware key'}
    $key=[Security.Cryptography.CngKey]::Open($SmartCardKey,[Security.Cryptography.CngProvider]::MicrosoftSmartCardKeyStorageProvider)
}elseif($PublicFile){
    $key=[Security.Cryptography.CngKey]::Import([IO.File]::ReadAllBytes($PublicFile),$blobFormat)
}else{
    if(![Security.Cryptography.CngKey]::Exists($keyName,$provider)){
        if($Action -ne 'prepare'){throw 'Development signing key has not been initialized'}
        $params=[Security.Cryptography.CngKeyCreationParameters]::new()
        $params.Provider=$provider
        $params.ExportPolicy=[Security.Cryptography.CngExportPolicies]::None
        $params.KeyUsage=[Security.Cryptography.CngKeyUsages]::Signing
        try{$new=[Security.Cryptography.CngKey]::Create([Security.Cryptography.CngAlgorithm]::ECDsaP256,$keyName,$params);$new.Dispose()}
        catch{if(![Security.Cryptography.CngKey]::Exists($keyName,$provider)){throw}}
    }
    $key=[Security.Cryptography.CngKey]::Open($keyName,$provider)
}
try{
    [byte[]]$pub=$key.Export($blobFormat)
    if($pub.Length -ne 72 -or [BitConverter]::ToUInt32($pub,0) -ne 0x31534345 -or [BitConverter]::ToUInt32($pub,4) -ne 32){throw 'Expected ECDSA P-256 public key'}
    if($PublicFile -and (Hex $pub) -ne (Hex ([IO.File]::ReadAllBytes($PublicFile)))){throw 'Official key mismatch'}
    $ecdsa=[Security.Cryptography.ECDsaCng]::new($key)
    if($Action -eq 'prepare'){
        $id=$repo.Substring('https://github.com/'.Length)
        $header="#pragma once`ninline constexpr unsigned char TrustedKey[] = {"+(($pub | ForEach-Object {'0x'+$_.ToString('x2')}) -join ',')+"};`ninline constexpr wchar_t Repository[] = L`"$id`";`n"
        $versions=Get-Content -LiteralPath "$root\VERSION_AGENT.md" -Raw -Encoding UTF8
        $m=[regex]::Match($versions,"(?m)^client = ([0-9.]+)\r?$")
        if(!$m.Success){throw 'Missing client version'}
        $header+=('inline constexpr wchar_t RunningClient[]=L"{0}";' -f $m.Groups[1].Value)+[Environment]::NewLine
        # ABI-1 minimum is independent of the core published alongside this client.
        $header+='inline constexpr wchar_t MinimumCore[]=L"1.2.0";'+[Environment]::NewLine
        AtomicBytes "$dir\release-public.blob" $pub
        AtomicBytes "$dir\TrustConfig.h" ([Text.Encoding]::ASCII.GetBytes($header))
        return
    }
    $dll="$dir\ImTiredPressF.dll";$exe="$dir\ImTiredPressF.exe";$manifest="$dir\ImTiredPressF.manifest"
    if($Action -ne 'verify'){
        if($PublicFile -and $Action -ne 'offline'){throw 'Release private signing is restricted to the offline hardware signer'}
        AtomicBytes ($dll+'.sig') (Record $dll 'ITPF-C01')
    }
    Verify $dll 'ITPF-C01'
    if($Action -eq 'core'){return}
    $clientVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
    $coreVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($dll).FileVersion
    foreach($v in @($clientVersion,$coreVersion)){if($v -notmatch '^(0|[1-9][0-9]?)\.(0|[1-9][0-9]?)\.(0|[1-9][0-9]?)$'){throw 'Invalid PE version'}}
    $mcpExe=Join-Path $dir 'ImTiredPressF(MCP).exe'
    if([Diagnostics.FileVersionInfo]::GetVersionInfo($mcpExe).FileVersion -cne $clientVersion){throw 'Client version mismatch'}
    foreach($variant in @('standard','mcp')){
        $schema='ImTiredPressF-release-v1'
        $image=$exe
        $manifest=Join-Path $dir 'ImTiredPressF.manifest'
        if($variant -eq 'mcp'){
            $schema='ImTiredPressF-mcp-release-v1'
            $image=$mcpExe
            $manifest=Join-Path $dir 'ImTiredPressF-MCP.manifest'
        }
        $doc="$schema`nrepository=$repo`nclient=$clientVersion`ncore=$coreVersion`nexe_sha256=$(Hex (HashFile $image))`ndll_sha256=$(Hex (HashFile $dll))`n"
        if($Action -ne 'verify'){
            AtomicBytes $manifest ([Text.Encoding]::ASCII.GetBytes($doc))
            AtomicBytes ($manifest+'.sig') (Record $manifest 'ITPF-M01')
        }
        Verify $manifest 'ITPF-M01'
        if([IO.File]::ReadAllText($manifest) -cne $doc){throw 'Release manifest and artifacts differ'}
    }
    Write-Output "Verified release $clientVersion / $coreVersion"
}finally{if(Get-Variable ecdsa -ErrorAction SilentlyContinue){$ecdsa.Dispose()};$key.Dispose();$sha.Dispose()}

