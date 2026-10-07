# Third-party art in Semu

Semu-owned code is covered by the repository's MIT LICENSE. Third-party code,
patches, icons, shaders and artwork retain their own terms; see
`packaging/LICENSES.md`. The MIT grant does not relicense them.

The repository contains bezel recipes and upstream pins, not bezel artwork.
Personal builds fetch the pinned originals and generate local adaptations.
Neither those adaptations nor a bundle containing them is cleared for public
distribution. Keeping generated files out of Git does not make a tarball or Nix
binary cache redistributable. Upstream licences accompany the staged layers.
Runtime colour and lighting effects do not change the source artwork's licence.

The unused menu-font atlas was removed because its provenance could not be
established. Current menu text does not use that atlas.

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
- Patched at build time: the `patches` in `config/assets/shaders.json` apply Semu's own diffs
  (`config/assets/shader-patches`) to five files of the staged tree, each pinned before and
  after: `handheld/shaders/lcd-cgwg/lcd-grid-v2.slang` (its edge fetch clamped onto the picture),
  and `handheld/authentic_gbc.slangp`, `handheld/agb001.slangp`, `handheld/gameboy.slangp` and
  `handheld/gameboy-pocket.slangp` (one wrap mode line each), so a picture's edge pixels are
  never lit from the black past it.
