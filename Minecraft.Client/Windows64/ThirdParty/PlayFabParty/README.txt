PlayFab Party (Microsoft GDK extension) — bundled layout for CMake
==================================================================

CMake looks here automatically (same layout as GDK’s PlayFab.Party.Cpp):

  Include/Party.h          (and the rest of the Party headers from GDK)
  Lib/x64/Party.lib
  Redist/x64/Party.dll

Populate this folder by copying from your GDK install, for example:

  %GameDKLatest%\GRDK\ExtensionLibraries\PlayFab.Party.Cpp\Include   -> Include
  %GameDKLatest%\GRDK\ExtensionLibraries\PlayFab.Party.Cpp\Lib\x64   -> Lib\x64
  %GameDKLatest%\GRDK\ExtensionLibraries\PlayFab.Party.Cpp\Redist\x64 -> Redist\x64

You may use Git LFS for Party.dll if the binary is large.

Redistribution: follow your Microsoft GDK and PlayFab Party license terms; this
repo does not ship Microsoft binaries—you add them locally or in CI.
