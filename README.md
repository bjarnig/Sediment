![Sediment](images/sediment.png)

# Sediment

Experimental granular UGens for SuperCollider. Work-in-progress.

## UGens

- **Sediment** : granular / time-stretch / looping-delay processor with macro
  controls (scatter / bloom / drift) and a feedback-delay-network space. Inspired
  by Mutable Instruments Clouds; original DSP.
- **Silt** : stochastic buffer grain cloud — grain position/pitch/pan drawn from a
  selectable probability distribution.
- **Talus** : granular feedback delay — overlapping grains read a delay line fed by
  their own (saturated) output, so echoes accrete.
- **Scree** : stutter / glitch shuffler — records the input and replays slices in
  randomized order with a repeat count.
- **Loess** : micro-dust — a dense cloud of sub-10ms grains finely detuned into a
  continuous smear.
- **Clast** : wavecycle grain cloud — grains span N wavecycles (zero-crossing spans)
  of the buffer, so grain length follows the source's period (pitch-synchronous).
- **Creep** : drunken-walk granular read-head over the live input (after CDP's DRUNK).
- **Moraine** : detects grains in the input by amplitude troughs, then omits /
  duplicates / reorders / timewarps them (after CDP's GRAIN suite).

Each is a stereo UGen with band-limited (sinc) interpolation. See the class help files
for per-UGen parameters and examples. Sediment's macros: **scatter** (busyness/width),
**bloom** (grain size + space), **drift** (slow position/pitch wander); modes `0` granular,
`1` stretch (WSOLA), `2` looping delay.

## Install (prebuilt)

Download the archive for your OS from
[Releases](https://github.com/bjarnig/Sediment/releases), extract the `Sediment/` folder
into your SuperCollider Extensions folder, then recompile the class library
(Cmd/Ctrl-Shift-L) and reboot the server. Binaries target SuperCollider 3.12–3.14.x
(plugin API v3). On macOS, clear the download quarantine:
`xattr -dr com.apple.quarantine <extracted Sediment folder>`.

## Build from source

```sh
./build.sh /path/to/supercollider
```

`SC_PATH` must point at a SuperCollider source tree whose plugin ABI (`sc_api_version`)
matches the scsynth you run: **v3 for SC 3.12–3.14.x**, v6 for develop/3.15. The default
is a sibling worktree of the 3.14.1 tag. After building, recompile the class library and
reboot the server. Tagging `vX.Y.Z` builds multi-platform binaries via GitHub Actions.

## License

MIT. The DSP is original. Clouds (Émilie Gillet) and CDP / waveset processes
(Trevor Wishart, CDP) were used only as algorithmic references — no source from those
projects is included.
