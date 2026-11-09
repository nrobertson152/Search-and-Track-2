# SEARCH / TRACK 2

**An original Cold War air-defense command simulator, written in C17.**  Choose NORAD or the Soviet PVO, operate a five-station radar network, identify contacts, scramble fighter aircraft, and defend your sector without bringing down civilian traffic.

![Built with C17](https://img.shields.io/badge/language-C17-informational) ![License](https://img.shields.io/badge/license-GPLv3-blue) ![Linux](https://img.shields.io/badge/platform-Linux%20X11-brightgreen)

**Independent project:** This game was developed independently of the original *Search and Track*. It uses no Godot or full game engine. Artwork consists of procedural shapes and text rendered with the native X11 library. It contains no third-party game assets or redistributed system fonts.

## AppImage (Linux)

Open the [GitHub Actions workflow runs](https://github.com/nrobertson152/Search-and-Track-2/actions/workflows/appimage.yml), select the latest successful **Linux AppImage** run, and download the `Search-and-Track-2-AppImage-x86_64` artifact. Unzip it, then run:

```sh
chmod +x Search-and-Track-2-x86_64.AppImage
./Search-and-Track-2-x86_64.AppImage
```

This is an offline native X11 application. Linux Mint on x86-64 is the primary target. You do not need to build from source to use the AppImage. The file is attached to the CI run (GitHub login may be required).

## Play

Requires Linux with X11/XWayland, a C17 compiler, `make`, and the X11 development package. On Linux Mint / Ubuntu:

```bash
sudo apt install build-essential libx11-dev
make
./build/search-and-track-2
```

Or CMake:

```bash
sudo apt install build-essential cmake libx11-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/search-and-track-2
```

The game starts with a faction-selection screen. Use **A** for US NORAD or **B** for Soviet PVO, then **Enter**. The app runs without an internet connection, telemetric services, or external game data.

## Controls

| Key | Action |
| --- | --- |
| 1–7 | Tactical plot, PPI radar, air operations, sensor network, intelligence, logbook, manual |
| Mouse | Choose radar tracks, fighters, sensor stations, and command buttons |
| Tab | Cycle detected contacts |
| F | Scramble the nearest ready fighter to selected track |
| H | Classify contact hostile |
| I | Classify contact friendly |
| E | Authorize/revoke weapons free (hostile tracks only) |
| K | Fire nearest in-range SAM battery (requires hostile + weapons authorization) |
| P | Toggle selected radar station transmitter |
| Z | Change PPI radar scale (160 / 300 / 520 km) |
| Space | Pause/resume |
| [ and ] | Change time scale (1x, 4x, 8x, 16x, 32x) |
| S and L | Save/load current operation |
| N or Esc | New operation / faction selection |

**Rules of engagement:** Declaring a contact hostile is a judgment call, not proof. A civilian aircraft can be mistakenly classified and shot down. Hold fire until you have sufficient evidence. Interceptors pursue tracks and fire only when explicitly authorized; missiles may miss. Radar coverage is affected by target signature, range, altitude, electronic interference, and station status. Tracks become uncertain when fresh sensor hits cease.

### Current vertical slice

- Two independent faction configurations (NORAD: F-106A/F-4E; PVO: Su-15TM/MiG-21) with distinct radar locations and names.
- Live stylized theater plot, selectable aircraft tracks, scan visualization, sensor sectors, missile batteries, and active interceptor paths.
- Dedicated rotating PPI radar, adjustable scale, target uncertainty, radar beam/sweep calculations.
- Multiple consoles: tactical plot, radar PPI, interceptor operations, sensor network, intelligence, event log, and manual.
- Radar detection model with range, target radar cross-section, altitude/radar horizon, beam scan, jamming and noise; sensor updates generate imperfect track estimates.
- Scramble and interception with launch delay, fuel use, pursuit, weapons restrictions, automatic return, and turnaround.
- Three SAM sites with finite missiles, probabilistic engagements, and operational status.
- Noncombatant aircraft with transponder responses, uncertain identification, civilian casualties/penalties, incoming bomber waves, damage to infrastructure, and overall sector integrity.
- Full-shift scoring and end-of-watch assessment; manual pause and time compression.
- Versioned, checksummed local save/load at `$XDG_DATA_HOME/search-and-track-2/watch.sat2` or `~/.local/share/search-and-track-2/watch.sat2`.
- Headless simulation smoke test for both factions and separate deterministic core tests.

**Scope note:** This is a playable vertical slice, not a historically certified NORAD/PVO reproduction or a finished full campaign. Geography is schematic rather than a literal world map; aircraft/radar performance is game-balanced; combat resolution uses stochastic approximations. More work would be needed for historical sector maps, proper fighter and weapons models, strategic campaigns, operational audio, cross-platform GUI backends, data-driven mission editing, localization, accessibility, and packaging. These limitations are deliberate and displayed rather than presenting the simulation as historical fact.

## Test

```bash
make test
make smoke
# or
ctest --test-dir build --output-on-failure
```

## Architecture

- `src/game.h`: Core model, mission state, radar/aircraft/weapon data, public simulation API.
- `src/sim.c`: Deterministic fixed-step simulation (seeded random generator), track updates, engagements, score, event log, save/load. No graphics dependencies.
- `src/main.c`: Native Linux/X11 renderer with a double-buffered procedural command-center interface, input, timing, save-file location.
- `tests/test_sim.c`: No-display core tests for both factions, radar acquisition, orders, save/load and scenario waves.

**Design intent:** Keep the simulator independent of the graphical presentation. C17 is the authoritative language. A future optional Scheme scripting layer could define scenarios and operational events without making the real-time simulation depend on a Scheme runtime.

## License

GPL-3.0-or-later; see [LICENSE](LICENSE) in the repository.

## Command-center presentation

The interface uses a Cold War equipment-inspired visual treatment: recessed CRT screens, phosphor radar sweeps, bronze/olive instrument housings, annunciator lamps, and separate American/Soviet command identification. It is rendered directly through X11 without a full game engine. See [visual design](docs/VISUAL_DESIGN.md).
