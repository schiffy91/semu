# Licences and distribution

The root MIT LICENSE applies to original Semu code and configuration only.
It does not replace the terms of third-party code, patches, icons, shader
presets, artwork or fetched dependencies. Copyright remains with the respective
authors. A package pin records provenance, not permission to relicense it.

## Code carried in this repository

| Material | Terms and retained notices |
| --- | --- |
| `src/renderer/stb_image.h` | Sean Barrett; dual MIT/Unlicense, full notices embedded in the file |
| `config/input/steam/icons` | Lucide contributors (ISC), Feather/Cole Bemis (MIT), plus original Semu controller drawings; `LICENSE.lucide.txt` and `PROVENANCE.md` accompany the icons |
| Azahar patches | Citra/Azahar contributors; GPL-2.0-or-later; `config/emulators/azahar/LICENSE.upstream` |
| Dolphin patches | Dolphin contributors; patched MainWindow.cpp is GPL-2.0-or-later; upstream's mixed-license explanation and GPL text are in `config/emulators/dolphin/LICENSE*` |
| Flycast patches | flyinghead and contributors; GPL-2.0-or-later; `config/emulators/flycast/LICENSE.upstream` |
| PCSX2 patches | PCSX2 Dev Team; GPL-3.0-or-later; `config/emulators/pcsx2/LICENSE.upstream` |
| RetroArch patches | RetroArch contributors; GPL-3.0-or-later; `config/emulators/retroarch/LICENSE.upstream`. The MD5 portion of `retroarch_darwin.patch` retains Alexander Peslyak's public-domain/permissive fallback notice in `LICENSE.md5` |
| Cemu patches | Cemu contributors; MPL-2.0; `config/emulators/cemu/LICENSE.upstream`. The H264 recovery patch identifies its upstream backport commit |
| Ryujinx patch | Ryujinx Team and Contributors; MIT; `config/emulators/ryujinx/LICENSE.upstream` |
| ES-DE patches | Northwestern Software AB, Leon Styhre, Alec Lofquist and contributors; MIT; `packaging/esde/LICENSE.upstream` |
| Shader patches and presets | Keep the per-file upstream terms; see `config/assets/NOTICE.md` and the revisions in `config/assets/shaders.json`. These are not covered by Semu's MIT grant. Files without explicit terms are not thereby cleared for redistribution |

Semu's changes to covered upstream code are offered under the corresponding
upstream terms. Applying a patch must retain the original file notices.

## Pinned sources of the retained notices

- azahar: [fbd3fb02f71e5f9ed5134037fd59bad96c7d2b8a](https://raw.githubusercontent.com/azahar-emu/azahar/fbd3fb02f71e5f9ed5134037fd59bad96c7d2b8a/license.txt)
- dolphin: [c77bbaa0f372c3f72281602a8b087206706542cb](https://raw.githubusercontent.com/dolphin-emu/dolphin/c77bbaa0f372c3f72281602a8b087206706542cb/COPYING)
- flycast: [5aa091fde632fb332c8d8c34e280d62dc951954c](https://raw.githubusercontent.com/flyinghead/flycast/5aa091fde632fb332c8d8c34e280d62dc951954c/LICENSE)
- pcsx2: [bc8151d2a46d4aba039ea5580afbfc7bfcf6d730](https://raw.githubusercontent.com/PCSX2/pcsx2/bc8151d2a46d4aba039ea5580afbfc7bfcf6d730/COPYING.GPLv3)
- retroarch: [69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576](https://raw.githubusercontent.com/libretro/RetroArch/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/COPYING)
- cemu: [a6fb0a48eb437a8a41c13b782ac8ae0433bf8f98](https://raw.githubusercontent.com/cemu-project/Cemu/a6fb0a48eb437a8a41c13b782ac8ae0433bf8f98/LICENSE.txt)
- Ryujinx: `https://git.ryujinx.app/projects/Ryubing`, revision `e2143d43bcb6762340d8a01f20e7b5fdf104f02f`, `LICENSE.txt`.
- ES-DE: `https://gitlab.com/es-de/emulationstation-de`, revision `50e4b600ae533d772bae3ff880d11a09b05dbe84`, `LICENSE`.

## Personal installation versus public distribution

The composed `semu`, `release` and `release-tree` outputs are personal-installation
outputs, not cleared public releases. In particular, generated Duimon adaptations
may not be publicly shared under CC BY-NC-ND 4.0. The release carries an explicit
`DISTRIBUTION.txt` notice; neither these files nor a Nix binary cache containing
them should be published. Soqueroeu's separate terms are in the artwork notice.
No ROMs, BIOS, console keys, firmware, saves, game screenshots or recordings
belong in this repository or a distributed bundle.

Horizon is no longer fetched or bundled: its pinned source lacked a clear
redistribution grant for the theme and its third-party artwork. A user-installed
copy can still be selected locally. The name in settings is a preference, not
an assertion of ownership or a licence to distribute the theme.

Before any public binary release, remove restricted or uncleared artwork and
fonts (or obtain sufficient permission), inventory the actual dependency closure,
retain every applicable notice, and provide the corresponding source and build
materials required by each dependency's licence, including Semu's modifications.
The source pins and this document alone are not a complete binary compliance
package. Existing personal installations are not changed by the Git cleanup.
