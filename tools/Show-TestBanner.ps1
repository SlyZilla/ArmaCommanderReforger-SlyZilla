# On-screen banner for an automated test that runs the game client on Jacob's screen.
# Started by the test launcher (ACR-Watcher.ps1) next to the game: a click-through,
# always-on-top strip at the top of the screen saying what is running. It never takes
# focus, isn't in the engine's screenshots (it's a desktop window), and closes itself
# when the game process ends.
#   powershell -File tools/Show-TestBanner.ps1 -SpecFile <json> -WatchPid <game pid>
# The spec is JSON: title, lines (array), timeoutSec, started (ISO time).
param([string]$SpecFile, [int]$WatchPid)

$ErrorActionPreference = 'Stop'
$spec = Get-Content -LiteralPath $SpecFile -Raw | ConvertFrom-Json
Remove-Item -LiteralPath $SpecFile -ErrorAction SilentlyContinue

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -ReferencedAssemblies System.Windows.Forms, System.Drawing -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;
public class AcrBannerForm : Form
{
    [DllImport("user32.dll")]
    static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    protected override bool ShowWithoutActivation { get { return true; } }
    protected override CreateParams CreateParams
    {
        get
        {
            CreateParams cp = base.CreateParams;
            // WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
            cp.ExStyle |= 0x8 | 0x80 | 0x80000 | 0x20 | 0x8000000;
            return cp;
        }
    }
    // A borderless game window can cover it: put it back on top without activating it.
    public void KeepOnTop()
    {
        SetWindowPos(Handle, new IntPtr(-1), 0, 0, 0, 0, 0x1 | 0x2 | 0x10 | 0x40);
    }
}
'@

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$form = New-Object AcrBannerForm
$form.FormBorderStyle = 'None'
$form.ShowInTaskbar = $false
$form.TopMost = $true
$form.StartPosition = 'Manual'
$form.BackColor = [System.Drawing.Color]::FromArgb(150, 20, 20)
$form.Opacity = 0.88
$width = [Math]::Min(1100, $screen.Width - 40)

$title = New-Object System.Windows.Forms.Label
$title.Font = New-Object System.Drawing.Font('Segoe UI', 13, [System.Drawing.FontStyle]::Bold)
$title.ForeColor = [System.Drawing.Color]::White
$title.AutoSize = $false
$title.TextAlign = 'MiddleCenter'
$title.SetBounds(0, 6, $width, 28)
$form.Controls.Add($title)

$body = New-Object System.Windows.Forms.Label
$body.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$body.ForeColor = [System.Drawing.Color]::FromArgb(255, 235, 235)
$body.AutoSize = $false
$body.TextAlign = 'TopCenter'
$lines = @($spec.lines)
$bodyHeight = 20 * [Math]::Max(1, $lines.Count)
$body.SetBounds(8, 36, $width - 16, $bodyHeight)
$body.Text = ($lines -join "`n")
$form.Controls.Add($body)

$form.SetBounds($screen.X + [int](($screen.Width - $width) / 2), $screen.Y + 8, $width, 46 + $bodyHeight)

$started = [DateTime]::Parse($spec.started)
$timeout = [int]$spec.timeoutSec
function Update-Title {
    $elapsed = [int]((Get-Date) - $started).TotalSeconds
    $text = [string]$spec.title + '   ' + [TimeSpan]::FromSeconds($elapsed).ToString('mm\:ss')
    if ($timeout -gt 0) { $text += ' / ' + [TimeSpan]::FromSeconds($timeout).ToString('mm\:ss') + ' max' }
    $title.Text = $text
}
Update-Title

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 1000
$timer.Add_Tick({
    if ($WatchPid -gt 0 -and !(Get-Process -Id $WatchPid -ErrorAction SilentlyContinue)) { $form.Close(); return }
    # Belt and braces: never outlive the launcher's own timeout by long.
    if ($timeout -gt 0 -and ((Get-Date) - $started).TotalSeconds -gt $timeout + 120) { $form.Close(); return }
    Update-Title
    $form.KeepOnTop()
})
$form.Add_Shown({ $timer.Start(); $form.KeepOnTop() })
[System.Windows.Forms.Application]::Run($form)
