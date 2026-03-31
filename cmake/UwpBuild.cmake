set(PLATFORM_NAME "Windows64")

# UWP/C++CX requires dynamic CRT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/cmake/sources/Common.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/cmake/sources/Durango.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/Common.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/Durango.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/ORBIS.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/PS3.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/PSVita.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/Windows.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/cmake/sources/Xbox360.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonSources.cmake")

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
  _CONTENT_PACKAGE
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
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/MinecraftWindows.rc"
)
list(APPEND MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/UWP_App.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/crt_compat.cpp"
)

add_executable(MinecraftLCE ${MINECRAFT_CLIENT_SOURCES})
target_include_directories(MinecraftLCE PRIVATE
  "${CMAKE_BINARY_DIR}/generated/"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
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
  _CONTENT_PACKAGE
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
    /DELAYLOAD:USER32.dll
    /DELAYLOAD:XINPUT1_4.dll
  )
endif()

target_link_libraries(MinecraftLCE PRIVATE
  Minecraft.World
  d3d11
  dxgi
  ws2_32
  xinput
  legacy_stdio_definitions
  WindowsApp.lib
  user32.lib
  delayimp.lib
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

set_property(TARGET MinecraftLCE PROPERTY VS_APPX_MANIFEST
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/Package.appxmanifest"
)
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
