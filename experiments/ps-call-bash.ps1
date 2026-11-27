# ps-call-bash.ps1
# PowerShell version of bash-experiment.cc: make bash crash.

# Usage:
#
#   powershell -ExecutionPolicy Bypass -File ps-call-bash.ps1 <bash> <size>
#
# Runs `"<bash>" -c "true #xxx...x"` with the command line padded with
# 'x' to <size> characters.  .NET's Process class with
# UseShellExecute=false calls CreateProcessW with that command line,
# putting the program name in double quotes.
#
# Exit codes as for bash-experiment.exe: 0 if bash succeeds, 1 if it
# fails, 2 for an error here.

param(
  [Parameter(Mandatory=$true)] [string] $Bash,
  [Parameter(Mandatory=$true)] [int] $Size
)

$prefix = "-c `"true #"
$fixed = "`"$Bash`" ".Length + $prefix.Length + 1
if ($Size -lt $fixed) {
  [Console]::Error.WriteLine(
    "ps-call-bash: size $Size is too small; the minimum is $fixed")
  exit 2
}
$arguments = $prefix + ('x' * ($Size - $fixed)) + '"'

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Bash
$psi.Arguments = $arguments
$psi.UseShellExecute = $false

# Set-Location doesn't change the process's Win32 current directory,
# which bash would otherwise inherit; and the directory matters.
$psi.WorkingDirectory =
  (Get-Location -PSProvider FileSystem).ProviderPath

try {
  $proc = [System.Diagnostics.Process]::Start($psi)
}
catch {
  [Console]::Error.WriteLine("ps-call-bash: start failed: $_")
  exit 2
}
$proc.WaitForExit()
$code = $proc.ExitCode
Write-Output "ps-call-bash: size $Size, bash exit code $code"
if ($code -eq 0) { exit 0 } else { exit 1 }

# EOF
