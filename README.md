# CoAVolFog

Volumetric fog for the Ascension/CoA 3.3.5a (build 12340) Direct3D 9 client.

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
density ×0.01, heights relative to the player when flag bit 1 is set, shadow colour and density for flag bit 0
while the light is below the horizon, `1 + strength·((d − start)/range)^exponent` over a 5,000-yd
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
- **Local lights**: up to eight nearby native point lights scatter into the medium with the client's constant,
  linear and quadratic attenuation. Light/ray intersections preserve small light volumes between march samples.
- **Interior transitions**: the camera's native WMO blend reduces outdoor layers and sunlight while preserving
  the interior's native fog colour and range.
- **God rays** (optional): a radial blur of the bright sky around the sun.

Each march step integrates only its overlap with a layer's start and end distances, sampling the height and
distance profile inside that overlap. This keeps thin layers and partial boundary steps consistent across
quality levels. With temporal filtering disabled, samples stay at fixed midpoints so a stationary frame does
not shimmer, and the composite reads the march result directly: the temporal and history-depth passes are skipped,
and the history counts as invalid until filtering is enabled again. With it enabled, samples vary between frames and
the filter rejects history from a different
surface depth or from a different depth class (world, distant terrain, sky). Each history tap is validated
before bilinear filtering; sky reprojection follows rotation without camera translation. Settings, map,
screen-effect slot, projection and large camera changes discard history.
Animated lighting also reduces history weight when current and previous radiance diverge, even if the surface
depth stays unchanged. The composite rejects depth-mismatched taps and marches at full resolution when all
nearby samples miss a thin silhouette.

Ground seen at a grazing angle changes depth by more than the 2% tap tolerance between neighbouring low-resolution
rows once it is farther than about 22 camera heights (1080p, the harness's 52° vertical field of view; 11 at
Quality 1, whose taps are twice as far apart). Every tap then fails, so a band below the horizon re-marched each of
its in-between pixels at full resolution: 1.2% of the benchmark street's pixels at Quality 2 and 2.9% at Quality 1,
spread over 8×4-pixel blocks (32 pixels, as many as a GPU thread group, whose other pixels wait for the march) that
cover 2.8% and 6.8% of the frame. Before marching, the composite now takes the bilinear average of the four taps
from the current frame's low-resolution march if all of these hold:

- the pixel lies on the plane of its taps: the taps share the pixel's depth class, and their bilinearly
  interpolated raw depth, which is linear in screen space across any plane, gives the pixel's distance within 0.5%;
- no tap reaches the horizon blend, where the march stretches and levels the ray;
- no local light's sphere meets the pixel's ray before the surface. The two-point light quadrature follows the
  march steps, which scale with the surface distance, so near small bright lamps its result wobbles by several
  levels from row to row, and an average would smooth out what the march draws;
- that march is linear across the taps: along each interpolated axis, the second differences over the taps and
  their outer neighbours stay within 2/255 of the displayed fog (opacity as is, radiance times the exposure and,
  when blending in linear light, square-rooted as a stand-in for the display gamma). This rejects the curvature
  near the horizon and at layer starts, density-variation detail, and the jitter noise of a single frame.

The current march is used rather than the temporally filtered result because the full-resolution march it replaces
has no history either: with the filtered result, a camera moving 0.6 yd per frame in the harness's scene left the
band up to 4/255 behind the march; with the current one, 1/255. Everything else, including thin silhouettes, still
marches. Rendering the benchmark street at 1080p at every quality with no lights, flood lights, street lamps,
Classic data, a moving camera and god rays, including the radiance and transmittance views, and the harness and
harbour scenes changed no pixel by more than 2/255 against the full march. Without lights, 0.22% of the pixels
still march at Quality 2 (in blocks covering 0.91% of the frame) and 1.4% at Quality 1 (3.4%); with the lamps or
flood lights, whose spheres cover most of the band, most of it still marches.

In the performance scene (temperature-gated, interleaved runs on the owner's RTX 2060 laptop) this saved 1–3% of the
fog's GPU time at Quality 2 and 3 and 4–6% at Quality 1 without lights; the light cases moved by 1% or less.
That is far less than the band's share suggests. With every full-resolution march removed (not a correct image)
the fog took 14–21% less time at Quality 2 and 3 and 35–64% less at Quality 1, and the same held with the march
still in the shader but never reached, so the cost lies in running it, not in its registers. Removing the marches
of only some of the band's rows (those above, at or below the wall's base) saved 0–6% at Quality 2 and 2–28% at
Quality 1, far from proportional to the pixels that stopped marching: the composite's time seems to follow the
latency of the marches that remain, and thin silhouettes always keep some, more than their number. Running the
marches in a separate full-resolution pass before the low-resolution march did not hide them behind other work
either and was 2–6% slower.

Local lights are marched by their own shaders. When at least one point light is uploaded the renderer draws the
march and the composite with the `ps_lit_*` variants (`LOCAL_LIGHTS=1`); otherwise it uses variants without any
light code, which need 21 instead of 30 temporary registers for the march and 24 instead of 32 for the composite.
The lit march still integrates each light with the two-point Gauss–Legendre quadrature over the overlap of the
light's sphere chord, the march step and each layer's start and limit, weighted by that layer's density, phase and
horizon-shadow density scale; only the order of evaluation changed:

- a chord depends only on the pixel's ray. The march keeps the next chord endpoint beyond the current step's start,
  whether a chord covers that point, and the range of lights whose chords still lie ahead, and recomputes them only
  when a step passes an endpoint. A step without an endpoint inside it and outside every chord does no light work,
  and one inside a chord knows that every light meeting it covers the whole step;
- lights covering the whole step share its two quadrature points: the density profile of every layer that covers
  the step, and the density variation, are evaluated once per point for all of them instead of once per light and
  layer. A light's distance, attenuation, fade and angle are evaluated once per point for all layers;
- a light that ends or begins inside the step, and a layer that starts or ends inside a light's interval, still use
  the Gauss points of their own interval;
- a layer's phase `lerp(HG, 1, isotropic)` is summed as `isotropic·ρ + (1 − isotropic)(1 − g)³·ρ / t^1.5` with
  `t = 1 + g(g − 2 cos θ)`, the two density weights folded per pixel.

ps_3_0 has 32 temporary registers, and fxc hoists every value derived only from constants out of the loops, where it
stays live through the whole march: the phase terms `1 + g²` and `2g` of four layers took three registers, and every
light-texture fetch site kept a register for its constant `(0.5, 0, 0)` coordinate components. The lit shaders
therefore form `t` from `g` itself, fold the phase weights into two per-pixel vectors, and address the light texels
with the coordinate replicated into every component and a negative level of detail (one level, point sampling
either way), which cost nothing measurable. The same addressing on the density-noise volume made the fog 4–29%
slower, so noise lookups keep a literal zero level of detail. The lit composite uses all 32 registers.

Rendering the benchmark street at 1080p at every quality with no lights, flood lights, street lamps at eight times
intensity, Classic data, a moving camera and god rays, including the radiance and transmittance views, changed 37
pixels in 19 of 57 images by 1/255; every image without lights, and the harness and harbour scenes, stayed
bit-identical. In the performance scene (temperature-gated, interleaved runs on the owner's RTX 2060 laptop) the fog
took 22%, 26% and 30% less GPU time with the flood lights at Quality 1, 2 and 3, 23–30% less with the lamps on the
derived layers, 26–35% less with the lamps on Classic layers, and 10–14% less without lights. With the flood lights
most of the remaining light cost is the per-light distance and phase work at the shared points, about 115
instructions per light and step, and the composite's full-resolution marches, since their spheres cover the whole
grazing band: an intermediate build without light code in the composite (not a correct image) took 2.3 ms (19%) less
at Quality 2.

The shaders without light code unroll the four layers and read each layer's constants directly. A step samples the
density variation once at its jittered distance, and every layer whose clipped sample falls on that distance reuses
it; a layer clipped by its start or limit inside the step samples its own point. The march stops at the first step
that begins beyond the end of every layer with density (Classic layers are unbounded, so this ends only derived and
distance-limited rays early). The result matches the per-layer loop exactly. The lit shaders keep the loop; they
already use 30 and 32 of the 32 temporary registers. In the performance scene on the owner's RTX 2060 laptop, runs
started at the same temperature (63–66 °C) put the fog without lights 16%, 22–26% and 25–27% faster at Quality 1, 2
and 3, on derived and Classic layers alike; the light cases, which use the unchanged lit shaders, stayed within 1%.
The laptop did not cool below 63 °C between runs, so later runs at 84–86 °C were throttled and are not compared.

Fog renders once after the world, including its late geometry and native sun/moon glare, and before screen
effects. The opaque M2 hook captures camera inputs only. Native material shaders and glare draws are untouched;
the former Material fog setting and its volume/instrumentation path have been removed. The former `LightShafts`
and `WorldShadows` settings, with their screen-space shadow march and native shadow-map capture, have been removed
as well; scene geometry no longer shadows the fog's sunlight. Old INI keys are ignored. Two in-game changes
follow that the offline harness cannot show, because it never supplied native shadow maps and renders with the
shipped settings:

- The forward-scattering halo always centres on the visible sun or moon sprite, like the god rays. With client
  shadows on and the former defaults, it followed the native shadow-light direction `[[0xCE04A8] + 0x7C]`, whose
  vertical component the client multiplies by five, up to 1.2, before normalising (`0x7BB570`–`0x7BB628`). If that
  direction otherwise matched the sprite, a sun 10° above the horizon had its halo about 41° high, 31° above the
  sprite; 5° gave 24°, and 30° gave 54°.
- An INI that still sets `LightShafts=0` used to switch off Classic's night shadow colour and density (flag bit 0)
  as well. Such an INI now gets them, as the default `LightShafts=1` always did.

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
  Suppressing writes around this call keeps glyph quads out of the world depth the fog is integrated against,
  where they would end fog rays at the text and leave thinner fog around names, while preserving the original
  draw and depth comparison. The wrapper restores the client's last requested write state even
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
- `LocalLights=0` skips the walk over the 4,096 point-light buckets; the interior inputs are still captured, and
  the frame summary logs `local points not captured`.

Two addresses that look relevant are not: `0xD38B98` is a density-like value that Extensions.dll patches (the fog
end is `0xD38BA8`), and `0xD38C9C` is a near-constant model lighting direction (polar angle 110–127°), not the
visible sun, so the fog's light direction follows the sprite positions.

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
  transform, stage result, sampler sRGB and mip filter). The full state block is created on the first frame the
  window draws and re-captured every frame after; it is released at the first `Present` after the window closes
  and before every `Reset`, so it holds no client resources while hidden. ImGui's DX9 backend still creates and
  releases its own full state block inside every `ImGui_ImplDX9_RenderDrawData` call. ImGui's font texture and
  buffers live in the default pool and are released before every `Reset`. The window scales with the back-buffer
  height above 1080 lines.
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
- with `Temporal=0`, exactly two low-resolution passes fewer (occlusion-query pixel counts): the temporal and
  history-depth draws are skipped;
- on a second, fresh fog device at `LogLevel=1`: the depth probe issued on frame 60 is not logged by that frame's
  render call but on a later frame, with all five rows, and the next frame summary reports a fog GPU time;
- the fog GPU timer with injected query-creation failures: out of memory retried after a `Reset` or 600 frames
  later, missing timestamp support found before the first summary, which then omits the time, and not retried;
- analytic Beer–Lambert opacity for partial cells and thin layers at every march quality, including jitter, in the
  march with and without local-light code;
- previous-depth rejection, world/sky separation and validated bilinear taps in the actual temporal shader;
- packed history depth accuracy and god-ray occlusion with an odd-sized, offset world viewport;
- point-light attenuation and selection, narrow ray/light intersections, HDR bounds and interior transitions;
  a light filling the march with a lamp inside it, and lamps separated by unlit fog, against a CPU integral;
- the renderer scattering point lights only through the lit shaders, and switching between lit and unlit shaders
  from frame to frame without changing either frame;
  native capture rejecting the harness image and clearing stale inputs whether or not the point-light walk is
  requested (skipping the walk with `LocalLights=0` reads the client and is not verified offline);
- the unrolled march without light code matching the per-layer loop exactly with clipped, empty and shadowed
  layers, sky and horizon rays, at every quality;
- Classic shadow colour and half shadow density only while the light is below the horizon, at every quality and
  with a foreground silhouette, and fog behind a sun occluder keeping its unshadowed in-scattering;
- thin silhouettes and world-anchored density variation;
- grazing ground between low-resolution rows interpolating the current march's linear ramp, not the filtered
  history, at every quality with half and quarter resolution taps and with both composites, also beside a light
  behind the ground, and marching instead where the current march curves between the taps or a light crosses the
  view ray;
- name text retaining its colour and depth test without changing world depth; text and liquid depth-write
  overrides restoring the client's state, including native state blocks;
- malformed fog-data counts and index ranges;
- `OverlayKey` parsing, and saving from the settings window into a copy of the shipped INI (only changed lines
  rewritten, comments and the removed `LightShafts`/`WorldShadows` keys kept but ignored, restart-only keys
  untouched, values clamped like the INI, Revert);
- the overlay hotkey through the chained window procedure, clicks and keys routed to the window or the client,
  device state restored around the overlay, its pixels confined to its window, nothing drawn while hidden, and
  drawing again after Reset;
- Reset at a new size and reference counts reaching zero.

It writes `before.png`, `after.png`, `overlay.png` and the debug views to `build/harness-out`.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration.

`vfog_harness --scene performance` measures the fog passes at 1920×1080 with 32 warmup frames and 60 measured
frames per case. Each quality runs six cases on the same street and camera:

- `derived-none`, `derived-flood8`, `derived-lamps8`: derived layers (`DataMode=0`) with no point lights, with
  eight flood lights reaching 130 yd that surround the camera, or with eight street lamps reaching 10–20 yd,
  30–140 yd ahead;
- `classic-none`, `classic-lamps8`: the street moved to the Stormwind harbour at 18:43 (the harbour scene's
  map, position, time and sun) with Classic fog data (`DataMode=1`, three Classic layers) and no lights or the
  eight lamps. These cases are skipped with a printed reason if `fogdata.bin` beside `CoAVolFog.dll` does not
  resolve the harbour;
- `derived-none-log1`: `derived-none` at the shipped `LogLevel=1`, which adds the fog GPU timer's queries to every
  frame (see Log); the run's one depth probe falls in its first untimed warmup frames. Every other case runs at
  `LogLevel=0`.

All cases retain the default density variation. Before the first timed case, the heaviest case runs untimed for at
least 1.5 s so that a laptop GPU has left its idle clocks; without it, the first case measured about 55% slow. The
scene reports median and p95 GPU time as `quality,case,point_lights,median_ms,p95_ms`, or explicitly labelled
event-flushed elapsed time if timestamp queries are unavailable. This excludes native draw overhead and in-game
CPU work; it is a controlled renderer cost, not a game frame-rate test.

## Install

Close the client, then copy `version.dll`, `CoAVolFog.dll`, `CoAVolFog.ini` and `fogdata.bin` next to
`Ascension.exe`.
Remove `version.dll` and `CoAVolFog.dll` to uninstall; nothing else in the client is changed. The DLL
writes `CoAVolFog.log` next to itself.

## Log

At `LogLevel=1` the log holds a frame summary on the first frame, whenever the viewport or far clip changes, and
every 60 s (`LogLevel=2` adds one every 600 frames). Its first line ends with the fog's GPU cost over the interval
since the previous summary, for example
`frame 3600: viewport 0,0 1920x1080 ... maxdist 5000, fog gpu 2.31 ms (median of 3542 frames, 0 skipped)`:

- The time runs from before the march to after the composite (god rays and the scene copy included, the depth
  probe excluded). Each frame issues a `D3DQUERYTYPE_TIMESTAMPDISJOINT` bracket around a `TIMESTAMPFREQ` and two
  `TIMESTAMP` queries from a ring of 32 such sets; finished sets are read without `D3DGETDATA_FLUSH` on later
  frames, and the line reports the median of the collected frames (the latest 16,384 at most).
- `skipped` counts frames whose time was lost: every set still in flight, a disjoint interval, a failed read, or
  no queries after a failed creation. `fog gpu no samples` means no set finished in the interval.
- The queries are created at the first summary, so without timestamp queries (`D3DERR_NOTAVAILABLE` or
  `D3DERR_INVALIDCALL` on a working device) the log says once, before the first summary, that GPU timing is
  unavailable, and every summary omits it. Any other creation failure, such as running out of memory, is logged
  and retried 600 frames later or after the next `Reset`. The queries are released before every `Reset` and
  recreated after it; at `LogLevel=0` none are issued.
- The time is the GPU work of the fog passes alone, without the client's own rendering or any CPU work; compare it
  with the frame time to see the fog's share of a GPU-bound frame.
- The timer itself cost no measurable GPU time on the owner's RTX 2060 laptop at 1080p: `derived-none-log1` stayed
  within 0.02 ms of `derived-none` at Quality 1 and 2 (see the performance scene), as it does without the timer,
  and the fog's render call took about 0.01 ms more CPU time. Other drivers, such as older AMD ones or DXVK, are
  unmeasured.

The depth probe logs raw depth, distance and fog opacity at 25 points on frame 60, then every 60 s up to five
times (every 30 s without limit at `LogLevel=2`). The probe is drawn after the composite and its
`GetRenderTargetData` into a system-memory surface is issued right away, followed by an event query; the rows are
logged on the first later frame whose event query has signalled, so the lock never waits. On the owner's NVIDIA
driver (RTX 2060 laptop) `GetRenderTargetData` returns at once, but the command-buffer flush that carries the
copy, usually the next `Present`, waits until the GPU has executed it; each probe therefore still costs one wait
for the GPU work queued before it. Issuing the copy only after an event query signalled was measured to wait
longer, because the copy then queues behind the frames issued in the meantime, and a lockable render target
polled with `D3DLOCK_DONOTWAIT` never became readable while the GPU stayed busy.

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
| `LogLevel` | 1 | 0 errors, 1 info (frame summary with the fog's GPU time, depth probe; see Log), 2 debug |

## Status and limits

This is an atmospheric approximation, not a reproduction of WoW Forever's complete lighting renderer.
[Blizzard's official overview](
https://news.blizzard.com/en-gb/article/24303862/world-of-warcraft-forever-whats-next-panel-recap)
describes mist over water and moonlight through trees. Matching those scenes requires matched camera, time,
weather and exposure captures; shared Classic fog data alone does not establish visual parity.

Native point-light scattering, camera interior transitions, thin-silhouette reconstruction and world-space
density variation are implemented. Their numerical and state contracts are
covered by the offline D3D9 harness. Client address and shader-format evidence is recorded above; these checks
do not establish in-game appearance or performance. Matched scene colour and exposure calibration still needs
owner-controlled client captures. Surface lighting, indirect illumination, bloom and colour grading remain
the native client's systems.

- The original path has been tested in the client with native D3D9. Changes to lighting and occlusion require
  an owner test. The build without fog shadows, with the halo on the sprite and the reworked point lights and
  upsample, was tested by the owner on 2026-09-28 at 2560x1440 on the RTX 2060 laptop in Elwynn Forest, Stormwind
  and Duskwood: the fog's GPU time was 8–11 ms with one to six local lights and 15–16 ms with seven or eight.
  The optimisations were measured on that GPU only; other vendors and integrated GPUs are unmeasured.
  DXVK and Wine are untested.
- The composite kernels and the lit march require more than the 512 instruction slots guaranteed by
  [baseline pixel shader 3.0](
  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-3-0).
  Without local lights the composite uses 990 slots and 24 temporary registers and the march 536 slots; with them
  1,528 slots with all 32 registers and 1,030 slots. Lower quality reduces integration work, not this static shader
  requirement. An unsupported required shader disables volumetric fog for that device and logs its name, HRESULT and
  available shader caps; native fog remains the fallback.
  Device-loss and memory-allocation failures are retried instead of being cached as unsupported.
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

CoAVolFog is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`); see [LICENSE](LICENSE).

As an additional permission under GPLv3 section 7, you may link or combine CoAVolFog, including modified versions,
with the World of Warcraft client, and distribute the resulting combination without providing the client's source
code. GPLv3 continues to apply to CoAVolFog.

`data/fogdata.bin` is converted from WoW Classic client data and is not covered by this license or exception.
