# Open-mode bot balance report

<small>Generated: 2026-10-29 · Game version: 1.4.1 · Baseline: the 1.4.0 source, with UI, profile-parser, and benchmark fixes in the 1.4.1 pass</small>

## Method

This report was generated locally with:

```sh
./build/test_game --open-balance 10 0x1a2b3c4d
```

The benchmark uses **10 runs per character per mode**, the `BOT_EXPERT` profile
from `tests/bot.h`, and the real game update loop. It enters each mode through
the character-select/start path, so Boss Rush includes its six-pick starting
loadout and later upgrade choices. The bot receives the `Game` state and emits
the same `Input` structure used by the keyboard path; it is a deterministic
scripted opponent, not a human participant.

The base seed is `0x1a2b3c4d`. For each run, the deterministic seed is:

```c
seed = base
     ^ (mode * 0x9e3779b9)
     ^ (character * 0x85ebca6b)
     ^ (sample * 0xc2b2ae35);
```

A zero result is remapped to `1`. Sample indices are `0..9`; character indices
follow `CHARS[]`; mode values are the `ScoreMode` enum values. Each attempt is
limited to 54,000 simulation updates (15 minutes at 60 Hz). The table reports
active gameplay frames from `Game.run_frames`, not menu/upgrade selection time.
No run in this sample reached the simulation cap.

## Endless Mode

| Character | Mean farthest wave | Mean survival (min) | Mean kills |
|---|---:|---:|---:|
| SysVinit | 12.10 | 3.26 | 97.90 |
| runit | 15.40 | 4.94 | 172.60 |
| s6 | 9.70 | 2.55 | 70.50 |
| OpenRC | 8.70 | 2.41 | 61.40 |
| BusyBox init | 10.90 | 3.66 | 98.50 |

## Boss Rush

| Character | Win rate | Mean bosses defeated (of 8) | Mean survival (min) |
|---|---:|---:|---:|
| SysVinit | 0% (0/10) | 2.10 | 2.75 |
| runit | 0% (0/10) | 1.40 | 2.07 |
| s6 | 0% (0/10) | 1.30 | 1.92 |
| OpenRC | 0% (0/10) | 1.10 | 2.08 |
| BusyBox init | 0% (0/10) | 0.90 | 2.14 |

## Interpretation and limits

- In this sample, runit reached the highest mean Endless wave and kill count.
- No expert-bot attempt completed Boss Rush. The mean number of bosses cleared
  ranged from 0.90 to 2.10. This is evidence that the current eight-round
  gauntlet is a substantial challenge for this particular bot, not proof that
  the game is impossible or that any one character is objectively strongest.
- An earlier session quoted a different 10-run table, including
  nonzero Boss Rush wins. That table did not include a recoverable run log,
  seed policy, or matching source revision, so it is retained as
  historical context only and is **not** substituted for this fresh sample.
- Bot behavior is heuristic and reacts to visible game state. It cannot assess
  whether dodging, aiming, upgrade choices, screen readability, or boss pacing
  feel fair to a person.
- No human playtest is included. The next balance pass should include keyboard
  play across all characters, with particular attention to Boss Rush round 2–4
  attrition and Endless waves around the first and second ten-wave bosses.

To reproduce the report, rebuild the headless target and run the command above.
The report is tied to the source tree, deterministic seed policy, 10-run sample,
and expert bot profile documented here; changing any of those can change the
results.
