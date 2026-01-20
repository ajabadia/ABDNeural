
$sourceDir = "D:\desarrollos\ABDNeural\DOC"
$destDir = "D:\desarrollos\ABDNeural\AnalyzedDocs"
New-Item -ItemType Directory -Force -Path $destDir

$files = Get-ChildItem -Path $sourceDir -Filter "*.docx"

foreach ($file in $files) {
    $tempDir = Join-Path $env:TEMP "extract_docx_$($file.BaseName)"
    $zipPath = Join-Path $env:TEMP "$($file.BaseName).zip"
    
    Copy-Item $file.FullName $zipPath
    Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force
    
    $xmlPath = Join-Path $tempDir "word\document.xml"
    if (Test-Path $xmlPath) {
        $xmlContent = Get-Content $xmlPath -Raw
        # Simple regex to extract text. Note: This collapses paragraphs, but captures text content.
        # A slightly better approach is to replace </w:p> with newline first.
        $xmlContent = $xmlContent -replace '</w:p>', "`n"
        $textContent = $xmlContent -replace '<[^>]+>', ""
        # Decode HTML entities if necessary (basic ones)
        $textContent = $textContent -replace '&lt;', "<" -replace '&gt;', ">" -replace '&amp;', "&"
        
        $outPath = Join-Path $destDir "$($file.Name).txt"
        $textContent | Set-Content -Path $outPath
        Write-Host "Extracted text to $outPath"
    } else {
        Write-Warning "Could not find document.xml in $($file.Name)"
    }
    
    Remove-Item $tempDir -Recurse -Force
    Remove-Item $zipPath -Force
}
