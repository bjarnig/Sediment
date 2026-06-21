Sediment : MultiOutUGen {
    // Granular texture processor (inspired by Mutable Instruments Clouds, original DSP).
    // Macro controls instead of a knob-for-knob layout:
    //   scatter — cloud busyness/width/randomness (density + jitter + spread)
    //   bloom   — grain size + the space (FDN) amount
    //   drift   — slow autonomous wander of position and pitch
    // Stereo in -> stereo out. mode: 0 granular, 1 stretch (WSOLA), 2 looping delay.
    *ar { |inL, inR, pitch = 0, position = 0.5, scatter = 0.5, bloom = 0.3,
           drift = 0.3, feedback = 0, dryWet = 0.5, freeze = 0, mode = 0,
           trigger = 0, mul = 1.0, add = 0.0|
        inR = inR ? inL;   // mono -> stereo convenience
        ^this.multiNew('audio', pitch, position, scatter, bloom, drift,
            feedback, dryWet, freeze, mode, trigger,
            inL.asAudioRateInput, inR.asAudioRateInput).madd(mul, add)
    }

    init { |... theInputs|
        inputs = theInputs;
        ^this.initOutputs(2, rate)
    }

    checkInputs { ^this.checkValidInputs }
}
