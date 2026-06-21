# Sediment

A granular texture-processor UGen for SuperCollider — an original DSP design
inspired by the Mutable Instruments **Clouds** eurorack module, but with its own
parameter vocabulary and its own sense of space.

Stereo in → stereo out. Audio is continuously recorded into an internal 2-second
buffer and reprocessed in one of three modes, then sent through a feedback-delay-network
"space" (a modulated, lightly saturated smear rather than a plate reverb).

| mode | name          | what it does                                            |
|------|---------------|---------------------------------------------------------|
| 0    | granular      | a cloud of overlapping windowed grains from the buffer  |
| 1    | stretch       | WSOLA time-stretch with correlation-matched splicing    |
| 2    | looping delay | looped buffer playback transposed by a pitch shifter    |

## Macro controls

Instead of Clouds' knob-for-knob layout, Sediment has three macros, each moving
several internals at once:

- **scatter** — cloud busyness, width and randomness (grain rate + jitter + spread)
- **bloom** — grain size and the amount of space (small/dry → long/blooming)
- **drift** — slow autonomous wander of position and pitch

`Sediment.ar(inL, inR, pitch, position, scatter, bloom, drift, feedback, dryWet, freeze, mode, trigger, mul, add)`

- `pitch` — base transposition in semitones (−48..48)
- `position` — read/scrub position into the buffer (0..1)
- `feedback` — recirculate the wet output into the buffer (0..1)
- `dryWet` — dry/wet mix (0..1, equal power)
- `freeze` — when > 0, stop recording so the buffer loops
- `mode` — 0 granular, 1 stretch, 2 looping delay

See the class help (`Sediment.schelp`) for per-mode examples.

## Build

```
./build.sh /path/to/supercollider
```

`SC_PATH` must point at a SuperCollider source tree whose plugin ABI
(`sc_api_version`) matches the scsynth you run: **v3 for SC 3.12–3.14.x**, v6 for
develop/3.15. The default is a sibling worktree of the 3.14.1 tag. After building,
recompile the class library (Cmd/Ctrl-Shift-L) and reboot the server.

## License

MIT. The DSP is original; the *architecture* of Mutable Instruments Clouds
(Émilie Gillet, also MIT) was used only as an algorithmic reference. No Mutable
Instruments source code is included.
