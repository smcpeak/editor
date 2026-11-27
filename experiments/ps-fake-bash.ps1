# ps-fake-bash.ps1
# Stand-in for bash: allocate heap memory, then load iphlpapi.dll.

# Usage:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File ps-fake-bash.ps1
#     [-HeapBytes N] [-ChunkBytes N] [-Method M] [-Pad <text>]
#
# -HeapBytes: total native heap memory to allocate first, in chunks of
#   -ChunkBytes (default 1000), with Marshal.AllocHGlobal.  It is
#   never freed.
#
# -Method:
#   LoadLibrary  LoadLibraryW("iphlpapi.dll") (the default)
#   FullPath     LoadLibraryW with the System32 path
#   Network      .NET's NetworkInterface.GetAllNetworkInterfaces(),
#                which uses iphlpapi.dll
#
# -Chdir: after allocating, call SetCurrentDirectoryW on the current
#   directory, as Cygwin's startup rewrites the Win32 current
#   directory.
#
# -Pad: ignored; it is there to make the command line long.
#
# Exit codes: 0 if iphlpapi.dll loaded, 1 if not, 2 for other errors.

param(
  [long] $HeapBytes = 0,
  [int] $ChunkBytes = 1000,
  [ValidateSet('LoadLibrary', 'FullPath', 'Network')]
  [string] $Method = 'LoadLibrary',
  [switch] $Chdir,
  [string] $Pad = ''
)

Add-Type -Namespace PsFakeBash -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true,
           CharSet = CharSet.Unicode)]
public static extern IntPtr LoadLibraryW(string name);

[DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
public static extern IntPtr GetModuleHandleW(string name);

[DllImport("kernel32.dll", SetLastError = true,
           CharSet = CharSet.Unicode)]
public static extern bool SetCurrentDirectoryW(string path);
'@

function Test-Loaded {
  return [PsFakeBash.Native]::GetModuleHandleW('iphlpapi.dll') -ne
    [IntPtr]::Zero
}

if (Test-Loaded) {
  Write-Output 'ps-fake-bash: iphlpapi.dll was already loaded'
  exit 2
}

# Allocate heap memory, touching each chunk so it is committed.
$allocated = 0
while ($allocated -lt $HeapBytes) {
  $n = [Math]::Min([long]$ChunkBytes, $HeapBytes - $allocated)
  $p = [Runtime.InteropServices.Marshal]::AllocHGlobal([IntPtr]$n)
  [Runtime.InteropServices.Marshal]::WriteByte($p, 0, 1)
  $allocated += $n
}

if ($Chdir) {
  $cwd = [Environment]::CurrentDirectory
  if (-not [PsFakeBash.Native]::SetCurrentDirectoryW($cwd)) {
    Write-Output "ps-fake-bash: SetCurrentDirectoryW($cwd) failed"
    exit 2
  }
}

$err = 0
switch ($Method) {
  'LoadLibrary' {
    $h = [PsFakeBash.Native]::LoadLibraryW('iphlpapi.dll')
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  }
  'FullPath' {
    $path = Join-Path ([Environment]::SystemDirectory) 'iphlpapi.dll'
    $h = [PsFakeBash.Native]::LoadLibraryW($path)
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  }
  'Network' {
    try {
      $null = [Net.NetworkInformation.NetworkInterface]::
        GetAllNetworkInterfaces()
    }
    catch {
      Write-Output "ps-fake-bash: GetAllNetworkInterfaces: $_"
    }
  }
}

if (Test-Loaded) {
  Write-Output "ps-fake-bash: heap $HeapBytes, $Method, loaded"
  exit 0
}
Write-Output ("ps-fake-bash: heap $HeapBytes, $Method, " +
  "unable to load iphlpapi.dll, Win32 error $err")
exit 1

# EOF
