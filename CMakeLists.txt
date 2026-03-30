cmake_minimum_required(VERSION 3.24)

project(MinecraftConsoles LANGUAGES C CXX)

if(WIN32)
  enable_language(RC ASM_MASM)
endif()

if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Use a 64-bit generator/toolchain (x64).")
endif()

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  function(configure_msvc_target target)
    target_compile_options(${target} PRIVATE
      $<$<AND:$<NOT:$<CONFIG:Release>>,$<COMPILE_LANGUAGE:C,CXX>>:/W3>
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W0>
      $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
      $<$<COMPILE_LANGUAGE:C,CXX>:/FS>
      $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GL /O2 /Oi /GT /GF>
    )
  endfunction()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WorldSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClientSources.cmake")

list(TRANSFORM MINECRAFT_WORLD_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/")
list(TRANSFORM MINECRAFT_CLIENT_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/")

if(WIN32)
  list(APPEND MINECRAFT_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/MinecraftWindows.rc"
  )
endif()

# ── Platform-specific source files ──────────────────────────────────────
if(UNIX AND NOT APPLE)
  # Remove Windows-specific sources from the shared list
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "Windows64/")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "Xbox/")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "iob_shim\\.asm$")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "compat_shims\\.cpp$")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "crt_compat\\.cpp$")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "PS3/")
  # Exclude Iggy-dependent UI code until stubs are complete
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "Common/UI/")
  list(FILTER MINECRAFT_CLIENT_SOURCES EXCLUDE REGEX "Common/Leaderboards/")

  # Add Linux-specific sources
  list(APPEND MINECRAFT_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/Linux64_App.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/Linux64_Minecraft.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/Linux64_UIController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/KeyboardMouseInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/Network/PosixNetLayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/4JLibs/4J_Stubs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/4JLibs/gl_api.cpp"
    # SentientManager stubs already in Extrax64Stubs.cpp
    # DynamicConfigurations stubs already in Extrax64Stubs.cpp
    # SocialManager stubs already in Extrax64Stubs.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64/Linux64_UIStubs.cpp"
  )
endif()

# ── MinecraftWorld (static library) ─────────────────────────────────────
add_library(MinecraftWorld STATIC ${MINECRAFT_WORLD_SOURCES})
target_include_directories(MinecraftWorld PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
)

if(UNIX AND NOT APPLE)
  target_include_directories(MinecraftWorld PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64"
  )
  target_compile_definitions(MinecraftWorld PRIVATE
    _LINUX64
    _LARGE_WORLDS
    _DEBUG_MENUS_ENABLED
    _CRT_NON_CONFORMING_SWPRINTFS
    _CRT_SECURE_NO_WARNINGS
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    $<$<CONFIG:Debug>:_DEBUG>
  )
  set_target_properties(MinecraftWorld PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  target_compile_options(MinecraftWorld PRIVATE
    -Wno-narrowing
    -Wno-write-strings
    -Wno-deprecated
    -Wno-multichar
    -Wno-sign-compare
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-reorder
    -Wno-parentheses
    -Wno-switch
    -fpermissive
    -Wno-invalid-offsetof
    -fstack-protector-strong
  )
else()
  target_compile_definitions(MinecraftWorld PRIVATE
    $<$<CONFIG:Debug>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_DEBUG;_LIB;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
    $<$<NOT:$<CONFIG:Debug>>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_LIB;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
  )
endif()

if(MSVC)
  configure_msvc_target(MinecraftWorld)
endif()

# ── MinecraftClient (executable) ────────────────────────────────────────
if(WIN32)
  add_executable(MinecraftClient WIN32 ${MINECRAFT_CLIENT_SOURCES})
else()
  add_executable(MinecraftClient ${MINECRAFT_CLIENT_SOURCES})
endif()

target_include_directories(MinecraftClient PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
)

if(UNIX AND NOT APPLE)
  target_include_directories(MinecraftClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Linux64"
  )
  target_compile_definitions(MinecraftClient PRIVATE
    _LINUX64
    _LARGE_WORLDS
    _DEBUG_MENUS_ENABLED
    _CRT_NON_CONFORMING_SWPRINTFS
    _CRT_SECURE_NO_WARNINGS
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    $<$<CONFIG:Debug>:_DEBUG>
  )
  set_target_properties(MinecraftClient PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  target_compile_options(MinecraftClient PRIVATE
    -Wno-narrowing
    -Wno-write-strings
    -Wno-deprecated
    -Wno-multichar
    -Wno-sign-compare
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-reorder
    -Wno-parentheses
    -Wno-switch
    -fpermissive
    -Wno-invalid-offsetof
    -fstack-protector-strong
  )
  find_package(Threads REQUIRED)
  find_package(PNG REQUIRED)
  find_package(OpenGL REQUIRED)
  target_link_libraries(MinecraftClient PRIVATE
    MinecraftWorld
    Threads::Threads
    ${CMAKE_DL_LIBS}
    PNG::PNG
    OpenGL::GL
    glfw
  )
else()
  target_include_directories(MinecraftClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
  )
  target_compile_definitions(MinecraftClient PRIVATE
    $<$<CONFIG:Debug>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_DEBUG;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
    $<$<NOT:$<CONFIG:Debug>>:_LARGE_WORLDS;_DEBUG_MENUS_ENABLED;_CRT_NON_CONFORMING_SWPRINTFS;_CRT_SECURE_NO_WARNINGS;_WINDOWS64>
  )
  if(MSVC)
    configure_msvc_target(MinecraftClient)
    target_link_options(MinecraftClient PRIVATE
      $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO>
    )
  endif()
  target_link_libraries(MinecraftClient PRIVATE
    MinecraftWorld
    d3d11
    XInput9_1_0
    wsock32
    legacy_stdio_definitions
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggy_w64.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggyperfmon_w64.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggyexpruntime_w64.lib"
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
endif()

set_target_properties(MinecraftClient PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MinecraftClient>"
)

# ── Asset copy ──────────────────────────────────────────────────────────
if(CMAKE_HOST_WIN32)
  message(STATUS "Starting redist copy...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
            "${CMAKE_CURRENT_BINARY_DIR}"
            /S /MT /R:0 /W:0 /NP
  )
  message(STATUS "Starting asset copy...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
            "${CMAKE_CURRENT_BINARY_DIR}"
            /S /MT /R:0 /W:0 /NP
            /XF "*.cpp" "*.c" "*.h" "*.hpp" "*.asm"
                "*.xml" "*.lang" "*.vcxproj" "*.vcxproj.*" "*.sln"
                "*.docx" "*.xls"
                "*.bat" "*.cmd" "*.ps1" "*.py"
                "*Test*"
            /XD "Durango*" "Orbis*" "PS*" "Xbox"
  )
  message(STATUS "Patching Windows64Media...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/DurangoMedia"
            "${CMAKE_CURRENT_BINARY_DIR}/Windows64Media"
            /S /MT /R:0 /W:0 /NP
            /XF "*.h" "*.xml" "*.lang" "*.bat"
  )
elseif(CMAKE_HOST_UNIX)
  message(STATUS "Starting asset copy...")
  execute_process(
    COMMAND rsync -av
            "--exclude=*.cpp" "--exclude=*.c" "--exclude=*.h" "--exclude=*.hpp" "--exclude=*.asm"
            "--exclude=*.xml" "--exclude=*.lang" "--exclude=*.vcxproj" "--exclude=*.vcxproj.*" "--exclude=*.sln"
            "--exclude=*.docx" "--exclude=*.xls"
            "--exclude=*.bat" "--exclude=*.cmd" "--exclude=*.ps1" "--exclude=*.py"
            "--exclude=*Test*"
            "--exclude=Durango*" "--exclude=Orbis*" "--exclude=PS*" "--exclude=Xbox"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/" "${CMAKE_CURRENT_BINARY_DIR}/"
  )
  message(STATUS "Patching Windows64Media...")
  execute_process(
    COMMAND rsync -av
            "--exclude=*.h" "--exclude=*.xml" "--exclude=*.lang" "--exclude=*.bat"
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/DurangoMedia/" "${CMAKE_CURRENT_BINARY_DIR}/Windows64Media/"
  )
endif()

# Copy server.txt (lives at repo root, not inside Minecraft.Client/)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server.txt")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/server.txt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

if(WIN32)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MinecraftClient)
endif()
