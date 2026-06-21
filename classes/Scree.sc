Scree : MultiOutUGen {
    // Stutter / glitch shuffler. Records the input and replays slices in
    // randomized order with a repeat count. maxLen is fixed at build time.
    *ar { |in, maxLen = 2, sliceDur = 0.1, jump = 0.3, repeats = 2, pitch = 0,
           scatter = 0.3, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', in.asAudioRateInput, maxLen, sliceDur, jump,
            repeats, pitch, scatter).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs {
        if (inputs[1].rate != 'scalar') { ^"Scree: maxLen must be a fixed number (scalar)" };
        ^this.checkValidInputs
    }
}
