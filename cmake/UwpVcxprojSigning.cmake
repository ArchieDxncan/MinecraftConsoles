# Patches the generated MinecraftLCE.vcxproj after CMake writes it.
# CMake's VS generator adds a second <PropertyGroup> that overrides PackageCertificate*
# with MinecraftLCE.dir\Windows_TemporaryKey.pfx + a fixed thumbprint, which breaks
# signing when you use UWP/archiedxncan.pfx (MSB error: thumbprint mismatch).

function(uwp_patch_vcxproj_cert_block _vcx_path _pfx_path _thumbprint)
  if(NOT EXISTS "${_vcx_path}")
    return()
  endif()
  file(READ "${_vcx_path}" _txt)
  file(TO_CMAKE_PATH "${_pfx_path}" _pfx_norm)
  string(REPLACE "\\" "/" _pfx_slashes "${_pfx_norm}")

  string(REGEX REPLACE
    "<PackageCertificateKeyFile>[^<]*Windows_TemporaryKey\\.pfx</PackageCertificateKeyFile>\r?\n[ \t]*<PackageCertificateThumbprint>[^<]*</PackageCertificateThumbprint>"
    "<PackageCertificateKeyFile>${_pfx_slashes}</PackageCertificateKeyFile>\n    <PackageCertificateThumbprint>${_thumbprint}</PackageCertificateThumbprint>"
    _txt "${_txt}")

  string(REGEX REPLACE "\r?\n[ \t]*<None Include=\"[^\"]*Windows_TemporaryKey\\.pfx\" \\/>" "" _txt "${_txt}")

  file(WRITE "${_vcx_path}" "${_txt}")
endfunction()

function(uwp_patch_minecraft_lce_vcxproj_signing _binary_dir _pfx_path _thumbprint)
  set(_vcx "${_binary_dir}/MinecraftLCE.vcxproj")
  if(NOT EXISTS "${_vcx}")
    message(WARNING "UWP: ${_vcx} not found; signing patch skipped.")
    return()
  endif()
  uwp_patch_vcxproj_cert_block("${_vcx}" "${_pfx_path}" "${_thumbprint}")
  message(STATUS "UWP: Patched MinecraftLCE.vcxproj (removed CMake temp-store cert overlay).")
endfunction()
