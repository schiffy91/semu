# Steam Input Templates

The Steam Deck default is declared under `config/input/` and compiled into
Semu-owned generated output:

- gyro opt-in
- right trackpad as mouse
- left trackpad as the radial hotkey surface
- `View` as the hotkey button (`HKB`), with `L4`/`R4` optional in exported
  Steam Input templates

## Unified Hotkeys

| Action | Steam Deck combo | Keyboard command |
| --- | --- | --- |
| Pause / resume | `HKB + A` | `Ctrl+P` |
| Screenshot | `HKB + B` | `Ctrl+X` |
| Fullscreen | `HKB + X` | `Ctrl+Enter` |
| Menu | `HKB + Y` | `Ctrl+M` |
| Quit emulator | `HKB + Start` | `Ctrl+Q` |
| Previous state slot | `HKB + D-Pad Left` | `Ctrl+J` |
| Next state slot | `HKB + D-Pad Right` | `Ctrl+K` |
| Load state | `HKB + L1` | `Ctrl+A` |
| Save state | `HKB + R1` | `Ctrl+S` |
| Rewind | `HKB + L2` | `Ctrl+-` |
| Fast forward | `HKB + R2` | `Ctrl++` |
| Swap screens | `HKB + L3` | `Ctrl+Tab` |
| Escape | `HKB + R3` | `Esc` |

Place exported Steam Deck controller template VDF files here:

- `neptune-simple.vdf`
- `neptune-full.vdf`

The expected files and install destination are declared in `config/input/`.
Run `semu build configs --project <dir>` to regenerate the selected template
metadata, then install the exported VDFs through Steam where required.
