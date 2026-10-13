# Get or set the mute state of the default audio output device (Core Audio IAudioEndpointVolume).
#   endpoint_mute.ps1            -> prints True/False
#   endpoint_mute.ps1 -Set 1|0   -> mutes/unmutes, prints the previous state
# Used by run_bench.py so both players keep a real audio pipeline while the user hears nothing.
param([Nullable[int]]$Set = $null)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
[Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioEndpointVolume {
  int f0(); int f1(); int f2(); int f3(); int f4(); int f5(); int f6(); int f7(); int f8(); int f9(); int f10();
  int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, ref Guid ctx);
  int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
}
[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice { int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o); }
[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator { int f0(); int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice dev); }
[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator { }
public static class EndpointMute {
  static IAudioEndpointVolume Get() {
    var e = (IMMDeviceEnumerator)new MMDeviceEnumerator(); IMMDevice d;
    Marshal.ThrowExceptionForHR(e.GetDefaultAudioEndpoint(0, 1, out d));
    var iid = typeof(IAudioEndpointVolume).GUID; object o;
    Marshal.ThrowExceptionForHR(d.Activate(ref iid, 23, IntPtr.Zero, out o));
    return (IAudioEndpointVolume)o;
  }
  public static bool IsMuted() { bool m; Marshal.ThrowExceptionForHR(Get().GetMute(out m)); return m; }
  public static void SetMuted(bool m) { var g = Guid.Empty; Marshal.ThrowExceptionForHR(Get().SetMute(m, ref g)); }
}
"@
$was = [EndpointMute]::IsMuted()
if ($null -ne $Set) { [EndpointMute]::SetMuted([bool]$Set) }
$was
