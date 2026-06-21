Silt : MultiOutUGen {
    // Stochastic buffer grain cloud. Grain position/pitch/pan are drawn from a
    // selectable probability distribution (dist), scaled by scatter.
    *ar { |bufnum, density = 20, dur = 0.1, position = 0.5, scatter = 0.3,
           dist = 0, distParam = 0.5, pitch = 0, shape = 0.5, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', bufnum, density, dur, position, scatter,
            dist, distParam, pitch, shape).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs { ^this.checkValidInputs }
}
