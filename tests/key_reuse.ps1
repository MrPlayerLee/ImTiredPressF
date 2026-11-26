param([string]$WorkingDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$test=Join-Path $WorkingDirectory ('key-'+[Guid]::NewGuid().ToString('N'))
$id='https://github.com/ImTiredPressF-test/'+[Guid]::NewGuid().ToString('N')
$hash=[Security.Cryptography.SHA256]::Create()
$name='ImTiredPressF.Dev.'+[BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($id))).Replace('-','').ToLowerInvariant()
$ksp=[Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
if([Security.Cryptography.CngKey]::Exists($name,$ksp)){throw 'Fixture key collision'}
New-Item -ItemType Directory "$test\tools" | Out-Null
Copy-Item -LiteralPath "$root\tools\sign.ps1" -Destination "$test\tools"
Copy-Item -LiteralPath "$root\VERSION_AGENT.md" -Destination $test
[IO.File]::WriteAllText("$test\BuildConfig.props","<Project><PropertyGroup><RepositoryUrl>$id</RepositoryUrl></PropertyGroup></Project>")
try{
    & "$test\tools\sign.ps1" prepare "$test\generated"
    $first=(Get-FileHash "$test\generated\TrustConfig.h").Hash
    & "$test\tools\sign.ps1" prepare "$test\generated"
    if((Get-FileHash "$test\generated\TrustConfig.h").Hash -ne $first){throw 'Key rotated on rebuild'}
    $key=[Security.Cryptography.CngKey]::Open($name,$ksp)
    try{if($key.ExportPolicy -ne [Security.Cryptography.CngExportPolicies]::None){throw 'Exportable key'}}finally{$key.Dispose()}
    Write-Output 'PASS first-use generation, persistent public key, non-exportable private key.'
}finally{
    if([Security.Cryptography.CngKey]::Exists($name,$ksp)){$tmp=[Security.Cryptography.CngKey]::Open($name,$ksp);$tmp.Delete();$tmp.Dispose()}
    $hash.Dispose()
}
