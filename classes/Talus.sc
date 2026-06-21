Talus : MultiOutUGen {
    // Granular feedback delay. Overlapping grains read a delay line fed by the
    // input plus their own (saturated) feedback. maxDelay is fixed at build time.
    *ar { |in, maxDelay = 2, delay = 0.25, density = 20, dur = 0.2, pitch = 0,
           feedback = 0.3, spread = 0.5, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', in.asAudioRateInput, maxDelay, delay, density,
            dur, pitch, feedback, spread).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs {
        if (inputs[1].rate != 'scalar') { ^"Talus: maxDelay must be a fixed number (scalar)" };
        ^this.checkValidInputs
    }
}
