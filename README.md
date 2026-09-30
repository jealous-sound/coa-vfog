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
  (`WaterZoneColors`). Players, creatures, pets and mounts that wade, swim or jump in leave trains of expanding
  rings (`WaterRipples`) in place of the client's flat splash and wake sprites (`WaterClientSplashes`). Magma,
  slime, custom non-water liquids and water seen from below keep the client's look.
- **Forever glow and colour grading** (both off by default): the client's full-screen glow can take WoW Forever's
  per-light glow amount (`ForeverGlow`), and the 3D view can be graded with Forever's per-light colour curve
  (`ColorGrading`), wherever Classic lights cover the camera.

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
bleach bright fog to white, so fogged pixels are pre-compensated with the glow amount the glow composite receives
this frame. God rays use the remaining display highlight range after the glow and need a scene copy; without one
they are omitted.

**Glow and colour grading.** Both use the existing hooks and are off by default. The client converts its day/night
glow to a byte and stores it as the alpha of the glow composite's pass colour, which the composite draws as its
vertex colour (see Engine inputs). With `ForeverGlow=1` the DLL rewrites that byte in both of the glow effect's pass
lists just before the frame effects run, to `lerp(client byte/255, Forever glow, t)` clamped to 0..1, with
`t = saturate((coverage − 0.5)/0.5)` from the Classic lights' share of the blend, so the glow fades back to the
client's where Classic coverage ends and Forever's data takes over only under full coverage. It never touches the
day/night glow itself, so the ghost view (which reads it) and Ascension's Extensions.dll keep theirs, and it writes
nothing while the ghost effect is current or the effect graph differs from the client's (vtables, pass counts). At
the frame end each byte that still holds the DLL's value gets the client's back, so the client's second FFX path
and a byte someone else wrote are untouched. `GlowCompensation` always uses the byte the composite receives,
which also fixes a mismatch without `ForeverGlow`: Extensions.dll's `ambientGlow` can raise the glow above 1, where
the client's byte wraps (1.1 gives 24) while the compensation clamped it to 1.

`ColorGrading` grades the 3D view at the end of the world render, after the glow and before names, icons and the
interface: the saved world viewport is copied (a resolve when the back buffer is multisampled), then one ps_3_0 pass
maps each channel through the blended 32-entry grading curve, interpolating linearly between entries as a 32³ LUT
does per channel, and writes `lerp(scene, curve(scene), ColorGrading·t)` back without depth. The curve is a 32×1
float texture that is updated only when the curve changes. The ghost view and the view from under water are not
graded, and neither is a frame whose world did not finish its frame effects. Where none of the Classic lights around
the camera has a grading key the curve is the identity, so the pass does not draw. The copy is released while
`ColorGrading=0`; while grading it costs one world-sized render target of the back buffer's format.

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

**Ripples.** Units in the water disturb it the way Forever's PBR water does, from the client's own contact state
and ripple events:

- *Contacts.* When the water pass arms, the DLL walks the client's visible objects (read-only, see Water contacts
  below) and keeps the 32 units nearest the camera target, within 48 yd, whose world entity reports the liquid
  surface crossing their body; their depth must stay below the client's own limit of max(1, 2·height).
- *Rings.* Each unit is tracked by GUID, and a track unseen for 0.5 s is dropped. The client's own ripple emitter
  keeps running for units in the water whether or not its sprites are drawn, and after each emission stores the time
  of the next one (see Water contacts below). Every change of that time to a new non-zero value is one ripple event,
  which becomes one ring impulse at the unit's position with the client's parameters: the kind from the movement
  flags (0xF moving, 0x30 turning, otherwise idle); the size clamp(scale / 3 · U(0.9, 1.1), 1/3, 5/3) yd times the
  depth strength, which falls from 1 at half of max(1, 2·height) to 0.5 at that depth, and 0.6 of that when idle;
  and the alpha, the strength, 0.8 of it when idle. The stamp's radius is the size, at least two texels (0.25 yd),
  and its depression 3 per unit of alpha; U is a hash of the GUID and the event's time in place of the client's
  `rand()`. The client emits every strength · 2.5 / min(speed, 20) · 250 ms while moving (89 ms at a 7 yd/s run)
  and every 400 to 449 ms otherwise, so a unit standing in the water keeps making small rings. A unit that is not
  swimming adds an entry splash when its depth crosses 0.4 of its height in either direction, the client's own
  splash rule; it takes the place of that frame's event (the client's 0xC9 splash) and has the ring's depression
  times the strength over twice the collision radius (0.5 to 6 yd). The client keeps that previous depth for the
  unit's lifetime, so a dropped track leaves its last one behind for 30 s and a track created again (after a dive,
  or back in range or among the nearest 32) starts from it. A unit never seen before starts from its current depth
  when it swims, and otherwise from 0, approximating the client's zero initial depth.
- *Simulation.* The disturbances drive Forever's local displacement recurrence, `next = 0.97·edge·(0.5·(L + D + R +
  U) − C.g)`, output `(next, C.r)`: R is the current height, G the previous one, and the edge ramp
  `saturate(16·min(u, 1 − u, v, 1 − v))` is taken on the shifted uv. It runs on a G16R16F ping-pong map
  (A16B16G16R16F where G16R16F cannot be rendered to or filtered) of 512² texels at 0.125 yd, a 64 yd window
  (256², 32 yd at `WaterQuality` 1), stepped at a fixed 30 Hz from the real clock, at most four steps a frame (the
  rest are dropped). The window follows the camera target by whole-texel shifts, so ripples stay put in the world;
  a target that is not finite or lies beyond 100000 yd leaves the window where it was.
  The first step after an event injects its ring as `A·(1 − smoothstep(0.20, 0.79, d/r))` around its point, an
  analytic fit of Forever's `PERTURBTEX`, so no Blizzard texels are shipped. The recurrence carries a ring outwards
  at 1/√2 texel a step, about 2.65 yd/s: about 2.7 yd after 1 s and 4 yd after 1.5 s, fading by 0.97 a step. A
  walking unit (2.5 yd/s) leaves trains of concentric rings; a running one (7 yd/s) outruns its rings, so they fold
  into a V-shaped front with arcs inside, as behind a real wader.
- *Client sprites.* The client draws its own ripples as flat `splash.blp` discs and `wake.blp` V trails after the
  water (`0x79D5E0`). With `WaterClientSplashes=0`, at the end of every frame whose modern water was shaded with
  `WaterRipples` above 0, the DLL holds the client's `waterRipples` value at 0, so no new sprites are made and the
  live ones run out within 0.7 s. It holds the value only while rings can be made: not when the unit walk is refused
  (a client whose object-manager bytes differ from the 12340 image) or the ripple map is unavailable (no filterable
  16-bit floating-point render target, a ripple shader the device rejects, or a failed map creation, which is
  retried when a unit next makes a ring). It remembers the value it replaced and any non-zero value set meanwhile
  (for example `/console waterRipples 1`) and puts it back at once when a setting ends the hold, the rings become
  unavailable, the water is turned off or stops after a fault, after 1 s without shaded water (camera under water,
  no water in view), and when the device is released or the DLL unloads. The value is global, so the hold hides the
  sprites of every unit, while rings only come from the 32 units nearest the camera target within 48 yd and only
  show within 24 yd of it along either axis, fading out by 28 yd (10 and 14 yd at `WaterQuality=1`), on liquids the
  modern water shades: a unit beyond that reach, a 33rd unit, or one wading in a liquid left to the client makes no
  wake at all.
- *Shading.* The shading ports the one-map slope of Forever's PBR prepass (7552035): the height `lerp(G, R, w)`,
  forward differences not divided by the texel, `(dx, dy)/√((1 + dx²)(1 + dy²))`, times 3·`WaterRipples`·fade.
  Forever takes `w` from its host, whose packing is not recovered; here it is the fraction of the next 30 Hz step
  that has elapsed, so the shaded height moves smoothly between steps. The result is added to the wave slope sum,
  so the normals, the slope variance (which ripples lower, as in Forever), refraction, reflection and the foam UV
  respond, and ripples make no foam. Forever fades linearly from the window centre, which would make the window
  size a gain; here ripples keep full strength to 4 yd inside the propagation border and fade out across those
  4 yd, so the two window sizes differ in reach, not strength. They also fade where a pixel covers more than two
  texels.
- *Lifetime.* The simulation runs only on frames that shade water, counts in the water GPU time, stops 15 s after
  the last disturbance, and restarts on a map change, a device `Reset`, a size change and after more than a second
  without shaded water. Without units in the water nothing is simulated and the ripple map is not sampled.

All ps_3_0 samplers were in use, so the three wave-foam masks share one RGB texture (high, mid and low foam in red,
green and blue); the harness shows each channel filtering exactly like the separate L8 mask it replaces, and each
foam layer shading from its own channel.

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
tags and shading are tested per sample, so water edges are antialiased too. The wave FFT and the ripple steps render
to single-sampled maps with no depth surface bound, and the shading samples them as it does without multisampling.

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

Each light params also carries glow and colour grading, which `ForeverGlow` and `ColorGrading` apply (both off by
default; see Glow and colour grading). They are blended from the Classic lights around the camera like the fog, but
on every map with a placed light, not only where Classic fog applies: 219 light params without fog but with glow are
placed on 3.3.5 maps, 167 of them only on the 17 maps without Classic fog, such as Blackwing Lair (469) and
Stratholme (329). `AuthoredFog::coverage` is the Classic lights' share of the blend, from which both fade toward the
client's own look; where no Classic light reaches there is no glow, and where none of the blended light params has a
graded key the curve is the identity and `AuthoredFog::hasGradingCurve` is false.

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
  depth writes), world-name text (`0x7E5818`, suppresses depth writes), and before the frame effects
  (`0x4F9281`, draws the fog over the completed world). The original bytes are checked first; on any mismatch
  nothing is patched. Two more retarget the far-clip clamp calls (`0x780810`, `0x781444`) when `FarClipMax` is
  set at start-up, independently of the fog hooks.
- **Water hooks.** Installed after the fog hooks and independently of them: the water pass call (`0x790AA2`) and
  the `Render` slots of the two water material vtables (`0xA5954C`, `0xA59580`, read-only data patched under
  `VirtualProtect`). The call site, both slots and the layout bytes the classification reads are checked first; on
  any mismatch none are patched and the fog is unaffected. `waterdata.bin` is loaded only once they are installed. A
  water exception restores the device state, turns water off for the session and leaves the fog running.
- **Glow and grading.** No further patch: the glow override runs in the world-done hook before the fog, and the
  grading last in the world render's frame-end hook. Once the fog hooks are installed, each is enabled only if its
  client bytes still match (see Engine inputs); a mismatch leaves it unavailable and is logged. Neither depends on
  the fog: the world viewport is captured after the opaque pass even after a fog exception, and an exception in the
  glow override or the grading turns only that one off for the session.
- **State.** Every state the passes touch is captured with a recorded state block and restored, plus render
  targets, depth and stream 0 (whose offset state blocks drop). The client's shader-constant cache stays valid:
  the pixel constants a pass sets, the grading's c0 and c1 included, are recorded in its state block, because the
  client's Gx constant setter (`0x6833E0`) uploads only values that differ from its own copy.
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
| Unit movement flags, current speed | `+0x7CC`, `+0x814` |
| Unit collision radius and height | `+0x850`, `+0x854` |
| World entity: world position, liquid flags, liquid surface | `+0x6C`, `+0x7C`, `+0x80` |

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
- Native glare. `0x4F9213` (`E8 58 76 2F 00`) calls the sun/moon glare pass `0x7F0870`, which is not patched.
  Its draw `0x9AC400` adds the `sunGlare`/`moonGlare` quads (`D3DBLEND_SRCALPHA`, `D3DBLEND_ONE`) at depth
  `[0.9990234375, 1]`, and its update `0x9AC3C0` runs a GPU occlusion query against the current world depth
  (`0x9ABE00`). World geometry `0x7984A0` also runs after the opaque hook (`0x4F9154` calls `0x77F010`, which
  jumps to it). Drawing the fog once at `0x4F9281` sees the completed depth and keeps the glare and its query
  order unchanged.
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`)
  and the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`); the
  ghost effect is `[0xB74368]` (`FFXDeath`, vtable `0xA418D8`, stored at `0x7EA274` by its constructor `0x7EA260`).
- Glow feed. The world frame callback `0x4FAF90` updates the world (`0x4FB031`: `0x4FA5F0` → `0x7831A0` →
  `0x7816F0` → `0x7F3920` → `0x7F3230`) before it calls the world render (`0x4FB03D`), so the day/night glow
  `0xD38C2C` (`0x7ECEF0()` + `0x12C`) is rewritten every world frame: from the blended light record (`0x7F34AF`), or
  0.5 on the path without lights (`0x7F35A6` loads `[0x9E2EC4]` = 0.5, which stays on the x87 stack for the `fst` at
  `0x7F3600`). In the world render, `0x4F8F3D`, the only call of `0x4F8770`, feeds the glow effect only while it is
  current (`0x4F8773`–`0x4F8781`): it turns the glow into a byte with the +512.0 trick (`0x4F878E`–`0x4F87AC`,
  floor(255·g) mod 256, so 1.1 gives 24) and passes {underwater, glow byte, blur byte} with id 3 to the effect's
  SetParam (`0x4F883C`; vtable `0xA941C8` from `0x8BFE98`, slot `0xA941D8` = `0x8BFDE0`). SetParam stores the
  underwater flag at `+0x2C` and the D3DCOLOR {B = G = R = blur, A = glow} at `+0x30` of pass [2] in both pass
  lists, clear view at `+0x08` (`0x8BFE14`, `0x8BFE21`) and underwater at `+0x1C` (`0x8BFDF2`, `0x8BFE08`), and
  returns with `ret 8` (`0x8BFE26`). A list is {capacity, count, array} (`0x7EA1C0`). The glow constructor
  `0x8BFE80` fills both lists with the same three passes: `0x8C1F70` (vtable `0xA94274`, `0x8C1F91`), the
  `FFXGauss4` blur `0x8C1B40` (vtable `0xA9425C`, `0x8C1B61`) and the composite `0x8C21E0` (vtable `0xA94294`,
  `0x8C2206`), which the underwater list gets only when `[0xD45768]` and a Gx capability allow (`0x8C00BC`–`0x8C00D4`;
  otherwise a null entry, `0x8C011F`). The composite's render `0x8C27B0` hands that colour to the draw `0x682400`
  (`0x8C28A1`).
- FFXGlow. The 12340 shader loader `0x684970` accepts only BLS version `0x10003` (`0x6849FE`). Ascension's patch
  ships ps_3_0 `FFXGlow` and `FFXDeath` as version `0x10004`, so the loader falls back through `0x684AA4` to the
  ps_2_0 profile (`0x684A71`), whose FFXGlow is `lerp(screen, blur, v0.z) + blur²·v0.w` with the pass colour as
  `v0`: the alpha byte is the glow weight. FFX end `0x8C1010` copies the default colour surface (`0x6A30D0`,
  StretchRect from Gx `+0x3B3C`) and runs the passes; every pass end rebinds the default target (`0x8C15A7`), and
  the composite's target descriptor `0xD45784` is only ever cleared (`0x8C056D`, `0x8C14E6`), so the composite draws
  into the back buffer.
- Other glow readers and writers. The ghost effect's composite `0x7E87B0` reads `0xD38C2C` while FFX end draws
  (`0x7E8898`, rounded, not floored) for `screen + g·blur²` before it desaturates; `0x4E442E` (in `0x4E3CD0`) also
  writes it, and the client's second FFX path (begin `0x4E61ED`, end `0x4E621E`) calls SetParam itself
  (`0x4E61EB`). The image holds no other access. Ascension's Extensions.dll (TimeDateStamp `0x6ABAD5C2`, read
  statically) detours the LightParams getter `0x7EB180` and multiplies the glow (LightParams `+0x10`, which becomes
  the light record's `+0x58` and then `0xD38C2C`) by its `ambientGlow` CVar, so the glow can exceed 1; it also
  detours the light blend `0x7F3230` (it copies the moon position `0xD38E48` to `0xD38E68` afterwards) and
  `0x681F60` (only for calls from `0x95EF4A`), and adds day/night lights through `0x7ED150`. It references none of
  `0xD38C2C`, `0x4F8770`, `0xB74364`, `0xD45780` or the FFX code.
- After the frame effects. FFX end is the world render's last draw: it only runs `0x747AE0`, which clears flag
  `0x1000` along the list `[0xCA1368]`, and returns (`0x4F9286`–`0x4F9290`); `0x4FAF90` then draws names and icons
  (`0x4FB042` → `0x7E5140` → `0x7E7490`) and the interface follows. The world render returns early at `0x4F8EDC`,
  `0x4F8EEF`, `0x4F8F0D` and `0x4F8F1E`, and `0x4F90F4` skips FFX end (and the world-done hook) when there is no
  world M2 scene; the grading runs only in frames that reached the world-done hook.
- Glow and grading guards. The glow override requires `0x4F883C`, `0x8BFDE0`, `0x8BFDF2`, `0x8BFE08`, `0x8BFE14`,
  `0x8BFE21`, `0x8BFE26`, `0x8BFE98`, `0x8C2206`, `0x8C28A1`, the world render's tail `0x4F9286` after the patched
  FFX end call, and the slot `0xA941D8`; the grading requires `0x4F9286`, `0x747AE0`, `0x4FB042` and `0x8C15A7`. Both
  also require the world render and world done calls to still reach the DLL, and none of these bytes is one the
  hooks patch. Each frame the glow override also checks the effect graph: the current effect is the glow effect,
  its vtable is `0xA941C8`, both lists hold at least three passes and pass [2]'s vtable is `0xA94294`.
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
- Water contacts. `0x4D7750` stores the new object manager at `[[0xC79CE0] + 0x2ED0]` (`0x4D77A9`,
  `8B 0D E0 9C C7 00 89 81 D0 2E 00 00`); `[0xC79CE0]` is null before login and is checked before use. The visible
  enumeration `0x4D4B30` starts at `[mgr + 0xAC]` (`0x4D4B44`) and steps to `[obj + [mgr + 0xA4] + 4]` (`0x4D4B80`),
  with the link offset 0x38 set by the constructor (`0x4D6193`, `0x4D6233`); a set low bit ends the list, and each
  link's `[obj + 0x38]` holds the address of the previous link (the head link `mgr + 0xA8` for the first object), which
  the walk checks. The GUID is at `obj + 0x30` (`0x4D4B6D`) and the type mask at `[[obj + 8] + 8]`, unit bit 0x8
  (`0x4D4DF1`). The unit constructor points `+0xD8` at the embedded movement block `+0x788` (`0x73F67A`), which the
  walk requires. From that block: the raw position `+0x10` (unit `+0x798`, vtable slot `0x30`, `0x6E6F13`), which
  is transport-local while the transport GUID `+0x8` (unit `+0x790`, slot `0x40`, `0x6E6F70`) is set; the movement
  flags `+0x44` (unit `+0x7CC`, swimming 0x200000 at `0x730DA2`); the current speed `+0x8C` (unit `+0x814`,
  `0x71CCA8`); the collision radius and height `+0xC8`/`+0xCC` (unit `+0x850`/`+0x854`, stored at
  `0x6E95AF`/`0x6E95CE` from `CreatureModelData` times the scale). Every loaded object keeps its world entity at
  `obj + 0xB8` (`0x7438B0`; null until the model loads). `0x780240` copies the model's world translation to the
  entity's `+0x6C`..`+0x74` (`0x7803BD`, `89 79 6C D8 65 BC 89 59 70 89 51 74`); that this is the feet in world
  space, transports included, is an inference, used only for passengers. The entity's liquid refresh `0x7A1BC0`
  (called at `0x7804E9` in `0x780240` and from the unit update through `0x77F2E0`/`0x7A1E90`) clears 0x369
  (`0x7A1BCD`), queries the liquid at `+0x6C`, sets 0x20 with the surface at `+0x80` (`0x7A1C19`), sets 0x40 when the
  surface is at or below the entity box top `+0x5C`, and only then copies `LiquidType` flag bits 0/1 into
  0x100/0x200: when the surface plus 0.01 lies above the feet `+0x74` (`0x7A1C53`), `0x782560` finds the area,
  `0x9905C0` the row, and, unless the row's flag bit 2 is set, the surface lies strictly above the feet
  (`0x7A1C94`). So 0x100 means that the surface crosses the body in a liquid with flag bit 0 (lake and ocean log
  flags 0xF); units the client does not update may keep stale flags. The client's own ripple emitter
  `CGUnit_C::UpdateWaterRipples` `0x71CBA0` uses the same gate (`0x71CBE3`), emits only while surface − z is below
  max(1, 2·height) (`0x71CC28`) and scales its strength from 1 at half that depth to 0.5 at it; the swim update
  `0x730D10` splashes (`0x730E42`, event 0xC9) when the depth crosses 0.4·height (`0x730DED`, compared with `+0x784`
  at `0x730E0C`) in either direction and stores it at `+0x784` (`0x730E4A`), only while the swimming flag is clear;
  the depth is 0 unless `0x77F1E0` finds liquid (`0x730D42`–`0x730D63`), and the unit constructor zeroes `+0x784`
  (`0x73F6BF`, `D9 9E 84 07 00 00`, after `fldz` at `0x73F6BA`). The emitter reads the unit's next-ripple time
  `+0xA58` (`0x71CC71`, `8B 86 58 0A 00 00 85 C0 8B 0D AC 76 CD 00`) and returns while it is non-zero and
  `[0xCD76AC]` minus it is negative (`0x71CC84`); a non-zero event argument clears it first (`0x71CBFC`). After the
  emission call `0x71CF16` (`E8 E5 24 06 00`, to `0x77F400` and `0x79D460`) it always stores the next time: for the
  moving kind (the kind table `0xADAC00` = 0 0 1 0, read at `0x71CC9C`) now − trunc(e · 2.5 / min(speed, 20) · 0.25
  · −1000) (`0x71CF58`, `89 86 58 0A 00 00`), otherwise now + 400 + (rand · 50 >> 32) (`0x71CF8D`,
  `89 96 58 0A 00 00`). The `waterRipples` gate is tested only inside `0x79D460`, so the time advances whether or not
  a sprite is made. The unit constructor zeroes it (`0x73F8A4`, `ebx` cleared at `0x73F69A`), and the only other
  store with displacement 0xA58 in `.text` (`0x875547`) writes a stack structure in `0x8753F0`. The kind comes from
  the movement flags (`0x71CC0B`, `8B 96 D8 00 00 00 8B 42 44 A8 0F`, then `A8 30` at `0x71CC1F`); the size from the
  object scale, vtable slot `0x3C` = `0x4D5F00` (`8B 41 08 D9 40 10 C3`, descriptor `+0x10`), times 1/3
  (`0x71CD08`, `8B 16 8B 42 3C 8B CE FF D0 D8 0D 98 20 A1 00`) and 1 + 0.2 · rand − 0.1 (`0x9E8D84`, `0xA349F0`),
  clamped to 1/3..5/3 (`0xA12098`, `0xA0B634`) and multiplied by e; the idle kind takes 0.8 of the alpha, 0.25 of
  the growth and 0.6 of the size (`0x71CE1D`–`0x71CE3A`), and every alpha is e / 6 before `0x79D460` multiplies it
  by 6. The emitter's facing comes from slot
  `0x34` (`0x6E6F40` → `0x4F42A0`), which for a passenger adds the transport's facing through an object lookup and a
  virtual call (`0x74B590`); the capture calls no client code and needs no facing, since the rings are
  round. Before its first walk the capture checks the timestamp, the base and the instruction bytes at
  `0x4D77A9`, `0x4D4B44`, `0x4D4B80`, `0x4D6193`, `0x4D6233`, `0x4D4B6D`, `0x4D4DF1`, `0x73F67A`, `0x6E6F13`,
  `0x6E6F70`, `0x730DA2`, `0x71CCA8`, `0x6E95AF`, `0x6E95CE`, `0x71CC28`, `0x7438B0`, `0x77F1EA`, `0x77F230`,
  `0x7803BD`, `0x4D5F00`, `0x71CD08`, `0x71CC0B`, `0x71CC71`, `0x71CF16`, `0x71CF58` and `0x71CF8D`, and logs the
  first mismatch. Reads are SEH-guarded with pointer and range checks; a link whose back
  pointer does not match rejects the frame, the walk keeps what it found when it reaches 4096 objects, and the
  manager is re-read afterwards. `WaterRipples=0` skips the walk. The client's ripple pool (`0x79D180`, 128 entries)
  and `+0x784` are not read (the tracker keeps its own previous depth), and the `[movement + 0x48]` bit 2 early
  return of `0x730D10` is not mirrored.
- Client ripple sprites. `waterRipples` is a console command, not a saved CVar: `0x7813A4`
  (`68 90 F6 77 00 68 00 E8 A3 00`) registers the handler `0x77F690` under the name at `0xA3E800` through `0x769100`
  (`0x7813AE`), and the handler, the only writer, reads its argument with `sscanf("%d")` into `0xADF7F0`
  (`0x77F696`, `68 F0 F7 AD 00 68 9C 28 9E 00`; the image's value is 1). The only reader is `0x79D463`
  (`83 3D F0 F7 AD 00 00 74 61`) at the top of `0x79D460`, which returns without touching the pool when the value is
  0; its only caller is `0x77F434` (`E8 27 E0 01 00`) in `0x77F400`, whose only caller is the unit emitter at
  `0x71CF16`. The update and draw `0x79D5E0` walks the live list and unlinks each entry whose expiry `+0x28` (now +
  life, stored at `0x79CFC8`; the life is at most 0.7·e s) has passed (`0x79D648`–`0x79D661`) without reading the
  value, so 0 stops new sprites and lets the live ones run out. No other code or data in the image holds the address,
  and Extensions.dll holds neither it nor the emitter's functions (a byte search, not proof that nothing reaches it
  indirectly). Before it binds, the DLL checks `0x77F696`, `0x7813A4`, `0x79D463`, `0x77F434`, `0x71CF16` and the
  expiry test at `0x79D648` (`D9 41 28 A1 58 FB AD 00 D8 1D A4 76 CD 00`) and logs the first mismatch; the value
  lies in `.data`, and every read and write is SEH-guarded.
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

This runs the comment check and `vfog_harness`, which creates a real D3D9 device through the wrapper with the
client's flags (`0x52`, auto depth D24X8), renders a Z-up test scene with the client's projection convention, and
runs the fog passes through the same entry the hook uses. Its checks, in `tests/`, cover the device wrapper and state
restoration, depth and sky handling, Classic light blending and slot selection, the authored noise, glow and grading
the Classic data resolves, the harbour's sunset halo hue with the shipped settings, the march against CPU integrals
at every quality, the authored noise against the modern curve and a CPU sample of the noise volume, the noisy
composites' full-resolution march at thin silhouettes, split sample sides included, temporal filtering and
upsampling, point lights and interiors, the text and liquid depth overrides, fog-data validation, the GPU timer and
depth probe, the settings window and INI saving, and `Reset`. The water suites check the water data and its loader,
the FFT against a double-precision reference, the liquid classification, the water pass driven through the hook entry
points (state restoration, stencil tagging, optics against a CPU reference, fault recovery), the water settings, the
packed foam masks, the unit walk on synthetic object-manager images, the contact tracker's rings against a harness
copy of the client's ripple clock, the ripple simulation against a CPU reference, the ripples in the water pass, whose
normals are compared with the 7552035 slope evaluated on the CPU (within the depth-copy precision, but for at most
0.5% of the pixels, up to 6/255 off where the GPU's bilinear weights meet fresh impulses), the rings' visibility in
shaded water with the real data, and the hold on the client's sprite value, driven through the hooks on a synthetic
value and code image, with the unit walk refused and the ripple map unsupported or failing.
The glow suite drives the world-done and frame-end entries against a fake glow effect graph that holds the client's
vtable values as plain integers the DLL only compares: the write to both pass lists and its restore, a foreign byte that survives, no write
for the ghost effect or a graph that differs, the fade with Classic coverage, the clamp above 1, the compensation
taking the delivered byte (24 for a wrapped 1.1), and guards that avoid every patched byte. The grading suite fills
the back buffer with every 8-bit code per channel and checks the identity curve bit-exact, the Stormwind noon curve
and half strength within D3D's float-to-8-bit tolerance of a CPU reference, the grading of what the glow drew after
the world was done, a sub-rectangle world viewport, state restoration with c0/c1, the skips (off, no world done,
ghost, under water, half coverage, lights without a grading key), curve uploads only on change and after `Reset`, and the INI keys.
The multisampling suite creates a 4x device through the wrapper with the client's D24X8 depth (and D16) and its
target-and-depth clear: the sample counts offered to the game, the kept back buffer and the D24S8 depth that replaces
the stencil-less one, the fog and water on the copied depth against the drawn depth and a single-sampled frame, a wading
unit's ripples in the 4x water (the normals against the same slope at the surface depth the copy holds, one sample per
pixel and up to 1% off the pixel centre's, the shading changed only around the path, and the tagged edges still blended
by coverage), the fog blended by coverage at a silhouette in both blend modes and with a Classic layer's authored noise,
`Reset` 4x→1x→4x, the colour grading of a 4x back buffer against the single-sampled one, the cost of the game's
multisample list, and the fallbacks (`Multisampling=0`, no copy method, a failing self-test). It expects the copy method
the DLL's own probe finds; without one it prints a `SKIP` line with the probe's reason instead of the 4x device checks.
They do not establish in-game appearance or performance. It writes `before.png`, `after.png`, `overlay.png` and the
debug views to `build/harness-out`.

`vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the
Stormwind harbour (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range, and
prints fog opacity and colour at probe points next to a CPU integration. `--classic-phase 1` renders it with
`ClassicPhase=1` and `--storm 1` in a full storm. The CPU integration takes layers with authored noise at their mean
density, while the GPU samples the noise, so in a storm a single probe's opacity differs from the CPU column where the
noise is patchy (by up to about 0.2 in the harbour's storm probes). The DLL reads the `fogdata.bin` beside it, so a
new file must be copied there as well as named with `--data`.

`vfog_harness --scene ripples <dir> --water-data data/waterdata.bin` renders a unit running at 7 yd/s through
shin- and waist-deep water and walking at 2.5 yd/s through shin-deep water on the basin's pebbled beach, seen from
7 yd behind and 4.5 yd above, and writes the shaded frame, the normal view (`-normals`) and the frame with a
negligible ripple gain (`-calm`) at 0.5 s, 1.5 s and 3 s.

`vfog_harness --scene performance` times the fog passes at 1920×1080 at each quality on one street: derived
layers with no point lights, eight flood lights or eight street lamps, Classic layers at the harbour with and
without the lamps, the same in a full storm, whose near layer carries authored noise (both skipped if `fogdata.bin`
beside `CoAVolFog.dll` does not resolve the harbour), and the shipped `LogLevel=1`, then the colour grading pass
alone (`colour-grading`, quality 0). It prints `quality,case,point_lights,median_ms,p95_ms` of GPU time; this is a
controlled renderer cost, not a game frame-rate test. `--samples 4` creates the device with 4x multisampling, as the
game's option would, so the times include the depth copies, the silhouette split and the grading's resolve; the
first lines say whether multisampling was kept. On the RTX 2060 Max-Q the grading took 0.25 ms single-sampled and
0.15 ms at 4x (medians of one run each).

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
adds the resolved glow and grading curve with the two settings that apply them, for example `Classic glow 0.00,
grading curve at inputs 8/31 16/31 24/31: 0.267 0.565 0.890 (ForeverGlow 0, ColorGrading 0.00)` (`grading curve (no
graded light)` where the curve is the identity for want of a graded key), and a line for each layer with authored
noise.

At start-up the log says whether `Forever glow` and `colour grading` are available, or which guarded client bytes
differ. `Forever glow: the glow composite gets 0 where the client set 102 (Classic weight 1.00)` is logged when the
override starts and `the client's own glow applies` when it stops. Grading logs `colour grading skipped: <reason>`
once per reason and its idle states (ghost effect, camera under water, no Classic light covers the camera, no Classic
light around the camera carries a grading curve) like the water's. `LogLevel=2` adds every 600 frames `Forever look: glow byte 0 (client 102, Classic weight 1.00), colour
grading 0.60`, with the grading's reason instead of its strength when it did not draw.

Every 60 s the water adds `water gpu 1.24 ms (median of 3500 frames, 0 skipped), classes lake+ocean, waves 256
(7 tiles), ripples 512 at 0.125 yd, 30 Hz, up to 3 contacts, 0 steps dropped`: its GPU time without the client's
own water draws, ripple steps included; `ripples idle` means no unit disturbed the water, `ripples off` that
`WaterRipples` is 0. `water: the client's splash and wake sprites are hidden while the ripples run (waterRipples 1)`
and `... are shown again (waterRipples 1)` report the hold on the client's sprites; after the first two, these lines
are logged at `LogLevel=2`. `water:` lines name each liquid type the first time it is classified; idle states (no
water in view, camera under water) are logged once and repeats only at `LogLevel=2`. Each settled change from the
settings window, an INI reload or Revert is logged as one line, for example
`settings: WaterFoam 1 -> 1.5, WaterWind 2 -> 4`.

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
| `ForeverGlow` | 0 | 1 gives the client's glow Forever's per-light amount where Classic lights cover the camera |
| `ColorGrading` | 0 | Strength 0..1 of Forever's per-light colour curve over the 3D view; 0 = off |
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
| `WaterRipples` | 1 | Rings from units in the water 0..2; 0 = no unit reads and no ripple simulation |
| `WaterClientSplashes` | 0 | 1 keeps the client's splash and wake sprites; 0 hides them for all units while rings run |
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
- Interior treatment follows the camera's transition weight, not rooms or portals along each ray, so views
  through doorways may differ.
- The `gxApi d3d9ex` path is not wrapped (fog and the settings window stay off there). The settings window also
  needs a fog device, so it is missing when INTZ depth is unsupported.
- Multisampling is harness-checked only, with NVAPI on the RTX 2060; how the client restarts its display after a
  Multisampling change, the in-game cost and the look need an owner test. The `RESZ` path (AMD, Intel, DXVK
  reporting AMD) has not run on hardware; its self-test decides. DXVK on NVIDIA reports NVIDIA without an NVAPI
  depth copy and stays single-sampled. The multisampled depth is D24S8 where the game asks for D24X8 (or D16); a
  driver without multisampled D24S8 keeps the game single-sampled. The depth copy holds one sample per pixel, so
  the water shades a partly covered edge pixel with that sample's depth, and every pixel reads the ripple map at
  that depth along its centre ray (in the harness view up to 1% off the depth at the pixel centre).
  Multisampling does not smooth alpha-tested leaves and grass (alpha-to-coverage is a follow-up), and without a
  copy method no post-process antialiasing replaces it (SMAA is a follow-up).
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
- Ripples are harness-checked only: the unit walk, the ripple clock and the sprite hold have run on synthetic
  memory, not in the client, and their look needs an owner test. The recurrence (0.97, 0.5, the edge ramp), the
  one-map slope with its gain of 3 and the display blend `lerp(G, R, w)` are Forever's; the contact gate, depth
  limit, strength fade, 0.4·height splash rule, swimming flag, ripple cadence, kinds, size clamp, idle size and
  alpha factors and the sprite gate are the client's. That Forever feeds these same client events into its map is
  the lead's reading of its videos, not a recovered fact. Everything else is a prototype choice, not a recovered
  value: the 30 Hz step, `w` as the elapsed fraction of the next step, 0.125 yd texels, 512²/256² windows, the ring
  (a stamp as wide as the client's starting sprite, at least 0.25 yd, 3 deep per unit of alpha, chosen on the
  `--scene ripples` renders so that the 4 yd ring at 1.5 s still shows at `WaterRipples=1` without covering the
  water), the hashed size jitter, the entry splash (the same depth over twice the collision radius, 0.5 to 6 yd),
  the 0.5 s track lifetime, the 30 s memory of a dropped track's depth and the swimming start depth of a new one,
  32 contacts within 48 yd, at most 128 queued rings, the 4 yd fade, the two-to-four-texel detail fade, the 15 s
  stop, the 1 s restart gap and the 1 s grace before the sprites come back. The tracker sees at most one event per
  frame per unit, so two client emissions in one frame make one ring, and it only approximates the client's
  previous depth: a unit that changes depth while it is not tracked, or leaves the water and is back within 30 s,
  can splash where the client does not, or not where it does. A running unit outruns its rings (2.65 yd/s), so its
  wake reads as a V-shaped front with arcs inside rather than separate rings; only a slower unit leaves separate
  ring trains. The harness scene has a flat pebble floor, a clear sky and no character model. While the sprites are
  hidden, `waterRipples 0` typed in the console cannot be told from the DLL's own 0, so the value put back is the
  last non-zero one. The hold is global: while it lasts, units beyond the ripple window (24 to 28 yd from the camera
  target along either axis, 10 to 14 yd at `WaterQuality=1`), beyond the 32 nearest, or wading in a liquid the
  modern water leaves to the client lose the client's wake and get no rings. The footstep spray `0x723A50` (its
  spell-visual call at `0x723CD1`, `E8 CA 56 FD FF`, for depths below half the height) is not hooked; hooking it
  would give rings the footsteps' animation cadence and is a possible later addition. Game objects (boats, bobbers)
  make no ripples, one map serves every water level inside the window, the window follows the camera target (which
  can leave the player in free-look or vehicle views), and FP16 render-target writes on the test GPU truncate, which
  damps ripples slightly more than the recurrence (about 6% of the amplitude over 2 s).
- `ForeverGlow` and `ColorGrading` are harness-checked only. The client's glow is `lerp(screen, blur, blur byte) +
  g·blur²`, while Forever's final composite adds or blends a linear blur term whose amounts are set on its CPU, so
  only a glow of 0 means the same in both, and on continents Forever may still bloom from values the kit does not
  hold. The grading curves brighten midtones and clip the brightest 10–19% of the range; whether Forever feeds them
  its tonemapped image and whether a curve set only on the noon key holds all day are inferred. Text drawn during
  the world render is graded with the 3D view, as the client's glow already applies to it; names, icons and the
  interface are not. The grading keeps its own world-sized copy (about 8 MB at 1920×1080), released while
  `ColorGrading=0`. Both need an owner comparison with Forever captures before either default changes.
- The zone lights' edge fade distance is chosen here: their `TransitionType` is 0 in every row and the modern
  client's transition rule is not known.

## License

CoAVolFog is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`); see [LICENSE](LICENSE).

As an additional permission under GPLv3 section 7, you may link or combine CoAVolFog, including modified versions,
with the World of Warcraft client, and distribute the resulting combination without providing the client's source
code. GPLv3 continues to apply to CoAVolFog.
