Loess : MultiOutUGen {
    // Micro-dust: a dense cloud of very short grains read from a short delay line
    // of the input, finely detuned and scattered into a continuous smear.
    *ar { |in, density = 300, grainDur = 0.006, timeSpread = 0.5, pitch = 0,
           pitchSpread = 0.5, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', in.asAudioRateInput, density, grainDur,
            timeSpread, pitch, pitchSpread).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs { ^this.checkValidInputs }
}
