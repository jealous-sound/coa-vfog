# CoAVolFog

Volumetric fog and modern water for the Ascension/CoA 3.3.5a (build 12340) Direct3D 9 client.

It consists of a loader, a D3D9 device wrapper with a readable depth buffer, world-render hooks, a per-pixel
ray-marching fog renderer and a water renderer. Fog layers come from the Classic client's own
`LightDataGlobalVolumeFog` data wherever its lights cover the map, and are derived from the 3.3.5 day/night lighting
elsewhere (Outland, Northrend, custom maps). Water is shaded like WoW Forever's PBR water, from its liquid presets,
wave spectra and foam textures.

## What it draws

- **Distance haze** that thickens toward the horizon and fades with altitude, tinted by the zone's
  stock fog colour.
- **Ground mist** that hugs the terrain around the player; zones with short stock fog get more.
- **Distance fog** that replaces the stock linear fog on maps without Classic data: it turns opaque where
  the stock fog did, is lit by the direct light (warm at sunset), and fades into a horizon band on the sky.
  With Classic layers it returns only across the edge of Classic coverage and where the Classic layers are
  too thin to hide the far clip.
- **Forward scattering** around the visible sun or moon sprite (Henyey–Greenstein phase).
- **Local lights**: up to eight nearby native point lights scatter into the medium with the client's constant,
  linear and quadratic attenuation. Light/ray intersections preserve small light volumes between march samples.
  Every light scatters with one shared phase (`LocalLightPhase`), and one brighter than 1 keeps its intensity.
- **Interior transitions**: the camera's native WMO blend reduces outdoor layers and sunlight while preserving
  the interior's native fog colour and range.
- **Authored noise**: drifting fog banks where the Classic layers carry the modern client's noise, mostly in
  storms (`ClassicNoise`).
- **Density variation**: a two-octave field anchored in world space modulates the scene layers around their
  authored mean and drifts along world +X; the distance fog stays smooth so it cannot uncover the far clip. This
  is an artistic control, not a reconstruction of Classic's authored noise.
  Layers with authored noise use that instead.
- **God rays** (optional): a radial blur of the bright sky around the sun.
- **Modern water** on lakes, rivers, the sea and indoor pools: FFT waves from Forever's wave tiles, refraction,
  depth absorption and in-scattering, foam on wave crests, along shores and in shallow water, a sun or moon glint,
  and reflections of the scene with the sky as a fallback. The zone's own water colours can tint it
  (`WaterZoneColors`). Magma, slime, custom non-water liquids and water seen from below keep the client's look.
  Players, creatures, pets and mounts that wade or swim drag a V-shaped wake and stir the water as they start and
  stop (`WaterRipples`), in place of the client's flat splash and wake sprites (`WaterClientSplashes`).
- **Multisampling**: the game's Multisampling option is kept where the DLL can copy the multisampled depth
  (`Multisampling`); fog and water edges are resolved per sample.
- **Opt-in** (all off by default): Forever's per-light glow amount (`ForeverGlow`) and colour curve (`ColorGrading`)
  where Classic lights cover the camera, see-through effects fogged at their own distance (`TransparentFog`), and an
  energy-normalised phase for the Classic layers' sun and moon scattering (`ClassicPhase`).

## How it renders

The march runs at quarter or half resolution (`Quality`). Each step integrates only its overlap with a layer's
start and end distances, so thin layers and partial boundary steps stay consistent across quality levels. The
march and composite use shader variants with light code only while a point light is uploaded.
Variants that sample authored noise run only while a Classic layer carries it.

With `Temporal` above 0, samples vary between frames and a temporal filter accumulates them. It rejects history
from a different surface depth or depth class (world, distant terrain, sky) and lowers its weight where animated
lighting changes the radiance; settings, map, screen-effect slot, projection and large camera changes discard
it. With `Temporal=0`, samples stay at fixed midpoints so a stationary frame does not shimmer, and the temporal
passes are skipped.
Where no history applies (a newly exposed surface, the screen edge while turning, the frame after a discard), the
filter returns the average of the current samples in the 3×3 low-resolution neighbourhood on the pixel's surface.

The full-resolution composite upsamples the march through depth-validated taps. Ground seen at a grazing angle
is interpolated from the current frame's march where the taps lie on the pixel's plane, the march is linear
across them and no light's sphere meets the ray; thin silhouettes that every tap misses are marched at full
resolution.

Every full-screen pass draws one triangle whose edges lie outside the viewport (NDC −1.5 and 4.5), so it covers every
4x sample of the edge columns; Direct3D 9 maps NDC −1 onto the first column's centres.

**Local lights.** The captured diffuse includes the M2 light's animated intensity (see World lights), so a light
brighter than 1 carries it in the colour. In linear light a colour whose brightest channel is above 1 decodes only
its chromaticity, `(c/peak)^2.2 · peak`, as the modern client scales light colours linearly (a choice; the kit lacks
its CPU packing). Every point light scatters with one Henyey–Greenstein phase normalised to 1 toward the light,
g = `LocalLightPhase`, as the modern fog-light kernel 6227851 reads one g for all local lights.

Fog renders once after the world, including its late geometry and the native sun/moon glare, and before screen
effects and the UI. While it draws, the stock fog is pushed out of range for the world render and restored
afterwards; frames without volumetric fog keep the stock fog, and an unexpected draw failure restores it on the
next frame. The client's glow (`screen + g·blur²`, with `g` from the day/night light) runs afterwards and would
bleach bright fog to white, so fogged pixels are pre-compensated with the live glow amount. God rays use the
remaining display highlight range after the glow and need a scene copy; without one they are omitted.

**Glow and colour grading.** With `ForeverGlow=1` the DLL rewrites the glow byte the client hands the glow composite
(see Glow feed) once per frame, before the fog composite, to `lerp(client byte/255, Forever glow, t)` with `t =
saturate((coverage − 0.5)/0.5)` from the Classic lights' share of the blend, and restores it at the frame end; it never
writes the day/night glow, and writes nothing under the ghost effect or an unexpected effect graph. `GlowCompensation`
uses the byte the composite receives, so a glow above 1, which wraps, is compensated as drawn.
`ColorGrading` passes a copy of the world viewport through the blended 32-entry curve per channel at the end of the
world render, before names and the interface, writing `lerp(scene, curve(scene), ColorGrading·t)`; the ghost view,
the view under water and a view without a grading key are left alone.

**Transparent fog.** Fog drawn once after the world gives every see-through effect the fog of the surface behind
it, where the stock client fogs it by its own distance. With `TransparentFog=1`, above water, the glare and the fog
are drawn at the end of the liquid pass instead, before the see-through models on the camera's side, and every later
M2 batch fog call gets a linear fog with exponent 1: a least-squares line in planar view depth, the depth the client's
M2 shaders fog by, through the volumetric transmittance along 15 rays within 100 yd, with the fitted in-scatter,
point lights included, as the colour where the batch uses its lighting colour (additive, modulate and modulate-2x
batches keep black, white and grey). God rays are added at the end of the world render. Under water, with
`StockFog=0`, a debug view or the sun marker, or when the early composite cannot run, the fog is drawn after the world.

**Water.** The client draws every water and ocean surface, and nothing else, inside one call of its liquid
renderer. Before that call the DLL copies the scene colour and its linear depth, clears the stencil and turns on
stencil writes of 0 and depth writes for the whole call. Each water draw is classified from its liquid type (lake,
river, ocean, indoor) and tags its pixels with that class in the stencil. After the call the DLL simulates the waves
of the classes drawn this frame, copies the water surface's depth, and draws one stencil-tested full-screen pass per
class that replaces the client's water colour. It runs before weather, alpha models and the fog, so the fog is
integrated against the water surface. The waves are a GPU FFT (Tessendorf/Phillips spectra with Forever's tile
parameters, 256² or 128² per tile, normalised to 256² so the wave height does not depend on the resolution) that
yields slopes, slope variance and a persistent foam state per tile. The shading ports Forever's deferred water pass
(Beer–Lambert absorption, Henyey–Greenstein in-scattering, the EnvBRDF reflection weight, foam compositing); its
unknown sun lookup table is replaced by a GGX lobe and its reflection probe by a screen-space march against the
pre-water copies. Frames without queued water skip the pass entirely.

**Ripples.** Units in the water drive Forever's local displacement recurrence, `next = 0.97·edge·(0.5·(L + D + R +
U) − C.g)`, output `(next, C.r)` (R the height, G the previous one, edge `saturate(16·min(u, 1 − u, v, 1 − v))`), on
a G16R16F ping-pong map that follows the camera target by whole texels. Every step holds the footprint of each unit
whose world entity reports the liquid surface crossing its body (see Water contacts) along its path, `R = lerp(R,
level, k)` then `G = lerp(G, R, k)` with `k` fading over the footprint, the level raised with immersion and speed. So a
standing unit leaves the water still, a unit faster than the ripples drags a V, starting and stopping each release one
ring, and wading in splashes once. The shading adds Forever's one-map slope (prepass 7552035) of `lerp(G, R, w)`, `w`
the elapsed fraction of the next step. With `WaterClientSplashes=0`, while ripples can run on shaded water, the DLL
holds the client's `waterRipples` value at 0, so its `splash.blp` and `wake.blp` sprites stop, and puts the value
back when the hold ends.

**Antialiasing.** D3D9 textures cannot be multisampled, so with the game's Multisampling option the depth the
passes read is a copy. `CheckDeviceMultiSampleType` offers the game's sample counts only while `Multisampling=1`
and a copy method exists: `NvAPI_D3D9_StretchRectEx` on native NVIDIA D3D9, or the driver's `RESZ` resolve where
`CheckDeviceFormat` reports it (AMD, Intel). The multisampled depth is created as D24S8, since the game's D24X8 has no
stencil for the water tags and the silhouette split, and a self-test copies two known depths before the game sees
the device; if it fails the device stays single-sampled. The depth is copied before the water pass, after the water
surfaces and before the fog, one sample per pixel; scene copies become resolves, and water is tagged and shaded per
sample. Where a pixel's 3×3 copied depths are not planar, a pass marks its near and far samples in the stencil with
an `oDepth` halfway between them (clamped to the viewport, so the split uses MinZ 0 and MaxZ 1), and the composite
fogs each side at its own depth, so a pixel half tree and half sky blends both fogs by coverage.

**View distance.** Ascension's Extensions.dll detours the far-clip clamp (`0x780770`) and caps maps 0, 1, 530
and 571 at 791.66 yd; the engine allows 1583.33 and instances use it. With `FarClipMax` set, the DLL's calls
to the clamp lift that cap. Terrain loading, the chunk pool, the WDL horizon and the fog follow the far clip;
placed objects keep their own size-class culling (`environmentDetail`), and creatures the server's visibility
distance.

## Classic fog data

`tools/convert_classic_fog.py` converts the WoW Forever fog and lighting kit (build 1.60.1.70009, a folder or its
zip: the decoded `LightData`, `LightDataGlobalVolumeFog`, `LightParams` and `ZoneLightPoint` tables and the
colour-grading LUTs, checked against the kit's `SHA256SUMS`) into `data/fogdata.bin`. The kit has no decrypted `Light`
and `ZoneLight`, so lights and zone lights are carried byte for byte from a previous `fogdata.bin` (the shipped file
keeps those of `9bf495a`, from 69876) or read from the Classic client's `Light.csv` and `ZoneLight.csv` exports. It
checks that the fog table's layout hash is still `24290E20`, which keeps the column numbers valid:

```powershell
python tools/convert_classic_fog.py <kit folder or zip> --placements data/fogdata.bin data/fogdata.bin
```

The converter keeps the fog rows the Classic client selects (flag bit 3) at their layer index (0–2). At run
time the DLL blends the Classic lights around the camera: spheres at full weight inside the falloff start and
linear to the falloff end, the rest to the zone light whose outline holds the camera (fading in over 100 yd,
innermost outline on top), and otherwise to the map's global light. Each light uses the condition slot the
client uses for its stock lighting: the slot a screen effect forces (the ghost effect forces slot 4, death),
otherwise clear weather (slot 0) blended toward storm (slot 2) by the client's storm weight. The two time keys
around the current time are interpolated and layers are paired by their Classic layer index.

Then the Classic transforms apply: density ×0.01, heights relative to the player when flag bit 1 is set, shadow
colour and density for flag bit 0 while the light is below the horizon, `1 + strength·((d − start)/range)^exponent`
over the `MaxDistance` fog range, and scatter intensities up to 50 in linear light. The sun scattering is scaled
by the client's direct-light luminance over Classic's, capped at 1, which keeps the fog consistent with the older
client's darker lighting; this is a compatibility calibration, not a reproduction of the modern renderer.

Classic data applies on maps where any Classic light has fog, wherever Classic lights hold at least half of the
blend weight. A light without fog in the active slot counts with zero density, so the fog thins smoothly into it
and the distance fog hides the far clip there. Other maps use the derived layers.

The phase of the Classic sun and moon scattering peaks at 1 toward the light. The authored intensities rise as g
falls, to about 4π for evenly scattering rows, which suggests that the modern lookup is energy-normalised (inferred;
it is not in the kit); `ClassicPhase=1` multiplies the scattering by `k(g) = (1+g)/(4π(1−g)²)`, capped at g = 0.95.

**Authored noise.** Layers with flag bit 2 modulate their density as the modern global fog kernel does (shader
6674335, variant 002): two octaves sample a tileable 3D noise volume at `frac((p − offset_i)·inverseTile_i)`, their
average passes through a fixed S-curve of contrast 20 about 0.5, and `f = lerp(1, curve, alpha)` scales the density
while the emission moves toward the fade colour by `1 − f`, so storm fog becomes patchier and about half as dense.
Alpha is the share of a layer's blend weight that carries noise, and each octave scrolls as a phase in tiles, so a
blend that changes the tile size scales the pattern about the camera. The modern `t_perlinNoise3D` is not in the kit;
the volume here is a 64³ tileable Perlin noise built at load. The water's reflection fog, the far-clip check and the
lit composites' full-resolution march take a noisy layer at its mean.

`fogdata.bin` format 4 holds the lights, the light params they reference (with `LightParams.Glow`), the fog keys (with
a grading curve index), the layers, the zone lights and their outlines, and the grading curves; the loader rejects any
other format. The layer noise columns are inferred (`FogData::UnpackNoise`): c4 the fade colour, c16–18 and c19–21 the
octaves' scroll directions, c27[i] octave i's tile in hundreds of yards, c28[i] its drift in yd/s; c24 is carried raw
and unused. `LightData.ColorGradingFileDataID` names a 32³ LUT; every LUT placed lights reach applies one curve alike
to R, G and B, so the file keeps 32 codes per LUT and the converter fails otherwise (`DarkerColorGradingFileDataID` is
left out). The curve is interpolated between the nearest keys that set one, wrapping past midnight (inferred). Glow
and grading blend like the fog, on every map with a placed light.

## Forever water data

`tools/convert_forever_water.py` converts the WoW Forever water kit (a folder or its zip: decoded `LiquidType`,
`LiquidTypeXTexture`, `LiquidTypeXFFTTile` and `FFTTile` tables and the foam BLP textures, checked against the kit's
SHA-256 manifest) into `data/waterdata.bin`:

```powershell
python tools/convert_forever_water.py <kit folder or zip> data/waterdata.bin
```

It holds four presets (Forever liquids 1240 generic lake, 1288 generic river, 1250 generic ocean, 1290 WMO interior),
their seven FFT tiles and six foam masks (1024², full mip chain, alpha as 8-bit coverage with a fitted linear
colour). Forever's CPU packing of the table fields into its GPU structure is not in the kit, so the mapping is
inferred from value ranges and shader use: absorption from `-ln(Color[0])` scaled by `Float[0]`, crest and trough
scattering colours from `Color[1]`/`Color[2]` with `Float[2]`/`Float[1]`, scattering weights `Float[3..6]`, depth,
shore and wave foam `Float[7..19]`, sun and reflection roughness `Float[27]`/`Float[29]` and reflection strength
`Float[30]`; tile size, amplitude, wind alignment and multiplier from `FFTTile` fields 0–3 and the foam and oxygen
rates from fields 4–9. The water is tuned by eye against this mapping, not matched to Forever captures.

## How it attaches

- **Loader.** A `version.dll` proxy; all 17 exports forward lazily to the system copy, and its static import
  loads `CoAVolFog.dll` before the client starts.
- **D3D9.** The client resolves `Direct3DCreate9` through the delay-loaded `GetProcAddress` slot `[0xB2ED98]`.
  The DLL points that slot at a filter that returns a wrapped `IDirect3D9`. No d3d9 code is patched, so DXVK or
  other `d3d9.dll` builds keep working underneath.
- **Depth.** The wrapper creates the device without auto depth and binds an `INTZ` texture as the depth-stencil,
  which the client caches as its world depth. Multisampling is kept only as described below, and
  `D3DCREATE_PUREDEVICE` is removed. The shaders read depth through the captured world viewport's range and
  treat anything deeper as beyond the far clip.
  When the game asks for multisampling and a depth copy passes its self-test (see Antialiasing), the device keeps
  the game's multisampled back buffer and automatic depth (D24S8 for D24X8) instead, and `INTZ` becomes a copy.
- **Hooks.** Five 5-byte call displacements: the world render call (`0x4FB03D`, stock-fog override and restore),
  after the opaque M2 pass (`0x4F911D`, captures camera inputs), the liquid surface pass (`0x4F9170`, forces
  depth writes), world-name text (`0x7E5818`, suppresses depth writes), and before the frame effects
  (`0x4F9281`, draws the fog over the completed world). The original bytes are checked first; on any mismatch
  nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when `FarClipMax` is
  set at start-up, independently of the fog hooks.
- **Transparent fog hooks.** The M2 batch fog call (`0x81FD15`) and the glare pass call (`0x4F9213`), installed
  after the fog hooks whatever `TransparentFog` says, and neither when a byte run checked under M2 batch fog, M2 fog
  setter or Native glare differs. The M2 thunk forwards to the client while nothing is armed and otherwise hands the
  cdecl argument block to the DLL (`pushad; lea eax, [esp+0x24]`); an exception in it turns transparent fog off.
- **Water hooks.** Installed after the fog hooks and independently of them: the water pass call (`0x790AA2`) and
  the `Render` slots of the two water material vtables (`0xA5954C`, `0xA59580`, read-only data patched under
  `VirtualProtect`). The call site, both slots and the layout bytes the classification reads are checked first; on
  any mismatch none are patched and the fog is unaffected. `waterdata.bin` is loaded only once they are installed. A
  water exception restores the device state, turns water off for the session and leaves the fog running.
- **Glow and grading.** No patch: the glow override runs before the fog composite and the grading in the world
  render's frame-end hook, each only while its guarded bytes match. An exception turns that one off for the session,
  or both when it comes from their shared inputs, and `GlowCompensation` then uses the clamped day/night glow.
- **State.** Every state the passes touch is captured with a recorded state block and restored, plus render
  targets, depth and stream 0 (whose offset state blocks drop). The client's shader-constant cache stays valid.
- **Overlay.** When a fog device is created, the device window's procedure is chained so the settings window
  sees input first, and the wrapper's `Present` draws the window over the finished frame.

## Engine inputs

All addresses are static in the 12340 image (base `0x400000`, timestamp `0x4C2452FE`, both checked at start-up);
Gx is the graphics device `[0xC5DF88]`.

| Input | Address |
|---|---|
| View, projection | Gx `+0x1B00 + 64·[+0x1AF8]`, `+0xF88`; copies at `0xADF5E8`, `0xADF628` |
| World viewport depth range (MinZ, MaxZ) | Gx `+0xF80` |
| Camera position / look-at target | `0xCD8F5C`, `0xCD8F68` |
| Current map | `0xAB63BC` |
| Day fraction | `0xD38B04` |
| Stock fog groups: colour, start, end (read by the render callbacks) | `0xD38B8C`–`0xD38B94`, `0xD38BA0`–`0xD38BA8` |
| Zone fog distance (light float band 0) | `0xD38C1C` |
| Light colours: ambient, direct, sun | `0xD38BD4`, `0xD38BD8`, `0xD38BF8` |
| Visible sun / moon sprite, sky centre | `0xD38E28`, `0xD38E48`, `0xD38B18` (day window `[0xA41CA4, 0xA41CA0]`) |
| Camera in liquid | `0xCD8794` |
| Storm weight (0..1) blending the clear and storm light params | `0xD38B88` |
| Light params slot forced by the current screen effect (−1 = none) | `0xD38B58` |
| Glow: `ffx` CVar, current effect, glow effect, glow amount | `0xD45774`, `0xD45780`, `0xB74364`, `0xD38C2C` |
| Ghost (death) screen effect | `0xB74368` |
| Glow byte: pass list count and array, pass [2], its colour's alpha | `+0x0C`/`+0x10`, `+0x20`/`+0x24`; `+8`; `+0x33` |
| Far clip | From the projection; `[[0xB7436C] + 0xB14]` as a fallback |
| World M2 scene (point lights) | `[0xCD754C]` |
| Camera WMO instance, group ids, group count, interior fog blend | `0xCD87A4`, `0xCDB0DC`, `0xCDB0D8`, `0xD38B9C` |
| Sky colours (bands 2–7: top, middle, band 1, band 2, smog, fog) | `0xD38BE0`–`0xD38BF4` |
| Water colours: ocean close/far, river close/far | `0xD38C0C`, `0xD38C10`, `0xD38C14`, `0xD38C18` |
| Liquid renderer, transparent (water) draws queued this frame | `[0xCD8610]`, `[[0xCD8610] + 0x14]` |
| Liquid settings bank: count, entries | `0xD43B18`, `0xD43B1C` |
| `LiquidType` rows: maximum id, minimum id, row pointers | `0xAD4070`, `0xAD4074`, `0xAD4084` |
| Object manager, visible list: link offset, first object | `[[0xC79CE0] + 0x2ED0]`, `+0xA4`, `+0xAC` |
| Object: descriptors (type mask `+8`), GUID, list link, world entity | `+0x08`, `+0x30`, `+0x38`, `+0xB8` |
| Unit: movement block (pointer, embedded block), transport GUID, raw position | `+0xD8`, `+0x788`, `+0x790`, `+0x798` |
| Unit movement flags (swimming), collision radius and height | `+0x7CC`, `+0x850`, `+0x854` |
| World entity: world position, liquid flags, liquid surface | `+0x6C`, `+0x7C`, `+0x80` |
| Client `waterRipples` value | `0xADF7F0` |

Engine notes behind the code:

- Call sites. `0x4FB03D` (`E8 5E DE FF FF`) calls the world render `0x4F8EA0`, a thiscall on the world frame
  with no stack arguments; the thunk keeps ECX across the frame-begin hook. `0x4F911D` (`E8 8E AB 32 00`) calls
  the opaque M2 pass `0x823CB0`, a thiscall with one stack argument (`ret 4`). `0x4F9170` (`E8 AB 5E 28 00`)
  calls the liquid surface pass `0x77F020` (no arguments, outside liquid only). `0x4F9281` (`E8 8A 7D 3C 00`)
  calls FFX end `0x8C1010` (no arguments); the thunk renders the fog, then tail-jumps to it.
- Loader slot. On disk `[0xB2ED98]` holds the delay-load thunk `0x41C654`, which pushes the slot (`0x41C65F`)
  before resolving it; `0x41C673` jumps through it. The hooks install only while the slot still holds the thunk.
- Matrices. `0x6821A4` reads the camera-relative view matrix at Gx `+0x1B00 + 64·[+0x1AF8]` and then the projection at
  `+0xF88`. The engine builds its projection with `0x6BF370` (OpenGL depth range, w = view z); the D3D backend
  converts it.
- Depth. The world viewport's MaxZ is `[0xADEEE4]` = 0.94 (loaded at `0x4F9019`, passed to GxXformSetViewport
  at `0x4F905A`) and uploaded as `D3DVIEWPORT9::MaxZ` by the D3D9 backend (`0x6A9ACC`). The Gx viewport is
  stored by `0x681890` and uploaded lazily on the next draw or clear (`0x6A99E0`), so after the opaque pass the
  device can still hold the sky's `[0.999, 1]`; the capture reads MinZ/MaxZ from the Gx viewport, which the sky
  and WDL passes restore (`0x7F0CB3`, `0x796466`). The distant WDL terrain draws into `[0.998, 0.999]` with its
  own viewport and projection (`0x7960EB`); the sky viewport is set at `0x7F0A79` and the sky writes no depth,
  so depth at or above max(deepest world depth, 0.99903) is sky.
- Multisampling. The Video options' multisample list (`0x54F1B0`) is built once per process (it returns while
  `[0xBEA744]` is set) by `0x68A170`, which drops each count of `0xAD8CE8` = {0, 2, 4, …, 16} that the wrapped
  `CheckDeviceMultiSampleType` refuses (`0x68A2B9`, `0x68A2DE`). `gxMultisample` (`0xCABCF8`) asks for `gxRestart`,
  not established to reset rather than recreate the device, so `CreateDevice` (`0x68F4DE`) and `Reset` both choose
  between the copied and the bound `INTZ` depth. The present-parameter builder `0x68E250` takes the depth format from
  `0xA2E4A8` = {…, D16, D24X8, D24S8, D32} at `[0xCABCE8]`, where `gxDepthBits` "24" stores D24X8 (`0x7692D0`).
- Depth copy. `NvAPI_D3D9_StretchRectEx` copies a multisampled D24S8 depth into a registered `INTZ` texture, one
  sample per pixel. The NVAPI functions come from `nvapi_QueryInterface` with the IDs of NVIDIA's public headers:
  `0x0150E828` Initialize, `0xA064BDFC` RegisterResource, `0xBB2B17AA` UnregisterResource, `0x22DE03AA` StretchRectEx.
- Liquid depth. While writes are forced on, the wrapper records the client's own `D3DRS_ZWRITEENABLE` requests
  and re-applies the last one afterwards, so the client's render-state cache stays accurate.
- World-name text. `0x7E5818` (`E8 23 76 ED FF`) calls `0x6BCE40`, a cdecl wrapper that takes the font batch
  `[0xD380A8]` and tail-jumps to `0x6C53A0`. The batch is created by `0x6BF160(1, 1)` at `0x7E6511`; its bit 0
  at `+8` selects world rendering (`0x6C5564`), which requests depth testing (Gx state 13, `0x6C5591`) and depth
  writes (Gx state 15, `0x6C55BE`, dispatched as `D3DRS_ZWRITEENABLE` at `0x6A8FC7`). Suppressing the writes
  keeps glyph quads out of the depth the fog is integrated against, where they would end fog rays at the text,
  while keeping the draw and its depth test. The later name/icon path at `0x4FB042` is left in place.
- Native glare. `0x4F9213` (`E8 58 76 2F 00`) calls the sun/moon glare pass `0x7F0870`, which is not patched.
  Its draw `0x9AC400` adds the `sunGlare`/`moonGlare` quads (`D3DBLEND_SRCALPHA`, `D3DBLEND_ONE`) at depth
  `[0.9990234375, 1]`, and its update `0x9AC3C0` runs a GPU occlusion query against the current world depth
  (`0x9ABE00`). World geometry `0x7984A0` also runs after the opaque hook (`0x4F9154` calls `0x77F010`, which
  jumps to it). Drawing the fog once at `0x4F9281` sees the completed depth and keeps the glare and its query
  order unchanged.
  With `TransparentFog=1` the liquid thunk calls the glare pass instead of its own call, after checking `0x4F9210`
  (`83 C4 14`), `0x4F9218` (`E8 63 C3 2E 00`, world text) and the pass after its entry, which Extensions.dll detours:
  `0x7F0877 74 3C D9 05 48 8B D3 00 51 B9 A8 8E D3 00`, the moon tail jump `0x7F08AB B9 58 8F D3 00 E9 4B BB 1B 00`
  and `ret` at `0x7F08B5`. The entry `0x7F0870 83 3D CC 8C D3 00 00` is only logged (see Extensions.dll). The call
  at `0x4F9213` itself is patched (see Transparent fog hooks).
- World render order. In `0x4F8EA0` the map render draws the sky, the WDL, terrain and the WMOs with their alpha
  batches, then M2 pass 0 (`0x4F911D`). Above water come M2 pass 2 (`0x4F9167 6A 02`), the liquid (`0x4F9170`),
  weather, the barrier effect and M2 pass 1 (`0x4F918C 6A 01`), under water pass 1, weather, the liquid (`0x4F91B0`)
  and pass 2; then lightning, missile arcs, the glare (`0x4F9213`), world text (`0x4F9218`) and FFX end (`0x4F9281`).
  `0x821A20` puts see-through batches (blend mode above 1 or alpha below 0.99999, `0x821F47`) in pass 1 on the
  camera's side of the model's liquid plane and in pass 2 beyond it. The liquid pass sets the Gx fog, draws the water
  (`0x790AA2`) and tail-jumps to the ripples `0x79D5E0`.
- M2 batch fog. `0x81FB10` picks the fog colour by blend mode through `0xA45390` = {1, 1, 1, 2, 2, 3, 4, 0}
  (`0x81FB7B 8B 04 8D 90 53 A4 00`) and the jump table `0x81FE7C` (`0x81FBA3 FF 24 85 7C FE 81 00` → `0x81FBAA`,
  `0x81FCAE`, `0x81FCC2`, `0x81FCD8`): mode 1 is the lighting fog colour with alpha 0xFF (`0x81FC5D C6 45 FB FF`),
  modes 2, 3 and 4 black, white and grey 0x80 with alpha 0 (`0x81FB20 33 DB`; `88 5D FB` at `0x81FCB7`, `0x81FCCD`
  and `0x81FCE3`), mode 0 unfogged. From `0x81FCEE` to `0x81FD14` it pushes the exponent (`[eax+0xB4]` of the
  lighting `[esi+0x70]`), the colour's address (`lea ecx, [ebp−4]`), the end (`+0xAC`) and the start (`+0xA8`),
  calls the setter at `0x81FD15` (`E8 F6 34 05 00` → `0x873210`), then `0x873390(1)`, and pops the arguments
  (`0x81FD1A 6A 01 E8 6F 36 05 00 83 C4 14`).
- M2 fog setter. `0x873210` (`55 8B EC 83 EC 14 83 3D 20 30 D4 00 00`) writes the colour's bytes +2, +1 and +0 over
  255 (`0x873225 8B 75 14 0F B6 46 02`, `0x873242 0F B6 4E 01`, `0x873254 0F B6 16`) to pixel c2 and builds vertex
  `c30 = (−1/(end − start), end/(end − start), exponent, 0)` from the start and end (`0x873263 D9 45 0C D9 45 08 D8
  E9`) and the exponent (`0x87328C D9 45 10`). The client's M2 and WMO shaders fog by planar view depth, `oFog =
  min(pow(max(c30.x·z + c30.y, 0), c30.z), 1)`, blending `lerp(c2, colour, fog)`. c2.w is the alpha-test reference,
  so the DLL changes the colour only through the setter's argument; each WMO render resets its fog cache.
- Weather fog. `0x78AE20` draws weather either with its own grey fog and a vertex fade (`0x78A7C7`) or with the Gx
  fog set to 0x80808080 from 70 yd (`0x7893B5`) to 75 yd (`0x7893C6`); no weather draw reads the zone fog.
- Extensions.dll. Ascension's Extensions.dll (TimeDateStamp `0x6ABAD5C2`, read statically) detours the glare pass
  `0x7F0870` (7 bytes; it then draws a second moon), the LightParams getter `0x7EB180` (it multiplies the glow by its
  `ambientGlow` CVar, so the glow can exceed 1), the light blend `0x7F3230` and M2 and DayNight functions. None
  touches `0x4F9170`, `0x81FD15`, `0x873210`, `0x4F9213`, `0x4F9281`, `0x4F8770`, `0xD38C2C` or the FFX code, or
  overlaps a checked byte run; at `LogLevel=2` the log says what `0x7F0870` begins with.
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`)
  and the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`);
  `0x4F8770` feeds it the DayNight glow (`0xD38C2C`) as the additive weight of `lerp(screen, blur, other) +
  g·blur²`, where `other` is the screen effect's own blend amount, which the fog leaves as is.
  The ghost effect is `[0xB74368]` (`FFXDeath`, vtable `0xA418D8`, stored at `0x7EA274` by its constructor).
- Glow feed. The world frame callback `0x4FAF90` rewrites `0xD38C2C` from the light blend every world frame before
  the world render. There `0x4F8F3D`, the only call of `0x4F8770`, feeds the glow effect while it is current: it
  turns the glow into a byte with the +512.0 trick (`0x4F878E`–`0x4F87AC`, floor(255·g) mod 256, so 1.1 gives 24) and
  passes {underwater, glow byte, blur byte} with id 3 to SetParam (`0x4F883C 8B 40 10 52 6A 03 FF D0`), slot
  `0xA941D8` of the glow vtable `0xA941C8` (stored at `0x8BFE98 C7 03 C8 41 A9 00`). SetParam `0x8BFDE0` (`55 8B EC 8B
  45 0C 8B 10 89 51 2C 8A 50 04 8A 40 08`) stores the D3DCOLOR {B = G = R = blur, A = glow} at `+0x30` of pass [2] in
  the clear-view list at `+0x08` (`0x8BFE14 8B 41 10 8B 48 08`, `0x8BFE21 89 51 30`) and the underwater list at
  `+0x1C` (`0x8BFDF2 8B 71 24 8B 76 08`, `0x8BFE08 89 7E 30`), and returns with `ret 8` (`0x8BFE26 C2 08 00`). A list
  is {capacity, count, array}; pass [2] is the composite (vtable `0xA94294`, stored at `0x8C2206 C7 06 94 42 A9 00`),
  which hands that colour to its draw (`0x8C28A1 83 C6 30`). `0x4F8F3D` precedes both liquid pass calls, so when the
  liquid pass returns the byte holds this frame's value. The ghost composite `0x7E87B0` reads `0xD38C2C` itself. The
  glow override requires these byte runs and `0x4F9286`, and each frame an effect graph with these vtables.
- FFXGlow. The shader loader `0x684970` accepts only BLS version `0x10003`, so Ascension's ps_3_0 `FFXGlow` falls
  back to ps_2_0, `lerp(screen, blur, v0.z) + blur²·v0.w` with the pass colour as `v0`. Every FFX pass end rebinds
  the default target (`0x8C15A7 8B 01 8B 50 5C 6A 00 6A 00 6A 00 FF D2`), so the composite draws to the back buffer.
- After the frame effects. After FFX end the world render only runs `0x747AE0` (`A1 68 13 CA 00`, clearing flag
  `0x1000` along `[0xCA1368]`) and returns (`0x4F9286 E8 55 E8 24 00 5F 5E 8B E5 5D C3`); names and icons follow
  (`0x4FB042 E8 F9 A0 2E 00`). Early returns skip FFX end and the world-done hook, and with them the grading, which
  requires `0x4F9286`, `0x747AE0`, `0x4FB042` and `0x8C15A7`. None of these guarded bytes is one the hooks patch.
- Light params slots. `0x7EB180` returns a light's `LightParams` for a slot (`Light` record `+0x1C + 4·slot`).
  For each light, `0x7EE510` takes slot 0 and, while the storm weight `[0xD38B88]` is above zero, blends in
  slot 2 by it (`0x7EC220`). The DayNight update sets that weight to `min(1, 4·[0xD38B4C])` (`0x7F3995`); the
  weather update writes `[0xD38B4C]` (`0x784A01`). When `[0xD38B58]` holds a slot, the light blend `0x7F3230`
  uses it instead (`0x7F346F`). `0x7ECEC0` stores it from the current `ScreenEffect` row (`+0x1C`, called at
  `0x4F712D`; values above 7 become −1) and `0x7ECEE0` clears it. In CoA's `ScreenEffect.dbc` the ghost effect
  (ID 1) and the other death-style effects use slot 4; a few event effects force slots 0, 1, 2, 3 or 5.
- Current map. The world-transfer loading screen `0x40AD50` (`LoadingScreen.cpp`) stores its map argument in
  `[0xAB63BC]` (`0x40AD56`, `A3 BC 63 AB 00`); `0x40AE0A` and `0x40AECF` set it to −1. In-game logs show map 0
  in Elwynn Forest.
- Far clip. The clamp `0x780770` is cdecl `float(float farclip, int mapId)`, result in ST0, caller pops; it
  bounds the value to `[0xA3E708]` (183.33) .. `[0xA3E710]` (1583.33). Its calls at `0x780810` (in the
  `farclip` CVar setter `0x780800`, also reached from Extensions.dll on zone changes) and `0x781444` (map load
  `0x781430`) are rare, so the hook re-reads the INI on every call.
- Present. The D3D9 backend presents through the device at Gx `+0x397C` (`0x6A3584`, `0x6A7724`), which is the
  wrapper, so every frame passes through its `Present`.
- Input. `GxWindowClassD3d` and `GxWindowClassD3d9Ex` share the window procedure `0x6A0360`, which hands keyboard
  and mouse messages to the Gx input callback. The message pump's pre-filter `0x86CB00` acts only on the client's
  own dialog windows, and DirectInput only enumerates game controllers (`0x870725`), so keyboard and mouse
  reach the chained window procedure. The client draws its own D3D cursor (`0x6A009C`, `0x6A058E`), so ImGui
  leaves the cursor alone.
- World lights. `0x4F90EC` loads the world M2 scene `[0xCD754C]` and passes it to the opaque M2 pass. The
  point-light registration `0x834C70` allocates a 64-by-64 pointer table at scene `+0x24`, hashes the light's
  world X/Y in 20-yard cells and inserts it into a null-terminated list. A light holds its scene at `+0`, type at
  `+8` (1 = point), world position at `+0xC`, diffuse RGB at `+0x3C`, constant/linear/quadratic attenuation at
  `+0x54`, enabled state at `+0x60`, the address of its preceding link at `+0x64` and the next light at
  `+0x68`; removal `0x834AB0` and the setters `0x835690`/`0x8356F0` maintain the links. Animated M2 positions
  are already transformed to world space before the setter (`0x828AF4`). The diffuse already includes the
  animated intensity and model scale (`0x8305B9`–`0x8305FE`) and is uploaded unchanged (`0x835527`,
  `0x6A462F`). The attenuation is uploaded as `D3DLIGHT9::Attenuation0/1/2` (`0x835539`, `0x6A46AC`) with the
  fixed range 10000 (`[0xA2F95C]`), so the fog's 200-yd bound and eight-light selection are its own policy. No
  native spotlight cone has been established.
  Registration skips a disabled light (`0x834C79`, `83 7E 60 00`) and the enable setter `0x8356F0` re-registers or
  unlinks it (`0x835716`, `0x835720`–`0x835740`), so the table holds only enabled lights.
- Interiors. `0x795D40` stores the camera's WMO instance at `[0xCD87A4]`, its active group IDs at `[0xCDB0DC]`
  and their count at `[0xCDB0D8]`. The instance's root is at `+0xF4`; the root's loaded flag is `+0x1E0`, its
  group count `+0x1F4` (set at `0x7D7F18`, bounded at `0x7AE467`) and inline group pointers start at `+0x1F8`;
  group `+0x198` bit 0 means loaded (`0x7AEA80`). The client's camera-fog query at `0x7A11C7` accepts an interior
  group when its `+0x30` flags have neither bit in `0x48` set. Day/night fog update `0x7F17B5` writes the
  transition weight to `[0xD38B9C]` at `0x7F1931` and from `0x7F1955` blends the outdoor fog toward the WMO fog,
  so the stock fog inputs already hold the blended colour, start and end.
- Capture guards. Before reading the light and interior inputs, the module checks the image timestamp and base
  and the instruction bytes at `0x4F90EC`, `0x834C8D`, `0x834D3C`, `0x835539`, `0x7A11B0`, `0x7A11C7`,
  `0x7AEA83` and `0x7F1931`. Reads are SEH-guarded with pointer, span and count limits (512 nodes per bucket,
  8192 in total, links checked against their owner); no client references survive the capture, and invalid
  inputs give an empty result. `LocalLights=0` skips the point-light walk.
- Water pass. `0x77F020` jumps to `0x790A80`, which calls the liquid renderer `0x8A2240` at `0x790AA2`
  (`E8 99 17 11 00`) with ECX = `[0xCD8610]` (`0x790A91`), the camera `0xCD8F5C` and pass 1 (`0x790A9B`, `6A 01`);
  it is a thiscall that returns with `ret 8` (`0x8A2376`). The same `0x77F020` runs from `0x4F9170` (camera above
  liquid) and `0x4F91B0` (below). Liquids are queued into one of two 16-byte buckets by their `LiquidMaterial`
  flag (`0x8A20D6`, settings `+0x360`): pass 1 holds every water and ocean material, pass 0 the magma and slime,
  drawn earlier in the map render (`0x79ACF1`). `0x8A2240` keeps the renderer in EBX (`0x8A224C`), indexes the
  bucket by `pass·16` (`0x8A229C`), loads its count from `+4` (`0x8A229F`), skips its draw loop when that is 0
  (`0x8A22B2`) and empties the bucket every frame (`0x8A2314`–`0x8A2319`). The water ripples `0x79D5E0` run after
  the call and are not part of the pass. Each instance is drawn by its material's vtable slot 2 through `call edx`
  (`0x8A22C7`–`0x8A22EA`), a thiscall with seven stack arguments, the last the `Liquid::CMaterialSettings*`:
  `CMaterialWater::Render` `0x8A5590` (vtable `0xA59544`, slot `0xA5954C`) and `CMaterialWaterNoSpec::Render`
  `0x8A5900` (vtable `0xA59578`, slot `0xA59580`), both `ret 0x1C` (`0x8A58FB`, `0x8A5C6B`). Stock water is
  alpha-blended without depth writes on D3D9 hardware, and the client never uses the stencil: its D3D9 backends set
  no stencil state and its clear (`0x6A74B0`) passes only the target and depth flags.
- Water classification. The settings bank `0x8A28F0` bounds a liquid type id by `[0xD43B18]` (`0x8A28F8`) and
  keeps one settings object per id in `[0xD43B1C]` (`0x8A2900`, `0x8A2952`), so the draw's settings map back to its
  id. The settings builder `0x8A27C0` copies `LiquidType.Texture[k]` (`+0x3C`, `0x8A280C`) into the settings at
  `+0x80·k` (`0x8A2809`, `0x8A282E`). `LiquidType` rows are looked up as at `0x793DF7`–`0x793E12`; the material id
  is `+0x38` (`0x8A1FE8`) and the sound bank `+0x0C` (0 water, 1 ocean; from the 45-field `LiquidType.dbc` layout).
  A draw is water when its material is 1 and its first texture lies in a `\river\` or `\ocean\` folder, which
  excludes CoA's liquid 181 (orange slime drawn with the water material); liquid 17 (WMO water, interior) is
  indoor, sound bank 1 ocean, liquid 9 (fast water) river, and the rest lake.
- Water contacts. `0x4D77A9` (`mov ecx, [0xC79CE0]; mov [ecx+0x2ED0], eax`) stores the object manager; `[0xC79CE0]`
  is null before login. The visible list starts at `[mgr + 0xAC]` (`0x4D4B44`, `add eax, 0xA8; mov eax, [eax+4]`) and
  steps to `[obj + [mgr + 0xA4] + 4]` (`0x4D4B80`, `add eax, 0xA4; mov eax, [eax]; add eax, ebx; mov ebx, [eax+4]`),
  link offset 0x38 (`0x4D6193 BA 38 00 00 00`, `0x4D6233 89 96 A4 00 00 00`); a set low bit ends it, and each link's
  `[obj + 0x38]` holds the previous link's address, which the walk checks. The GUID is at `+0x30` (`0x4D4B6D 8B 4B 34
  8B 43 30`), the type mask at `[[obj + 8] + 8]`, unit bit 0x8 (`0x4D4DF1`, `test [ecx+8], edx`). A unit's `+0xD8`
  points at its movement block `+0x788` (`0x73F67A`, `lea eax, [esi+0x788]; mov [esi+0xD8], eax`): raw position
  `+0x10` (`0x6E6F13`, `mov edx, [ecx+0x10]`), transport-local while the transport GUID `+0x8` is set (`0x6E6F70`,
  `mov eax, [ecx+0x790]`), movement flags `+0x44`, swimming 0x200000 (`0x730DA2 8B 40 44 A9 00 00 20 00`), collision
  radius and height `+0xC8`/`+0xCC` (`0x6E95AF D9 9E C8 00 00 00`, `0x6E95CE D9 9E CC 00 00 00`). The world entity is
  at `+0xB8` (`0x7438B0 89 86 B8 00 00 00`), its world position at `+0x6C` (`0x7803BD`, `mov [ecx+0x6C], edi`; the
  feet in world space, inferred, used only for passengers). The entity's liquid refresh `0x7A1BC0` sets 0x20 with the
  surface at `+0x80` (read at `0x77F1EA`, `test byte [eax+0x7C], 0x20; jz; fld [eax+0x80]`) and 0x100 when the surface
  lies above the feet in a liquid with `LiquidType` flag bit 0 (read at `0x77F230`, `mov ecx, [eax+0x7C]; …; shr ecx,
  8; and ecx, 1`). The client's ripple emitter `0x71CBA0` emits only while surface − z is below max(1, 2·height)
  (`0x71CC28`, `fld [esi+0x854]; fadd st, st; fld1`), at strength 1 at half that depth to 0.5 at it, and the swim
  update `0x730D10` splashes when the depth crosses 0.4·height (`0x730DED`) while not swimming. The walk calls no
  client code, is SEH-guarded with pointer and range checks, rejects a frame with a broken back link and stops at 4096
  objects.
- Client ripple sprites. `waterRipples` is a console command registered at `0x7813A4` (`68 90 F6 77 00 68 00 E8 A3
  00`); its handler, the only writer, reads its argument into `0xADF7F0` (`0x77F696 68 F0 F7 AD 00 68 9C 28 9E 00`).
  The only reader, `0x79D463` (`83 3D F0 F7 AD 00 00 74 61`), makes no new sprite while it is 0; it is reached only
  through `0x77F434` (`E8 27 E0 01 00`) from the unit emitter `0x71CF16` (`E8 E5 24 06 00`). The update `0x79D5E0`
  expires each sprite (`0x79D648 D9 41 28 A1 58 FB AD 00 D8 1D A4 76 CD 00`, at most 0.7·e s) without reading it.
  Extensions.dll holds neither the address nor the emitter's functions (a byte search).
- Water colours. The DayNight light record at `0xD38BD4` (`0x7F3574`) stores the sky bands 2–7 at `0xD38BE0`–
  `0xD38BF4` (`0x7EC03C`–`0x7EC09C`) and the ocean and river close/far colours at `0xD38C0C`–`0xD38C18`
  (`0x7EC11D`–`0x7EC152`); the client bakes the water colours into its per-frame depth ramps. The water is lit by
  the direct light (band 0, `0xD38BD8`), which `0x7EE756` copies to the world's direct light; band 9
  (`0xD38BF8`) only colours the sun and moon sprites (`0x7F36F6`).
- Not used. `0xD38B98` is fog group 0's exponent, which Extensions.dll overrides (group 0's end is `0xD38B94`), and
  `0xD38C9C` is a near-constant model lighting direction, not the visible sun, so the fog's light direction
  follows the sprite positions.

## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. On laptops whose F-keys send media
keys by default, hold Fn (the log names the key that arrived). The window edits every setting below except
`Enable`, `EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change. **Save** writes the
settings changed in the window back to `CoAVolFog.ini`, keeping the comments and every other line; settings not
changed in the window keep what the file holds. **Revert** reloads the file, and a hand edit of the file also
replaces unsaved changes. The window shows whether the fog drew in the last frame, or why it did not.
It also shows whether the game's multisampling is kept, or why it is off (`Multisampling` applies later, see
Settings). It can be moved and resized, and keeps its place and size for the session.

While the window is open, clicks and the wheel go to it only while the cursor is over it or a drag started on
it, and keys only while one of its text fields is active (Ctrl+click on a slider). The hotkey never reaches the
client; mouse moves and key-ups always do, so no game key sticks. The window is drawn in the wrapper's
`Present` with the device state saved and restored, and releases its resources before every `Reset`. An
exception in the overlay turns it off for the session and is logged; `Overlay=0` leaves the game window
untouched from the next start.

## Build

Requirements: Visual Studio 2022 (C++ x86), the Windows 10/11 SDK (`fxc.exe`), CMake 3.20+. The first configure
downloads Dear ImGui v1.92.9b (FetchContent, pinned by SHA-256) into the build directory.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Outputs in `build/Release`: `version.dll`, `CoAVolFog.dll` and `vfog_harness.exe`, with `CoAVolFog.ini`,
`fogdata.bin` and `waterdata.bin` copied beside them.

## Test

```powershell
ctest --test-dir build -C Release --output-on-failure
```

This runs the comment check and `vfog_harness`, which creates a real D3D9 device through the wrapper with the
client's flags (`0x52`, auto depth D24X8), renders a Z-up test scene with the client's projection convention,
and runs the fog passes through the same entry the hook uses. Its checks, in `tests/`, cover the device wrapper
and state restoration, depth and sky handling, Classic light blending and slot selection, the march against CPU
integrals at every quality, temporal filtering and upsampling, point lights and interiors, the text and liquid
depth overrides, fog-data validation, the GPU timer and depth probe, the settings window and INI saving, and
`Reset`. The water suites check the water data and its loader, the FFT against a double-precision reference, the
liquid classification, the water pass driven through the hook entry points (state restoration, stencil tagging,
optics against a CPU reference, fault recovery) and the water settings. They do not establish in-game appearance or
performance. It writes `before.png`, `after.png`,
`overlay.png` and the debug views to `build/harness-out`.

Further suites check the authored noise and `ClassicPhase`, the transparent fog, glow and grading through the hooks,
a 4x device (`SKIP` without a copy method), and the ripples.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration.
`--classic-phase 1` renders it with `ClassicPhase=1`, and `--storm 1` in a full storm (noisy layers at their mean).

`vfog_harness --scene ripples <dir> --water-data data/waterdata.bin` renders standing, running, walking, swimming
and wading-in units at `WaterRipples=0.5`, each as the shaded frame, its normals and a calm reference.

`vfog_harness --scene performance` times the fog passes at 1920×1080 at each quality on one street: derived
layers with no point lights, eight flood lights or eight street lamps, Classic layers at the harbour with and
without the lamps (skipped if `fogdata.bin` beside `CoAVolFog.dll` does not resolve the harbour), and the shipped
`LogLevel=1`. It prints `quality,case,point_lights,median_ms,p95_ms` of GPU time; this is a controlled renderer
cost, not a game frame-rate test.
It also times the harbour in a full storm and the colour grading pass; `--samples 4` creates a 4x device.

## Install

Close the client, then copy `version.dll`, `CoAVolFog.dll`, `CoAVolFog.ini`, `fogdata.bin` and `waterdata.bin`
next to `Ascension.exe`.
Remove `version.dll` and `CoAVolFog.dll` to uninstall; nothing else in the client is changed. The DLL
writes `CoAVolFog.log` next to itself.

## Log

At `LogLevel=1` the log holds a frame summary on the first frame, whenever the viewport or far clip changes, and
every 60 s (`LogLevel=2` adds one every 600 frames). Its first line ends with the median GPU time of the fog
passes since the previous summary, for example
`frame 3600: viewport 0,0 1920x1080 ... maxdist 5000, fog gpu 2.31 ms (median of 3542 frames, 0 skipped)`.
The time covers the fog passes alone, god rays included, without the client's own rendering or any CPU work;
compare it with the frame time to see the fog's share of a GPU-bound frame. `skipped` counts frames whose timing
was lost, and `fog gpu no samples` means none finished in the interval. Without timestamp queries the log says
so once and the summaries omit the time; at `LogLevel=0` no queries are issued.
With Classic data the summary adds the resolved glow, three points of the grading curve and each noisy layer.

Every 60 s the water adds `water gpu 1.24 ms (median of 3500 frames, 0 skipped), classes lake+ocean, waves 256
(7 tiles)`: its GPU time without the client's own water draws. `water:` lines name each liquid type the first
time it is classified; idle states (no water in view, camera under water) are logged once and repeats only at
`LogLevel=2`. Each settled change from the settings window, an INI reload or Revert is logged as one line, for
example `settings: WaterFoam 1 -> 1.5, WaterWind 2 -> 4`.
The water line goes on with the ripples (`ripples 512 at 0.125 yd, 30 Hz, up to 3 contacts, 0 steps dropped`, or
`ripples idle`/`off`), and `water: the client's splash and wake sprites are hidden` or `shown again` reports the hold.

The depth probe logs raw depth, distance and fog opacity at 25 points on frame 60, then every 60 s up to five
times (every 30 s without limit at `LogLevel=2`); its rows are read back on a later frame. The first reason a
fog draw is skipped is logged as `fog skipped: <reason>`; a camera under water is logged once as `fog idle`.

Each device creation and `Reset` logs the adapter, the sample count requested and used, the depth format bound,
and the depth copy method with its self-test result or why multisampling is off. Guarded client bytes that differ
are logged with the feature they leave off.

## Settings

`CoAVolFog.ini` is re-read within a second while the game runs (except `Enable` and `EngineHooks`); its comments
describe every key. In the game, `Ctrl+F7` opens the same settings in a window (see In-game settings).

| Key | Default | Meaning |
|---|---|---|
| `Enable` | 1 | Master switch (restart) |
| `EngineHooks` | 1 | Install the hooks; 0 leaves the client unmodified (restart) |
| `Overlay` | 1 | In-game settings window; 0 hides it at once, and from the next start leaves the game window alone |
| `OverlayKey` | Ctrl+F7 | Key that shows and hides the window, with optional `Ctrl+`, `Shift+`, `Alt+` |
| `Quality` | 2 | 1 quarter resolution / 16 steps, 2 half / 24, 3 half / 32 |
| `Density`, `Haze`, `GroundFog`, `FarFog` | 1, 1, 0.6, 1 | Density multipliers |
| `NoiseAmount` | 0.15 | World-space density variation, 0..1; 0 disables it |
| `NoiseScale` | 0.025 | Inverse feature size of the variation |
| `NoiseWindSpeed` | 0.5 | Drift of the variation along world +X in yd/s |
| `ClassicNoise` | 1 | The modern client's authored noise on the Classic layers that carry it (mostly storms); 0 = off |
| `StockFog` | 1 | 1 replaces the stock fog with the distance fog, 0 keeps it |
| `TransparentFog` | 0 | 1 fogs see-through effects at their own distance, 0 at the scene behind (see Transparent fog) |
| `DataMode` | 1 | 1 Classic layers where available, 0 derived layers everywhere |
| `ColorSpace` | 1 | 1 scatter and blend in linear light with a highlight roll-off, 0 gamma |
| `SunScatter`, `Ambient`, `Exposure` | 1, 1, 1 | Light in the fog |
| `ClassicExposure` | 1 | Brightness of the Classic layers (1 = as authored) |
| `ClassicPhase` | 0 | Classic sun and moon scattering: 0 phase peaks at 1 toward the light, 1 energy-normalised |
| `LocalLights`, `LocalLightIntensity` | 1, 1 | Scatter up to eight nearby world point lights; intensity 0..8 |
| `LocalLightPhase` | 0.3 | Henyey–Greenstein g of every point light in the fog, −0.9..0.9 (a calibration) |
| `InteriorAware`, `InteriorDensity` | 1, 0.15 | Fade outdoor layers indoors, keeping this fraction of their density |
| `GodRays` | 0 | Radial sky rays, 0 = off |
| `GlowCompensation` | 1 | Pre-compensate the fog for the client's glow |
| `ForeverGlow` | 0 | 1 gives the client's glow Forever's per-light amount where Classic lights cover the camera |
| `ColorGrading` | 0 | Strength 0..1 of Forever's per-light colour curve over the 3D view; 0 = off |
| `FarClipMax` | 1583 | Continent view distance up to 1583 yd, within `farclip`; 0 keeps Ascension's 791 cap |
| `MaxDistance` | 5000 | Fog range: sky integration length and the Classic distance-curve scale |
| `Temporal` | 0.85 | History weight, 0 = off |
| `Underwater` | 0 | Keep the effect under water |
| `LiquidDepth` | 1 | Water surfaces write depth so fog uses their distance (always while modern water is drawn) |
| `Multisampling` | 1 | Keep the game's Multisampling when its depth can be copied; 0 = multisampling off (see below) |
| `DebugView` | 0 | 1 radiance, 2 transmittance, 3 linear depth |
| `SunMarker` | 0 | Red dot where the light direction projects |
| `LogLevel` | 1 | 0 errors, 1 info (frame summary with the fog's GPU time, depth probe; see Log), 2 debug |
| `Water` | 1 | Modern water, 0 = the client's own water |
| `WaterQuality` | 2 | 1 128² waves, sky reflections only; 2 256², scene reflections; 3 256², finer reflections |
| `WaterWaves` | 1 | Wave height 0..2; 0 = flat water |
| `WaterWind` | 2 | Wind driving the waves, 0.5..10 |
| `WaterFoam`, `WaterReflections` | 1, 1 | Foam and reflection strength 0..2 |
| `WaterSpecular` | 1 | Sun or moon glint 0..4 |
| `WaterClarity` | 1 | How far you see into the water 0.25..4 |
| `WaterZoneColors` | 0.5 | Tint by the zone's own water colours 0..1 |
| `WaterRipples` | 0.5 | Wakes of units in the water 0..2; 0 = no unit reads and no ripple simulation |
| `WaterClientSplashes` | 0 | 1 keeps the client's splash and wake sprites; 0 hides them for all units while wakes run |
| `WaterDebugView` | 0 | 1 wave normals, 2 foam, 3 transmittance, 4 reflection, 5 liquid class, 6 ripple height |

Turning `FarClipMax` on from 0 needs a restart; other changes, including 0, apply at the next `farclip` change,
map load or zone change, where raising it shows a loading screen.

Turning `Multisampling` on from 0 also needs a restart: the game builds its Video options' Multisampling list
once per start (see Engine inputs), and with 0 that list holds only 1x. Turning it off applies the next time the
game resets or recreates its display, for example after changing Multisampling or the resolution.

## Status and limits

This is an atmospheric approximation, not a reproduction of WoW Forever's complete lighting renderer.
[Blizzard's official overview](
https://news.blizzard.com/en-gb/article/24303862/world-of-warcraft-forever-whats-next-panel-recap)
describes mist over water and moonlight through trees; matching those scenes needs matched camera, time, weather
and exposure captures. Surface lighting and bloom remain the client's own; the modern client's glow amounts and
colour grading can be applied with `ForeverGlow` and `ColorGrading`, which stay off until they are compared with
Forever in the game, so by default colours still differ from Classic.

- The fog has been tested in the client with native D3D9 on an RTX 2060 laptop. A first modern-water build ran
  there on Elwynn lakes and the Darkshore coast; the review fixes since (direct-light shading, narrower shore
  foam, fogged reflections, the dry-frame skip) are harness-checked only. Changes to lighting and occlusion require
  an owner test; the offline harness does not establish in-game appearance or performance. Other GPU vendors,
  integrated GPUs, DXVK and Wine are untested.
- The composite and the lit march need more than the 512 instruction slots guaranteed by
  [baseline pixel shader 3.0](
  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-3-0), and lower
  quality does not reduce that. An unsupported required shader disables volumetric fog for that device and logs
  its name, HRESULT and the shader caps; native fog remains the fallback.
- Scene geometry does not shadow the fog's sunlight.
- Local lighting captures the current M2 scene's point-light table. It does not create spotlight cones or local
  shadows, and coverage of WMO-only lights is unverified. Influence is capped at 200 yd, with a smooth outer
  fade and an attenuation denominator floor of 1.
- Transparent materials without depth writes are fogged at the depth behind them.
  With `TransparentFog=1` (harness-checked only) M2 effects take one line fitted to the fog of the first 100 yd,
  which cannot follow height fog and fully fogs effects past its end (500–900 yd in tests), so far glows vanish.
- Interior treatment follows the camera's transition weight, not rooms or portals along each ray, so views
  through doorways may differ.
- The `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off there). The settings window also
  needs a fog device, so it is missing when INTZ depth is unsupported.
- Multisampling is harness-checked only (NVAPI on the RTX 2060; `RESZ` untested); alpha-tested foliage stays jagged.
- The silhouette split sees one copied depth sample per pixel, so sub-pixel geometry it misses takes the far fog.
- The distance fog (`FarFog`, maps without Classic data) has no modern counterpart: it stands in for the stock
  fog up to the 3.3.5 far clip, which is far shorter than the modern client's.
- `FarClipMax` raises memory use (about four times the loaded terrain in a 32-bit process); Ascension's reason
  for the continent cap is unknown. Without the key in the INI it stays off.
- Modern water: water seen from below keeps the client's look; rivers have no flow direction (the client has no
  flow maps), so Forever's river foam is omitted; the surface stays flat and only its shading moves; reflections are
  screen-space, so what is off screen falls back to the sky colours; interiors are detected by liquid type only.
- The noise column mapping, the noise volume and `ClassicPhase`'s normalisation are inferred; `ClassicPhase` stays 0.
- `ForeverGlow` sets only the amount: the client's glow adds `g·blur²`, Forever's a linear term.
- Ripples are harness-checked only; walkers (2.5 yd/s) push a mound rather than a V, and every unit is stamped round.
- The ripple step, texel, window, footprint, fades, splash and contact limits are choices, not recovered values.
- The sprite hold is global: units past the 32 nearest, the 24 yd window or the modern water get no wake at all.
- The client's footstep spray (`0x723A50`) is not hooked, and game objects such as boats make no ripples.
- The zone lights' edge fade distance is chosen here: their `TransitionType` is 0 in every row and the modern
  client's transition rule is not known.

## License

CoAVolFog is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`); see [LICENSE](LICENSE).

As an additional permission under GPLv3 section 7, you may link or combine CoAVolFog, including modified versions,
with the World of Warcraft client, and distribute the resulting combination without providing the client's source
code. GPLv3 continues to apply to CoAVolFog.
