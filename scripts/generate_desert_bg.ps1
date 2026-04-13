$ErrorActionPreference = "Stop"

$workflow = @{
  "1" = @{ class_type="EmptyImage"; inputs=@{ width=480; height=320; batch_size=1; color=0xF7F1E8 } }
  "2" = @{ class_type="EmptyImage"; inputs=@{ width=480; height=190; batch_size=1; color=0xB89B6D } }
  "3" = @{ class_type="ImageBlend"; inputs=@{ image1=@("1",0); image2=@("2",0); blend_factor=0.14; blend_mode="normal" } }
  "4" = @{ class_type="EmptyImage"; inputs=@{ width=220; height=86; batch_size=1; color=0xB89B6D } }
  "5" = @{ class_type="ImageBlur"; inputs=@{ image=@("4",0); blur_radius=11; sigma=3.2 } }
  "6" = @{ class_type="ImageCompositeMasked"; inputs=@{ destination=@("3",0); source=@("5",0); x=14; y=182; resize_source=$false } }
  "7" = @{ class_type="EmptyImage"; inputs=@{ width=250; height=92; batch_size=1; color=0xB89B6D } }
  "8" = @{ class_type="ImageBlur"; inputs=@{ image=@("7",0); blur_radius=11; sigma=3.2 } }
  "9" = @{ class_type="ImageCompositeMasked"; inputs=@{ destination=@("6",0); source=@("8",0); x=152; y=172; resize_source=$false } }
  "10" = @{ class_type="EmptyImage"; inputs=@{ width=240; height=90; batch_size=1; color=0xD47B4A } }
  "11" = @{ class_type="ImageBlend"; inputs=@{ image1=@("10",0); image2=@("1",0); blend_factor=0.23; blend_mode="normal" } }
  "12" = @{ class_type="ImageBlur"; inputs=@{ image=@("11",0); blur_radius=9; sigma=2.8 } }
  "13" = @{ class_type="ImageCompositeMasked"; inputs=@{ destination=@("9",0); source=@("12",0); x=38; y=222; resize_source=$false } }
  "14" = @{ class_type="EmptyImage"; inputs=@{ width=280; height=95; batch_size=1; color=0xD47B4A } }
  "15" = @{ class_type="ImageBlend"; inputs=@{ image1=@("14",0); image2=@("1",0); blend_factor=0.20; blend_mode="normal" } }
  "16" = @{ class_type="ImageBlur"; inputs=@{ image=@("15",0); blur_radius=9; sigma=2.6 } }
  "17" = @{ class_type="ImageCompositeMasked"; inputs=@{ destination=@("13",0); source=@("16",0); x=190; y=226; resize_source=$false } }
  "18" = @{ class_type="EmptyImage"; inputs=@{ width=480; height=72; batch_size=1; color=0xD47B4A } }
  "19" = @{ class_type="ImageBlend"; inputs=@{ image1=@("18",0); image2=@("1",0); blend_factor=0.30; blend_mode="normal" } }
  "20" = @{ class_type="ImageCompositeMasked"; inputs=@{ destination=@("17",0); source=@("19",0); x=0; y=248; resize_source=$false } }
  "21" = @{ class_type="SaveImage"; inputs=@{ images=@("20",0); filename_prefix="core_weather/desert_minimal_bg" } }
}

$body = @{ prompt = $workflow } | ConvertTo-Json -Depth 30
$response = Invoke-RestMethod -Uri "http://127.0.0.1:8188/prompt" -Method Post -ContentType "application/json" -Body $body
$promptId = $response.prompt_id
if (-not $promptId) { throw "No prompt_id returned by ComfyUI." }

$deadline = (Get-Date).AddMinutes(3)
$history = $null
while ((Get-Date) -lt $deadline) {
  $history = Invoke-RestMethod -Uri ("http://127.0.0.1:8188/history/" + $promptId) -Method Get
  if ($history.$promptId) { break }
  Start-Sleep -Milliseconds 800
}
if (-not $history.$promptId) { throw "Timed out waiting for ComfyUI output." }

$entry = $history.$promptId
$images = @()
foreach ($node in $entry.outputs.PSObject.Properties.Value) {
  if ($node.images) { $images += $node.images }
}
if ($images.Count -eq 0) { throw "No images in ComfyUI output." }

$img = $images[0]
$src = Join-Path "C:\Flic\ComfyUI\output" (Join-Path $img.subfolder $img.filename)
$dst = "src/web/web_assets/backgrounds/current.png"
Copy-Item -Path $src -Destination $dst -Force
Write-Output ("GENERATED: " + $dst)
