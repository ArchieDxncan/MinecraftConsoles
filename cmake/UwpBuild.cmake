set(PLATFORM_NAME "Windows64")
set(IGGY_LIBS "iggy_w64.lib;iggyperfmon_w64.lib;iggyexpruntime_w64.lib")

# Required for encrypted PFX: plain password for thumbprint + PRE_BUILD patch (never prompts like Get-PfxCertificate).
# Set at configure time: cmake ... -DUWP_CERT_PFX_PASSWORD=secret  (not cmake --build).
# Or set env UWP_CERT_PFX_PASSWORD before configure (optional fallback when cache is empty).
set(UWP_CERT_PFX_PASSWORD "" CACHE STRING "Plain-text password for UWP/archiedxncan.pfx if the file is encrypted")
if(NOT UWP_CERT_PFX_PASSWORD AND DEFINED ENV{UWP_CERT_PFX_PASSWORD} AND NOT "$ENV{UWP_CERT_PFX_PASSWORD}" STREQUAL "")
  set(UWP_CERT_PFX_PASSWORD "$ENV{UWP_CERT_PFX_PASSWORD}" CACHE STRING "Plain-text password for UWP/archiedxncan.pfx if the file is encrypted" FORCE)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UwpVcxprojSigning.cmake")

# UWP/C++CX requires dynamic CRT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(_MC_ROOT "${CMAKE_SOURCE_DIR}")
set(_SAVED_CURRENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# The source-list files use CMAKE_CURRENT_SOURCE_DIR for relative entries.
# Set it to each module root before including those lists.
set(CMAKE_CURRENT_SOURCE_DIR "${_MC_ROOT}/Minecraft.World")
include("${_MC_ROOT}/Minecraft.World/cmake/sources/Common.cmake")
include("${_MC_ROOT}/Minecraft.World/cmake/sources/Durango.cmake")

set(CMAKE_CURRENT_SOURCE_DIR "${_MC_ROOT}/Minecraft.Client")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/Common.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/Durango.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/ORBIS.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/PS3.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/PSVita.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/Windows.cmake")
include("${_MC_ROOT}/Minecraft.Client/cmake/sources/Xbox360.cmake")

set(CMAKE_CURRENT_SOURCE_DIR "${_MC_ROOT}")
include("${_MC_ROOT}/cmake/CommonSources.cmake")
set(CMAKE_CURRENT_SOURCE_DIR "${_SAVED_CURRENT_SOURCE_DIR}")

# World
set(MINECRAFT_WORLD_SOURCES
  ${MINECRAFT_WORLD_COMMON}
  ${SOURCES_COMMON}
)

add_library(Minecraft.World STATIC ${MINECRAFT_WORLD_SOURCES} "Minecraft.World/IceSpikeFeature.cpp" "Minecraft.World/BlockBlobFeature.cpp")
target_include_directories(Minecraft.World
  PRIVATE
  "${CMAKE_BINARY_DIR}/generated/"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
)
target_compile_definitions(Minecraft.World PRIVATE
  ${MINECRAFT_SHARED_DEFINES}
  _LIB
  _UWP
  _WINDOWS64
)
target_precompile_headers(Minecraft.World PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:stdafx.h>")
configure_compiler_target(Minecraft.World)

# Client (reuse Windows source list, swap entrypoint to UWP)
set(MINECRAFT_CLIENT_SOURCES
  ${MINECRAFT_CLIENT_COMMON}
  ${MINECRAFT_CLIENT_WINDOWS}
  ${SOURCES_COMMON}
)
list(REMOVE_ITEM MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Windows64_Minecraft.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/iob_shim.asm"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/MinecraftWindows.rc"
)
# Be defensive: source lists are generated from many includes; strip by filename too.
list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX ".*/Windows64_Minecraft\\.cpp$")
list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX ".*/iob_shim\\.asm$")
list(APPEND MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/UWP_App.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/crt_compat.cpp"
)

add_executable(MinecraftLCE ${MINECRAFT_CLIENT_SOURCES})
target_include_directories(MinecraftLCE PRIVATE
  "${CMAKE_BINARY_DIR}/generated/"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
  # nlohmann/json (WindowsLeaderboardManager) — same as Minecraft.Client on Windows64
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Server/vendor"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP"
)
target_compile_definitions(MinecraftLCE PRIVATE
  ${MINECRAFT_SHARED_DEFINES}
  _UWP
  _WINDOWS64
)
target_precompile_headers(MinecraftLCE PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:stdafx.h>")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/compat_shims.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
configure_compiler_target(MinecraftLCE)

if(MSVC)
  target_compile_options(MinecraftLCE PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/ZW>)
  target_link_options(MinecraftLCE PRIVATE
    /NODEFAULTLIB:libcmt.lib
    /NODEFAULTLIB:libcmtd.lib
    /NODEFAULTLIB:libcpmt.lib
    /NODEFAULTLIB:libcpmtd.lib
  )
endif()

target_link_libraries(MinecraftLCE PRIVATE
  Minecraft.World
  d3d11
  dxgi
  ws2_32
  xinput
  winhttp
  legacy_stdio_definitions
  WindowsApp.lib
  user32.lib
  $<$<CONFIG:Debug>:
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC_d.lib"
  >
  $<$<NOT:$<CONFIG:Debug>>:
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC.lib"
  >
)

foreach(lib IN LISTS IGGY_LIBS)
  target_link_libraries(MinecraftLCE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/${lib}"
  )
endforeach()

set_property(TARGET MinecraftLCE PROPERTY VS_APPX_MANIFEST
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/Package.appxmanifest"
)

# Signing: CMake still injects a *second* PropertyGroup with Windows_TemporaryKey.pfx that
# overrides Globals — see uwp_patch_minecraft_lce_vcxproj_signing() deferred below.
set(_UWP_CERT_PFX "${CMAKE_CURRENT_SOURCE_DIR}/UWP/archiedxncan.pfx")
set(_UWP_CERT_THUMB "")
if(EXISTS "${_UWP_CERT_PFX}")
  set_property(TARGET MinecraftLCE PROPERTY VS_PACKAGE_CERTIFICATE_KEY_FILE "${_UWP_CERT_PFX}")
  set_property(TARGET MinecraftLCE PROPERTY VS_GLOBAL_PackageCertificateKeyFile "${_UWP_CERT_PFX}")
  # Ask MSBuild not to mint a temp cert (helps some VS versions; patch below is the real fix).
  set_property(TARGET MinecraftLCE PROPERTY VS_GLOBAL_GenerateTemporaryStoreCertificate false)

  # Use X509Certificate2 in a script — Get-PfxCertificate prompts for encrypted PFX and breaks
  # under CMake's NonInteractive PowerShell.
  execute_process(
    COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UwpReadPfxThumbprint.ps1"
      -PfxPath "${_UWP_CERT_PFX}"
      -Password "${UWP_CERT_PFX_PASSWORD}"
    OUTPUT_VARIABLE _UWP_CERT_THUMB
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _UWP_THUMB_RC
    ERROR_VARIABLE _UWP_THUMB_ERR
  )
  if(NOT _UWP_THUMB_RC EQUAL 0)
    message(WARNING "UWP: failed to read PFX thumbprint (set cache UWP_CERT_PFX_PASSWORD for encrypted PFX): ${_UWP_THUMB_ERR}")
    set(_UWP_CERT_THUMB "")
  endif()

  # PRE_BUILD: CMake regex patch missed some layouts; PowerShell rewrites *every*
  # PackageCertificate* tag + strips F63A... + temp-key None (runs after ZERO_CHECK regen).
  add_custom_command(
    TARGET MinecraftLCE PRE_BUILD
    COMMAND "${CMAKE_COMMAND}" -E env "UWP_CERT_PFX_PASSWORD=${UWP_CERT_PFX_PASSWORD}"
            powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UwpFixVcxprojSigning.ps1"
            -VcxPath "${CMAKE_BINARY_DIR}/MinecraftLCE.vcxproj"
            -PfxPath "${_UWP_CERT_PFX}"
    COMMENT "UWP: force PackageCertificate* to archiedxncan.pfx + matching thumbprint"
    VERBATIM
  )
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MinecraftLCE)

# Build versioning
set(BUILDVER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildVer.cmake")
set(BUILDVER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/Common/BuildVer.h")
add_custom_target(GenerateBuildVer
  COMMAND ${CMAKE_COMMAND}
    "-DOUTPUT_FILE=${BUILDVER_OUTPUT}"
    -P "${BUILDVER_SCRIPT}"
  COMMENT "Generating BuildVer.h..."
  VERBATIM
)
add_dependencies(Minecraft.World GenerateBuildVer)
add_dependencies(MinecraftLCE GenerateBuildVer)

# Same loose-file layout as the Win32 Minecraft.Client target (music, Common/*, Windows64Media).
# Without this, the MSIX/Appx from VS only contains the exe + manifest (~few MB).
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Utils.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyAssets.cmake")
set(UWP_ASSET_FOLDER_PAIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/music"                  "music"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/Media"           "Common/Media"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/res"             "Common/res"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/Trial"         "Common/Trial"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Common/Tutorial"      "Common/Tutorial"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64Media"         "Windows64Media"
)
setup_asset_folder_copy(MinecraftLCE "${UWP_ASSET_FOLDER_PAIRS}")
add_copyredist_target(MinecraftLCE)
add_gamehdd_target(MinecraftLCE)

# Tell VS to bundle output-dir folders into the Store package (MSIX/Appx).
# Paths use $<TARGET_FILE_DIR:...> so they match Release/Debug output next to MinecraftLCE.exe.
# CopyRedist drops iggy_w64.dll + mss64.dll next to the exe — they are NOT under the folders
# below, so they must be listed explicitly or the packaged app on Xbox has no Miles/Iggy DLLs
# (LoadPackagedLibrary fails with 126 even though the file exists in the build tree).
set_property(TARGET MinecraftLCE PROPERTY VS_DEPLOYMENT_CONTENT
  "$<TARGET_FILE_DIR:MinecraftLCE>/music"
  "$<TARGET_FILE_DIR:MinecraftLCE>/Common"
  "$<TARGET_FILE_DIR:MinecraftLCE>/Windows64Media"
  "$<TARGET_FILE_DIR:MinecraftLCE>/Windows64"
  "$<TARGET_FILE_DIR:MinecraftLCE>/iggy_w64.dll"
  "$<TARGET_FILE_DIR:MinecraftLCE>/mss64.dll"
)
