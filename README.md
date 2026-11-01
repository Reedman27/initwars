# INIT WARS: The Last PID

A small, funny, slightly heartfelt 1990s-style UNIX arcade shooter in C and SDL2.
You are one of the last init systems, fighting systemd's army of service units.
Runs as an ordinary resizable window (fine under Window Maker). Everything
shown in-game is simulated: the game never touches real services, processes or
your init system.

## Build

Needs a C compiler, GNU make and the SDL2 development files. No other libraries.

| System | Packages |
|---|---|
| Void Linux | `doas xbps-install -S base-devel SDL2-devel` |
| Debian/Ubuntu/Mint | `sudo apt install build-essential libsdl2-dev` |
| Fedora | `sudo dnf install gcc make SDL2-devel` |
| Arch / EndeavourOS / Manjaro | `sudo pacman -S --needed base-devel sdl2` (provides `sdl2-config`; the package ships the headers) |
| NixOS | `nix-shell -p gcc gnumake SDL2 pkg-config` |

```
make            # builds build/init-wars
make run        # build and run
make test       # headless test-suite (no window or audio needed)
make strict     # strict-core + strict-sdl: every file at -O0/-O2/-O3, gcc and clang, -Werror and extra warnings
make strict-core  # same, game logic + tests only: needs no SDL headers
make analyze    # clang static analyzer (incl. experimental stream/security checkers) on the SDL-free core
make sanitize   # whole headless suite (incl. random-input fuzz) under ASan + UBSan
make smoke      # runs the real binary on a private Xvfb (+ Window Maker if installed): CLI, audio fallbacks, minimise/pause/fullscreen/close
make install        # optional: /usr/local/bin/init-wars (run with doas/sudo if needed)  (PREFIX=... DESTDIR=... supported)
```

Options: `--crt` (scanline effect), `--no-audio`, `--no-save`, `--seed N`,
`--windowed-size WxH`, `--help`.

## Controls

| Key | Action |
|---|---|
| WASD / arrows | move (and menu navigation) |
| Space | fire (auto-aims at the nearest real enemy; falls back to facing) |
| Left/Right Shift or X | special ability |
| Enter / Space | confirm, advance story |
| Esc | pause (in story: skip scene) |
| F2 | toggle CRT scanlines |
| - / = (also [ / ] and numpad +/-) | volume down / up, 10 steps, works on every screen |
| Left/Right on the VOLUME row | volume slider in the main menu and the pause menu |
| Esc, then SETTINGS | settings screen (also on the main menu): volume, music, mute, CRT scanlines, screen shake, fullscreen |
| M | mute |
| F or Alt+Enter | toggle fullscreen (F11 also works outside Window Maker, which grabs F11 for its window list) |

The game auto-pauses when the window loses focus or is minimised; resuming is always your choice (Esc).

The main menu includes a persistent achievements cabinet with eight unlocks and a Run Statistics screen. Lifetime runs, wins, kills, uptime, best score and farthest wave are tracked independently for Campaign, Endless and Boss Rush. The character-select screen also lets you switch modes with Up/Down; Left/Right still selects a character. The high-score and statistics screens use Left/Right to switch modes. Each leaderboard keeps its own top eight, stored in a separate mode file. Legacy combined score files are imported without deleting the old data.

## Screenshots

All 22 images are real frames from the game's own SDL2 renderer at 960x720 (an exact 3x of the 320x240
screen), captured on the 1.5.x build (screenshots predate 1.6.0 and still show the old 10-wave numbering) with `make capture` / `tools/capture.c`: the real game logic and window,
driven by the test bot, with pixels read back from the window's renderer. Nothing is mocked up or redrawn.
Shots 04 to 12 and 16 to 22 start the run at a chosen wave with the test-suite jump hooks, and some use god
mode to reach late screens (see [`screenshots/README.md`](screenshots/README.md)). Shots 13 to 15 show sample
data from bot runs in a throwaway data directory, not a human player's history.

**Core gallery**

| | | | |
|---|---|---|---|
| **01 · Boot screen**<br>[<img src="screenshots/01-title-screen.png" width="220" alt="INITWARS BIOS boot sequence">](screenshots/01-title-screen.png) | **02 · Main menu**<br>[<img src="screenshots/02-main-menu.png" width="220" alt="Main menu with all modes and cabinets">](screenshots/02-main-menu.png) | **03 · Character select**<br>[<img src="screenshots/03-character-select.png" width="220" alt="Character select with the mode picker">](screenshots/03-character-select.png) | **04 · Campaign gameplay**<br>[<img src="screenshots/04-campaign-gameplay.png" width="220" alt="Campaign wave 5 as runit">](screenshots/04-campaign-gameplay.png) |
| **05 · Enemy showcase**<br>[<img src="screenshots/05-enemy-showcase.png" width="220" alt="Wave 8 with six service units on screen">](screenshots/05-enemy-showcase.png) | **06 · Wave clear**<br>[<img src="screenshots/06-wave-clear.png" width="220" alt="Wave Cleared banner with the +750 bonus">](screenshots/06-wave-clear.png) | **07 · Upgrade screen**<br>[<img src="screenshots/07-upgrade-screen.png" width="220" alt="Between-wave patch selection">](screenshots/07-upgrade-screen.png) | **08 · SysVinit story**<br>[<img src="screenshots/08-sysvinit-story.png" width="220" alt="Mid-wave Act II Red Hat scene over the fight">](screenshots/08-sysvinit-story.png) |
| **09 · systemd boss**<br>[<img src="screenshots/09-systemd-boss.png" width="220" alt="systemd boss, first phase">](screenshots/09-systemd-boss.png) | **10 · Boss phase**<br>[<img src="screenshots/10-boss-phase.png" width="220" alt="Later boss phase with summons and a barrier">](screenshots/10-boss-phase.png) | **11 · Endless Mode**<br>[<img src="screenshots/11-endless-mode.png" width="220" alt="Endless wave 9 as BusyBox init">](screenshots/11-endless-mode.png) | **12 · Boss Rush**<br>[<img src="screenshots/12-boss-rush.png" width="220" alt="Boss Rush round 3 of 8 as s6">](screenshots/12-boss-rush.png) |
| **13 · High scores**<br>[<img src="screenshots/13-high-scores.png" width="220" alt="Campaign tab of the leaderboard (sample bot runs)">](screenshots/13-high-scores.png) | **14 · Achievements**<br>[<img src="screenshots/14-achievements.png" width="220" alt="Achievement cabinet (sample bot runs)">](screenshots/14-achievements.png) | **15 · Run statistics**<br>[<img src="screenshots/15-run-statistics.png" width="220" alt="Campaign tab of run statistics (sample bot runs)">](screenshots/15-run-statistics.png) | **16 · Ending**<br>[<img src="screenshots/16-results-or-ending.png" width="220" alt="Final ending card: Dinit takes PID 1, SysVinit exits cleanly">](screenshots/16-results-or-ending.png) |

**1.5.0 story scenes**

| | | | |
|---|---|---|---|
| **17 · Dinit introduction**<br>[<img src="screenshots/17-dinit-introduction.png" width="220" alt="Act II: The Other Session, with the dependency graph">](screenshots/17-dinit-introduction.png) | **18 · The Last Runlevel**<br>[<img src="screenshots/18-last-runlevel.png" width="220" alt="Act III scene before wave 9 (subdued palette)">](screenshots/18-last-runlevel.png) | **19 · PID 1 handoff**<br>[<img src="screenshots/19-pid1-handoff.png" width="220" alt="Dinit offers to take the first process before the final fight">](screenshots/19-pid1-handoff.png) | **20 · Epilogue: Dinit**<br>[<img src="screenshots/20-epilogue-dinit.png" width="220" alt="Post-boss epilogue, Dinit accepts PID 1">](screenshots/20-epilogue-dinit.png) |
| **21 · Epilogue: SysVinit**<br>[<img src="screenshots/21-epilogue-sysvinit.png" width="220" alt="SysVinit's last runlevel line">](screenshots/21-epilogue-sysvinit.png) | **22 · Ending card**<br>[<img src="screenshots/22-ending-card.png" width="220" alt="Final card, all three lines typed">](screenshots/22-ending-card.png) |  |  |

## Codex

**CODEX** on the main menu (and in the pause menu) lists every enemy, init system, patch and rule with its name, stats and a plain description plus a tip. Left/Right switch tab (Units, Init Systems, Patches, Rules), Up/Down pick an entry, Esc goes back. Opened from the pause menu it leaves your run frozen.

## Boss movement

systemd no longer slides along one line: each phase has its own path (figure-eight, orbit, diagonal sweeps, frantic weave) across the upper arena, eased so phase changes never teleport him.

## Window behaviour

Normal decorated window, starts at 800x600, resizable (minimum 320x240), never
forced fullscreen (toggle with F or Alt+Enter; the windowed size is restored exactly). The 320x240 logical screen is scaled by an integer factor
with nearest-neighbour filtering and letterboxed, so pixels are never
distorted. WM_CLASS is `InitWars` for Window Maker app settings.

## The game

Choose **Campaign** for a 24-wave story told in four acts, many interludes, and a post-boss epilogue; **Endless Mode** for an open-ended survival run; or **Boss Rush** for eight consecutive systemd encounters. Up/Down on character select switches between the three modes; Left/Right chooses your init system.

Endless Mode builds each wave from a growing threat budget (12 + 3 per wave, capped at 100). Enemy types unlock in stages from waves 2–7. `Restart=always` begins appearing at wave 4 and becomes more common as the run continues. Regular enemy health rises by 3.5% per wave, capped at 75% above base; movement, attack cadence and enemy projectile speed rise by 2.5% per wave, capped at 65%. Every tenth wave is a systemd boss, with 35% additional base health for each later ten-wave boss tier. Entering every seventh wave restores 1 HP when you are below maximum.

Boss Rush skips campaign scenes and starts with a six-choice upgrade loadout. It then runs eight boss fights; each round adds 35% of base boss health, enemy tempo rises by 4% per round (up to 50%), and each cleared round before the final fight grants an upgrade and restores up to 3 HP. In Endless and Boss Rush, ordinary upgrades cap at five stacks and special recharge caps at three. BusyBox's emergency special cooldown is 50% longer in these two modes only. Campaign's wave tables and balance path remain unchanged.

The Campaign has 24 authored waves (the boss is wave 24). Cron daemons and OOM killers join from Act III; units get 2.5% tougher per wave after wave 9, the campaign boss has 25% more health, and patches now stack to five (special recharge: three) in every mode. A maintenance break (+1 HP) opens every fourth wave. Scenes run on most transitions; the migration notices (Red Hat, SUSE, Ubuntu) interrupt wave 9 while the fight is in progress. After the final boss, a new epilogue resolves the PID 1 handoff before the victory is recorded. Between waves pick one of three patches (HP, fire rate, damage, speed, special recharge). Waves 4, 7 and 10 begin with a maintenance break (+1 HP). The HUD shows HP, a labelled special-recharge bar and the boss bar; a red border flashes when you are hit, white on boss phase changes, and the boss announces each of its 4 phases.

SysVinit's Act II weakening is announced on screen (LEGACY MODE: fire rate -17%, damage
-10%) and a LEGACY tag stays in the HUD. He remains playable through the final fight. In the
story epilogue he performs one last clean shutdown and exits; the game treats this as a
fictional, bittersweet retirement, with his runlevel notes preserved in the archive.

Dinit appears first as a user-session service supervisor in wave 6, then offers to take PID 1
after the systemd boss is defeated. The story uses Dinit's real design ideas: dependency-aware
service ordering, parallel starts when dependencies permit, process supervision, rate-limited
restarts, and recovery of dependent services after a dependency fails. Dinit is not portrayed
as a new all-in-one system manager: the log, device, and network services remain separate.
This is a fictional arcade handoff, not a prediction about any distribution. See
[`docs/STORY.md`](docs/STORY.md) for the expanded story outline and its technical grounding.

The migration-notice scene happens **in the middle of wave 9**, not as a blank screen before it: when
half of the wave has spawned the arena freezes, enemy shots are cleared, the dialogue is drawn
over the dimmed arena (on the half the player is not in), and then the same fight resumes with
a short grace period (and the LEGACY MODE banner for SysVinit). Every character sees the
scene; only SysVinit is weakened. Esc skips it, button mashing in the first 40 frames is ignored
so a player who is firing does not skip it by accident.

## Volume

Ten steps, default 6 (the old fixed level is step 10, so the default is about 9 dB quieter).
The level is saved in `settings.txt` next to the scores (`volume N`) a moment after you change it
and again on exit. A missing, unreadable or corrupt settings file just means the default.

Characters (fictional arcade versions): **SysVinit** (old veteran; weakens
after the Act II scene but stays playable), **runit** (fast, short cooldown),
**s6** (stasis slows nearby enemies), **OpenRC** (clears shots, lines and
barriers), **BusyBox init** (tiny hitbox, emergency shield + heal).

Enemies: journald (log bursts), logind (closes routes), networkd (slowing
lines), timer (countdown barrage), udevd (hardware minions, including a printer
for no reason), resolved (decoy targets that vanish when hit), dependency.target
(links and toughens others), cron (scheduled radial bullet bursts), and the OOM
killer (telegraphed high-speed charges). `Restart=always` is a modifier (amber **R** badge):
a killed unit leaves a telegraphed restart marker; shoot the marker to cancel,
otherwise it comes back once at reduced HP. The boss has four phases ending in a
9-second survival sequence.

High scores, volume, achievement unlocks and lifetime profile statistics are stored in `$XDG_DATA_HOME/init-wars/` (or `~/.local/share/init-wars/`); override with `INITWARS_DATA_DIR`. The score files are `scores-campaign.txt`, `scores-endless.txt` and `scores-bossrush.txt`, with `settings.txt`, `achievements.txt` and `profile.txt` alongside them. Each mode keeps its own top eight. Existing combined `scores.txt` files are imported for any mode that does not yet have its own file.

## Settings

The SETTINGS screen opens from the pause menu (the run stays frozen and untouched) and from the main menu. Rows: master volume, music on/off (sound effects stay), mute all sound, CRT scanlines, screen shake, fullscreen, back. Esc also goes back. Volume, music, CRT and screen shake are saved in `settings.txt` (`volume N`, `crt 0|1`, `music 0|1`, `shake 0|1`) a moment after each change; mute is per session. `--crt` still forces scanlines on for that launch. Bad or unknown lines in the file are ignored.

## Tuning

Characters, enemies, waves and upgrades: `src/balance.c`.
Global constants (i-frames, delays, ability numbers): `src/balance.h`.

## Assets and licenses

There are **no external assets**. Sprites (`src/sprites.c`), the 5x7 font table
(`src/font.c`), sound effects and music (synthesised in `src/audio.c`) and all
text are original to this project. Code and art: CC0 / public domain.

Optional overrides: `sfx/<name>.wav` (shoot, hit, kill, hurt, special, wave,
select, beep, chime, panic, pickup, warn, victory), looked up in
`$INITWARS_ASSETS`, `./assets`, `<exe dir>/assets`, `<exe dir>/../share/init-wars`,
`/usr/local/share/init-wars`, `/usr/share/init-wars`. Missing or invalid files
fall back to the built-in sounds with a message on stderr. Any file you add
yourself needs its own license noted here.

## Layout

`main.c` SDL loop/input · `game.c` state, waves, enemies, boss · `view.c`
drawing · `render.c` software rasteriser · `story.c` scenes/quips · `balance.c`
tuning tables · `audio.c` synth · `assets.c` lookup · `score.c` high scores ·
`achievement.c` unlocks · `profile.c` lifetime statistics · `tests/test_game.c`
headless tests.

## Balance

See `docs/BALANCE.md`, the Campaign baseline at `docs/balance-report.txt`, and
the reproducible open-mode study at
[`docs/open-mode-balance-report.md`](docs/open-mode-balance-report.md). Regenerate
the reports with `./build/test_game --balance 30` and
`./build/test_game --open-balance 10 0x1a2b3c4d`. All current balance numbers are
bot-based; no human playtest has happened yet.

## Tests

* `make test`: 473,000+ headless checks (the exact count is printed at the end), including upgrade/character-select text-layout pixel regressions, 406 deterministic seeds across 32 Endless waves each, data tables, high-score and settings handling incl. hostile files,
  every enemy behaviour, Campaign, Endless and Boss Rush progression, mode-separated scores and legacy score-file migration, achievement unlock conditions/persistence/screen navigation, lifetime statistics aggregation/persistence/mode tabs, specials, Restart=always, SysVinit weakening, pause/restart/quit,
  rendering of every screen, the mid-wave scene, Dinit's dependency-graph mark, the SysVinit epilogue, volume,
  boss banners, a random-input fuzz run, and full bot playthroughs of all five characters.
* `make strict-core`, `make analyze`, `make test`, and `make sanitize` need only a C compiler (gcc or clang; `analyze` needs clang). They do not need SDL, a display or audio. On constrained build hosts, `make SAN_CC=gcc SAN_OPT='-O0 -g0' sanitize` reduces sanitizer compile overhead.
* `./build/test_game --open-balance 10 0x1a2b3c4d` runs a deterministic expert-bot comparison for Endless and Boss Rush (10 runs per character); the complete method and output are recorded in `docs/open-mode-balance-report.md`.
* `make smoke`: the real binary on a private Xvfb. The command-line and audio-fallback checks need only `Xvfb`.
  The Window Maker part (decoration, resize, minimise/restore, auto-pause, fullscreen round trips, closing through
  the title-bar button) needs all of: `wmaker`, `xdotool`, ImageMagick (`convert` and `import`), `xwininfo`, `xprop`
  and `bc`. If any is missing that part prints one SKIP line and is not run (it is *not* counted as passed).
  Package names differ per distribution; on Debian/Ubuntu: `xvfb wmaker xdotool imagemagick x11-utils bc`.


## Verification status

Verification details are recorded in `ROADMAP.md` and `docs/BALANCE.md`. The source-only test suite does not require SDL2 or a display. On the 1.5.0 build the real SDL2 binary, strict SDL compile and Window Maker smoke test (27/27, under Xvfb) have been run. A human playthrough, real speakers and other window managers are still untested; do not treat a skipped graphical check as a pass.
