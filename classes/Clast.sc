Clast : MultiOutUGen {
    // Wavecycle grain cloud: each grain spans `cycles` wavecycles of the buffer
    // (zero-crossing spans), so grain length follows the source's period.
    *ar { |bufnum, cycles = 4, density = 40, position = 0.5, scan = 0.0,
           pitch = 0, spread = 0.4, shape = 0.5, mul = 1.0, add = 0.0|
        ^this.multiNew('audio', bufnum, cycles, density, position, scan,
            pitch, spread, shape).madd(mul, add)
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, rate) }
    checkInputs { ^this.checkValidInputs }
}
