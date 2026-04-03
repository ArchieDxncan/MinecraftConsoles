include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/Common.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/Durango.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/ORBIS.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/PS3.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/PSVita.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/Windows.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sources/Xbox360.cmake")

include("${CMAKE_SOURCE_DIR}/cmake/CommonSources.cmake")

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")

# Same layout as GDK: .../PlayFab.Party.Cpp/{Include,Lib/x64,Redist/x64}.
# GitHub-hosted runners do not install the Microsoft GDK, so auto-detect fails unless you set this
# (cache or env) to a copy of the extension tree (e.g. extracted from a private artifact or self-hosted GDK).
set(MINECRAFT_PLAYFAB_PARTY_CPP_ROOT "" CACHE PATH
  "Path to PlayFab.Party.Cpp root (overrides GDK search; needed for CI without GDK)")

set(_PARTY_EXT_BASE "")
if(MINECRAFT_PLAYFAB_PARTY_CPP_ROOT AND EXISTS "${MINECRAFT_PLAYFAB_PARTY_CPP_ROOT}")
  set(_PARTY_EXT_BASE "${MINECRAFT_PLAYFAB_PARTY_CPP_ROOT}")
elseif(DEFINED ENV{MINECRAFT_PLAYFAB_PARTY_CPP_ROOT} AND NOT "$ENV{MINECRAFT_PLAYFAB_PARTY_CPP_ROOT}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{MINECRAFT_PLAYFAB_PARTY_CPP_ROOT}" _PARTY_EXT_BASE)
endif()

# Repo-bundled SDK (same tree as GDK's PlayFab.Party.Cpp). Lowest priority before GDK: cache/env wins.
set(_PARTY_BUNDLE_ROOT "${CMAKE_SOURCE_DIR}/Minecraft.Client/Windows64/ThirdParty/PlayFabParty")
if(_PARTY_EXT_BASE STREQUAL "" AND EXISTS "${_PARTY_BUNDLE_ROOT}/Include/Party.h")
  set(_PARTY_EXT_BASE "${_PARTY_BUNDLE_ROOT}")
endif()

if(_PARTY_EXT_BASE STREQUAL "" AND DEFINED ENV{GameDKLatest} AND NOT "$ENV{GameDKLatest}" STREQUAL "")
  set(_PARTY_GDK_BASE "$ENV{GameDKLatest}")
  if(EXISTS "${_PARTY_GDK_BASE}/GRDK/ExtensionLibraries/PlayFab.Party.Cpp")
    set(_PARTY_EXT_BASE "${_PARTY_GDK_BASE}/GRDK/ExtensionLibraries/PlayFab.Party.Cpp")
  endif()
endif()

if(_PARTY_EXT_BASE STREQUAL "" AND EXISTS "C:/Program Files (x86)/Microsoft GDK")
  file(GLOB _PARTY_GDK_DIRS LIST_DIRECTORIES true "C:/Program Files (x86)/Microsoft GDK/[0-9]*")
  list(SORT _PARTY_GDK_DIRS COMPARE NATURAL ORDER DESCENDING)
  foreach(_gdk_dir IN LISTS _PARTY_GDK_DIRS)
    if(EXISTS "${_gdk_dir}/GRDK/ExtensionLibraries/PlayFab.Party.Cpp")
      set(_PARTY_EXT_BASE "${_gdk_dir}/GRDK/ExtensionLibraries/PlayFab.Party.Cpp")
      break()
    endif()
  endforeach()
endif()

set(_PARTY_INCLUDE_DIR "${_PARTY_EXT_BASE}/Include")
set(_PARTY_LIB "${_PARTY_EXT_BASE}/Lib/x64/Party.lib")
set(_PARTY_DLL "${_PARTY_EXT_BASE}/Redist/x64/Party.dll")
set(_PARTY_AVAILABLE FALSE)
if(EXISTS "${_PARTY_INCLUDE_DIR}/Party.h" AND EXISTS "${_PARTY_LIB}" AND EXISTS "${_PARTY_DLL}")
  set(_PARTY_AVAILABLE TRUE)
endif()
if(PLATFORM_NAME STREQUAL "Windows64")
  if(_PARTY_AVAILABLE)
    message(STATUS "PlayFab Party SDK: yes (${_PARTY_EXT_BASE})")
  else()
    message(STATUS "PlayFab Party SDK: not found — Party.lib/Party.dll copy disabled. Populate Windows64/ThirdParty/PlayFabParty, install GDK, or set MINECRAFT_PLAYFAB_PARTY_CPP_ROOT.")
  endif()
endif()

# Combine all source files into a single variable for the target
# We cant use CMAKE_CONFIGURE_PRESET here as VS doesn't set it, so just rely on the folder
set(MINECRAFT_CLIENT_SOURCES
  ${MINECRAFT_CLIENT_COMMON}
  $<$<STREQUAL:${PLATFORM_NAME},Durango>:${MINECRAFT_CLIENT_DURANGO}>
  $<$<STREQUAL:${PLATFORM_NAME},Orbis>:${MINECRAFT_CLIENT_ORBIS}>
  $<$<STREQUAL:${PLATFORM_NAME},PS3>:${MINECRAFT_CLIENT_PS3}>
  $<$<STREQUAL:${PLATFORM_NAME},PSVita>:${MINECRAFT_CLIENT_PSVITA}>
  $<$<STREQUAL:${PLATFORM_NAME},Windows64>:${MINECRAFT_CLIENT_WINDOWS}>
  $<$<STREQUAL:${PLATFORM_NAME},Xbox>:${MINECRAFT_CLIENT_XBOX360}>
  ${SOURCES_COMMON}
)

add_executable(Minecraft.Client ${MINECRAFT_CLIENT_SOURCES} )

# Only define executable on windows
if(PLATFORM_NAME STREQUAL "Windows64")
  set_target_properties(Minecraft.Client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(Minecraft.Client PRIVATE
  "${CMAKE_BINARY_DIR}/generated/" # This is for the generated BuildVer.h
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/Iggy/include"
  "${CMAKE_SOURCE_DIR}/include/"
  $<$<STREQUAL:${PLATFORM_NAME},Windows64>:${CMAKE_SOURCE_DIR}/Minecraft.Server/vendor>
  $<$<BOOL:${_PARTY_AVAILABLE}>:${_PARTY_INCLUDE_DIR}>
)
target_compile_definitions(Minecraft.Client PRIVATE
  ${MINECRAFT_SHARED_DEFINES}
  MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE=$<IF:$<BOOL:${_PARTY_AVAILABLE}>,1,0>
)
target_precompile_headers(Minecraft.Client PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:stdafx.h>")
set_source_files_properties(compat_shims.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON) # This redefines internal MSVC CRT symbols which will cause an issue with PCH

configure_compiler_target(Minecraft.Client)

set_target_properties(Minecraft.Client PROPERTIES
  OUTPUT_NAME "Minecraft.Client"
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Minecraft.Client>"
)

target_link_libraries(Minecraft.Client PRIVATE
  Minecraft.World
  d3d11
  d3dcompiler
  XInput9_1_0
  wsock32
  legacy_stdio_definitions
  $<$<STREQUAL:${PLATFORM_NAME},Windows64>:winhttp>
  $<$<CONFIG:Debug>: # Debug 4J libraries
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Input_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Storage_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Render_PC_d.lib"
  >
  $<$<NOT:$<CONFIG:Debug>>: # Release 4J libraries
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Input.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Storage.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/4JLibs/libs/4J_Render_PC.lib"
  >
  $<$<BOOL:${_PARTY_AVAILABLE}>:${_PARTY_LIB}>
)

# Iggy libs
foreach(lib IN LISTS IGGY_LIBS)
  target_link_libraries(Minecraft.Client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}/Iggy/lib/${lib}")
endforeach()

if(_PARTY_AVAILABLE)
  add_custom_command(TARGET Minecraft.Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_PARTY_DLL}" "$<TARGET_FILE_DIR:Minecraft.Client>/Party.dll"
    COMMENT "Copying PlayFab Party runtime (Party.dll)"
    VERBATIM
  )
endif()

# ---
# Asset / redist copy
# ---
include("${CMAKE_SOURCE_DIR}/cmake/CopyAssets.cmake")
set(ASSET_FOLDER_PAIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/music"                  "music"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/Media"           "Common/Media"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/res"             "Common/res"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/Trial"           "Common/Trial"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/Tutorial"        "Common/Tutorial"
  "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_NAME}Media"  "${PLATFORM_NAME}Media"
)
setup_asset_folder_copy(Minecraft.Client "${ASSET_FOLDER_PAIRS}")

# Copy redist files
add_copyredist_target(Minecraft.Client)

# Make sure GameHDD exists on Windows
if(PLATFORM_NAME STREQUAL "Windows64")
  add_gamehdd_target(Minecraft.Client)
endif()
