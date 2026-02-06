# Mermaid Diagram Validation Script (PowerShell Edition)
# Supports: stdin (pipeline), single diagram files (.mmd), and markdown files with embedded diagrams

param (
    [Parameter(ValueFromPipeline = $true)]
    [string]$InputObject,

    [Parameter(Position = 0)]
    [string]$FilePath
)

# Configuration
$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.Encoding]::UTF8

# Colors
$Green = "Green"
$Red = "Red"
$Yellow = "Yellow"

# Statistics
$Stats = @{
    Passed = 0
    Failed = 0
    Total  = 0
}

function Validate-DiagramString {
    param (
        [string]$Content,
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Content)) {
        return
    }

    # Create temp files
    $TempDiagram = [System.IO.Path]::GetTempFileName() + ".mmd"
    $TempOutput = [System.IO.Path]::GetTempFileName() + ".svg"
    $TempStdOut = [System.IO.Path]::GetTempFileName() + ".out"
    $TempStdErr = [System.IO.Path]::GetTempFileName() + ".err"

    try {
        # Write content to temp file (Force UTF8NoBOM for compatibility)
        $Content | Set-Content -Path $TempDiagram -Encoding UTF8

        # Run mermaid-cli (mmdc)
        # Note: Using cmd /c to ensure npx is resolved correctly in all shells
        $Process = Start-Process -FilePath "cmd" -ArgumentList "/c npx -y @mermaid-js/mermaid-cli@latest -i `"$TempDiagram`" -o `"$TempOutput`" -q" -RedirectStandardOutput $TempStdOut -RedirectStandardError $TempStdErr -PassThru -WindowStyle Hidden -Wait

        if ($Process.ExitCode -eq 0 -and (Test-Path $TempOutput) -and (Get-Item $TempOutput).Length -gt 0) {
            Write-Host "✅ $Label`: Valid" -ForegroundColor $Green
            $Stats.Passed++
            return $true
        }
        else {
            Write-Host "❌ $Label`: Invalid" -ForegroundColor $Red
            # Combine output and error for display
            if (Test-Path $TempStdErr) { Get-Content $TempStdErr | Write-Host -ForegroundColor $Red }
            if (Test-Path $TempStdOut) { Get-Content $TempStdOut | Write-Host -ForegroundColor $Red }
            $Stats.Failed++
            return $false
        }
    }
    finally {
        # Cleanup
        if (Test-Path $TempDiagram) { Remove-Item $TempDiagram -ErrorAction SilentlyContinue }
        if (Test-Path $TempOutput) { Remove-Item $TempOutput -ErrorAction SilentlyContinue }
        if (Test-Path $TempStdOut) { Remove-Item $TempStdOut -ErrorAction SilentlyContinue }
        if (Test-Path $TempStdErr) { Remove-Item $TempStdErr -ErrorAction SilentlyContinue }
    }
}

function Validate-MarkdownFile {
    param ([string]$Path)

    Write-Host "Validating Mermaid diagrams in: $Path" -ForegroundColor $Yellow
    Write-Host ""

    $Lines = Get-Content -Path $Path -Encoding UTF8
    $InMermaid = $false
    $CurrentDiagram = [System.Text.StringBuilder]::new()
    $DiagramNum = 0
    $FoundAny = $false

    foreach ($Line in $Lines) {
        if ($Line -match '^```mermaid') {
            $InMermaid = $true
            $CurrentDiagram.Clear() | Out-Null
            $DiagramNum++
            $FoundAny = $true
            continue
        }
        
        if ($InMermaid) {
            if ($Line -match '^```$') {
                $InMermaid = $false
                $Stats.Total++
                Validate-DiagramString -Content $CurrentDiagram.ToString() -Label "Diagram $DiagramNum"
                Write-Host ""
            }
            else {
                $CurrentDiagram.AppendLine($Line) | Out-Null
            }
        }
    }

    if (-not $FoundAny) {
        Write-Host "⚠️  No Mermaid diagrams found in $Path" -ForegroundColor $Yellow
    }
}

# --- Main Logic ---

# Scenario 1: Input from Pipeline (stdin)
if ($PSCmdlet.MyInvocation.BoundParameters.ContainsKey('InputObject') -or $Input) {
    # Check if we have accumulated pipeline input or explicit parameter
    $PipelineContent = @($Input) 
    
    if ($PipelineContent.Count -gt 0) {
        # Combine all lines if passed via pipeline
        $FullContent = $PipelineContent -join [Environment]::NewLine
        $Stats.Total++
        Validate-DiagramString -Content $FullContent -Label "Diagram (Pipeline)"
    }
    elseif (-not [string]::IsNullOrEmpty($InputObject)) {
        $Stats.Total++
        Validate-DiagramString -Content $InputObject -Label "Diagram (Pipeline)"
    }
}
# Scenario 2: Input from File
elseif (-not [string]::IsNullOrEmpty($FilePath)) {
    if (-not (Test-Path $FilePath)) {
        Write-Error "File not found: $FilePath"
        exit 1
    }

    $FullPath = Resolve-Path $FilePath
    $Extension = [System.IO.Path]::GetExtension($FullPath).ToLower()

    if ($Extension -eq ".md") {
        Validate-MarkdownFile -Path $FullPath
    }
    else {
        # Treat as single diagram file (mmd, mermaid, etc.)
        $Content = Get-Content -Path $FullPath -Raw -Encoding UTF8
        $Stats.Total++
        Validate-DiagramString -Content $Content -Label "Diagram $(Split-Path $FullPath -Leaf)"
    }
}
else {
    Write-Host "Usage: "
    Write-Host "  1. echo 'graph TD; A-->B' | .\validate_mermaid.ps1"
    Write-Host "  2. .\validate_mermaid.ps1 -FilePath 'diagram.mmd'"
    Write-Host "  3. .\validate_mermaid.ps1 'doc.md'"
    exit 1
}

# Summary
if ($Stats.Total -gt 1) {
    Write-Host "================================"
    Write-Host "Validation Summary"
    Write-Host "================================"
    Write-Host "Total:  $($Stats.Total)"
    Write-Host "Passed: $($Stats.Passed)" -ForegroundColor $Green
    Write-Host "Failed: $($Stats.Failed)" -ForegroundColor $Red
    Write-Host "================================"
}

if ($Stats.Failed -gt 0) {
    exit 1
}
exit 0
