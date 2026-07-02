Semu Visual Evidence

Project: /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu
Visual evidence root: /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence

For each system:
1. Launch a representative game through Semu.
2. Capture screenshots for game-priority and bezel-priority.
3. Capture variants A, B, and C for each priority.
4. Record a 30-second start-of-gameplay clip.
5. Verify the game viewport lands exactly in the bezel cutout.
6. Verify controller input moves/responds in game.
7. Write start-of-gameplay-analysis.txt with the required analysis tokens.

Expected files:

gb analysis must include:
  - system=gb
  - emulator=retroarch
  - visual=ok
  - input=ok
  - duration_s=30
  - viewport_alignment=ok
  - cutout_mask=ok
  - controller_input=ok
  - radial_menu=ok
  - clip.start_of_gameplay=ok
  - bezel.A=ok
  - bezel.B=ok
  - bezel.C=ok
  - bezel.Off=ok
  - shader.A=ok
  - shader.B=ok
  - shader.C=ok
  - shader.Off=ok
  - screenshot.game-priority.A=ok
  - screenshot.game-priority.B=ok
  - screenshot.game-priority.C=ok
  - screenshot.bezel-priority.A=ok
  - screenshot.bezel-priority.B=ok
  - screenshot.bezel-priority.C=ok
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/game-priority/A.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/game-priority/B.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/game-priority/C.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/bezel-priority/A.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/bezel-priority/B.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/bezel-priority/C.jpg
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/start-of-gameplay-30s.mp4
- /Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/generated/test/visual-evidence/gb/start-of-gameplay-analysis.txt
