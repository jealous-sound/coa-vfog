# CoAVolFog

Volumetric fog and light shafts for the Ascension/CoA 3.3.5a (build 12340) Direct3D 9 client.

It implements the `coa-vfog-kit` plan's loader, D3D9 device wrapper with a readable depth buffer,
world-render hooks and per-pixel ray-march tier. Fog layers come from the Classic client's own
`LightDataGlobalVolumeFog` data wherever its lights cover the map, and are derived from the 3.3.5
day/night lighting elsewhere (Outland, Northrend, custom maps).

## Classic fog data

`tools/convert_classic_fog.py` converts the kit's Classic `Light`, `LightData`, `LightDataGlobalVolumeFog`,
`ZoneLight` and `ZoneLightPoint` exports into `data/fogdata.bin` (625 lights with all eight condition slots,
2,079 time keys, 6,234 layer slots, 18 zone-light outlines):

```powershell
python tools/convert_classic_fog.py <path>\coa-vfog-kit.zip data/fogdata.bin
```

At run time the DLL blends the Classic lights around the camera (spheres: full weight inside the
falloff start, linear to the falloff end; the rest goes to the zone light whose outline holds the camera,
fading in over 100 yd inside the outline with the innermost outline on top, and otherwise to the map's
global light). Each light uses the condition slot the client itself uses for its stock lighting: the
slot a screen effect forces (the ghost effect forces slot 4, death), otherwise clear weather (slot 0)
blended toward storm (slot 2) by the client's storm weight. The DLL interpolates the two time keys around
the current time and pairs layers by their Classic layer index; a layer only one side has keeps its
colours and shape and has its density scaled by that side's weight. Each time key also keeps Classic's direct
light colour, blended the same way. In a storm the fog's sun scattering is scaled toward the client's direct
light luminance over Classic's (at most 1, weighted by the storm weight): 3.3.5 storms are lit far darker than
Classic's (0.41 at 20:00 in Goldshire), so the storm fog no longer glows like a Classic storm in front of a grey
scene. Clear weather keeps the authored scattering, which zones like Duskwood rely on although their client
light is darker than Classic's (0.34 at Darkshire at 18:00). Then it applies the Classic transforms:
density ×0.01, heights relative to the player when flag bit 1 is set, sun shadowing for flag bit 0
(a light below the horizon counts as shadow), `1 + strength·((d − start)/range)^exponent` over a 5,000-yd
fog range, and scatter intensities up to 10 in linear light. The fog is blended over the scene in linear
light, as the modern client adds its volume to a linear frame, with a per-channel highlight roll-off. The
data is Classic-derived; keep it in private repositories.

## What it draws

- **Distance haze** that thickens toward the horizon and fades with altitude, tinted by the zone's
  stock fog colour.
- **Ground mist** that hugs the terrain around the player; zones with short stock fog get more.
- **Distance fog** that replaces the stock linear fog on maps without Classic data: it turns opaque where
  the stock fog did, is lit by the direct light (warm at sunset), and fades into a horizon band on the sky.
  With Classic layers the geometry near the far clip fades into the sky column instead; the distance fog
  returns only across the edge of Classic coverage and where the Classic layers are too thin to hide the
  far clip.
- **Forward scattering** around the sun or moon (Henyey–Greenstein phase).
- **Light shafts**: in-scattering is shadowed by a screen-space march toward the light, so trees,
  buildings and terrain cast shafts into the fog.
- **God rays** (optional): a radial blur of the bright sky around the sun.

The effect is composited over the world before glow and the UI. The client's glow (`screen + g·blur²`, with
`g` from the day/night light) runs afterwards and would bleach bright fog to white, so fogged pixels are
pre-compensated with the live glow amount. Its other term, a blend toward the blur while drunk or under
water, is left as is. While the effect draws, the stock fog is pushed out of range for
the world render and restored afterwards; a frame the effect skips keeps the stock fog.

**View distance.** Ascension's Extensions.dll detours the far-clip clamp (`0x780770`) and caps maps 0, 1, 530
and 571 at 791.66 yd; the engine allows 1583.33 and instances use it. With `FarClipMax` set, the DLL's calls
to the clamp (`0x780810` when the `farclip` CVar is set, `0x781444` on map load) lift that cap. Terrain
loading, the chunk pool, the WDL horizon and the fog follow the far clip; placed objects keep their own
size-class culling (`environmentDetail`), and creatures the server's visibility distance.

## How it attaches

| Piece | Mechanism |
|---|---|
| Loader | `version.dll` proxy (all 17 exports forward lazily to the system copy). Its static import loads `CoAVolFog.dll` before the client starts. |
| D3D9 | The client resolves `Direct3DCreate9` through the delay-loaded `GetProcAddress` slot `[0xB2ED98]`. The DLL points that slot at a filter that returns a wrapped `IDirect3D9`. No d3d9 code is patched, so DXVK or other `d3d9.dll` builds keep working underneath. |
| Depth | The wrapper creates the device without auto depth and binds an `INTZ` texture as the depth-stencil, which the client caches as its world depth. MSAA is reported unavailable and forced off; `D3DCREATE_PUREDEVICE` is removed. The client draws the world with viewport depth `[0, 0.94]` (`[0xADEEE4]`, set at `0x4F9019`), the distant WDL terrain into `[0.998, 0.999]` with its own projection, and leaves the sky at the clear depth 1; the shaders read depth through the captured world viewport's range and treat anything deeper as beyond the far clip. |
| Hooks | Four 5-byte call displacements: the world render call (`0x4FB03D`, stock-fog override and restore), after the opaque M2 pass (`0x4F911D`, records the world viewport and matrices), the liquid surface pass (`0x4F9170`, depth writes forced on so water is fogged by its own distance) and before the frame effects (`0x4F9281`, renders the fog). The original bytes are checked first; on any mismatch nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when `FarClipMax` is set at start-up, independently of the fog hooks. |
| State | Every state the passes touch is captured with a recorded state block and restored, plus render targets, depth and stream 0 (whose offset state blocks drop). The client's shader-constant cache stays valid. |
| Overlay | When a fog device is created, the device window's procedure is chained so the settings window sees input first, and the wrapper's `Present` draws the window over the finished frame (see In-game settings). |

Engine inputs (all static addresses in the 12340 image):

| Input | Address |
|---|---|
| World view / projection (camera-relative view; OpenGL depth range, converted by the D3D backend) | device `+0x1B00` stack, `+0xF88`; copies at `0xADF5E8`, `0xADF628` |
| Camera position / look-at target | `0xCD8F5C`, `0xCD8F68` |
| Day fraction | `0xD38B04` |
| Stock fog groups: colour, start, end (read by the render callbacks) | `0xD38B8C`–`0xD38B94`, `0xD38BA0`–`0xD38BA8` |
| Zone fog distance (light float band 0) | `0xD38C1C` |
| Light colours: ambient, direct, sun | `0xD38BD4`, `0xD38BD8`, `0xD38BF8` |
| Visible sun / moon sprite positions, sky centre | `0xD38E28`, `0xD38E48`, `0xD38B18` (day window `[0xA41CA4, 0xA41CA0]`) |
| Camera in liquid | `0xCD8794` |
| Storm weight (0..1) used to blend the clear and storm light params | `0xD38B88` |
| Light params slot forced by the current screen effect (−1 = none) | `0xD38B58` |
| Far clip | From the projection; `[[0xB7436C] + 0xB14]` as a fallback |

Engine notes behind the code:

- Call sites. `0x4FB03D` calls the world render `0x4F8EA0`, a thiscall on the world frame with no stack arguments;
  the thunk keeps ECX across the frame-begin hook. `0x4F911D` calls the opaque M2 pass `0x823CB0`, a thiscall with one
  stack argument (`ret 4`). `0x4F9170` calls the liquid surface pass `0x77F020` (no arguments, outside liquid only).
  `0x4F9281` calls FFX end `0x8C1010` (no arguments); the thunk renders the fog, then tail-jumps to it.
- Depth. The world viewport's MaxZ is `[0xADEEE4]` = 0.94, passed to GxXformSetViewport at `0x4F905A` and uploaded as
  `D3DVIEWPORT9::MaxZ` by the D3D9 backend (`0x6A9ACC`). The Gx viewport (`[[0xC5DF88] + 0xF80]`, MinZ/MaxZ) is stored
  by `0x681890` and uploaded lazily on the next draw or clear (`0x6A99E0`), so after the opaque pass the device can
  still hold the sky's `[0.999, 1]`; the capture reads MinZ/MaxZ from the Gx viewport, which the sky and WDL passes
  restore (`0x7F0CB3`, `0x796466`). The WDL uses its own projection (`0x7960EB`); the sky viewport is set at
  `0x7F0A79` and the sky writes no depth, so depth at or above max(deepest world depth, 0.99903) is sky. The engine
  builds its projection with `0x6BF370` (OpenGL depth range, w = view z).
- Liquid depth. While writes are forced on, the wrapper records the client's own `D3DRS_ZWRITEENABLE` requests and
  re-applies the last one afterwards, so the client's render-state cache stays accurate.
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`) and
  the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`); `0x4F8770`
  feeds it the DayNight glow (`0xD38C2C`) as the additive weight of `lerp(screen, blur, other) + g·blur²`, where
  `other` is the drunk or underwater amount. Under liquid the wave-glow pass list is used and nothing is compensated.
- Far clip. The clamp `0x780770` is cdecl `float(float farclip, int mapId)`, result in ST0, caller pops; it bounds
  the value to `[0xA3E708]` (183.33) .. `[0xA3E710]` (1583.33). Its calls at `0x780810` (in the `farclip` CVar setter
  `0x780800`, also reached from Extensions.dll on zone changes) and `0x781444` (map load `0x781430`) are rare, so the
  hook re-reads the INI on every call.
- Light params slots. `0x7EB180` returns a light's `LightParams` for a slot (`Light` record `+0x1C + 4·slot`). For
  each light, `0x7EE510` takes slot 0 (1 under water) and, while the storm weight `[0xD38B88]` is above zero, blends
  in slot 2 (3 under water) by it (`0x7EC220`). The DayNight update sets that weight to `min(1, 4·[0xD38B4C])` just
  before the light blend (`0x7F3995`); the weather update writes `[0xD38B4C]` (`0x784A01`). The under-water flag is
  the camera's liquid type (`LiquidType` `+0x28` light). When `[0xD38B58]` holds a slot, the light blend `0x7F3230`
  uses that slot instead (`0x7F346F`). `0x7ECEC0` stores it from the current `ScreenEffect` row (`+0x1C`, called at
  `0x4F712D`; values above 7 become −1) and `0x7ECEE0` clears it when the effect ends. In CoA's `ScreenEffect.dbc`
  the Ghost effect (ID 1) and the other death-style effects use slot 4; a few event effects force slots 0, 1, 2, 3
  or 5, which the DLL follows the same way.
- Classic data. The converter keeps the `LightDataGlobalVolumeFog` rows the Classic client selects (flag bit 3) and
  stores them at their layer index (0–2), leaving an empty slot where a key has no layer at that index. On maps where
  any Classic light has fog (the old continents), Classic data applies wherever Classic lights hold at least half of
  the blend weight; a light without fog in the active slot counts with zero density, so the fog thins smoothly
  into it and the distance fog hides the far clip there. Maps without Classic fog use the derived layers.

- Present. The D3D9 backend presents with `IDirect3DDevice9::Present(NULL, NULL, NULL, NULL)` (vtable `+0x44`) on the
  device it keeps at Gx device `+0x397C`: `0x6A3584` in `0x6A3450` and `0x6A7724` in `0x6A7610`. That device is the
  wrapper, so every frame passes through its `Present`.
- Input. `GxWindowClassD3d` and `GxWindowClassD3d9Ex` (registered at `0x68EB9C` and `0x6A08BC`) share the window
  procedure `0x6A0360`, which reads the Gx device from `GWL_USERDATA` and hands keyboard and mouse messages to its
  input callback `[gx + 0xF54]` (`DefWindowProcA` without one). The pump runs `GetMessageA` (`0x869F72`), a pre-filter
  `0x86CB00`, then `TranslateMessage` and `DispatchMessageA` (`0x869F9E`, `0x869FA4`). The pre-filter only acts on the
  client's own dialog windows (the registry at `0xD41618`): accelerators through `TranslateAcceleratorA` while such a
  dialog is the active window, and Escape and characters for their controls. DirectInput only enumerates game
  controllers (`EnumDevices` class 4 at `0x870725`), so keyboard and mouse reach the chained window procedure.

Two readings in the kit were corrected against the disassembly: the fog end is `0xD38BA8` (the kit's
`0xD38B98` is a density-like value that Extensions.dll patches), and `0xD38C9C` is a near-constant model
lighting direction (polar angle 110–127°), not the visible sun, so shafts use the sprite positions.

## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. It edits every setting below except
`Enable`, `EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change. **Save** writes the changed
keys back to `CoAVolFog.ini` through `WritePrivateProfileString`, which keeps the comments and every other line;
**Revert** reloads the file. Editing the INI by hand still works while the window is open, and the reload replaces
unsaved changes made in the window. The window also shows whether the fog drew in the last frame, or why it did not.

- Input. The hotkey, its key-up and its characters never reach the client. While the window is open, clicks and the
  wheel go to the client unless ImGui wants the mouse (the cursor is over the window, or a drag started on it);
  key-downs and characters go to the client unless an ImGui text field is active (Ctrl+click on a slider). Mouse
  moves and key-ups always reach the client, so its cursor keeps following the pointer and no game key sticks. The
  client draws a D3D cursor (`SetCursorProperties` at `0x6A009C`, `ShowCursor` on `WM_SETCURSOR` at `0x6A058E`), so
  ImGui leaves the cursor shape alone. Coordinates are scaled from the client area to the back buffer.
- Drawing. The window is drawn in the wrapper's `Present`, over the client's UI, in its own scene. A full state
  block, the render targets and stream 0 (with its offset) are captured and restored, and the states ImGui's DX9
  backend leaves alone are set for it (colour write mask, sRGB write, clip planes, texture-coordinate index and
  transform, stage result, sampler sRGB and mip filter). ImGui's font texture and buffers live in the default pool
  and are released before every `Reset`. The window scales with the back-buffer height above 1080 lines.
- Hidden, the overlay only checks the hotkey. An exception in it turns the overlay off for the session and is
  logged; `Overlay=0` leaves the game window untouched from the next start.

## Build

Requirements: Visual Studio 2022 (C++ x86), the Windows 10/11 SDK (`fxc.exe`), CMake 3.20+. The first configure
downloads Dear ImGui v1.92.9b (FetchContent, pinned by SHA-256) into the build directory.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Outputs in `build/Release`: `version.dll`, `CoAVolFog.dll`, and `vfog_harness.exe`.

## Test

```powershell
ctest --test-dir build -C Release --output-on-failure
```

`vfog_harness` creates a real D3D9 device through the wrapper with the client's flags (`0x52`, auto
depth D24S8), renders a Z-up test scene with the client's projection convention, runs the fog passes
through the same entry the hook uses, and checks:

- device wrapping, INTZ substitution, pure-device removal and preserved engine-visible parameters;
- restoration of render, sampler, texture, shader, constant, stream, viewport, scissor and target state;
- pixels outside the world viewport (the glow sub-rectangle case) left untouched;
- linear depth against the scene geometry, also with the client's world depth range `[0, 0.94]` and a
  distant-terrain patch behind it, and sky transmittance against a CPU reference integration for
  derived and Classic layers;
- Classic light blending and time-key interpolation at a known position against hand-computed values;
- storm and screen-effect slot selection, layers paired by Classic index, zone-light outlines (interior,
  edge fade, nesting) and the fog thinning into a Classic light without fog;
- the storm fog's sun scattering following the client's darker storm light;
- temporal accumulation converging on a static camera;
- `OverlayKey` parsing, and saving from the settings window into a copy of the shipped INI (only changed lines
  rewritten, comments kept, restart-only keys untouched, values clamped like the INI, Revert);
- the overlay hotkey through the chained window procedure, clicks and keys routed to the window or the client,
  device state restored around the overlay, its pixels confined to its window, nothing drawn while hidden, and
  drawing again after Reset;
- Reset at a new size and reference counts reaching zero.

It writes `before.png`, `after.png`, `overlay.png` and the debug views to `build/harness-out`.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration.

## Install

Close the client, then copy `version.dll`, `CoAVolFog.dll`, `CoAVolFog.ini` and `fogdata.bin` next to
`Ascension.exe`.
Remove `version.dll` and `CoAVolFog.dll` to uninstall; nothing else in the client is changed. The DLL
writes `CoAVolFog.log` next to itself.

## Settings

`CoAVolFog.ini` is re-read within a second while the game runs (except `Enable` and `EngineHooks`). In the game,
`Ctrl+F7` opens the same settings in a window (see In-game settings).

| Key | Default | Meaning |
|---|---|---|
| `Enable` | 1 | Master switch (restart) |
| `EngineHooks` | 1 | Install the hooks; 0 leaves the client unmodified (restart) |
| `Overlay` | 1 | In-game settings window; 0 hides it at once, and from the next start leaves the game window alone |
| `OverlayKey` | Ctrl+F7 | Key that shows and hides the window: F1-F24, Insert, Delete, Home, End, PageUp, PageDown, Pause, ScrollLock, a letter or a digit, with optional `Ctrl+`, `Shift+`, `Alt+` |
| `Quality` | 2 | 1 quarter resolution / 16 steps, 2 half / 24, 3 half / 32 |
| `Density`, `Haze`, `GroundFog`, `FarFog` | 1, 1, 0.6, 1 | Density multipliers |
| `StockFog` | 1 | 1 replaces the stock fog with the distance fog, 0 keeps it |
| `DataMode` | 1 | 1 Classic layers where available, 0 derived layers everywhere |
| `ColorSpace` | 1 | 1 scatter and blend in linear light with a highlight roll-off, 0 gamma |
| `SunScatter`, `Ambient`, `Exposure` | 1, 1, 1 | Light in the fog |
| `ClassicExposure` | 1 | Brightness of the Classic layers (1 = as authored) |
| `LightShafts` | 1 | Shadowed in-scattering |
| `GodRays` | 0 | Radial sky rays, 0 = off |
| `GlowCompensation` | 1 | Pre-compensate the fog for the client's glow |
| `FarClipMax` | 1583 | Continent view distance up to 1583 yd, within the `farclip` setting (0 = Ascension's 791 cap). Turning it on from 0 needs a restart; other changes (including 0) apply at the next `farclip` change, map load or zone change, where raising it shows a loading screen |
| `MaxDistance` | 5000 | Fog range: sky integration length and the Classic distance-curve scale |
| `Temporal` | 0.85 | History weight, 0 = off |
| `Underwater` | 0 | Keep the effect under water |
| `LiquidDepth` | 1 | Water surfaces write depth while the fog draws |
| `DebugView` | 0 | 1 radiance, 2 transmittance, 3 linear depth |
| `SunMarker` | 0 | Red dot where the light direction projects |
| `LogLevel` | 1 | 0 errors, 1 info (frame summary every 60 s, depth probe every 30 s), 2 debug |

## Status and limits

- Tested in the client with native D3D9; DXVK and Wine are untested.
- Transparent effects, particles and water are fogged by the opaque depth behind them (kit IP-B), so
  near effects in front of the sky are dimmed slightly.
- Interiors get the outdoor layers; the `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off
  there). The settings window also needs a fog device, so it is missing when INTZ depth is unsupported.
- Water surfaces write depth only in the outdoor liquid pass; WMO liquids (city canals) still do not.
  Pixels beyond the far clip below the horizon are marched as level rays so they meet the sky at eye level.
- The modern fog path applies no exposure or tonemap and its frame is graded with a clamp and a LUT; the
  LUT (and the modern lighting and bloom) are not reproduced, so colours still differ from Classic.
- The distance fog (`FarFog`, maps without Classic data) has no modern counterpart: it stands in for the
  stock fog up to the 3.3.5 far clip, which is far shorter than the modern client's.
- `FarClipMax` raises memory use (about four times the loaded terrain in a 32-bit process); Ascension's
  reason for the continent cap is unknown. Without the key in the INI it stays off.
- Classic data follows the client's clear, storm and screen-effect slots and Classic's zone-light outlines.
  The underwater slots and noise modulation are not used. The zone lights' edge fade distance is chosen here:
  their `TransitionType` is 0 in every row and the modern client's transition rule is not known.
- Not implemented from the kit: the froxel pipeline (M3), fitted fog for transparents (M6), in-game CVars.

## License

GPL-2.0, see `LICENSE`. `data/fogdata.bin` is converted from WoW Classic client data and is not covered
by it.
