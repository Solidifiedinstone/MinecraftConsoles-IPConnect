# Minecraft Console Edition - Linux Port

A port of Minecraft Console Edition to Linux x86-64, starting from Xbox 360 console source.

## Status

**Fully runnable on Linux** — binary initializes, loads all textures, streams audio, and runs game logic without crashing.

**Rendering**: Not yet implemented (stubs only). Game runs headless with audio.

**Latest fixes** (2026-03-28):
- Fixed 8 sequential crashes preventing launch
- Implemented PNG texture loading with libpng
- Implemented profile data storage for multiplayer
- Binary now runs stably for 60+ seconds with 16 healthy threads

## Build

### Requirements
- Linux x86-64 (Ubuntu 24.04+)
- CMake 3.24+
- GCC 13+
- libpng-dev
- pthread (standard)

### Compile
```bash
cd MinecraftConsoles-IPConnect
cmake .
make -j$(nproc) MinecraftClient
```

Binary: `./MinecraftClient`

## Run

```bash
./MinecraftClient
```

The game will:
1. Load and parse texture PNGs
2. Initialize audio (miniaudio)
3. Start background game logic threads
4. Load recipes, items, blocks
5. Begin menu music streaming
6. Wait indefinitely (no window/input)

Monitor with:
```bash
./MinecraftClient 2>&1 | tee game.log
```

To kill: `pkill -9 MinecraftClient`

## Architecture

- **Game Core** (`Minecraft.World/`): Level, blocks, entities, physics — platform-agnostic
- **Client** (`Minecraft.Client/`): Rendering, UI, I/O — heavily stubbed for Linux
- **4J Libraries** (`Minecraft.Client/Linux64/4JLibs/`): Platform abstraction (4J_Render, 4J_Input, 4J_Storage, 4J_Profile)

### Linux-Specific Code

| File | Purpose |
|------|---------|
| `Minecraft.Client/Linux64/Linux64_Minecraft.cpp` | Main entry point, profile init |
| `Minecraft.Client/Linux64/4J_Stubs.cpp` | Render, input, storage, profile stubs + libpng PNG loader |
| `Minecraft.Client/Linux64/LinuxTypes.h` | Windows/Xbox type compatibility (HRESULT, DWORD, etc.) |
| `Minecraft.Client/Linux64/Linux64_UIController.cpp` | UI stubs |
| `Minecraft.Client/Linux64/KeyboardMouseInput.cpp` | Keyboard/mouse input (stubbed) |
| `Minecraft.Client/Linux64/Network/PosixNetLayer.cpp` | UDP socket networking |

## Recent Fixes

### Crash 1: va_arg(vl, wchar_t) → SIGILL
**Issue**: GCC 13 generates `ud2` (invalid instruction) for `va_arg(vl, wchar_t)` because the C++ ABI promotes `wchar_t` to `int` in varargs.

**Fix**: Cast to int then wchar_t: `(wchar_t)va_arg(vl, int)`

**Files**: `Minecraft.World/Recipes.cpp` (3 instances)

### Crash 2: Null Level in Chunk Rebuild
**Issue**: Render update thread fires before Level is assigned to permaChunk, causing SIGSEGV at `level->dimension->id`.

**Fix**: Add null checks in `getGlobalIndexForChunk()` and `Chunk::rebuild()`

**Files**: `Minecraft.Client/LevelRenderer.cpp`, `Minecraft.Client/Chunk.cpp`

### Crash 3: Null levelRenderer in GameRenderer
**Issue**: Update thread accesses `minecraft->levelRenderer` before it's initialized.

**Fix**: Guard calls with null check

**Files**: `Minecraft.Client/GameRenderer.cpp`

### Crash 4: Texture Mip Calculation Bug
**Issue**: `int hh = height >> height;` shifts by height itself (not level), producing zero-size mips.

**Fix**: Correct to `int hh = height >> level;`

**Files**: `Minecraft.Client/Texture.cpp:137`

### Crash 5: PNG Not Decoded
**Issue**: `RenderManager.LoadTextureData()` stub returned S_OK with ImageInfo.Width/Height = 0, causing downstream ArrayWithLength assertion.

**Fix**: Implement real PNG decoding using libpng

**Files**: `Minecraft.Client/Linux64/4JLibs/4J_Stubs.cpp`

### Crash 6: Profile Data Null
**Issue**: `ProfileManager.GetGameDefinedProfileData()` returned nullptr, SIGSEGV in `InitGameSettings()`.

**Fix**: Allocate static 4-player profile buffer, implement Initialise() and GetGameDefinedProfileData()

**Files**: `Minecraft.Client/Linux64/4JLibs/4J_Stubs.cpp`

### Crash 7: Missing libpng
**Issue**: Linker couldn't find libpng symbols.

**Fix**: Add `find_package(PNG)` and `PNG::PNG` to CMakeLists.txt

**Files**: `CMakeLists.txt`

## What Works
✅ Archive loading
✅ Texture PNG decoding (all mip levels)
✅ Audio initialization and streaming
✅ Recipe system
✅ Block/item definitions
✅ Network (LAN listening on UDP 25566)
✅ Game logic threads (16 threads healthy)
✅ Profile/player data

## What's Missing
❌ Window rendering (RenderManager is all stubs)
❌ OpenGL/D3D graphics pipeline
❌ Iggy UI rendering
❌ Input handling
❌ Screen output

## Build Flags

Linux build uses:
```cmake
-Wno-narrowing -Wno-write-strings -Wno-deprecated -fpermissive
-fcf-protection=none  # Disable Intel CET for older GCC compat
```

Compile defines:
```cmake
_LINUX64 _LARGE_WORLDS _DEBUG_MENUS_ENABLED _CRT_NON_CONFORMING_SWPRINTFS
```

## Thread Model

Game spawns ~16 threads:
- Main: Input/menu loop (sleeping until window events)
- Render: Chunk update thread (10 updates/frame max)
- Audio: Music/SFX streaming
- Network: LAN game discovery
- Logic: Entity updates, physics (background)

All threads stable and healthy when running.

## Next Steps

To add rendering:
1. Implement `RenderManager.Initialise()` to create X11/Wayland window
2. Wire OpenGL or Vulkan into `RenderManager.TextureData()`, `DrawPrimitive()`, state functions
3. Hook keyboard/mouse from Linux input system
4. Implement UI rendering (Iggy library stubs or replace with custom)

## References

- **Console Edition**: Minecraft for Xbox 360 / PlayStation 3 era (2011-2019)
- **Networking**: Minecraft LAN discovery on UDP:25566
- **Audio**: miniaudio + WAV streaming
- **Graphics**: Stub render API compatible with Iggy (4J's internal UI lib)

## License

Minecraft source code, assets, and IP owned by Microsoft. This is an educational reverse-engineering port.
