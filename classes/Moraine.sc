Moraine : MultiOutUGen {
    // Detects grains in the live input by amplitude troughs, then transforms them.
    // mode: 0 omit, 1 duplicate, 2 reorder, 3 timewarp. maxLen fixed at build time.
    *ar { |in, maxLen = 4, mode = 0, gate = 0.05, minHole = 0.02, amount = 0.5,
           pitch = 0, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', in.asAudioRateInput, maxLen, mode, gate,
            minHole, amount, pitch).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs {
        if (inputs[1].rate != 'scalar') { ^"Moraine: maxLen must be a fixed number (scalar)" };
        ^this.checkValidInputs
    }
}
