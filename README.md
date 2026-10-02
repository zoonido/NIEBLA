# NIEBLA

Ambient pad synth for Ableton Live (VST3 + AU, macOS). By ZOONIDO.

Four layers (A-D), each with its own voice (Flute, Bells, FM, Sine, Saw, Choir, Organ, E. Piano, Strings, Air),
envelope, filter, Time Retard (tempo-synced), scale-snapped Pitch Shift with Jump/Glide, delay and reverb
(Hall, Pipe, Wood, with Shimmer and Freeze). Global scale, Warmth and Drift, presets and hover help.

## Build on GitHub
1. Push this repository to GitHub.
2. Open the Actions tab and wait for the `build` workflow (about 15 minutes).
3. Download the `niebla-mac` artifact and unzip it.
4. Copy `NIEBLA.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `NIEBLA.component` to `~/Library/Audio/Plug-Ins/Components/`.
5. In Ableton: Settings > Plug-ins > Rescan. NIEBLA appears under ZOONIDO.

If macOS blocks the plugin the first time, run in Terminal:
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/NIEBLA.vst3 ~/Library/Audio/Plug-Ins/Components/NIEBLA.component`

## Layout
- `source/` - the plugin code (processor + editor)
- `previews/` - audio previews rendered from the plugin code
- `docs/` - screenshots of the plugin window
- `.github/workflows/build.yml` - the cloud build
