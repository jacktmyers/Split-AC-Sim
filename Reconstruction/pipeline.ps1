param(
    [Parameter(Mandatory=$true)]
    [string]$Mp4File,

    [int]$Fps = 2
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$SlamDir = Join-Path $ProjectRoot "SLAM3R"

if (-not (Test-Path $Mp4File)) {
    Write-Error "File '$Mp4File' not found."
    exit 1
}

if ($Fps -le 0) {
    Write-Error "FPS must be a positive integer."
    exit 1
}

$basename = [System.IO.Path]::GetFileNameWithoutExtension($Mp4File)
$outputDir = Join-Path $ProjectRoot "img" $basename

if (Test-Path $outputDir) {
    Remove-Item -Recurse -Force $outputDir
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Write-Host "Extracting frames at $Fps fps to $outputDir ..."
ffmpeg -i $Mp4File -vf "fps=$Fps" -start_number 0 "$outputDir/frame_%d.png"
if ($LASTEXITCODE -ne 0) {
    Write-Error "ffmpeg failed to extract frames."
    exit 1
}
Write-Host "Frames extracted successfully."

Write-Host "Activating slam3r environment and running reconstruction..."
conda activate slam3r
Push-Location $SlamDir
python recon.py --device cuda --dataset ../img/$basename/ --keyframe_stride -1 --buffer_size 70 --test_name $basename --save_dir ../points/original/
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    Write-Error "SLAM3R reconstruction failed."
    exit 1
}
Pop-Location
Write-Host "SLAM3R reconstruction complete."

Write-Host "Switching to open3d environment and running patch review GUI..."
conda activate open3d
python "$ProjectRoot/reconstruct_gui.py" $basename
if ($LASTEXITCODE -ne 0) {
    Write-Error "reconstruct_gui.py failed."
    exit 1
}
Write-Host "Patch review complete."

$meshPath = Join-Path $ProjectRoot "mesh" $basename "mesh.ply"
if (-not (Test-Path $meshPath)) {
    Write-Error "Mesh file not found at $meshPath"
    exit 1
}

Write-Host "Opening mesh viewer..."
python "$ProjectRoot/simple_viewer.py" $meshPath
