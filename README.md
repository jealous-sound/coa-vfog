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
- **Interior transitions**: the camera's native WMO blend reduces outdoor layers and sunlight while preserving
  the interior's native fog colour and range.
- **Authored noise**: drifting fog banks where the Classic layers carry the modern client's noise, mostly in
  storms (see Classic fog data).
- **Density variation**: a two-octave field anchored in world space modulates the scene layers around their
  authored mean and drifts along world +X; the distance fog stays smooth so it cannot uncover the far clip. This
  is an artistic control; layers with authored noise use that instead.
- **God rays** (optional): a radial blur of the bright sky around the sun.
- **Modern water** on lakes, rivers, the sea and indoor pools: FFT waves from Forever's wave tiles, refraction,
  depth absorption and in-scattering, foam on wave crests, along shores and in shallow water, a sun or moon glint,
  and reflections of the scene with the sky as a fallback. The zone's own water colours can tint it
  (`WaterZoneColors`). Magma, slime, custom non-water liquids and water seen from below keep the client's look.

## How it renders

The march runs at quarter or half resolution (`Quality`). Each step integrates only its overlap with a layer's
start and end distances, so thin layers and partial boundary steps stay consistent across quality levels. The
march and composite use shader variants with light code only while a point light is uploaded, and with noise
sampling only while a Classic layer carries authored noise.

With `Temporal` above 0, samples vary between frames and a temporal filter accumulates them. It rejects history
from a different surface depth or depth class (world, distant terrain, sky) and lowers its weight where animated
lighting changes the radiance; settings, map, screen-effect slot, projection and large camera changes discard
it. With `Temporal=0`, samples stay at fixed midpoints so a stationary frame does not shimmer, and the temporal
passes are skipped.

The full-resolution composite upsamples the march through depth-validated taps. Ground seen at a grazing angle
is interpolated from the current frame's march where the taps lie on the pixel's plane, the march is linear
across them and no light's sphere meets the ray; thin silhouettes that every tap misses are marched at full
resolution.

Fog renders once after the world, including its late geometry and the native sun/moon glare, and before screen
effects and the UI. While it draws, the stock fog is pushed out of range for the world render and restored
afterwards; frames without volumetric fog keep the stock fog, and an unexpected draw failure restores it on the
next frame. The client's glow (`screen + g·blur²`, with `g` from the day/night light) runs afterwards and would
bleach bright fog to white, so fogged pixels are pre-compensated with the live glow amount. God rays use the
remaining display highlight range after the glow and need a scene copy; without one they are omitted.

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

**View distance.** Ascension's Extensions.dll detours the far-clip clamp (`0x780770`) and caps maps 0, 1, 530
and 571 at 791.66 yd; the engine allows 1583.33 and instances use it. With `FarClipMax` set, the DLL's calls
to the clamp lift that cap. Terrain loading, the chunk pool, the WDL horizon and the fog follow the far clip;
placed objects keep their own size-class culling (`environmentDetail`), and creatures the server's visibility
distance.

## Classic fog data

`tools/convert_classic_fog.py` converts the WoW Forever fog and lighting kit (build 1.60.1.70009, a folder or its
zip: the decoded `LightData`, `LightDataGlobalVolumeFog`, `LightParams` and `ZoneLightPoint` tables and the
colour-grading LUTs, each checked against the kit's `SHA256SUMS`) into `data/fogdata.bin`. The kit could not decrypt
the `Light` and `ZoneLight` tables, so the lights and zone lights are placed from a previous `fogdata.bin`, whose
records are carried over byte for byte, or from the Classic client's `Light.csv` and `ZoneLight.csv` exports:

```powershell
python tools/convert_classic_fog.py <kit folder or zip> --placements data/fogdata.bin data/fogdata.bin
```

The shipped file takes its fog tables and zone outlines from 70009 and its placements from 69876, carried from the file
committed before the change (`9bf495a`, converted from 69876 CSV exports); the converter checks that the written lights
and zone lights equal the placement source and that the fog table's layout hash is still `24290E20`, which keeps the
column numbers valid. 70009 re-authored 18 light params that lights on 3.3.5 maps use (among them Stormwind, Goldshire,
Westfall, Loch Modan, Durotar, Mulgore, the Barrens and the Eastern Kingdoms storm slot) and deselected the third layer
of the Un'Goro storm; the 152 params with fog that no 69876 light references cannot be placed and are left out.

The converter keeps the fog rows the Classic client selects (flag 0x8), the lowest row ID at each layer index
(0–2). The other flags are 0x1 shadowed, 0x2 relative heights and 0x4 authored noise; flag values are written in
hex here, and the GPU flags above 0x4 are packed by the modern client's CPU. At run time the DLL blends the Classic
lights around the camera: spheres at full weight inside the falloff start and linear to the falloff end, the rest to
the zone light whose outline holds the camera (fading in over 100 yd, innermost outline on top), and otherwise to the
map's global light. Each light uses the condition slot the client uses for its stock lighting: the slot a screen
effect forces (the ghost effect forces slot 4, death), otherwise clear weather (slot 0) blended toward storm (slot 2)
by the client's storm weight. The two time keys around the current time are interpolated and layers are paired by
their Classic layer index.

Classic data applies on maps where any Classic light has fog, wherever Classic lights hold at least half of the
blend weight. A light without fog in the active slot counts with zero density, so the fog thins smoothly into it
and the distance fog hides the far clip there. Other maps use the derived layers.

Then the Classic transforms apply:

- Density and both height falloffs ×0.01, as the modern client's interior fog shader scales them; each height term
  is `min(1, exp(...))`, full density between the lower and upper height.
- With flag 0x2 the heights are relative to a reference near the player: the lower of the camera and its target,
  minus a yard. The modern interior layers add the camera height instead; the global layers' CPU offset is not in the
  kit, so which height the modern client uses there is inferred.
- With flag 0x1, while the sun or moon is below the horizon, the shadow emissive colour and density multiplier;
  this is the modern shader's form with shadow maps on in open sky, where its shadow term is the light's
  above-horizon ramp.
- The distance curve `1 + strength·((d − start)/range)^exponent`, with `MaxDistance` as the range; the modern client
  uses its fog volume depth, whose value is not in the kit.
- The authored scatter intensity (0–50 in the data) times a Henyey–Greenstein phase normalised to 1 toward the
  light, or energy-normalised with `ClassicPhase=1` (below), in linear light, plus the emissive colour.

The sun scattering is scaled by the client's direct-light luminance over Classic's, capped at 1, which keeps the fog
consistent with the older client's darker lighting; this is a compatibility calibration, not a reproduction of the
modern renderer.

**Phase normalisation.** The authored intensities suggest that the modern client's Henyey–Greenstein lookup is
energy-normalised, while the phase here peaks at 1 toward the light (inferred, confidence about 0.65; the lookup table
is not in the kit). The intensity falls as g falls: evenly scattering rows (g = 0) have a median of 12, about 4π, and
in the 370 rows with g = 0 and intensity 12 the emissive colour equals the diffuse one, which an energy-normalised
phase turns into equal scattered and emitted light. `ClassicPhase=1` multiplies the Classic layers' sun and moon
scattering by the ratio of the two phases, `k(g) = (1+g)/(4π(1−g)²)`, capped at g = 0.95 (k = 62) because a few rows
reach g = 1. With k(0) = 0.080 the horizon band of the evenly scattering far wall is about 12 times dimmer; the near
fog (g ≈ 0.7) gains 1.5 times and the forward haze (g ≈ 0.9) 15 times, so the sun's halo grows larger and much
brighter. The default stays 0 until the owner compares it with Forever at noon, at sunset toward the sun and in a
storm; the direct-light calibration, the highlight roll-off and `ClassicExposure` were tuned with the peak-normalised
phase and may need retuning. Point lights keep their peak-normalised phase, as the modern local-light fog shader
evaluates its phase analytically.

**Authored noise.** Layers with flag 0x4 modulate their density as the modern global fog kernel does (shader
6674335, variant 002). Two octaves sample a tileable 3D noise volume at `frac((p − offset_i)·inverseTile_i)`. Their
average over the octaves present passes through a fixed S-curve of contrast 20 about 0.5, `x < ½ ? x²(k+1)/(x+k/2) :
1 − (1−x)²(k+1)/((1−x)+k/2)` with `k = −19/18`. The curve is blended by alpha, `f = lerp(1, curve, alpha)`; the
density is multiplied by `f`, and after the shadow colour the emission moves toward the fade colour by `1 − f`. The
noise only thins: at alpha 1 the curve is nearly a threshold and a layer keeps about half its mean density, so
layers with noise, mostly storm fog, become patchier and thinner than before. The artistic density variation does not
apply to them. Through the placed lights the noise reaches 270 layers in 27 light params, most of them storm slots
(106 Kalimdor and 30 Eastern Kingdoms storm lights), plus a few clear-weather ones such as Hyjal's haze.

The column mapping is inferred (confidence in brackets) and lives in one place, `FogData::UnpackNoise`:

- c4, the fade colour (0.6): set on 187 of the 648 noise rows and on 15 rows without noise, so assigned by
  elimination among the four colour columns.
- c16–18 and c19–21, the scroll directions of octaves 0 and 1 (0.7), used unnormalised; lengths √3 and √2 are common.
- c27[i], octave i's scale in hundreds of yards (0.45): a tile of the volume spans `100·c27` yd, as the interior
  form's `0.01/noiseScale` implies, and an octave is present when its scale is above 0. The DB2 arrays hold one
  property per octave, which favours this reading over (scale, speed) pairs per octave.
- c28[i], octave i's drift speed in yd/s (0.45); the time unit of the modern shader is not in the kit.
- c24 is carried raw and unused: it is 0 or 1, constant within each light params and 0 on 21 of the 64 noise
  presets, so it does not behave like a per-layer alpha.

Alpha is the share of a layer's blend weight that carries the noise, so the noise fades in over key, light and weather
blends; the octave weights and the noise parameters are averaged over the contributions that carry them. The modern
shader subtracts a CPU scroll offset; here each octave keeps it as a phase in tiles, advanced by direction × speed ×
elapsed seconds ÷ tile and wrapped to one tile, and uploads phase × tile. When a blend changes an octave's tile size,
the phase moves so that the pattern scales about the camera. An offset kept in yards would instead slide the pattern
by offset × Δ(1/tile), which grows with the time the noise has scrolled: after an hour at Hyjal a storm's 5-second
blend would sweep the haze through 11 tiles. Scaled about the camera, fog 300 yd away moves by 0.03 tiles.
`ClassicNoise=0` turns the noise off. The frame summary logs each noisy layer's share, the alpha actually drawn (0,
marked off, with `ClassicNoise=0`, or marked as the mean if the noise volume could not be created), tiles, drift and
fade colour.

The modern client's noise texture (`t_perlinNoise3D`) is not in the kit, so the volume is ours: a 64³ tileable
gradient (Perlin) noise with detail layers of 4, 8 and 16 lattice cells per tile at gain 0.5, quantised about its
median so the S-curve splits it evenly. Its features are about a quarter of a tile, 75 yd in the Eastern Kingdoms
storm's 300-yd tiles and 1250 yd at Hyjal. The DLL builds it when it installs its hooks at load, in about 6 ms (each
row sums its lattice gradients once), so the first frame with noise only uploads it. The march samples the noise once per step at the step's sample point for all
three layers, as the modern client evaluates each froxel once, and the point lights scatter off the same noisy
density. Where the noise is not sampled a noisy layer takes its mean, density ×(1 − alpha/2) with the emission
weighted by where fog remains: in the water's reflection fog, which is integrated analytically, in the check of
whether the Classic layers hide the far clip, and in the lit composite's full-resolution march at silhouettes, which
has no temp register to spare in ps_3_0. Frames without noise use the shader variants without it, at their previous
cost.

`fogdata.bin` format 4 holds, after a header of counts: the lights (id, map, position, falloff, eight light params
slots); every light params a light references, with its `LightParams.Glow` and the range of its fog keys (none for
params without fog); the fog keys (time, Classic direct light, layer range, grading curve index, 0 for none); the
layers (the columns above plus the authored noise columns: fade colour c4, scroll directions c16–18 and c19–21, the
pairs c27 and c28, and c24 carried raw); the zone lights and their outlines; and the grading curves. The loader
rejects any other format and logs which one it found.

Each light params also carries glow and colour grading, which the DLL resolves but does not render yet. They are
blended from the Classic lights around the camera like the fog, but on every map with a placed light, not only where
Classic fog applies: 219 light params without fog but with glow are placed on 3.3.5 maps, 167 of them only on the 17
maps without Classic fog, such as Blackwing Lair (469) and Stratholme (329). `AuthoredFog::coverage` is the Classic
lights' share of the blend, for the renderer to fade toward the client's own values; where no Classic light reaches
there is no glow and the curve is the identity.

- **Glow.** `LightParams.Glow`, blended like the fog by light weight, weather and screen-effect slot. Forever sets it
  to 0 on 130 of 131 Kalimdor and 59 of 80 Eastern Kingdoms clear-weather lights, where CoA's own 3.3.5 data holds
  0.3–1.0.
- **Colour grading.** `LightData.ColorGradingFileDataID` names a 32³ BGRA LUT stored as a 1024×32 strip (R across each
  32-texel tile, G down the rows, B by tile). Every LUT that lights on 3.3.5 maps reach (1140733, an identity, and
  8248426, 8248427, 8286665–8286669) applies one curve alike to R, G and B, so the file keeps 32 codes per LUT; the
  converter checks this exactly and fails otherwise. `DarkerColorGradingFileDataID` is left out: its LUT 1308655 is a
  real colour grade that no single curve reproduces, and only params 6563, on the Classic-only map 2835, uses it. A
  key sets a curve or none. The curve at a time of day is interpolated between the nearest earlier and later keys that
  set one, wrapping past midnight, so the params that grade only at 12:00 (135 of the 147 graded) hold their curve all
  day, and param 7605's explicit identity keys fade into its 18:00 grade. This rule is inferred (confidence about
  0.6): 7605 would not need identity keys if a key without a LUT meant identity. Lights and weather blend curves by
  their weights, and params without a graded key count as identity. The Eastern Kingdoms clear-weather light (params
  7748) grades with 8286666 and Kalimdor's (7636) with the milder 8286665.

The frame summary logs the resolved glow, the Classic coverage and three points of the grading curve wherever Classic
lights reach the camera.

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
  which the client caches as its world depth. MSAA is reported unavailable and forced off, and
  `D3DCREATE_PUREDEVICE` is removed. The shaders read depth through the captured world viewport's range and
  treat anything deeper as beyond the far clip.
- **Hooks.** Five 5-byte call displacements: the world render call (`0x4FB03D`, stock-fog override and restore),
  after the opaque M2 pass (`0x4F911D`, captures camera inputs), the liquid surface pass (`0x4F9170`, forces
  depth writes), world-name text (`0x7E5818`, suppresses depth writes), and before the frame effects
  (`0x4F9281`, draws the fog over the completed world). The original bytes are checked first; on any mismatch
  nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when `FarClipMax` is
  set at start-up, independently of the fog hooks.
- **Water hooks.** Installed after the fog hooks and independently of them: the water pass call (`0x790AA2`) and
  the `Render` slots of the two water material vtables (`0xA5954C`, `0xA59580`, read-only data patched under
  `VirtualProtect`). The call site, both slots and the layout bytes the classification reads are checked first; on
  any mismatch none are patched and the fog is unaffected. `waterdata.bin` is loaded only once they are installed. A
  water exception restores the device state, turns water off for the session and leaves the fog running.
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
| Far clip | From the projection; `[[0xB7436C] + 0xB14]` as a fallback |
| World M2 scene (point lights) | `[0xCD754C]` |
| Camera WMO instance, group ids, group count, interior fog blend | `0xCD87A4`, `0xCDB0DC`, `0xCDB0D8`, `0xD38B9C` |
| Sky colours (bands 2–7: top, middle, band 1, band 2, smog, fog) | `0xD38BE0`–`0xD38BF4` |
| Water colours: ocean close/far, river close/far | `0xD38C0C`, `0xD38C10`, `0xD38C14`, `0xD38C18` |
| Liquid renderer, transparent (water) draws queued this frame | `[0xCD8610]`, `[[0xCD8610] + 0x14]` |
| Liquid settings bank: count, entries | `0xD43B18`, `0xD43B1C` |
| `LiquidType` rows: maximum id, minimum id, row pointers | `0xAD4070`, `0xAD4074`, `0xAD4084` |

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
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`)
  and the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`);
  `0x4F8770` feeds it the DayNight glow (`0xD38C2C`) as the additive weight of `lerp(screen, blur, other) +
  g·blur²`, where `other` is the screen effect's own blend amount, which the fog leaves as is.
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
- Water colours. The DayNight light record at `0xD38BD4` (`0x7F3574`) stores the sky bands 2–7 at `0xD38BE0`–
  `0xD38BF4` (`0x7EC03C`–`0x7EC09C`) and the ocean and river close/far colours at `0xD38C0C`–`0xD38C18`
  (`0x7EC11D`–`0x7EC152`); the client bakes the water colours into its per-frame depth ramps. The water is lit by
  the direct light (band 0, `0xD38BD8`), which `0x7EE756` copies to the world's direct light; band 9
  (`0xD38BF8`) only colours the sun and moon sprites (`0x7F36F6`).
- Not used. `0xD38B98` is a density-like value that Extensions.dll patches (the fog end is `0xD38BA8`), and
  `0xD38C9C` is a near-constant model lighting direction, not the visible sun, so the fog's light direction
  follows the sprite positions.

## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. On laptops whose F-keys send media
keys by default, hold Fn (the log names the key that arrived). The window edits every setting below except
`Enable`, `EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change. **Save** writes the
settings changed in the window back to `CoAVolFog.ini`, keeping the comments and every other line; settings not
changed in the window keep what the file holds. **Revert** reloads the file, and a hand edit of the file also
replaces unsaved changes. The window shows whether the fog drew in the last frame, or why it did not.

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
client's flags (`0x52`, auto depth D24S8), renders a Z-up test scene with the client's projection convention,
and runs the fog passes through the same entry the hook uses. Its checks, in `tests/`, cover the device wrapper
and state restoration, depth and sky handling, Classic light blending and slot selection, the authored noise, glow
and grading the Classic data resolves, the march against CPU integrals at every quality, the authored noise against
the modern curve and a CPU sample of the noise volume, temporal filtering and upsampling, point lights and
interiors, the text and liquid depth overrides, fog-data validation, the GPU timer and depth probe, the settings
window and INI saving, and `Reset`. The water suites check the water data and its loader, the FFT against a
double-precision reference, the liquid classification, the water pass driven through the hook entry points (state
restoration, stencil tagging, optics against a CPU reference, fault recovery) and the water settings. They do not
establish in-game appearance or performance. It writes `before.png`, `after.png`, `overlay.png` and the debug views
to `build/harness-out`.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration. `--classic-phase 1` renders it with
`ClassicPhase=1` and `--storm 1` in a full storm. The DLL reads the `fogdata.bin` beside it, so a new file must be
copied there as well as named with `--data`.

`vfog_harness --scene performance` times the fog passes at 1920×1080 at each quality on one street: derived
layers with no point lights, eight flood lights or eight street lamps, Classic layers at the harbour with and
without the lamps, the same in a full storm, whose near layer carries authored noise (both skipped if `fogdata.bin`
beside `CoAVolFog.dll` does not resolve the harbour), and the shipped `LogLevel=1`. It prints
`quality,case,point_lights,median_ms,p95_ms` of GPU time; this is a controlled renderer cost, not a game frame-rate
test.

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
so once and the summaries omit the time; at `LogLevel=0` no queries are issued. With Classic data the summary
adds the resolved glow and grading curve, for example `Classic glow 0.00, grading curve at inputs 8/31 16/31 24/31:
0.267 0.565 0.890 (not rendered)`, and a line for each layer with authored noise.

Every 60 s the water adds `water gpu 1.24 ms (median of 3500 frames, 0 skipped), classes lake+ocean, waves 256
(7 tiles)`: its GPU time without the client's own water draws. `water:` lines name each liquid type the first
time it is classified; idle states (no water in view, camera under water) are logged once and repeats only at
`LogLevel=2`. Each settled change from the settings window, an INI reload or Revert is logged as one line, for
example `settings: WaterFoam 1 -> 1.5, WaterWind 2 -> 4`.

The depth probe logs raw depth, distance and fog opacity at 25 points on frame 60, then every 60 s up to five
times (every 30 s without limit at `LogLevel=2`); its rows are read back on a later frame. The first reason a
fog draw is skipped is logged as `fog skipped: <reason>`; a camera under water is logged once as `fog idle`.

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
| `DataMode` | 1 | 1 Classic layers where available, 0 derived layers everywhere |
| `ColorSpace` | 1 | 1 scatter and blend in linear light with a highlight roll-off, 0 gamma |
| `SunScatter`, `Ambient`, `Exposure` | 1, 1, 1 | Light in the fog |
| `ClassicExposure` | 1 | Brightness of the Classic layers (1 = as authored) |
| `ClassicPhase` | 0 | Classic sun and moon scattering: 0 phase peaks at 1 toward the light, 1 energy-normalised |
| `LocalLights`, `LocalLightIntensity` | 1, 1 | Scatter up to eight nearby world point lights; intensity 0..8 |
| `InteriorAware`, `InteriorDensity` | 1, 0.15 | Fade outdoor layers indoors, keeping this fraction of their density |
| `GodRays` | 0 | Radial sky rays, 0 = off |
| `GlowCompensation` | 1 | Pre-compensate the fog for the client's glow |
| `FarClipMax` | 1583 | Continent view distance up to 1583 yd, within `farclip`; 0 keeps Ascension's 791 cap |
| `MaxDistance` | 5000 | Fog range: sky integration length and the Classic distance-curve scale |
| `Temporal` | 0.85 | History weight, 0 = off |
| `Underwater` | 0 | Keep the effect under water |
| `LiquidDepth` | 1 | Water surfaces write depth so fog uses their distance (always while modern water is drawn) |
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
| `WaterDebugView` | 0 | 1 wave normals, 2 foam, 3 transmittance, 4 reflection, 5 liquid class |

Turning `FarClipMax` on from 0 needs a restart; other changes, including 0, apply at the next `farclip` change,
map load or zone change, where raising it shows a loading screen.

## Status and limits

This is an atmospheric approximation, not a reproduction of WoW Forever's complete lighting renderer.
[Blizzard's official overview](
https://news.blizzard.com/en-gb/article/24303862/world-of-warcraft-forever-whats-next-panel-recap)
describes mist over water and moonlight through trees; matching those scenes needs matched camera, time, weather
and exposure captures. Surface lighting, bloom and colour grading remain the client's own: the modern client's glow
amounts and LUT grading are resolved from the Classic data but not applied yet, so colours still differ from
Classic.

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
- Interior treatment follows the camera's transition weight, not rooms or portals along each ray, so views
  through doorways may differ.
- The `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off there). The settings window also
  needs a fog device, so it is missing when INTZ depth is unsupported.
- The distance fog (`FarFog`, maps without Classic data) has no modern counterpart: it stands in for the stock
  fog up to the 3.3.5 far clip, which is far shorter than the modern client's.
- `FarClipMax` raises memory use (about four times the loaded terrain in a 32-bit process); Ascension's reason
  for the continent cap is unknown. Without the key in the INI it stays off.
- Modern water: water seen from below keeps the client's look; rivers have no flow direction (the client has no
  flow maps), so Forever's river foam is omitted; the surface stays flat and only its shading moves; reflections are
  screen-space, so what is off screen falls back to the sky colours; interiors are detected by liquid type only.
- The zone lights' edge fade distance is chosen here: their `TransitionType` is 0 in every row and the modern
  client's transition rule is not known.

## License

CoAVolFog is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`); see [LICENSE](LICENSE).

As an additional permission under GPLv3 section 7, you may link or combine CoAVolFog, including modified versions,
with the World of Warcraft client, and distribute the resulting combination without providing the client's source
code. GPLv3 continues to apply to CoAVolFog.
