# CoAVolFog

Volumetric fog and light shafts for the Ascension/CoA 3.3.5a (build 12340) Direct3D 9 client.

It consists of a loader, a D3D9 device wrapper with a readable depth buffer, world-render hooks and a
per-pixel ray-marching renderer. Fog layers come from the Classic client's own
`LightDataGlobalVolumeFog` data wherever its lights cover the map, and are derived from the 3.3.5
day/night lighting elsewhere (Outland, Northrend, custom maps).

## Classic fog data

`tools/convert_classic_fog.py` converts the Classic client's `Light`, `LightData`, `LightDataGlobalVolumeFog`,
`ZoneLight` and `ZoneLightPoint` tables, exported from its DB2 files as CSV, into `data/fogdata.bin` (625 lights
with all eight condition slots, 2,079 time keys, 6,234 layer slots, 18 zone-light outlines):

```powershell
python tools/convert_classic_fog.py <folder or zip with the CSV exports> data/fogdata.bin
```

At run time the DLL blends the Classic lights around the camera (spheres: full weight inside the
falloff start, linear to the falloff end; the rest goes to the zone light whose outline holds the camera,
fading in over 100 yd inside the outline with the innermost outline on top, and otherwise to the map's
global light). Each light uses the condition slot the client itself uses for its stock lighting: the
slot a screen effect forces (the ghost effect forces slot 4, death), otherwise clear weather (slot 0)
blended toward storm (slot 2) by the client's storm weight. The DLL interpolates the two time keys around
the current time and pairs layers by their Classic layer index; a layer only one side has keeps its
colours and shape and has its density scaled by that side's weight. Each time key also keeps Classic's direct
light colour, blended the same way. The fog's sun scattering is scaled by the client's direct-light luminance
over Classic's, capped at 1, in clear weather, storms and screen-effect conditions. This keeps fog lighting
consistent with the older client's darker world lighting (about 0.41 in Goldshire's 20:00 storm and 0.34 at
Darkshire at 18:00 in linear mode). The correction preserves authored hue, extinction and ambient emission;
it is a compatibility calibration, not a measured reproduction of the modern renderer. Missing or near-black
Classic reference light leaves the authored scattering unchanged. Then it applies the Classic transforms:
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
- **Light shafts**: native world shadow maps include off-screen casters. Character shadows and three
  environment regions are filtered and blended at their edges; a screen-space march covers unavailable regions.
  The fallback projects a ray up to the far-clip distance toward the light, clips it to the viewport and near
  plane, and checks 12 perspective-correct depth intervals. This finds distant blockers that a short contact
  trace misses. For forward rays through fog in front of opaque geometry, depth silhouettes conservatively
  cover the hidden space behind them, so a nearer tree or character cannot reveal sunlight through a mountain.
  Sky rays retain finite occluder thickness. World-name text tests against depth without writing into it.
- **Local lights**: up to eight nearby native point lights scatter into the medium with the client's constant,
  linear and quadratic attenuation. Light/ray intersections preserve small light volumes between march samples.
- **Interior transitions**: the camera's native WMO blend reduces outdoor layers and sunlight while preserving
  the interior's native fog colour and range.
- **God rays** (optional): a radial blur of the bright sky around the sun.

Each march step integrates only its overlap with a layer's start and end distances, sampling the height and
distance profile inside that overlap. This keeps thin layers and partial boundary steps consistent across
quality levels. With temporal filtering disabled, samples stay at fixed midpoints so a stationary frame does
not shimmer. With it enabled, samples vary between frames and the filter rejects history from a different
surface depth or from a different depth class (world, distant terrain, sky). Each history tap is validated
before bilinear filtering; sky reprojection follows rotation without camera translation. Settings, map,
screen-effect slot, projection and large camera changes discard history.
Animated lighting also reduces history weight when current and previous radiance diverge, even if the surface
depth stays unchanged. The composite rejects depth-mismatched taps and marches at full resolution when all
nearby samples miss a thin silhouette.

Fog renders once after the world, including its late geometry and native sun/moon glare, and before screen
effects. The opaque M2 hook captures camera inputs only. Native material shaders and glare draws are untouched;
the former Material fog setting and its volume/instrumentation path have been removed. Old INI keys are ignored.

The three scene layers share a continuous, two-octave density field anchored in world space. `NoiseAmount`
controls modulation around the authored mean, `NoiseScale` controls feature size, and `NoiseWindSpeed` drifts
the field along world +X in yards per second. Zero amount restores homogeneous layers; zero wind keeps the
field stationary. The distance-fog layer stays smooth so variation cannot uncover the native far-clip boundary.
This procedural variation is an artistic control, not a reconstruction of Classic's authored noise data.

The effect is composited over the world before glow and the UI. The client's glow (`screen + g·blur²`, with
`g` from the day/night light) runs afterwards and would bleach bright fog to white, so fogged pixels are
pre-compensated with the live glow amount. Its other term, a blend toward the blur used by screen effects such as
drunkenness, is left as is. While the effect draws, the stock fog is pushed out of range for
the world render and restored afterwards. Known unavailable frames keep the stock fog; an unexpected draw
failure restores the fallback on the next frame.

Optional radial god rays use the remaining display highlight range with a smooth exponential blend. The blend
accounts for the client's glow before adding rays, then converts back to the pre-glow colour. Zero ray strength
preserves the fog-only result. This artistic screen-space effect requires a scene copy in both colour modes;
if the copy is unavailable, fog keeps its fixed-function fallback and radial rays are omitted.

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
| Hooks | Five 5-byte call displacements: the world render call (`0x4FB03D`, stock-fog override and restore), after the opaque M2 pass (`0x4F911D`, captures world inputs), the liquid surface pass (`0x4F9170`, forces depth writes), world-name text (`0x7E5818`, suppresses depth writes), and before the frame effects (`0x4F9281`, draws fog over the completed world). The original bytes are checked first; on any mismatch nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when `FarClipMax` is set at start-up, independently of the fog hooks. |
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
  `0x4F9213` contains `E8 58 76 2F 00`, calling the native sun/moon glare pass `0x7F0870` with no arguments.
  That glare call is not patched.
  `0x4F9281` calls FFX end `0x8C1010` (no arguments); the thunk renders the fog, then tail-jumps to it.
- Depth. The world viewport's MaxZ is `[0xADEEE4]` = 0.94, passed to GxXformSetViewport at `0x4F905A` and uploaded as
  `D3DVIEWPORT9::MaxZ` by the D3D9 backend (`0x6A9ACC`). The Gx viewport (`[[0xC5DF88] + 0xF80]`, MinZ/MaxZ) is stored
  by `0x681890` and uploaded lazily on the next draw or clear (`0x6A99E0`), so after the opaque pass the device can
  still hold the sky's `[0.999, 1]`; the capture reads MinZ/MaxZ from the Gx viewport, which the sky and WDL passes
  restore (`0x7F0CB3`, `0x796466`). The WDL sets its own viewport (`0x7960EB`); the sky viewport is set at
  `0x7F0A79` and the sky writes no depth, so depth at or above max(deepest world depth, 0.99903) is sky. The engine
  builds its projection with `0x6BF370` (OpenGL depth range, w = view z).
- Liquid depth. While writes are forced on, the wrapper records the client's own `D3DRS_ZWRITEENABLE` requests and
  re-applies the last one afterwards, so the client's render-state cache stays accurate.
- World-name text. `0x7E5818` contains `E8 23 76 ED FF`, calling `0x6BCE40` with the font batch at `[0xD380A8]`.
  This cdecl wrapper takes one pointer and tail-jumps to `0x6C53A0`. The batch is created by `0x6BF160(1, 1)` at
  `0x7E6511`. Its bit 0 at `+8` selects world rendering (`0x6C5564`), which requests Gx state 13 = 1 for depth
  testing (`0x6C5591`/`0x6C5596`) and state 15 = 1 for depth writes (`0x6C55BE`/`0x6C55C3`). The Gx state-15
  dispatch at `0x6A8F99` calls D3D9 `SetRenderState` with state 14, `D3DRS_ZWRITEENABLE`, at `0x6A8FC7`.
  Suppressing writes around this call keeps glyphs out of the depth sampled for fog shadows while preserving
  the original draw and depth comparison. The wrapper restores the client's last requested write state even
  when the draw changes it. The later name/icon path at `0x4FB042` is left in place.
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`) and
  the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`); `0x4F8770`
  feeds it the DayNight glow (`0xD38C2C`) as the additive weight of `lerp(screen, blur, other) + g·blur²`, where
  `other` is the screen effect's own blend amount.
- Native glare. `0x7EE150` initializes the sun-glare object at `0xD38EA8` from `Textures\sunGlare.blp`
  (`0xA41BC4`); `0x7EE230` initializes `0xD38F58` from `Textures\moonGlare.blp` (`0xA41BEC`). The late pass
  `0x7F0870` updates each with `0x7EF6E0` and draws it with `0x9AC400`. That draw changes the viewport depth to
  `[0.9990234375, 1]` at `0x9AC54B`, draws its quad at `0x9AC610`, and restores the viewport at `0x9AC63E`.
  It sets Gx blend state 6 to mode 3 at `0x9AC55E`-`0x9AC562`. The D3D9 backend reads source factor 5
  (`D3DBLEND_SRCALPHA`) from `[0xA2F964 + 3*4]` and destination factor 2 (`D3DBLEND_ONE`) from
  `[0xA2F994 + 3*4]`, uploading them at `0x6A4DAF` and `0x6A4DDC`; its second backend has identical tables at
  `0xA2FB68`/`0xA2FB98`. The glare depth range differs from the opaque world's `[0, 0.94]`.
  Moving the whole pass earlier would change occlusion: update method `0x9AC3C0` selects the GPU-query
  path `0x9ABE00`, which reads the previous result, begins a new query at `0x9ABE5A`, draws against the current
  world depth at `0x9AC2B1`, and ends it at `0x9AC367`. World geometry pass `0x7984A0` runs after the opaque hook:
  `0x4F9154` contains `E8 B7 5E 28 00`, calling `0x77F010`, which jumps to `0x7984A0`.
  The single final fog pass sees this completed depth and preserves the original glare and occlusion-query order.
- Far clip. The clamp `0x780770` is cdecl `float(float farclip, int mapId)`, result in ST0, caller pops; it bounds
  the value to `[0xA3E708]` (183.33) .. `[0xA3E710]` (1583.33). Its calls at `0x780810` (in the `farclip` CVar setter
  `0x780800`, also reached from Extensions.dll on zone changes) and `0x781444` (map load `0x781430`) are rare, so the
  hook re-reads the INI on every call.
- Light params slots. `0x7EB180` returns a light's `LightParams` for a slot (`Light` record `+0x1C + 4·slot`). For
  each light, `0x7EE510` takes slot 0 and, while the storm weight `[0xD38B88]` is above zero, blends in slot 2 by it
  (`0x7EC220`). The DayNight update sets that weight to `min(1, 4·[0xD38B4C])` just before the light blend
  (`0x7F3995`); the weather update writes `[0xD38B4C]` (`0x784A01`). When `[0xD38B58]` holds a slot, the light blend
  `0x7F3230` uses that slot instead (`0x7F346F`). `0x7ECEC0` stores it from the current `ScreenEffect` row (`+0x1C`,
  called at `0x4F712D`; values above 7 become −1) and `0x7ECEE0` clears it when the effect ends. In CoA's
  `ScreenEffect.dbc` the Ghost effect (ID 1) and the other death-style effects use slot 4; a few event effects force
  slots 0, 1, 2, 3 or 5, which the DLL follows the same way.
- Classic data. The converter keeps the `LightDataGlobalVolumeFog` rows the Classic client selects (flag bit 3) and
  stores them at their layer index (0–2), leaving an empty slot where a key has no layer at that index. On maps where
  any Classic light has fog (the old continents), Classic data applies wherever Classic lights hold at least half of
  the blend weight; a light without fog in the active slot counts with zero density, so the fog thins smoothly
  into it and the distance fog hides the far clip there. Maps without Classic fog use the derived layers.

- Native world shadows. The 12340 image registers the map-shadow callbacks at `0x7BD3A0`: matrix construction
  `0x7BAC10`, cascade setup `0x7BAFD0`, caster gathering `0x7BD200`, and rendering `0x7BBC50`. Exterior shadow quality
  is `[0xD43154]`; `[0xB1D51C]` means its resources need rebuilding. Qualities 1 and 2 supply the dynamic character
  map; qualities 3 and 4 add three environment caches, and quality 5 updates cascaded environment maps. The default
  character extent is 20 yd (`0x875E6E`); environment extents are 40, 160 and 640 yd (`0xB1D520`). Caster gathering
  traverses map chunks and tests their shadow frusta (`0x7BC490`, `0x7BCC00`, `0x7BC890`, `0x7BCF20`), so visibility
  includes geometry outside the camera image, within the client's loaded and culled shadow scene.
  `0x8750B0` constructs four view-space-to-shadow transforms, packs three float4 rows each at `0xD43348`, and uploads
  all twelve rows at `0x87450A`. It inverts the current Gx view (`0x7BAE25`), builds the light view, and applies a Y
  flip and a depth scale of 1/4000 (`0x875197`, `0x7BC105`). Both final XY axes map to UV with `0.5*x + 0.5`; Z is
  normalized linear light-view depth. The constant receiver biases are 0.4 yd for the character and first environment
  map, 0.8 yd for the second and 1.6 yd for the third (`0x7BAF4E`–`0x7BAFBE`), with no surface-normal dependence.
  The native direction comes from `[[0xCE04A8] + 0x7C]`, has its Z multiplied by five and capped at -1.2, and is
  normalized before storage at `0xD43180` (`0x7BB570`–`0x7BB628`). It points from the light into the world; volumetric
  direct scattering reverses it. This direction follows the client's surface-shadow lighting, not the visible
  celestial sprite. The optional sprite-centred god rays remain a separate effect.
  Texture handles are `[0xD43250 + 4*hardwareComparison]` and
  `[0xD43290 + 0x3C*cascade + 4*[0xD432C8 + 0x3C*cascade]]`, with hardware comparison selected by `[0xD43014]`.
  A texture resource holds its Gx texture at `+0x44`, or its cache at `+0x5C` holds the Gx texture at `+0x18`
  (`0x4B6CB0`–`0x4B6D80`); Gx `+0x38` is the D3D texture passed to `SetTexture` (`0x6A492E`, `0x6A88D6`). Native
  format 11 is `R32F` (`0xA2F81C`), while format 12 explicitly creates `D24X8` depth textures (`0x6A2C62`,
  `0x6A2D01`). The shipped `ShadowMapSL.bls` pixel shader writes `max(lightViewZ/4000, 0)` to red. The shipped
  `Terrain2_pcf.bls` uses `texldl` with `(0.5*XY+0.5, Z, 0)` for hardware depth comparison; the native texture flags
  select linear minification and magnification with no mip filtering for that path (`0x875DC1`, `0x6A4944`).
  Capture checks the image and relevant instruction bytes, the live formats and matrix values, and texture device
  ownership. It retains D3D references only for the fog draw, releases them on every exit, and falls back when the
  native maps are unavailable. These static contracts and offline tests do not establish in-game visual alignment.

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

Local light and interior inputs:

- World lights. `0x4F90EC` loads the world M2 scene from `[0xCD754C]`, then passes it to the opaque M2 render at
  `0x4F911D`. The point-light registration function `0x834C70` allocates a 64-by-64 pointer table at scene `+0x24`
  (`0x4000` bytes at `0x834CAE`), hashes the light's world X/Y in 20-yard cells and inserts a null-terminated
  list. Each light contains its scene at `+0`, type at `+8` (`1` is point), world position at `+0xC`, diffuse RGB
  at `+0x3C`, constant/linear/quadratic attenuation at `+0x54`, enabled state at `+0x60`, the address of its
  preceding link at `+0x64`, and the next light at `+0x68`. Removal at `0x834AB0` and the position/enabled
  setters at `0x835690`/`0x8356F0` maintain those links. Animated M2 positions are transformed to world space
  before the position setter at `0x828AF4`; the capture must not transform them again.
- Native colour. The M2 animation update at `0x8305B9` multiplies the evaluated intensity by the model scale
  at `+0x198`, then multiplies the evaluated RGB channels and stores the result in the native diffuse vector
  at `0x8305EA`-`0x8305FE`. The light upload at `0x835527`-`0x835536` and D3D9 diffuse upload at
  `0x6A462F`-`0x6A464A` copy those values unchanged. Captured colour already includes native intensity;
  it has no separate intensity field. The arithmetic alone does not establish a linear-light colour space.
- Attenuation. `0x835539` copies the three coefficients into the Gx light; the D3D9 backend at
  `0x6A46AC`-`0x6A46C1` uploads them as `D3DLIGHT9::Attenuation0/1/2`. Its range is the fixed value 10000 at
  `[0xA2F95C]`, not an authored cutoff radius. The volumetric selection bounds the influence where the brightest
  channel divided by `max(1, a0 + a1*d + a2*d*d)` falls to `1/256`, capped at 200 yards. It considers centres
  within that radius plus 200 yards of the camera and retains eight lights in descending camera contribution,
  with deterministic ties. These bounds are renderer policy. The source supports point and directional lights;
  no native spotlight cone has been established. Coverage of WMO-only lights through this scene is unverified.
- Interiors. `0x795D40` queries the camera position and stores the camera WMO instance at `[0xCD87A4]`, with
  active group IDs at `[0xCDB0DC]` and count `[0xCDB0D8]`. The instance's root is `+0xF4`; the loaded root flag
  is `+0x1E0`, group count `+0x1F4`, and inline group pointers start at `+0x1F8`. The allocation loop sets the
  count at `0x7D7F18`, and the destruction loop bounds it at `0x7AE467`. Group `+0x198` bit 0 means loaded.
  The client's camera-fog query at `0x7A11C7` accepts an interior group when its `+0x30` flags have neither
  bit in `0x48` set. Day/night fog update `0x7F17B5` obtains the distance to the exterior boundary and writes
  its clamped transition weight to `[0xD38B9C]` at `0x7F1931`. `0x7F1955` onward uses that weight to blend the
  native outdoor fog toward WMO fog. The existing frame inputs at `0xD38BA0`/`0xD38BA4`/`0xD38BA8` therefore
  already contain the blended native colour/start/end. The captured weight is gated by actual interior group
  membership; it describes the camera's transition, not a spatial room or portal field along each fog ray.
- Capture guards. Before reading these inputs, the module checks the 12340 image timestamp/base and the exact
  instructions for the scene load, point-light buckets/links/attenuation, camera groups/interior flags and blend
  store. Reads are protected by SEH, pointer/span and count limits. Light lists also validate ownership and
  preceding links, with limits of 512 nodes per bucket and 8192 total. No client references survive the capture.
  Invalid inputs return an empty result. These checks verify layout compatibility, not in-game appearance.

Two addresses that look relevant are not: `0xD38B98` is a density-like value that Extensions.dll patches (the fog
end is `0xD38BA8`), and `0xD38C9C` is a near-constant model lighting direction (polar angle 110–127°), not the
visible sun, so shafts use the sprite positions.

## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. On laptops whose F-keys send media keys
by default, hold Fn (the log names the key that arrived: `overlay: Ctrl+Media Previous pressed`). The window edits
every setting below except `Enable`, `EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change.
**Save** writes the settings changed in the window back to `CoAVolFog.ini` through `WritePrivateProfileString`,
which keeps the comments and every other line, rounded as the file stores them; settings not changed in the window
take what the file holds, so hand edits made meanwhile survive. **Revert** reloads the file. The world render also
reloads a hand-edited file, which replaces unsaved changes made in the window. The window shows whether the fog drew
in the last frame, or why it did not. The log records when it opens and closes, modified non-typing keys that
arrive (never letters, digits or punctuation) and the first reason a draw is skipped.

- Input. The hotkey, its key-up and its characters never reach the client; with Ctrl held, Windows reports Pause and
  ScrollLock as Cancel, which also matches. While the window is open, clicks and the wheel go to the client unless
  ImGui wants the mouse (the cursor is over the window, or a drag started on it), and a button's release goes where
  its press went. Key-downs and characters go to the client unless an ImGui text field is active (Ctrl+click on a
  slider); the window takes no keyboard navigation, so Tab stays the client's. IME messages reach ImGui only while it
  wants text. Modifier state is read from the keyboard every frame. Mouse moves and key-ups always reach the client,
  so its cursor keeps following the pointer and no game key sticks. A text field left active is released when the
  window is shown or hidden. The
  client draws a D3D cursor (`SetCursorProperties` at `0x6A009C`, `ShowCursor` on `WM_SETCURSOR` at `0x6A058E`), so
  ImGui leaves the cursor shape alone. Coordinates are scaled from the client area to the back buffer.
- Drawing. The window is drawn in the wrapper's `Present`, over the client's UI, in its own scene. A full state
  block, the render targets and stream 0 (with its offset) are captured and restored, and the states ImGui's DX9
  backend leaves alone are set for it (colour write mask, sRGB write, clip planes, texture-coordinate index and
  transform, stage result, sampler sRGB and mip filter). ImGui's font texture and buffers live in the default pool
  and are released before every `Reset`. The window scales with the back-buffer height above 1080 lines.
- Hidden, the overlay only checks the hotkey. An exception in it turns the overlay off for the session and is
  logged; the frame's scene is ended, the device state restored and its references released even then, so a later
  `Reset` still succeeds. `Overlay=0` leaves the game window untouched from the next start.
- Widgets in each section get their own ID scope, so a control named like its section header (Quality, Density)
  is not cancelled by the header.

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
- authored fog's sun scattering following the client's darker direct light in clear weather and storms;
- temporal accumulation converging on a static camera, and identical stationary frames with it disabled;
- analytic Beer–Lambert opacity for partial cells and thin layers at every march quality, including jitter;
- previous-depth rejection, world/sky separation and validated bilinear taps in the actual temporal shader;
- packed history depth accuracy and god-ray occlusion with an odd-sized, offset world viewport;
- native hardware shadow comparison and float-depth maps, coverage transitions and off-screen blockers;
- point-light attenuation and selection, narrow ray/light intersections, HDR bounds and interior transitions;
- screen-space shadows for distant rocks, angled rays, nearer blockers, clear sky and foreground silhouettes;
  full fog integration verifies blocked sunlight behind nearer silhouettes, retained ambient light and the
  Light shafts switch at every quality;
- thin silhouettes and world-anchored density variation;
- name text retaining its colour and depth test without changing world depth; text and liquid depth-write
  overrides restoring the client's state, including native state blocks;
- malformed fog-data counts and index ranges;
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

`vfog_harness --scene performance` measures the fog passes at 1920×1080 with 32 warmup frames and 60 measured
frames per case. Each quality compares zero lights and eight lights;
all cases retain the default density variation. It reports median and p95 GPU time, or explicitly labelled
event-flushed elapsed time if timestamp queries are unavailable. This excludes native shadow-map generation,
native draw overhead and in-game CPU work; it is a controlled renderer cost, not a game frame-rate test.

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
| `NoiseAmount`, `NoiseScale`, `NoiseWindSpeed` | 0.15, 0.025, 0.5 | World-space scene-density variation, inverse feature scale, and drift in yd/s; amount 0 disables it |
| `StockFog` | 1 | 1 replaces the stock fog with the distance fog, 0 keeps it |
| `DataMode` | 1 | 1 Classic layers where available, 0 derived layers everywhere |
| `ColorSpace` | 1 | 1 scatter and blend in linear light with a highlight roll-off, 0 gamma |
| `SunScatter`, `Ambient`, `Exposure` | 1, 1, 1 | Light in the fog |
| `ClassicExposure` | 1 | Brightness of the Classic layers (1 = as authored) |
| `LightShafts` | 1 | Shadowed in-scattering |
| `WorldShadows` | 1 | Use the client's native world shadow maps when available; requires `LightShafts` |
| `LocalLights`, `LocalLightIntensity` | 1, 1 | Scatter up to eight nearby world point lights; intensity 0..8 |
| `InteriorAware`, `InteriorDensity` | 1, 0.15 | Follow the camera's WMO interior transition; retain this fraction of outdoor layer density while preserving native interior distance fog |
| `GodRays` | 0 | Radial sky rays, 0 = off |
| `GlowCompensation` | 1 | Pre-compensate the fog for the client's glow |
| `FarClipMax` | 1583 | Continent view distance up to 1583 yd, within the `farclip` setting (0 = Ascension's 791 cap). Turning it on from 0 needs a restart; other changes (including 0) apply at the next `farclip` change, map load or zone change, where raising it shows a loading screen |
| `MaxDistance` | 5000 | Fog range: sky integration length and the Classic distance-curve scale |
| `Temporal` | 0.85 | History weight, 0 = off |
| `Underwater` | 0 | Keep the effect under water |
| `LiquidDepth` | 1 | Water surfaces write depth so fog uses their distance |
| `DebugView` | 0 | 1 radiance, 2 transmittance, 3 linear depth |
| `SunMarker` | 0 | Red dot where the light direction projects |
| `LogLevel` | 1 | 0 errors, 1 info (frame summary every 60 s, depth probe every 30 s), 2 debug |

## Status and limits

This is an atmospheric approximation, not a reproduction of WoW Forever's complete lighting renderer.
[Blizzard's official overview](
https://news.blizzard.com/en-gb/article/24303862/world-of-warcraft-forever-whats-next-panel-recap)
describes mist over water and moonlight through trees. Matching those scenes requires matched camera, time,
weather and exposure captures; shared Classic fog data alone does not establish visual parity.

World shadows, native point-light scattering, camera interior transitions, thin-silhouette reconstruction
and world-space density variation are implemented. Their numerical and state contracts are
covered by the offline D3D9 harness. Client address and shader-format evidence is recorded above; these checks
do not establish in-game appearance or performance. Matched scene colour and exposure calibration still needs
owner-controlled client captures. Surface lighting, indirect illumination, bloom and colour grading remain
the native client's systems.

- The original path has been tested in the client with native D3D9. Changes to lighting and occlusion require
  an owner test; DXVK and Wine are untested.
- The current march and composite kernels require more than the 512 instruction slots guaranteed by
  [baseline pixel shader 3.0](
  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-3-0).
  The largest current pass uses 1,016 slots, 31 temporary registers and four nested loops. Lower quality reduces
  integration work, not this static shader requirement. An unsupported required shader disables volumetric fog
  for that device and logs its name, HRESULT and available shader caps; native fog remains the fallback.
  Device-loss and memory-allocation failures are retried instead of being cached as unsupported.
- World shadows follow the client's shadow-light direction and available map coverage. The visible celestial
  sprite can use a different direction. Screen-space fallback cannot include unseen blockers; its conservative
  treatment of depth silhouettes can darken fog where geometry hidden behind them would permit light.
- Local lighting captures the current M2 scene's point-light table. It does not create spotlight cones, local
  shadow maps or guarantee coverage of every WMO-only light. Influence is capped at 200 yd, with a smooth outer
  fade and an attenuation denominator floor of 1 to bound the point emitter's near-field brightness.
- Transparent materials without depth writes inherit the depth behind them in the final fog pass. Fog at each
  transparent surface's own depth is currently unsupported.
- Stock fog suppression is prepared before the world draw; an unexpected fog draw failure can leave one frame
  without replacement fog before the next frame restores the fallback.
- Interior treatment follows camera membership and its native transition weight. It is not a spatial room or
  portal volume, so views through doorways still need scene-specific evaluation.
- The `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off there). The settings window also
  needs a fog device, so it is missing when INTZ depth is unsupported.
- Pixels beyond the far clip below the horizon are marched as level rays so they meet the sky at eye level.
- The modern fog path applies no exposure or tonemap and its frame is graded with a clamp and a LUT; the
  LUT (and the modern lighting and bloom) are not reproduced, so colours still differ from Classic.
- The distance fog (`FarFog`, maps without Classic data) has no modern counterpart: it stands in for the
  stock fog up to the 3.3.5 far clip, which is far shorter than the modern client's.
- `FarClipMax` raises memory use (about four times the loaded terrain in a 32-bit process); Ascension's
  reason for the continent cap is unknown. Without the key in the INI it stays off.
- Classic data follows the client's clear, storm and screen-effect slots and Classic's zone-light outlines.
  Procedural noise is configured independently of that data. The zone lights' edge fade distance is chosen here:
  their `TransitionType` is 0 in
  every row and the modern client's transition rule is not known.
- In-game CVars are not implemented; the INI and settings window control the effect.

## License

GPL-2.0, see `LICENSE`. `data/fogdata.bin` is converted from WoW Classic client data and is not covered
by it.
