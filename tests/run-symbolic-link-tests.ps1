# Run with Developer Mode enabled or from an administrator terminal.
param([Parameter(Mandatory = $true)][string]$ShellExe)

$ErrorActionPreference = 'Stop'
$helperPath = (Resolve-Path -LiteralPath $ShellExe).Path
$testDirectory = [IO.Path]::GetFullPath("$PSScriptRoot\..\artifacts\symlink-verification\tests")
$testRoot = Join-Path $testDirectory ([guid]::NewGuid().ToString())
$source = Join-Path $testRoot 'source'
$destination = Join-Path $testRoot 'destination'
$fileName = "文件 & [1].txt"
$file = Join-Path $source $fileName
$folder = Join-Path $source 'folder.with.dots'
$fileLink = Join-Path $destination $fileName
$folderLink = Join-Path $destination 'folder.with.dots'

try {
    New-Item -ItemType Directory -Path $folder, $destination -Force | Out-Null
    [IO.File]::WriteAllText($file, 'original')
    [IO.File]::WriteAllText((Join-Path $folder 'child.txt'), 'child')
    $helperArguments = '--paste-symlink ' + ((@($destination, $file, $folder) | ForEach-Object { '"' + $_ + '"' }) -join ' ')

    foreach ($run in 1..2) {
        $process = Start-Process -FilePath $helperPath -ArgumentList $helperArguments -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Link creation failed: $($process.ExitCode)" }
    }

    foreach ($link in @($fileLink, $folderLink, (Join-Path $destination '文件 & [1] (2).txt'), (Join-Path $destination 'folder.with.dots (2)'))) {
        if ((Get-Item -LiteralPath $link).LinkType -ne 'SymbolicLink') { throw "Expected a symbolic link: $link" }
    }
    if ([IO.File]::ReadAllText((Join-Path $folderLink 'child.txt')) -ne 'child') { throw 'Folder link does not resolve.' }

    [IO.File]::WriteAllText($fileLink, 'changed')
    if ([IO.File]::ReadAllText($file) -ne 'changed') { throw 'File link does not update its target.' }
    [IO.Directory]::Delete($folderLink)
    if (-not [IO.File]::Exists((Join-Path $folder 'child.txt'))) { throw 'Deleting a link affected its target.' }

    Write-Output 'PASS: file and folder links, batch creation, name conflicts, and target preservation.'
} finally {
    if ([IO.Path]::GetDirectoryName($testRoot) -eq $testDirectory -and (Test-Path -LiteralPath $testRoot)) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
