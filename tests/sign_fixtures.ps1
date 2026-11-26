param([string]$Directory,[string]$Binaries)
$ErrorActionPreference='Stop'
$dir=[IO.Path]::GetFullPath($Directory)
[IO.Directory]::CreateDirectory($dir) | Out-Null
$key=[Security.Cryptography.ECDsaCng]::new(256)
$sha=[Security.Cryptography.SHA256]::Create()
function Hex([byte[]]$data){[BitConverter]::ToString($data).Replace('-','').ToLowerInvariant()}
function Sig([byte[]]$data,$kind){
    [byte[]]$head=[Text.Encoding]::ASCII.GetBytes($kind)+$sha.ComputeHash($data)
    return ,([byte[]]($head+$key.SignHash($sha.ComputeHash($head))))
}
try {
    $pub=$key.Key.Export([Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob)
    $repo=([xml](Get-Content (Join-Path (Split-Path $PSScriptRoot) BuildConfig.props) -Raw)).Project.PropertyGroup.RepositoryUrl
    $id=$repo.Substring(19)
    $header="inline constexpr unsigned char TrustedKey[]={"+(($pub | ForEach-Object {$_.ToString()}) -join ',')+"};`ninline constexpr wchar_t Repository[]=L`"$id`";`ninline constexpr wchar_t MinimumCore[]=L`"0.0.0`";"
    [IO.File]::WriteAllText("$dir\TrustConfig.h",$header)
    $exe=[IO.File]::ReadAllBytes("$Binaries\ImTiredPressF.exe");$dll=[IO.File]::ReadAllBytes("$Binaries\ImTiredPressF.dll")
    [IO.File]::WriteAllBytes("$dir\foreign.sig",(Sig $dll 'ITPF-C01'))
    # Use ECDSA fixture signatures for test resources only; the product does not trust this key.
    foreach($file in @('exe','mcp.exe','dll')){
        if($file -eq 'mcp.exe'){$source=[IO.File]::ReadAllBytes("$Binaries\ImTiredPressF(MCP).exe")}elseif($file -eq 'exe'){$source=$exe}else{$source=$dll}
        $str=[Text.Encoding]::GetEncoding(28591).GetString($source)
        $index=$str.IndexOf([Text.Encoding]::GetEncoding(28591).GetString([byte[]](0xbd,0x04,0xef,0xfe,0,0,1,0)),[StringComparison]::Ordinal)
        if($index -lt 0){throw 'Missing fixture version resource'}
        foreach($patch in @(1,2)){
            [byte[]]$copy=$source.Clone()
            [BitConverter]::GetBytes([uint32]0x10014).CopyTo($copy,$index+8)
            [BitConverter]::GetBytes([uint32]($patch -shl 16)).CopyTo($copy,$index+12)
            [IO.File]::WriteAllBytes("$dir\$patch.$file",$copy)
            if($file -eq 'dll'){[IO.File]::WriteAllBytes("$dir\$patch.dll.sig",(Sig $copy 'ITPF-C01'))}
        }
    }
    $dh=Hex $sha.ComputeHash([IO.File]::ReadAllBytes("$dir\2.dll"))
    foreach($variant in @('standard','mcp')){
        $image='2.exe';$schema='ImTiredPressF-release-v1';$name='manifest'
        if($variant -eq 'mcp'){$image='2.mcp.exe';$schema='ImTiredPressF-mcp-release-v1';$name='mcp.manifest'}
        $eh=Hex $sha.ComputeHash([IO.File]::ReadAllBytes("$dir\$image"))
        $manifest=[Text.Encoding]::ASCII.GetBytes("$schema`nrepository=$repo`nclient=1.20.2`ncore=1.20.2`nexe_sha256=$eh`ndll_sha256=$dh`n")
        [IO.File]::WriteAllBytes("$dir\$name",$manifest)
        [IO.File]::WriteAllBytes("$dir\$name.sig",(Sig $manifest 'ITPF-M01'))
    }
}finally{$key.Dispose();$sha.Dispose()}
