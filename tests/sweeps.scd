// Sediment — interactive listening tests. Boot the server, evaluate a block, move
// the mouse, then `x.free`. Load a source buffer first:

(
b = Buffer.read(s, Platform.resourceDir +/+ "sounds/a11wlk01.wav");
)

// --- granular: X = position, Y = scatter (sparse/narrow .. dense/wide) ---
(
x = {
    var in = PlayBuf.ar(1, b, BufRateScale.kr(b), loop: 1);
    Sediment.ar(in, in, pitch: 0, position: MouseX.kr(0,1),
        scatter: MouseY.kr(0.1,0.95), bloom: 0.4, drift: 0.3, dryWet: 1, mode: 0);
}.play;
)
x.free;

// --- bloom + drift: frozen cloud that blooms and wanders ---
(
x = {
    var in = PlayBuf.ar(1, b, BufRateScale.kr(b), loop: 1);
    Sediment.ar(in, in, pitch: 0, position: 0.4, scatter: 0.5,
        bloom: MouseX.kr(0,1), drift: MouseY.kr(0,1), dryWet: 1, freeze: 1, mode: 0);
}.play;
)
x.free;

// --- stretch (WSOLA): X = speed (slow..fast), Y = bloom (size) ---
(
x = {
    var in = PlayBuf.ar(1, b, BufRateScale.kr(b), loop: 1);
    Sediment.ar(in, in, pitch: 0, position: 0.5, scatter: MouseX.kr(0.15,0.85),
        bloom: MouseY.kr(0.2,0.85), drift: 0, dryWet: 1, mode: 1);
}.play;
)
x.free;

// --- looping delay: X = position, Y = pitch, with feedback ---
(
x = {
    var in = PlayBuf.ar(1, b, BufRateScale.kr(b), loop: 1);
    Sediment.ar(in, in, pitch: MouseY.kr(-12,7).round, position: MouseX.kr(0,0.6),
        scatter: 0.5, bloom: 0.4, drift: 0, dryWet: 1, feedback: 0.4, mode: 2);
}.play;
)
x.free;
