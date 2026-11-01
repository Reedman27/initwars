# Screenshot assets

Twenty-two real captures of the SDL2 game on the 1.5.0 build, 960x720 PNG (exact 3x scale of the 320x240
logical screen). They are genuine renderer output; no frame is a mockup or redrawn.

## How they were made

`tools/capture.c` (`make capture`; run as `build/capture SCENARIO OUTDIR [SEED] [CHARACTER] [--nogod]`)
runs the real `game_update` + `view_draw` in a real SDL2 window, drives it with the test bot and a few
scripted key presses, and saves frames with `SDL_RenderReadPixels` from the window's own renderer. Use a
private display (`Xvfb :99`), `SDL_AUDIODRIVER=dummy` and a throwaway `INITWARS_DATA_DIR`, then convert the
BMPs with ImageMagick (`convert x.bmp x.png`). Exit status 0 means at least one frame was saved.

| File | What it shows | Scenario and setup |
|---|---|---|
| `01-title-screen.png` | Boot screen | `menu` |
| `02-main-menu.png` | Main menu, all modes and cabinets | `menu` |
| `03-character-select.png` | Character select with mode picker | `menu` |
| `04-campaign-gameplay.png` | Campaign combat (wave 5, runit) | `campaign 3 1 --nogod`: starts at wave 3, expert bot, god off |
| `05-enemy-showcase.png` | Six service units at once (wave 8) | `enemies 3 1 --nogod`: starts at wave 7 |
| `06-wave-clear.png` | Wave Cleared, +750 bonus | `campaign 3 1 --nogod` |
| `07-upgrade-screen.png` | Between-wave patch selection | `campaign 3 1 --nogod` |
| `08-sysvinit-story.png` | SysVinit mid-wave Red Hat scene | `story 3 1 --nogod`: SysVinit starts at wave 5 |
| `09-systemd-boss.png` | systemd boss, first phase | `boss 3 1`: starts at wave 10, **god mode on** |
| `10-boss-phase.png` | Later boss phase | `boss 3 1`: same run, god mode on |
| `11-endless-mode.png` | Endless wave 9 (BusyBox init) | `endless 5 4 --nogod`: starts at wave 8 |
| `12-boss-rush.png` | Boss Rush round 3/8 (s6) | `bossrush 5 2`: starts at round 3, god mode on |
| `13-high-scores.png` | Leaderboard, Campaign tab | `tabs 1`: 15 bot runs first (some god mode), then real key presses |
| `14-achievements.png` | Achievement cabinet, 7/8 | `tabs 1`: sample data, some unlocks via god-mode runs |
| `15-run-statistics.png` | Run statistics, Campaign tab | `tabs 1` |
| `16-results-or-ending.png` | Final ending card (SysVinit run) | `ending 3 0`: starts at wave 10, god mode on, after the epilogue |
| `17-dinit-introduction.png` | Dinit in "The Other Session", dependency graph | `storyA 3 1`: starts at wave 5; scene plays before wave 6 |
| `18-last-runlevel.png` | Act III "The Last Runlevel" | `storyB 3 0`: starts at wave 8, god mode on |
| `19-pid1-handoff.png` | Dinit's offer in the PID 1 scene | `storyB 3 0` |
| `20-epilogue-dinit.png` | Epilogue, Dinit accepts PID 1 | `storyB 3 0` |
| `21-epilogue-sysvinit.png` | Epilogue, SysVinit's last-runlevel line | `storyB 3 0` |
| `22-ending-card.png` | Ending card with all three lines | `storyB 3 0` |

Notes: god mode and the start-at-wave hooks are the ones the test-suite uses, applied only to reach late
screens quickly. Only the Campaign tab of the scores and statistics screens was captured. The title slot is
the boot sequence; the logo screen is the main menu (02). The four headless previews in `docs/previews/`
are separate software-renderer images and are not part of this gallery.
