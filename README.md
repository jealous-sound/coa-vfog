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

**Transparent fog.** Fog drawn once after the world gives every see-through effect (particles, spell effects,
ribbons, alpha and additive models) the fog of the surface behind it: an additive glow 10 yd away in front of
terrain 800 yd away is dimmed to the far terrain's transmittance, while the stock client fogs it by its own
distance. With `TransparentFog=1`, above water, the fog is composited at the end of the liquid pass instead, when
the sky, terrain, buildings, opaque models, models beyond a water plane and the shaded water are in the colour and
depth and no see-through model on the camera's side has been drawn yet. The sun and moon glare pass is called just
before that composite and skipped at its own call, so it is fogged per pixel as before. From then to the end of the
world render every M2 batch fog call is rewritten: its start and end become a linear fog fitted to the volumetric
fog, its exponent 1, and its colour the fitted fog colour when the client passes the model's lighting colour
(alpha 0xFF); the black, white and grey colours of additive, modulate and modulate-2x batches are kept, which is
the stock form of the modern client's per-material fog modes. The fit samples the volumetric transmittance and
in-scatter within 100 yd along 15 rays across the view (weighted toward the centre, rise clamped to ±0.26) with the
march's layer terms at the layers' mean noise, and solves the least-squares line in planar view depth, the depth
the client's M2 shaders fog by; samples clamped to zero by the shader are refitted without. The colour is the
in-scatter over the opacity, exposed, rolled off, gamma-encoded and glow-compensated as the composite shows it. God
rays are traced from the scene copied just before the early composite, the unfogged image the single composite traces
them from, and added over the finished world at the end of the world render, over a new scene copy. Under water,
with `StockFog=0`, a debug view or the sun marker, or when the early composite fails, the fog is drawn after the
world as before. The fit is linear in depth, so it cannot follow the medium's height and distance-curve shape: in
the harness it stays within 0.03 of the volumetric transmittance along the view axis and within 0.06 at the side of
the view for thin homogeneous fog, ground fog and the Classic harbour sunset; each vertex is fogged by its own
planar depth, as in the stock client.

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

**Antialiasing.** D3D9 textures cannot be multisampled, so with the game's Multisampling option the depth the
passes read is a copy. `CheckDeviceMultiSampleType` offers the game's sample counts only while `Multisampling=1`
and a copy method exists: `NvAPI_D3D9_StretchRectEx` on native NVIDIA D3D9 (the system `d3d9.dll` and
`nvapi.dll`), or the driver's `RESZ` resolve where `CheckDeviceFormat` reports it (AMD, Intel, DXVK reporting an
AMD GPU). The game's D24X8 depth has no stencil, so the multisampled depth is created as D24S8 for the water
tags and the silhouette split. When the device is created or reset multisampled, the DLL clears the game's depth
to two known values, copies it and reads the copy back, twice with the values swapped, before the game sees the
device; if that fails the device is created or reset single-sampled as before and the log says why. Each frame
the depth is copied before the water pass arms, after the water surfaces and before the fog; the copy holds one
sample per pixel. The scene-colour copies become resolves of the multisampled back buffer, and the water's stencil
tags and shading are tested per sample, so water edges are antialiased too.

At silhouettes the fog composite splits each pixel's samples by depth, so a pixel that is half tree and half sky
resolves to the tree's and the sky's fog weighted by coverage instead of taking one of them. A small pass finds the
pixels whose 3×3 copied depths hold a discontinuity: another depth class, or a separation beyond the same-surface
tolerance that is not planar (raw depth is affine across a plane, so grazing ground is not split). It writes an
`oDepth` halfway between the nearest and farthest depth, tests it against the multisampled depth and marks the near
and far samples in the stencil, which the client does not use, after clearing it over the world viewport. The
composite then draws the unmarked pixels as before, and one pass per marked side draws its samples with the fog at
that side's depth; when the composite overwrites the scene copy the first pass draws every pixel without depth and
the side passes redraw only the pixel's other side. `oDepth` is clamped to the viewport's depth range, so the split
uses MinZ 0 and MaxZ 1 instead of the world viewport's 0.94. When a Classic layer carries authored noise the passes
use the noisy composites, whose full-resolution march samples the noise at each side's depth (fxc: about 1150
instruction slots and 31 of the 32 temp registers); with point lights they use the lit composites and the noise at
its mean, as without multisampling (see Authored noise).

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
is not in the kit). The intensity rises as g falls: over the client-selected rows its median is 1 for g 0.65–0.85, 2
for 0.35–0.65, 4 for 0.05–0.35 and 12, about 4π, for evenly scattering rows (g < 0.05; rows above 0.85 have 2), and in
334 of the 370 rows with g = 0 and intensity 12 the emissive colour equals the diffuse one, which an energy-normalised
phase turns into equal scattered and emitted light. `ClassicPhase=1` multiplies the Classic layers' sun and moon
scattering by the ratio of the two phases, `k(g) = (1+g)/(4π(1−g)²)`, capped at g = 0.95 (k = 62) because a few rows
reach g = 1. With k(0) = 0.080 the horizon band of the evenly scattering far wall is about 12 times dimmer; the near
fog (g ≈ 0.7) gains 1.5 times and the forward haze (g ≈ 0.9) 15 times, so the sun's halo grows larger and much
brighter. The default stays 0 until the owner compares it with Forever at noon, at sunset toward the sun and in a
storm; the direct-light calibration, the highlight roll-off and `ClassicExposure` were tuned with the peak-normalised
phase and may need retuning. In the logged harbour frame at sunset `ClassicPhase=1` makes the forward haze (g 0.93)
scatter about 30 times more, and 6° below the sun the fog saturates every channel, so the pixel turns white (255 255
255) where the peak-normalised phase keeps the warm halo (255 229 171); the harness checks that this halo keeps its
hue with the shipped settings and the built-in defaults. Point lights keep their peak-normalised phase, as the modern
local-light fog shader evaluates its phase analytically.

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

The modern client's noise texture (`t_perlinNoise3D`) is not in the kit, so the volume is ours: a 64³ tileable gradient
(Perlin) noise with detail layers of 4, 8 and 16 lattice cells per tile at gain 0.5, quantised about its median so the
S-curve splits it evenly. Its features are about a quarter of a tile, 75 yd in the Eastern Kingdoms storm's 300-yd tiles
and 1250 yd at Hyjal. The DLL builds it when it installs its hooks at load, in about 6 ms (each row sums its lattice
gradients once), so the first frame with noise only uploads it. The march samples the noise once per step at the step's
sample point for all three layers, as the modern client evaluates each froxel once, and the point lights scatter off the
same noisy density. Where the noise is not sampled a noisy layer takes its mean, density ×(1 − alpha/2) with the
emission weighted by where fog remains: in the water's reflection fog, which is integrated analytically, in the check of
whether the Classic layers hide the far clip, and in the lit composites' full-resolution march at silhouettes, the
multisampled split's included, which have no temp register to spare in ps_3_0 (fxc stops at the 32-temp limit when
they sample the noise). Frames without noise use the shader variants without it, at their previous cost.

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
  converter checks this exactly (every channel code equal) and fails otherwise. `DarkerColorGradingFileDataID` is left
  out. Its real colour grade, LUT 1308655, which no single curve reproduces, belongs to params 6829, 6832, 6842 and
  6934, which no placed light references. The only placed light params with a darker LUT is 6563, on the Classic-only
  map 2835, and its darker LUT is the identity 1140733 on a key without fog, which the file does not keep either; the
  converter lists the darker LUTs of placed params that it leaves out. A key sets a curve or none. The curve at a time
  of day is interpolated between the nearest earlier and later keys that set one, wrapping past midnight, so the params
  that grade only at 12:00 (135 of the 147 graded) hold their curve all day, and param 7605's explicit identity keys
  fade into its 18:00 grade. This rule is inferred (confidence about 0.6): 7605 would not need identity keys if a key
  without a LUT meant identity. Lights and weather blend curves by their weights, and params without a graded key count
  as identity. The Eastern Kingdoms clear-weather light (params 7748) grades with 8286666 and Kalimdor's (7636) with the
  milder 8286665.

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
- **Depth.** The shaders read depth from an `INTZ` texture through the captured world viewport's range and treat
  anything deeper as beyond the far clip; `D3DCREATE_PUREDEVICE` is removed. Without multisampling the wrapper
  creates the device without auto depth and binds the `INTZ` texture as the depth-stencil, which the client caches
  as its world depth. When the game asks for multisampling and a depth copy passes its self-test (see
  Antialiasing), the device keeps the game's own parameters, a multisampled back buffer and automatic depth, and
  the `INTZ` texture becomes an unbound copy that is refreshed before every pass that reads it. A game depth
  format without an 8-bit stencil (the game asks for D24X8) is created as D24S8 once `CheckDeviceFormat`,
  `CheckDepthStencilMatch` and `CheckDeviceMultiSampleType` accept it, and the game is handed back its own format;
  otherwise the device stays single-sampled.
- **Hooks.** Five 5-byte call displacements: the world render call (`0x4FB03D`, stock-fog override and restore),
  after the opaque M2 pass (`0x4F911D`, captures camera inputs), the liquid surface pass (`0x4F9170`, forces
  depth writes; with `TransparentFog=1` it draws the glare and the fog when the pass returns), world-name text
  (`0x7E5818`, suppresses depth writes), and before the frame effects (`0x4F9281`, draws the fog over the
  completed world, or only the god rays after an early composite). The original bytes are checked first; on any
  mismatch nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when
  `FarClipMax` is set at start-up, independently of the fog hooks.
- **Transparent fog hooks.** Installed after the fog hooks, whatever `TransparentFog` says, so the setting can be
  turned on and off in the game: the M2 batch fog call (`0x81FD15`) and the glare pass call (`0x4F9213`). The
  M2 thunk forwards straight to the client while nothing is armed and no counters run; otherwise it hands the
  caller's cdecl argument block to the DLL (`pushad; lea eax, [esp+0x24]`) and jumps to the client's setter. Both
  call sites and the byte runs listed under Engine inputs are checked first; on any mismatch neither is patched,
  the log says so and the fog is drawn after the world. An exception in the M2 hook turns transparent fog off for
  the session and leaves the fog running.
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
- Multisampling. The Video options' multisample list (`0x54F1B0`, calling the Gx enumeration `0x682B00` at
  `0x54F2C1`) comes, for D3D9, from `0x68A170`, which creates its `IDirect3D9` through the wrapped slot and calls
  `CheckDeviceMultiSampleType` (`+0x2C`, adapter 0, HAL, windowed FALSE) for the display format (`0x68A2B9`) and
  the depth format (`0x68A2DE`) with each count of the table `0xAD8CE8` = {0, 2, 4, …, 16}; a failure drops the
  entry (`jl 0x68A3F7`), which is why the list offered only 1x while the wrapper refused every count. With the
  132 display modes of the RTX 2060 that is about 7,000 calls, so the wrapper probes the depth copy method once
  per adapter (`GetAdapterIdentifier` alone costs about 0.19 ms). The list is built once per process: `0x54F1B0`
  returns while `[0xBEA744]` is set, and only `0x54F3CF` in it writes that. The
  present-parameter builder `0x68E250` sets `MultiSampleType` to the chosen count when it is above 1 (`0x68E3BB`),
  `MultiSampleQuality` to (levels − 1)·`gxMultisampleQuality` from the device's own `CheckDeviceMultiSampleType`
  (`0x68E3DC`–`0x68E41E`) and `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` only without multisampling (`0x68E42D`);
  `CreateDevice` is called at `0x68F4DE` with flags `0x22` or `0x52`. The `gxMultisample` CVar (registered at
  `0x76A8FA`) is clamped to 1..16 by its callback `0x769610`, stored at `0xCABCF8`, and prints
  "set pending gxRestart". Whether that restart resets or recreates the device is not established, so both
  `CreateDevice` and `Reset` choose between the copied and the bound `INTZ` depth.
- Game depth format. The builder sets `EnableAutoDepthStencil` to 1 (`0x68E353`) and `AutoDepthStencilFormat` to
  the table `0xA2E4A8` = {…, D16, D24X8, D24S8, D32} (indices 4–7) at the depth index `[0xCABCE8]`
  (`0x68E356`–`0x68E360`); the samples come from `[0xCABCF8]` of the same block (`0x68E3B4`). The `gxDepthBits`
  callback `0x7692D0` stores index 4 for "16", 5 for "24" and 7 for "32". At start-up `0x769950` and in the
  Video options setter (`0x54FA46`–`0x54FAFD`) `gxDepthBits` is set through `CVar::Set` `0x7668C0` with the
  callback forced (second argument 1), from the table `0xAD87C4`, which names index 6 (D24S8) "24" as well; so a
  24-bit depth always becomes D24X8, which has no stencil. The client's clear `0x6A74B0` passes only
  `D3DCLEAR_TARGET` and `D3DCLEAR_ZBUFFER` (its flags 1 and 2). On the RTX 2060 a 4x D24X8 depth rejects
  `D3DCLEAR_STENCIL` (`D3DERR_INVALIDCALL`) and a stencil-EQUAL draw on it always passes, while
  `NvAPI_D3D9_StretchRectEx` copies it into `INTZ` without error; the water tags and the silhouette split need the
  stencil, so the multisampled device is created with D24S8 (the `INTZ` the single-sampled path binds is also
  24-bit depth with an 8-bit stencil).
- Depth copy. On an RTX 2060 Max-Q (driver 566.36, 32-bit process) `CheckDeviceFormat` does not report `RESZ`,
  the `POINTSIZE` resolve leaves `INTZ` unchanged and `StretchRect` from a D24S8 surface into `INTZ` is rejected,
  multisampled or not. `NvAPI_D3D9_StretchRectEx` copies 2x, 4x and 8x D24S8 exactly into a registered `INTZ`
  texture, inside or outside a scene and with any filter, taking one sample at silhouettes; it cost about 0.15 ms
  at 2560×1440 4x. `NvAPI_D3D9_RegisterResource` adds no reference and `Reset` succeeds with the resources
  registered. The functions come from `nvapi_QueryInterface` with the IDs of NVIDIA's public NVAPI headers:
  `0x0150E828` Initialize, `0xA064BDFC` RegisterResource, `0xBB2B17AA` UnregisterResource, `0x22DE03AA`
  StretchRectEx. `StretchRect` from the multisampled back buffer (whole, a sub-rectangle, a quarter-size linear
  copy) resolves it, and a stencil-EQUAL full-screen pass on multisampled colour and depth tests each sample. A
  pixel shader's `oDepth` against the multisampled depth with `ZFUNC` LESS or GREATEREQUAL divides a pixel's
  samples by depth, and the resolve weighs the two results by coverage; `oDepth` is clamped to the viewport's
  MinZ..MaxZ (an `oDepth` of 1.0 fails against a stored 1.0 when MaxZ is 0.94).
- Liquid depth. While writes are forced on, the wrapper records the client's own `D3DRS_ZWRITEENABLE` requests
  and re-applies the last one afterwards, so the client's render-state cache stays accurate.
- World-name text. `0x7E5818` (`E8 23 76 ED FF`) calls `0x6BCE40`, a cdecl wrapper that takes the font batch
  `[0xD380A8]` and tail-jumps to `0x6C53A0`. The batch is created by `0x6BF160(1, 1)` at `0x7E6511`; its bit 0
  at `+8` selects world rendering (`0x6C5564`), which requests depth testing (Gx state 13, `0x6C5591`) and depth
  writes (Gx state 15, `0x6C55BE`, dispatched as `D3DRS_ZWRITEENABLE` at `0x6A8FC7`). Suppressing the writes
  keeps glyph quads out of the depth the fog is integrated against, where they would end fog rays at the text,
  while keeping the draw and its depth test. The later name/icon path at `0x4FB042` is left in place.
- Native glare. `0x4F9213` (`E8 58 76 2F 00`, after `83 C4 14`, before the world-text call `E8 63 C3 2E 00`)
  calls the sun/moon glare pass `0x7F0870`, a function without arguments (`83 3D CC 8C D3 00 00 74 3C D9 05 48 8B
  D3 00 51 B9 A8 8E D3 00`; `ret` at `0x7F08B5`) that updates and draws the sun (`0x7F088D B9 A8 8E D3 00`,
  `0x7F0892 E8 69 BB 1B 00`) and tail-jumps to draw the moon (`0x7F08AB B9 58 8F D3 00 E9 4B BB 1B 00`). Its draw
  `0x9AC400` adds the `sunGlare`/`moonGlare` quads (`D3DBLEND_SRCALPHA`, `D3DBLEND_ONE`) at depth
  `[0.9990234375, 1]` with Gx fog, depth writes and depth test off (`0x9AC55E`–`0x9AC593`), and its update
  `0x9AC3C0` runs a GPU occlusion query against the current world depth (`0x9ABE00`); `0x7EF6E0` derives each
  frame's glare alpha from the previous one. World geometry `0x7984A0` also runs after the opaque hook (`0x4F9154`
  calls `0x77F010`, which jumps to it). Drawing the fog once at `0x4F9281` sees the completed depth and keeps the
  glare and its query order unchanged; with `TransparentFog=1` the glare pass is called from the liquid thunk just
  before the early composite and its own call is skipped, so the glare and its query run before the see-through
  models of M2 pass 1.
- World render order. In `0x4F8EA0` the map render `0x79A870` (`0x4F909F E8 4C 5F 28 00`) draws the sky with its
  skybox models, the WDL, terrain and the WMOs with their alpha batches, then come M2 pass 0 (`0x4F911D`) and
  `0x7984A0` (`0x4F9154`). `0x780620` returns `[0xCD8794]`; above water M2 pass 2 (`0x4F9167 6A 02`, `0x4F916B E8
  40 AB 32 00`), the liquid pass (`0x4F9170 E8 AB 5E 28 00`), weather (`0x4F9175 E8 B6 5E 28 00`), the barrier
  effect (`0x4F9184 E8 F7 67 28 00`) and M2 pass 1 (`0x4F918C 6A 01`, `0x4F91B9 E8 F2 AA 32 00`) follow; under
  water pass 1 (`0x4F91A6`), weather (`0x4F91AB`), the liquid (`0x4F91B0`) and pass 2 (`0x4F91B5 6A 02`,
  `0x4F91B9`). Then lightning (`0x4F91D9` → `0x9AB070`, Gx fog off at `0x9AB17A`), missile arcs (`0x4F91DE` →
  `0x6FDFB0`), render list 1 (`0x4F91FE`, `0x4F920B`), the glare (`0x4F9213`), world text (`0x4F9218`) and FFX end
  (`0x4F9281`). The liquid pass `0x77F020` jumps to `0x790A80`, which first sets the Gx fog from group 0 (`E8 8B 0B
  FF FF` → `0x781610`), then draws the water (`0x790AA2`) and tail-jumps to the ripples `0x79D5E0`.
- M2 passes. The draw-list builder `0x821A20` puts a batch in class 1 when its material blend mode (word `+2`) is
  above 1 or its alpha below 0.99999 (`0x821F47 B9 01 00 00 00 66 39 48 02 77 16 D9 05 28 55 A4 00`,
  `[0xA45528]` = 0.99999); class 1 goes to pass 1 on the camera's side of the model's liquid plane and to pass 2
  beyond it. Without liquid information a model is on the camera's side (the lighting constructor sets only flag
  0x20, `0x83491E 83 4E 14 20`). Transparent ribbons and particle emitters follow the same rule; emitters with flag
  0x40000 always go to pass 2 (`0x8219F3 39 55 18 74 0F F7 86 34 01 00 00 00 00 04 00 8D 4F 64 74 03 8D 4F 74`).
- M2 batch fog. `0x81FB10`, reached by every M2 entry type, picks the fog colour by blend mode through the table
  `0xA45390` = {1, 1, 1, 2, 2, 3, 4, 0} (`0x81FB7B 8B 04 8D 90 53 A4 00`) and the jump table `0x81FE7C`
  (`0x81FBA3 FF 24 85 7C FE 81 00` → `0x81FBAA`, `0x81FCAE`, `0x81FCC2`, `0x81FCD8`): mode 1 is the model's
  lighting fog colour with alpha 0xFF (`0x81FC5D C6 45 FB FF`), modes 2, 3 and 4 are black, white and grey 0x80
  with alpha 0 (`ebx` zeroed at `0x81FB20 33 DB`, stored at `0x81FCB7`, `0x81FCCD` and `0x81FCE3`, `88 5D FB`), and
  mode 0 (material flag 2, or no fog range) draws unfogged without the call. At `0x81FCEE` it pushes the lighting's
  fog exponent, the address of the colour at `[ebp−4]`, the end and the start (`8B 46 70 D9 80 B4 00 00 00 8D 4D FC
  51 83 EC 0C D9 5C 24 08 D9 80 AC 00 00 00 D9 5C 24 04 D9 80 A8 00 00 00 D9 1C 24`) and calls the setter at
  `0x81FD15` (`E8 F6 34 05 00` → `0x873210`), then `0x873390(1)` and pops the arguments (`6A 01 E8 6F 36 05 00 83
  C4 14`). The lighting holds DayNight fog group 0 or 1, copied by the lighting callback (`0x780D33`–`0x780D51` →
  `0x834990`).
- M2 fog setter. `0x873210` (`55 8B EC 83 EC 14 83 3D 20 30 D4 00 00`) with shaders on writes the colour's bytes
  +2, +1 and +0 times 1/255 (`0x873225 8B 75 14 0F B6 46 02`, `0x873242 0F B6 4E 01`, `0x873254 0F B6 16`) to the
  pixel constant c2 (`vtable+0x118(4, 2, 0xD43058, 1)`) and builds `c30 = (−k/(end − start), end/(end − start),
  exponent, 0)` from the start, end (`0x873263 D9 45 0C D9 45 08 D8 E9`) and exponent (`0x87328C D9 45 10`) with k =
  `[0xD4300C]` = 1; `0x873390` uploads it as vertex register 30 (`0x8733C6`). Without shaders it sets the Gx fog
  start, end and colour, which the D3D9 backend turns into `D3DRS_FOGSTART` (`0x6A8E61`, `6A 24`), `FOGEND` and
  `FOGCOLOR` (`0x6A8E9F`, `6A 22`) with linear vertex fog (`0x6A3AB5 6A 03 68 8C 00 00 00`). The engine review
  extracted the client's M2 and WMO vs_2_0/vs_3_0 shaders from its MPQs: they fog by planar view depth, `oFog =
  min(pow(max(c30.x·z + c30.y, 0), c30.z), 1)` with `z` from `dp4 r0.z, c33, v0`, and the ps_3_0 combiners blend
  `lerp(c2, colour, fog)`; c2.w is the alpha-test reference, which `0x873BA0` re-uploads with the colour after every
  batch, so the DLL changes the colour only through the setter's argument. The exponent is DayNight `+0x98`/`+0xAC`,
  light float band 2 (1.0 in about 97% of the rows), doubled under water (`0x7F1A09`–`0x7F1A1B`, `fmul 2.0`,
  `0x7F1A13 D9 1D 98 8B D3 00`); the transparent fog forces it to 1. Each WMO render resets its fog cache
  `[0xCFBEB0]` (`A3 B0 BE CF 00` at `0x7A93C2`, `0x7AC702` and `0x7ACA51`), so a fitted c30 left by the last M2
  batch does not reach the next frame's WMO batches; terrain, liquids and detail doodads use other registers.
- Weather fog. The rain `0x78A640` sets its own grey fog colour and uploads vertex c0 = (−0.2, 15, 1, 0)
  (`0x78A7C7` `fmul [0xA3EBDC]` = −0.2, `0x78A7DE` `fld [0x9E8D7C]` = 15), a fade by height above the camera; the
  weather code writes only the Gx fog colour and enable, never the start and end, so weather drawn with the
  fixed-function fog takes the range `0x781610` set from fog group 0 when the liquid pass began (inferred).
- Extensions.dll. The engine review of Ascension's Extensions.dll (not re-read in this change) found detours of the
  glare pass `0x7F0870` (7 bytes; the hook calls the original, then draws a second moon, `Textures\moon02Glare.blp`,
  through `mov eax, 0x9AC400; jmp eax`), of the M2 functions `0x81F700`, `0x81F8F0`, `0x81F970`, `0x823ED0`,
  `0x823F10`, `0x824ED0` and `0x824FC0`, of `0x7F2790`, `0x77EED0`, `0x7EECC0` and `0x7F3230`, a data patch at
  `0x82080B`, and the DayNight exponent stores at `0x7F1777`, `0x7F19F4`, `0x7F1A13`, `0x7F1A1B` and `0x7F1A49`
  patched out for a Lua setter of `[0xD38B98]`. None touches `0x4F9170`, `0x81FD15`, `0x873210`, `0x4F9213` or
  `0x4F9281`, and none overlaps the checked byte runs. Calling `0x7F0870` from the liquid thunk enters the detour,
  so the second moon is drawn before the fog as well. At `LogLevel=2` the log says what `0x7F0870` begins with.
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
- Not used. `0xD38B98` is fog group 0's exponent, which Extensions.dll overrides (the fog end is `0xD38BA8`), and
  `0xD38C9C` is a near-constant model lighting direction, not the visible sun, so the fog's light direction
  follows the sprite positions.

## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. On laptops whose F-keys send media
keys by default, hold Fn (the log names the key that arrived). The window edits every setting below except
`Enable`, `EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change, except `Multisampling`
(see Settings). **Save** writes the settings changed in the window back to `CoAVolFog.ini`, keeping the comments
and every other line; settings not changed in the window keep what the file holds. **Revert** reloads the file,
and a hand edit of the file also replaces unsaved changes. The window shows whether the fog drew in the last
frame, or why it did not, and whether the game's multisampling is kept, or why it is off.

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

This runs the comment check and `vfog_harness`, which creates a real D3D9 device through the wrapper with the client's
flags (`0x52`, auto depth D24X8), renders a Z-up test scene with the client's projection convention, and runs the fog
passes through the same entry the hook uses. Its checks, in `tests/`, cover the device wrapper and state restoration,
depth and sky handling, the transparent fog (the fitted stock fog against the volumetric transmittance through the
client's planar-depth formula, blend-mode colours, the M2 and glare thunks, an additive effect drawn through a copy of
the client's M2 fog shader after the early composite, which the fog must dim by its own distance, the fallbacks to one
composite after the world, and the early composite on a 4x device), Classic light blending and slot selection, the
authored noise, glow and grading the Classic data resolves, the harbour's sunset halo hue with the shipped settings, the
march against CPU integrals at every quality, the authored noise against the modern curve and a CPU sample of the noise
volume, the noisy composites' full-resolution march at thin silhouettes, split sample sides included, temporal filtering
and upsampling, point lights and interiors, the text and liquid depth overrides, fog-data validation, the GPU timer and
depth probe, the settings window and INI saving, and `Reset`. The water suites check the water data and its loader, the
FFT against a double-precision reference, the liquid classification, the water pass driven through the hook entry points
(state restoration, stencil tagging, optics against a CPU reference, fault recovery) and the water settings. The
multisampling suite creates a 4x device through the wrapper with the client's D24X8 depth (and D16) and its
target-and-depth clear: the sample counts offered to the game, the kept back buffer and the D24S8 depth that replaces
the stencil-less one, the fog and water on the copied depth against the drawn depth and a single-sampled frame, the fog
blended by coverage at a silhouette in both blend modes and with a Classic layer's authored noise, `Reset` 4x→1x→4x, the
cost of the game's multisample list, and the fallbacks (`Multisampling=0`, no copy method, a failing self-test). It
expects the copy method the DLL's own probe finds; without one it prints a `SKIP` line with the probe's reason instead
of the 4x device checks. They do not establish in-game appearance or performance. It writes `before.png`, `after.png`,
`overlay.png` and the debug views to `build/harness-out`.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration. `--classic-phase 1` renders it with
`ClassicPhase=1` and `--storm 1` in a full storm. The CPU integration takes layers with authored noise at their mean
density, while the GPU samples the noise, so in a storm a single probe's opacity differs from the CPU column where the
noise is patchy (by up to about 0.2 in the harbour's storm probes). The DLL reads the `fogdata.bin` beside it, so a
new file must be copied there as well as named with `--data`.

`vfog_harness --scene performance` times the fog passes at 1920×1080 at each quality on one street: derived
layers with no point lights, eight flood lights or eight street lamps, Classic layers at the harbour with and
without the lamps, the same in a full storm, whose near layer carries authored noise (both skipped if `fogdata.bin`
beside `CoAVolFog.dll` does not resolve the harbour), and the shipped `LogLevel=1`. It prints
`quality,case,point_lights,median_ms,p95_ms` of GPU time; this is a controlled renderer cost, not a game frame-rate
test. `--samples 4` creates the device with 4x multisampling, as the game's option would, so the times include the
depth copies and the silhouette split; the first lines say whether multisampling was kept.

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

Each device creation and each `Reset` of a fog device logs the adapter (description, vendor and device IDs,
driver version), the sample count and quality the game requested and the count used, the depth format it
requested and the one bound (`INTZ` single-sampled, D24S8 in place of the game's D24X8 multisampled), and either
the depth copy method with its self-test result or why multisampling is off (`Multisampling=0`, the game's option
at 1x, no copy method on this driver, a stencil-less depth without D24S8, a failed self-test). When the game's
Video options ask for a sample count, the log says whether multisampling is offered or hidden and why; the line
is repeated only when that answer changes.

The depth probe logs raw depth, distance and fog opacity at 25 points on frame 60, then every 60 s up to five
times (every 30 s without limit at `LogLevel=2`); its rows are read back on a later frame. The first reason a
fog draw is skipped is logged as `fog skipped: <reason>`; a camera under water is logged once as `fog idle`.

With `TransparentFog=1` each reason the fog is drawn after the world instead is logged once, for example
`transparent fog: the fog is drawn after the world because camera under liquid`, and an early composite that fails
is logged with its reason, like the fog's skips, as `transparent fog: the early composite was skipped: <reason>; the
fog is drawn after the world`. At `LogLevel=2` the transparent
fog hooks count every 60 s, even with `TransparentFog=0`: M2 batch fog calls before the liquid pass ends, after
it and outside the world render, how many were rewritten, the share of lighting, black, white, grey and other fog
colours, the share of fog exponents at, below and above 1 with the lowest and highest, the glare pass drawn
before the fog, at its own call and skipped there, and what the glare pass entry `0x7F0870` holds (the 12340 code,
or a jump into a detour).

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
| `TransparentFog` | 0 | 1 fogs see-through effects by their own distance, 0 with the scene behind them |
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
| `Multisampling` | 1 | Keep the game's Multisampling when its depth can be copied; 0 = off as before (see below) |
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

Turning `Multisampling` on from 0 also needs a restart: the game builds its Video options' Multisampling list
once per start (see Engine inputs), and with 0 that list holds only 1x. Turning it off applies the next time the
game resets or recreates its display, for example after changing Multisampling or the resolution.

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
- Transparent fog is opt-in (`TransparentFog=0` by default) until an owner test in the game; the harness checks the
  equations, the hook plumbing and the device state, not the look. With `TransparentFog=0`, and in the fallbacks (under
  water, `StockFog=0`, a debug view, the sun marker, a failed early composite), see-through materials without depth
  writes are fogged at the depth behind them. With `TransparentFog=1`: WMO alpha batches (windows) and M2 pass 2
  (see-through models beyond a water plane, and particle emitters with flag 0x40000) are drawn before the composite and
  keep the fog of the surface behind them; lightning, missile arcs, the barrier effect and world-name text are drawn
  after it unfogged, as in the stock client, and weather without fog but the rain's own height fade, because its draws
  take the stock fog range, which stays pushed out of range (inferred; the stock client fogs weather that uses it by the
  zone fog); the linear fit cannot follow height fog or the distance curves, so see-through effects well above or below
  the view and far from the view axis get less exact fog, and models fading in or out, which M2 pass 1 draws while their
  alpha is below 1, take the line even far beyond 100 yd, where it can be denser or thinner than the volumetric fog
  around them; the glare and its occlusion query run before M2 pass 1, so depth writes of fading models no longer
  occlude it; the god rays read the depth copied at the early composite; and the fitted colour is not glow
  pre-compensated per pixel for bright effects.
- Interior treatment follows the camera's transition weight, not rooms or portals along each ray, so views
  through doorways may differ.
- The `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off there). The settings window also
  needs a fog device, so it is missing when INTZ depth is unsupported.
- Multisampling is harness-checked only, with NVAPI on the RTX 2060; how the client restarts its display after a
  Multisampling change, the in-game cost and the look need an owner test. The `RESZ` path (AMD, Intel, DXVK
  reporting AMD) has not run on hardware; its self-test decides. DXVK on NVIDIA reports NVIDIA without an NVAPI
  depth copy and stays single-sampled. The multisampled depth is D24S8 where the game asks for D24X8 (or D16); a
  driver without multisampled D24S8 keeps the game single-sampled. The depth copy holds one sample per pixel, so
  the water shades a partly covered edge pixel with that sample's depth. Multisampling does not smooth
  alpha-tested leaves and grass (alpha-to-coverage is a follow-up), and without a copy method no post-process
  antialiasing replaces it (SMAA is a follow-up).
- The silhouette split sees only the one-sample copy of the 3×3 neighbourhood: geometry thinner than a pixel that
  no copied sample hits takes the far fog, a pixel with three depth layers is split in two, and over the scene
  copy both sides blend with the resolved scene colour. On the RTX 2060 Max-Q at 1920×1080 (performance scene,
  `--samples 4`) the depth copy adds up to 0.3 ms to the fog passes and the split 0.2 to 0.8 ms, the most with
  eight large point lights.
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
