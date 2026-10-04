# Steam Input Binding Icons

These PNGs are Semu-owned raster derivatives of icons from the official
[Lucide](https://github.com/lucide-icons/lucide) repository at commit
`b442632ee6fe6250bf24fef026e44244a33812c9`.

Each upstream 24px SVG was rendered on a transparent canvas, scaled to 192px,
centered in a 256x256 PNG, converted to a white stroke, and stripped of
nonessential metadata. No RetroDECK artwork is copied.

`packaging/steam/render-icons.sh` is that recipe (bash, curl and ImageMagick):
it renders every row below whose source is a backticked Lucide name, from the
commit above. `render-icons.sh --check` re-renders them all into a scratch
directory and requires each committed file to match at RMSE 0; a new icon is a
new row plus `render-icons.sh config/input/steam/icons semu-<name>.png`.

The Wii controller-mode icons use original Semu-owned 24px line drawings. They
follow the same 2px rounded white stroke and the same 192px-on-256px raster
pipeline as the Lucide derivatives.

| Semu file | Lucide source icon |
| --- | --- |
| `semu-aspect.png` | `ratio` |
| `semu-back.png` | `undo-2` |
| `semu-bezel-toggle.png` | `square-dashed` |
| `semu-bezel.png` | `picture-in-picture-2` |
| `semu-classic-controller.png` | Original Semu Classic Controller line drawing |
| `semu-confirm.png` | `circle-check-big` |
| `semu-down.png` | `circle-arrow-down` |
| `semu-fast-forward.png` | `fast-forward` |
| `semu-fit.png` | `scaling` |
| `semu-fullscreen.png` | `scan` |
| `semu-gamecube-controller.png` | Original Semu GameCube Controller line drawing |
| `semu-load.png` | `folder-down` |
| `semu-menu.png` | `menu` |
| `semu-next.png` | `circle-chevron-right` |
| `semu-open.png` | `folder-open` |
| `semu-pause.png` | `circle-pause` |
| `semu-previous.png` | `circle-chevron-left` |
| `semu-quit.png` | `power` |
| `semu-reset.png` | `rotate-ccw` |
| `semu-rewind.png` | `rewind` |
| `semu-save.png` | `save` |
| `semu-screenshot.png` | `camera` |
| `semu-shader-toggle.png` | `wand-sparkles` |
| `semu-shader.png` | `sparkles` |
| `semu-swap.png` | `arrow-left-right` |
| `semu-up.png` | `circle-arrow-up` |
| `semu-wiimote-nunchuk.png` | Original Semu Wiimote and Nunchuk line drawing |
| `semu-wiimote.png` | Original Semu Wiimote line drawing |

Lucide is distributed under the ISC license, with the upstream Feather-derived
subset covered by MIT. The complete upstream notice is preserved in
`LICENSE.lucide.txt`.
