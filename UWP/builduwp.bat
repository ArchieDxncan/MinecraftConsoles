"C:\Program Files\CMake\bin\cmake.exe" --preset uwp-x64 -S "D:\Github\MinecraftConsoles" -B "D:\Github\MinecraftConsoles\build\uwp-x64" -DUWP_CERT_PFX_PASSWORD="Macca10?"
"C:\Program Files\CMake\bin\cmake.exe" --build "D:\Github\MinecraftConsoles\build\uwp-x64" --config Release --target MinecraftLCE
powershell -ExecutionPolicy Bypass -File "D:\Github\MinecraftConsoles\scripts\package-full-uwp.ps1" -BuildDir "D:\Github\MinecraftConsoles\build\uwp-x64" -Configuration Release -CertPfx "D:\Github\MinecraftConsoles\UWP\archiedxncan.pfx" -CertPassword "Macca10?"
pause