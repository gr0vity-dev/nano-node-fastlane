$ErrorActionPreference = "Continue"

$env:S3_BUCKET_NAME = $env:S3_BUCKET_NAME ?? "repo.nano.org"
if ( "${env:NETWORK}" -eq "BETA" ) {
    $network_cfg = "beta"
}
elseif ( "${env:NETWORK}" -eq "TEST" ) {
    $network_cfg = "test"
}
else {
    $network_cfg = "live"
}

if (![string]::IsNullOrEmpty($env:S3_BUILD_DIRECTORY)) {
    $directory = "$env:S3_BUILD_DIRECTORY/$network_cfg"
}
else {
    $directory = $network_cfg
}

$exe = Resolve-Path -Path $env:GITHUB_WORKSPACE\build\nano-node-*-win64.exe
$zip = Resolve-Path -Path $env:GITHUB_WORKSPACE\build\nano-node-*-win64.zip

$exe_hash = ((Get-FileHash $exe).hash.ToLower())+"  "+(split-path -Path $exe -Resolve -leaf)
$zip_hash = ((Get-FileHash $zip).hash.ToLower())+"  "+(split-path -Path $zip -Resolve -leaf)

$exe_sha256_file = "$env:GITHUB_WORKSPACE\build\nano-node-$env:TAG-win64.exe.sha256"
$zip_sha256_file = "$env:GITHUB_WORKSPACE\build\nano-node-$env:TAG-win64.zip.sha256"

$exe_hash | Out-file -FilePath $exe_sha256_file
$zip_hash | Out-file -FilePath $zip_sha256_file

Write-Output "Hash: $exe_hash"
Write-Output "Hash: $zip_hash"

aws s3 cp "$exe" s3://$env:S3_BUCKET_NAME/$directory/binaries/nano-node-$env:TAG-win64.exe --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$exe.sha256" s3://$env:S3_BUCKET_NAME/$directory/binaries/nano-node-$env:TAG-win64.exe.sha256 --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$zip" s3://$env:S3_BUCKET_NAME/$directory/binaries/nano-node-$env:TAG-win64.zip --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$zip.sha256" s3://$env:S3_BUCKET_NAME/$directory/binaries/nano-node-$env:TAG-win64.zip.sha256 --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers