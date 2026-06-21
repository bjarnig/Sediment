// Clouds — original granular/stretch/looping DSP, inspired by Mutable Instruments "Clouds".
// All state lives in sediment::Engine (embedded in the Unit); arrays are RTAlloc'd by the host.
// Helpers are static inline with a local PI constant (never M_PI — undefined under MSVC).
#pragma once
#include <cmath>
#include <cstring>

namespace sediment {

static const double CL_PI  = 3.14159265358979323846;
static const double CL_2PI = 6.28318530717958647692;

static const int   kMaxGrains    = 64;
static const float kBufferDur     = 2.0f;  // recording-buffer length, seconds (stereo)

static inline float cl_clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline double cl_clampd(double x, double lo, double hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline float cl_sanitize(float x) { return std::isfinite(x) ? x : 0.0f; }

// Wrap pos into [0, n).
static inline double cl_wrap(double x, double n) {
    if (n <= 0.0) return 0.0;
    x = std::fmod(x, n);
    if (x < 0.0) x += n;
    return x;
}

// xorshift32 — fast per-grain randomness, seeded from the parent RGen at Ctor.
static inline float cl_rand(uint32& s) {
    uint32 x = s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s = x;
    return (float)((double)x / 4294967296.0);
}

// 4-point cubic (Hermite) read of one channel of an interleaved stereo buffer.
static inline float cl_read(const float* buf, int frames, double pos, int ch) {
    if (!buf || frames < 4) return 0.0f;
    pos = cl_wrap(pos, (double)frames);
    int i1 = (int)pos;
    double frac = pos - (double)i1;
    int i0 = (i1 - 1 + frames) % frames;
    int i2 = (i1 + 1) % frames;
    int i3 = (i1 + 2) % frames;
    float y0 = buf[i0 * 2 + ch], y1 = buf[i1 * 2 + ch];
    float y2 = buf[i2 * 2 + ch], y3 = buf[i3 * 2 + ch];
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * (float)frac + c2) * (float)frac + c1) * (float)frac + c0;
}

// Grain window at phase p in [0,1]: a Hann raised to exponent k. k=1 is a true
// Hann (constant-power 50% overlap, used by stretch); k>1 narrows toward a
// percussive spike, k<1 broadens toward a plateau (granular texture morph).
static inline float cl_window(float p, float k) {
    if (p < 0.0f || p > 1.0f) return 0.0f;
    float base = 0.5f - 0.5f * std::cos((float)CL_2PI * p);
    return std::pow(base, k);
}
// Map texture 0..1 to a window exponent: 0 -> narrow/percussive, 1 -> broad.
static inline float cl_texture_k(float texture) { return 0.2f + (1.0f - texture) * 3.0f; }

// Semitone -> playback ratio.
static inline double cl_pitch_ratio(float semitones) {
    return std::pow(2.0, (double)semitones / 12.0);
}

struct Grain {
    bool   active;
    double readPos;     // samples advanced since grain start
    double readInc;     // playback rate (samples per sample)
    double start;       // start index into the recording buffer (frames)
    double phase;       // window phase [0,1]
    double phaseInc;    // window advance per sample
    float  winK;        // window exponent, sampled at birth
    float  ampL, ampR;  // equal-power pan, sampled at birth
};

// ---------------------------------------------------------------------------
// FX building blocks: a circular delay line and a Schroeder allpass on top of
// it. Lengths are fixed at init (scaled to the sample rate); taps are read with
// read()/readf() for the reverb's multi-tap output.
// ---------------------------------------------------------------------------
struct Delay {
    float* buf; int size; int w;
    bool init(World* world, int n) {
        size = n; w = 0;
        buf = (float*)RTAlloc(world, (size_t)size * sizeof(float));
        if (!buf) return false;
        memset(buf, 0, (size_t)size * sizeof(float));
        return true;
    }
    void release(World* world) { if (buf) RTFree(world, buf); buf = nullptr; }
    inline void write(float x) { buf[w] = x; if (++w >= size) w = 0; }
    inline float read(int d) const { int i = w - 1 - d; i %= size; if (i < 0) i += size; return buf[i]; }
    inline float readf(double d) const {
        double rp = (double)(w - 1) - d;
        rp = std::fmod(rp, (double)size); if (rp < 0) rp += size;
        int i1 = (int)rp; double fr = rp - i1;
        int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
        float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
        float c0 = y1, c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * (float)fr + c2) * (float)fr + c1) * (float)fr + c0;
    }
};

struct Allpass {
    Delay d; float g; int len;
    bool init(World* world, int maxLen, float gain, int length) {
        g = gain; len = length;
        return d.init(world, maxLen);
    }
    void release(World* world) { d.release(world); }
    inline float process(float x) {
        float dl = d.read(len);
        float v = x - g * dl;
        d.write(v);
        return dl + g * v;
    }
    inline float processMod(float x, double modLen) {
        float dl = d.readf(modLen);
        float v = x - g * dl;
        d.write(v);
        return dl + g * v;
    }
    inline float tap(int t) const { return d.read(t); }
};

// 4-stage stereo allpass diffuser — smears grain transients before the reverb.
struct Diffuser {
    Allpass ap[8];   // 0..3 left, 4..7 right
    bool init(World* world, double sr) {
        double s = sr / 29761.0;
        const int len[4] = {142, 107, 379, 277};
        const float g[4] = {0.75f, 0.75f, 0.625f, 0.625f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) {
                int L = (int)(len[k] * s) + 4;
                if (!ap[c * 4 + k].init(world, L + 4, g[k], L - 4)) return false;
            }
        return true;
    }
    void release(World* world) { for (int i = 0; i < 8; ++i) ap[i].release(world); }
    inline void process(float inL, float inR, float& outL, float& outR) {
        float l = inL, r = inR;
        for (int k = 0; k < 4; ++k) { l = ap[k].process(l); r = ap[4 + k].process(r); }
        outL = l; outR = r;
    }
};

// Sediment's "space": a 4-line feedback delay network (FDN) rather than a plate
// reverb. Mutually-detuned modulated delays + a unitary Hadamard feedback matrix
// + a soft saturator in each line give a chorused, slightly dirty smear/bloom
// that grows from the input rather than the clean tail of a Griesinger plate.
struct Space {
    Delay  line[4];
    int    len[4];
    float  lp[4];            // per-line damping one-pole
    double lfo[4], lfoInc[4];

    bool init(World* world, double sr) {
        const double ms[4] = {37.0, 43.0, 53.0, 61.0};   // mutually prime-ish, ms
        bool ok = true;
        for (int i = 0; i < 4; ++i) {
            len[i] = (int)(ms[i] * 0.001 * sr);
            ok &= line[i].init(world, len[i] + 64);       // headroom for modulation
            lp[i] = 0.f;
            lfo[i] = (double)i * 1.7;
            lfoInc[i] = (0.11 + 0.07 * i) * CL_2PI / sr;  // 0.11..0.32 Hz, detuned
        }
        return ok;
    }
    void release(World* world) { for (int i = 0; i < 4; ++i) line[i].release(world); }

    // decay 0..~0.92 (feedback gain; <1 keeps the unitary network stable),
    // damp 0..1 (1 = darkest).
    inline void process(float inL, float inR, float& outL, float& outR, float decay, float damp) {
        float d[4];
        for (int i = 0; i < 4; ++i) {
            double exc = 8.0 * std::sin(lfo[i]);
            lfo[i] += lfoInc[i]; if (lfo[i] > CL_2PI) lfo[i] -= CL_2PI;
            float v = line[i].readf((double)len[i] + exc);
            lp[i] += damp * (v - lp[i]);                  // damping
            d[i] = lp[i];
        }
        // unitary (normalized Hadamard) feedback mixing
        float h0 = 0.5f * ( d[0] + d[1] + d[2] + d[3]);
        float h1 = 0.5f * ( d[0] - d[1] + d[2] - d[3]);
        float h2 = 0.5f * ( d[0] + d[1] - d[2] - d[3]);
        float h3 = 0.5f * ( d[0] - d[1] - d[2] + d[3]);
        const float drive = 1.4f, inv = 1.0f / 1.4f;
        line[0].write(std::tanh(drive * (inL + decay * h0)) * inv);
        line[1].write(std::tanh(drive * (inR + decay * h1)) * inv);
        line[2].write(std::tanh(drive * (inL + decay * h2)) * inv);
        line[3].write(std::tanh(drive * (inR + decay * h3)) * inv);
        outL = (d[0] + d[2]) * 0.5f;
        outR = (d[1] + d[3]) * 0.5f;
    }
};

// Crossfaded dual-tap delay-line pitch shifter (constant-power: the two taps are
// a quarter-cycle apart so sin^2 + cos^2 = 1). Transposes a stream without
// changing its duration — used by the looping-delay mode.
struct PitchShifter {
    Delay d; double ph; int winLen;
    bool init(World* world, double sr) {
        winLen = (int)(0.05 * sr); ph = 0.0;
        return d.init(world, (int)(0.12 * sr) + 4);
    }
    void release(World* world) { d.release(world); }
    inline float process(float x, double ratio) {
        d.write(x);
        ph += (1.0 - ratio) / (double)winLen;
        ph -= std::floor(ph);
        double ph2 = ph + 0.5; ph2 -= std::floor(ph2);
        float a = d.readf(ph * winLen);
        float b = d.readf(ph2 * winLen);
        float w1 = std::sin((float)CL_PI * (float)ph);
        float w2 = std::sin((float)CL_PI * (float)ph2);
        return a * w1 + b * w2;
    }
};

struct Engine {
    double sr;
    float* buf;         // interleaved stereo recording buffer, bufFrames*2 floats
    int    bufFrames;
    int    writeHead;   // frames
    bool   frozen;

    Grain  grains[kMaxGrains];
    double triggerPhase;
    uint32 rng;

    Diffuser diffuser;
    Space    space;
    float    fbL, fbR;        // last wet output, recirculated into the recording
    float    fbHpL, fbHpR;    // one-pole state for the feedback DC/low cut
    double   driftPhase;      // slow autonomous wander of position/pitch (drift macro)

    // stretch (WSOLA) state
    double   wsA;             // analysis pointer into the buffer (frames)
    double   wsPrevStart;     // previous segment start (for splice correlation)
    int      wsCounter;       // output samples remaining until the next splice
    bool     wsInit;

    // looping-delay state
    PitchShifter pshiftL, pshiftR;
    double   loopPhase;       // position within the current loop (frames)
    double   loopStartIdx;    // absolute buffer index latched at each loop start
    bool     loopInit;

    // --- lifecycle ---------------------------------------------------------
    bool init(World* world, double sampleRate, uint32 seed) {
        sr = sampleRate;
        bufFrames = (int)(kBufferDur * sr);
        buf = (float*)RTAlloc(world, (size_t)bufFrames * 2 * sizeof(float));
        if (!buf) return false;
        memset(buf, 0, (size_t)bufFrames * 2 * sizeof(float));
        writeHead = 0;
        frozen = false;
        triggerPhase = 0.5;
        rng = seed | 1u;
        memset(grains, 0, sizeof(grains));
        fbL = fbR = fbHpL = fbHpR = 0.f;
        wsA = wsPrevStart = 0.0; wsCounter = 0; wsInit = false;
        loopPhase = 0.0; loopStartIdx = 0.0; loopInit = false;
        driftPhase = 0.0;
        if (!diffuser.init(world, sr)) return false;
        if (!space.init(world, sr)) return false;
        if (!pshiftL.init(world, sr)) return false;
        if (!pshiftR.init(world, sr)) return false;
        pshiftR.ph = 0.5;   // decorrelate L/R so a mono source still images in stereo
        return true;
    }
    void release(World* world) {
        diffuser.release(world);
        space.release(world);
        pshiftL.release(world);
        pshiftR.release(world);
        if (buf) RTFree(world, buf);
        buf = nullptr;
    }

    int findFreeGrain() {
        int best = 0; double hi = -1.0;
        for (int i = 0; i < kMaxGrains; ++i) {
            if (!grains[i].active) return i;
            if (grains[i].phase > hi) { hi = grains[i].phase; best = i; }
        }
        return best;
    }

    void triggerGrain(float position, float size, float pitch, float texture, float spread) {
        Grain* g = &grains[findFreeGrain()];
        double lengthSamp = 0.008 * std::pow(50.0, (double)cl_clampf(size, 0.0f, 1.0f)) * sr;
        if (lengthSamp < 4.0) lengthSamp = 4.0;
        if (lengthSamp > bufFrames * 0.5) lengthSamp = bufFrames * 0.5;
        double rate = cl_clampd(cl_pitch_ratio(pitch), 0.0625, 16.0);

        // Place the grain start safely behind the write head so a recording head
        // can't overrun the read head mid-grain. Over a grain's lifetime (lengthSamp
        // samples) the read head gains length*(rate-1) on the write head, so that's
        // the gap to reserve when rate > 1; rate <= 1 only needs a small guard.
        double minDelay = 64.0 + (rate > 1.0 ? lengthSamp * (rate - 1.0) : 0.0);
        double recordSpan = (double)bufFrames - 64.0;
        if (recordSpan < minDelay) recordSpan = minDelay;
        float posJit = (cl_rand(rng) * 2.0f - 1.0f) * texture * 0.1f;
        double pos01 = cl_clampd((double)position + posJit, 0.0, 1.0);
        double delay = minDelay + pos01 * (recordSpan - minDelay);

        g->active   = true;
        g->readPos  = 0.0;
        g->readInc  = rate;
        g->start    = cl_wrap((double)writeHead - delay, (double)bufFrames);
        g->phase    = 0.0;
        g->phaseInc = 1.0 / lengthSamp;
        g->winK     = cl_texture_k(texture);

        float pan = 0.5f + (cl_rand(rng) * 2.0f - 1.0f) * spread * 0.5f;
        pan = cl_clampf(pan, 0.0f, 1.0f);
        float a = pan * (float)(CL_PI * 0.5);
        g->ampL = std::cos(a);
        g->ampR = std::sin(a);
    }

    // --- stretch (WSOLA) ---------------------------------------------------
    inline float monoAt(double pos) const {
        int i = (int)cl_wrap(pos, (double)bufFrames);
        return 0.5f * (buf[i * 2] + buf[i * 2 + 1]);
    }

    // Search near `center` for the segment best matching the previous segment's
    // natural continuation (`target`), so the OLA splice stays phase-coherent.
    double bestSpliceOffset(double center, double target, int W, int S) {
        float bestCorr = -1e30f; double bestPos = center;
        for (int o = -S; o <= S; o += 2) {
            double cand = center + (double)o;
            float corr = 0.f;
            for (int k = 0; k < W; k += 4)
                corr += monoAt(cand + k) * monoAt(target + k);
            if (corr > bestCorr) { bestCorr = corr; bestPos = cand; }
        }
        return cl_wrap(bestPos, (double)bufFrames);
    }

    void triggerStretchGrain(double start, int segLen, double rate, float spread) {
        Grain* g = &grains[findFreeGrain()];
        g->active   = true;
        g->readPos  = 0.0;
        g->readInc  = rate;
        g->start    = cl_wrap(start, (double)bufFrames);
        g->phase    = 0.0;
        g->phaseInc = 1.0 / (double)segLen;
        g->winK     = 1.0f;   // true Hann for constant-power 50% overlap
        float pan = 0.5f + (cl_rand(rng) * 2.0f - 1.0f) * spread * 0.5f;
        pan = cl_clampf(pan, 0.0f, 1.0f);
        float a = pan * (float)(CL_PI * 0.5);
        g->ampL = std::cos(a);
        g->ampR = std::sin(a);
    }

    // Called once per output sample; emits a correlation-matched overlapping
    // segment every synthesis hop. speed = analysis advance per output sample
    // (< 1 stretches time, > 1 compresses); position scrubs; pitch transposes.
    void stretchTick(float position, float size, float pitch, float spread, float speed) {
        int segLen = (int)(0.01 * std::pow(25.0, (double)cl_clampf(size, 0.f, 1.f)) * sr);
        if (segLen < 64) segLen = 64;
        if (segLen > bufFrames / 2) segLen = bufFrames / 2;
        int Hs = segLen / 2;                       // synthesis hop = 50% overlap
        double rate = cl_clampd(cl_pitch_ratio(pitch), 0.0625, 16.0);

        if (!wsInit) {
            wsA = cl_wrap((double)writeHead - bufFrames * 0.5, (double)bufFrames);
            wsPrevStart = wsA; wsCounter = 0; wsInit = true;
        }
        if (wsCounter <= 0) {
            double center = cl_wrap(wsA + (double)position * bufFrames, (double)bufFrames);
            double target = wsPrevStart + (double)Hs;   // natural continuation
            int W = Hs < 256 ? Hs : 256;
            int S = Hs / 2; if (S > 400) S = 400;
            double start = bestSpliceOffset(center, target, W, S);
            triggerStretchGrain(start, segLen, rate, spread);
            wsPrevStart = start;
            wsCounter = Hs;
            wsA = cl_wrap(wsA + (double)Hs * speed, (double)bufFrames);
        }
        wsCounter--;
    }

    // --- looping delay ----------------------------------------------------
    // Loop a region of the recording (length = size, scrubbed by position) and
    // transpose it with the dual-tap pitch shifter. position scrubs how far back
    // the loop sits; one output sample per call.
    void loopingSample(float position, float size, float pitch, float spread,
                       float& outL, float& outR) {
        double loopLen = 0.05 * std::pow(40.0, (double)cl_clampf(size, 0.f, 1.f)) * sr;
        if (loopLen < 64.0) loopLen = 64.0;
        if (loopLen > bufFrames - 64.0) loopLen = bufFrames - 64.0;
        double back = loopLen + (double)position * ((double)bufFrames - loopLen - 64.0);

        // Latch the loop's start index in the buffer at each cycle so playback runs
        // at 1x (recomputing it from the moving write head every sample would double
        // the read rate). position/size therefore update only at loop boundaries.
        if (!loopInit || loopPhase >= loopLen) {
            loopPhase = 0.0;
            loopStartIdx = cl_wrap((double)writeHead - back, (double)bufFrames);
            loopInit = true;
        }
        // read L/R from slightly offset loop positions (up to ~20 ms apart) so a
        // mono source decorrelates into a stereo image as spread increases.
        double off = (double)spread * 0.02 * sr;
        double rpL = cl_wrap(loopStartIdx + loopPhase, (double)bufFrames);
        double rpR = cl_wrap(loopStartIdx + loopPhase + off, (double)bufFrames);
        float sL = cl_read(buf, bufFrames, rpL, 0);
        float sR = cl_read(buf, bufFrames, rpR, 1);
        loopPhase += 1.0;

        double ratio = cl_clampd(cl_pitch_ratio(pitch), 0.0625, 16.0);
        outL = pshiftL.process(sL, ratio);
        outR = pshiftR.process(sR, ratio);
    }
};

} // namespace sediment
