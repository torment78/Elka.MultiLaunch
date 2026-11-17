$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$images = @()
foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
    $bitmap = [Drawing.Bitmap]::new($size, $size)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.ScaleTransform($size / 64.0, $size / 64.0)
    $g.Clear([Drawing.Color]::Transparent)
    $back = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(24, 30, 38))
    $accent = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(77, 218, 191))
    $pen = [Drawing.Pen]::new($accent, 4)
    $g.FillEllipse($back, 1, 1, 62, 62)
    $g.DrawLine($pen, 21, 32, 35, 32)
    $g.DrawLine($pen, 35, 18, 35, 46)
    $g.DrawLine($pen, 35, 18, 45, 18)
    $g.DrawLine($pen, 35, 46, 45, 46)
    $g.FillRectangle($accent, 12, 26, 12, 12)
    $g.FillRectangle($accent, 43, 13, 10, 10)
    $g.FillRectangle($accent, 43, 41, 10, 10)
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
    $images += ,@($size, $stream.ToArray())
    $stream.Dispose(); $pen.Dispose(); $accent.Dispose(); $back.Dispose(); $g.Dispose(); $bitmap.Dispose()
}
$file = [IO.File]::Create((Join-Path $root 'assets\app.ico'))
$writer = [IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($entry in $images) {
    $size = if ($entry[0] -eq 256) { 0 } else { $entry[0] }
    $writer.Write([byte]$size); $writer.Write([byte]$size); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$entry[1].Length); $writer.Write([uint32]$offset)
    $offset += $entry[1].Length
}
foreach ($entry in $images) { $writer.Write([byte[]]$entry[1]) }
$writer.Dispose()
