Creep : MultiOutUGen {
    // Drunken-walk read-head over the live input: a wandering locus reads windowed
    // grains, creeping through time. maxAmbitus is fixed at build time.
    *ar { |in, maxAmbitus = 2, ambitus = 1, step = 0.2, grainDur = 0.12,
           overlap = 0.5, pitch = 0, pause = 0.0, spread = 0.5, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', in.asAudioRateInput, maxAmbitus, ambitus, step,
            grainDur, overlap, pitch, pause, spread).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs {
        if (inputs[1].rate != 'scalar') { ^"Creep: maxAmbitus must be a fixed number (scalar)" };
        ^this.checkValidInputs
    }
}
