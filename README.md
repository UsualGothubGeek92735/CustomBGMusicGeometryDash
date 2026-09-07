# Custom BG Music (Geode mod)

Replace Geometry Dash's background/menu music with any MP3 from your own
music library.

## How it works
- Files live in `<mod save dir>/GD Music Library/` (auto-created on first
  run; folder name is configurable in mod settings).
- In-game, click the new music-note button on the main menu's bottom bar
  to open the picker and pick a track.
- The mod hooks `FMODAudioEngine::playMusic` **through Geode's modify
  system**, not raw memory patches. Geode chains every mod's hook on the
  same function together, so this coexists with other music/gameplay mods
  instead of overwriting their hook.
- If no track is selected, or the mod is disabled in settings, playback
  falls straight through to the base game / next mod in the hook chain —
  it never silently breaks default music.

## Build (Windows 11, Steam release of Geometry Dash)

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the
   "Desktop development with C++" workload, plus
   [CMake](https://cmake.org/download/) (3.21+, added to PATH).
2. Install the [Geode SDK](https://docs.geode-sdk.org/getting-started/) —
   the installer detects your Steam copy of GD automatically
   (`...\Steam\steamapps\common\Geometry Dash`) and installs the loader
   into it, plus sets up the SDK on your machine.
3. Set the `GEODE_SDK` environment variable to wherever the SDK installed
   (the Geode installer usually does this for you; verify with
   `echo %GEODE_SDK%` in a new terminal).
4. From the `CustomBGMusic` project folder, in a "x64 Native Tools Command
   Prompt for VS 2022" (or regular terminal with VS on PATH):
   ```
   cmake -B build
   cmake --build build --config Release
   ```
5. Package it:
   ```
   geode package new build --output CustomBGMusic.geode
   ```
   (or use the Geode CLI's project template commands if you generated this
   project with `geode new` instead of by hand).
6. Install: drop `CustomBGMusic.geode` into
   `...\Steam\steamapps\common\Geometry Dash\geode\mods\`, or just
   double-click it if the Geode installer registered `.geode` files.
7. Launch Geometry Dash through Steam as normal — Geode loads
   automatically and the mod will be active.

## Using it in-game
- On the main menu, a new music-note button opens your **Windows file
  explorer** directly — pick any `.mp3` on your PC and it starts playing
  immediately.
- A second small button next to it reopens your library list, so you can
  quickly switch back to a track you imported earlier without re-browsing
  your whole filesystem.
- Imported files are copied into
  `<Geode save data>\CustomBGMusic\GD Music Library\` so they survive even
  if you move/delete the original file.


## Known assumptions to verify against your GD version
FMOD member field names (`m_system`, `m_backgroundMusicChannel`,
`m_backgroundMusicVolume`) are based on current public Geode bindings for
GD 2.2074. If Geode updates bindings for a new GD version, re-check these
against the latest `Bindings.json` before rebuilding.

## No admin rights on your PC? Build in the cloud instead
Visual Studio's installer normally needs admin, but you can skip installing
it locally entirely by letting GitHub build the mod for you on a free,
already-configured Windows runner:

1. Create a free GitHub account if you don't have one, and create a new
   **public or private repository**.
2. Upload this whole `CustomBGMusic` folder into that repo (drag-and-drop
   works fine on github.com, or use `git push` if you have Git installed —
   Git itself does **not** require admin to install).
3. Go to the repo's **Actions** tab. A workflow called "Build Geode Mod"
   (in `.github/workflows/build.yml`) will run automatically on push, or
   click **Run workflow** to trigger it manually.
4. Once it finishes (a few minutes), open the completed run and download
   the **CustomBGMusic-geode** artifact from the bottom of the page — it's
   a zip containing your built `.geode` file.
5. Unzip it and drop the `.geode` file straight into
   `<Steam GD folder>\geode\mods\`. That copy step is just moving a file
   into a folder Geode already created — it doesn't need admin either.

This way the only things you ever install locally are the Geode Loader
(which installs into your own GD folder, not Program Files, and doesn't
need admin) and optionally Git — never Visual Studio itself.

## Windows 11 + Steam setup (if you do have admin / a build machine)
This mod works identically on the Steam build of GD — Geode hooks the same
game binary regardless of storefront.

1. Install the **Geode SDK/Loader** for Windows from geode-sdk.org/install.
   Point the installer at your Steam GD folder (usually
   `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash`).
2. Install **Visual Studio 2022** with the "Desktop development with C++"
   workload, and **CMake 3.21+**.
3. Set the environment variable: `setx GEODE_SDK "C:\Geode\Geode"` (or
   wherever the SDK installed to), then open a **new** terminal so it picks
   up the change.
4. From the `CustomBGMusic` folder:
   ```
   cmake -B build
   cmake --build build --config Release
   ```
5. Package it: `geode package new build` from the project root, which
   produces `CustomBGMusic.geode`.
6. Drop that file into
   `<Steam GD folder>\geode\mods\`, then launch the game through Steam as
   normal.

## Using it in-game
Click the music-note icon on the main menu's bottom bar. The popup opens
with a **"Choose File From PC..."** button at the top — click it and
Windows' native file dialog opens, filtered to `.mp3` files. Pick one and
it starts playing immediately; it's also copied into your GD music
library so it shows up in the list below for quick re-selection later
without re-browsing your whole PC.

## Mistake-scan log
- Fixed: track list cache never refreshed after first scan → now
  `refresh()` is forced from the picker UI each time it opens.
- Fixed: missing explicit `fmod.hpp` include for `FMOD::Sound`/`Channel`
  types used directly in `main.cpp`.
- Fixed: re-importing an mp3 that's already inside the library folder
  used to create a redundant "(1)" duplicate copy every time — now it
  detects the file is already in-place and just selects it.
- Verified: custom-track flag (`s_playingCustomTrack`) is only touched by
  our own channel, so we never stop audio started by another mod.
- Verified: settings toggle and missing-file cases both fall back to
  calling the original `playMusic`, so failure modes never produce silence
  or a crash.
- Verified: button ID uses Geode's `_spr` suffix to avoid ID collisions
  with other mods' menu buttons.
- Verified: native file dialog cancellation (Err result / no value) is a
  no-op — it doesn't clear your existing selection or crash the popup.
- Added: a GitHub Actions workflow (`.github/workflows/build.yml`) so the
  mod can be built entirely on a hosted Windows runner, avoiding the need
  for a local, admin-installed Visual Studio at all.
- Fixed: the new "pick MP3" button used a plausible-but-nonexistent sprite
  frame name (`GJ_musicListBtn_001.png`) for the secondary library button,
  which would have returned a null sprite and likely crashed on menu open.
  Swapped to `GJ_infoIcon_001.png`, which reliably exists in GD's sheet.
- Fixed: `file::pick(...)`'s result callback was typed as
  `Result<std::filesystem::path>*` (single template arg) but Geode's file
  API returns a `Result<T, E>` with an explicit error type; corrected to
  `Result<std::filesystem::path, std::string>*` so it actually compiles.
- Fixed: selecting or importing a track only updated saved state — it
  didn't restart playback, so you'd have to leave and re-enter the menu to
  actually hear the new track. Added `applySelectedTrackImmediately()`,
  called right after both the file-picker import and the library-list
  selection.
- Verified: importing a file whose name collides with an existing library
  file overwrites it intentionally (re-importing the "same" track updates
  it), rather than silently failing or duplicating.
