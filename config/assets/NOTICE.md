# Third-party art in Semu

Semu's code is MIT. The bezel art it draws is not Semu's and keeps its authors' terms.
The repository carries recipes and pins, never the art itself or anything adapted from it:
the build fetches each pinned upstream, copies the plates it draws verbatim into the bundle
(`share/semu/assets/bezels/layers`, with each upstream's own licence file beside them), and
bakes the few flattened, recoloured and night-lit plates on the machine that builds Semu,
for that machine's own use. Recolours and the night light in the live renderer are drawn at
run time over the unmodified plates.

## Duimon Mega Bezel graphics

- Source: https://github.com/Duimon/Duimon-Mega-Bezel (pinned in `config/assets/bezels.json`)
- Author: Duimon, https://duimon.github.io/Gallery-Guides/
- Licence: Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International
  (https://creativecommons.org/licenses/by-nc-nd/4.0/). No commercial use; adapted material
  may not be shared.
- Used for: the Game Boy, Game Boy Color, Game Boy Advance, DS, 3DS and PSP device plates,
  their lenses and decals, the desk background and the late-night lighting plate.

## Soqueroeu TV Backgrounds V2.0

- Source: https://github.com/soqueroeu/Soqueroeu-TV-Backgrounds_V2.0 (pinned in `config/assets/bezels.json`)
- Author: soqueroeu
- Terms (from its README): may be distributed and reproduced with credit to the authors
  involved; no profit from products containing the material without the author's permission.
- Used for: the living-room television scenes of the NES, SNES, Genesis, N64, PlayStation,
  PlayStation 2, Dreamcast, GameCube and Wii.

## libretro slang-shaders and the Mega Bezel shader

- Source: https://github.com/libretro/slang-shaders (pinned in `config/assets/shaders.json`)
- Licences: per shader, as recorded in that repository. Semu reads the Mega Bezel presets
  and parameters to place the art and runs the CRT and LCD presets through librashader.
