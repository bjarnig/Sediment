![Sediment](images/sediment.png)

# Sediment

Experimental granular UGens for SuperCollider.

## UGens

- **Sediment**: granular / stretch / looping processor with macro controls and an FDN space.
- **Silt**: stochastic buffer grain cloud.
- **Talus**: granular feedback delay.
- **Scree**: stutter / glitch slice shuffler.
- **Loess**: micro-dust smear.
- **Clast**: wavecycle grain cloud (pitch-synchronous).
- **Creep**: drunken-walk read-head.
- **Moraine**: detect grains, then omit / duplicate / reorder / timewarp.

See each class's help file for parameters and examples.

## Install

Download your platform's archive from [Releases](https://github.com/bjarnig/Sediment/releases), extract the `Sediment/` folder into your SuperCollider Extensions folder, recompile the class library, and reboot the server.

## Build

```sh
./build.sh /path/to/supercollider
```

Needs a SuperCollider source tree whose plugin `api_version` matches your scsynth (v3 for 3.12 to 3.14.x).

## License

MIT. The DSP is original. Clouds (Émilie Gillet) and CDP / waveset processes (Trevor Wishart, CDP) were used only as inspiration/references.
