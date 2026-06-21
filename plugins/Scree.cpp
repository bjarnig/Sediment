// Scree — stutter / glitch shuffler. Records the input, then replays slices in
// randomized order with a variable repeat count: rhythmic buffer-mangling.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

enum { In, MaxLen, SliceDur, Jump, Repeats, Pitch, Scatter, kNumArgs };

struct Scree : public Unit {
    float* buf; int frames, writeHead;
    double anchor, delay, slicePos, sliceLen, readInc;
    int repeatsLeft;
    float panL, panR;
    double sr;
    uint32 rng;
    bool failed, started;
};

static void Scree_clear(Scree* unit, int n) { ClearUnitOutputs(unit, n); }

// Pick the next slice: either jump to a random spot or continue to the next one.
static void screeNextSlice(Scree* unit, float jump, int repeats, double sliceDur,
                           double rate, float scatter) {
    unit->sliceLen = gr::gr_clampd(sliceDur * unit->sr, 16.0, (double)unit->frames * 0.4);
    unit->readInc = rate;
    unit->repeatsLeft = repeats < 1 ? 1 : repeats;
    double margin = unit->sliceLen + 64.0;
    double maxBack = (double)unit->frames - 64.0;
    bool doJump = (gr::gr_rand(unit->rng) < jump) || (unit->delay - unit->sliceLen < margin);
    if (!unit->started || doJump) {
        unit->anchor = (double)unit->writeHead;
        unit->delay  = margin + gr::gr_rand(unit->rng) * (maxBack - margin);
    } else {
        unit->delay -= unit->sliceLen;     // continue: same anchor, earlier-recorded audio plays next
    }
    unit->slicePos = 0.0;
    float pan = 0.5f + (gr::gr_rand(unit->rng) * 2.f - 1.f) * scatter * 0.5f;
    gr::gr_pan(pan, unit->panL, unit->panR);
    unit->started = true;
}

static void Scree_next(Scree* unit, int nSamples) {
    const float* in = IN(In);
    float* outL = OUT(0); float* outR = OUT(1);

    const double sliceDur = gr::gr_clampd(ZIN0(SliceDur), 0.005, 1.0);
    const float jump   = gr::gr_clampf(ZIN0(Jump), 0.f, 1.f);
    const int   repeats = (int)gr::gr_clampf(ZIN0(Repeats), 1.f, 32.f);
    const float pitch  = ZIN0(Pitch);
    const float scatter = gr::gr_clampf(ZIN0(Scatter), 0.f, 1.f);
    const double rate = gr::gr_clampd(gr::gr_pitch_ratio(pitch), 0.0625, 16.0);

    float* buf = unit->buf; const int F = unit->frames;

    for (int s = 0; s < nSamples; ++s) {
        buf[unit->writeHead] = gr::gr_sanitize(in[s]);
        unit->writeHead = (unit->writeHead + 1) % F;

        if (!unit->started) screeNextSlice(unit, jump, repeats, sliceDur, rate, scatter);

        double readAbs = unit->anchor - unit->delay + unit->slicePos;
        float v = gr::gr_cubic(buf, F, 1, 0, readAbs);
        // trapezoidal de-click at slice edges
        double xf = unit->sliceLen * 0.25; if (xf > 0.005 * unit->sr) xf = 0.005 * unit->sr;
        double edge = unit->slicePos < (unit->sliceLen - unit->slicePos) ? unit->slicePos : (unit->sliceLen - unit->slicePos);
        float env = (float)gr::gr_clampd(edge / (xf + 1e-9), 0.0, 1.0);
        v *= env;
        outL[s] = gr::gr_sanitize(v * unit->panL);
        outR[s] = gr::gr_sanitize(v * unit->panR);

        unit->slicePos += unit->readInc;
        if (unit->slicePos >= unit->sliceLen) {
            if (--unit->repeatsLeft > 0) unit->slicePos -= unit->sliceLen;
            else screeNextSlice(unit, jump, repeats, sliceDur, rate, scatter);
        }
    }
}

static void Scree_Ctor(Scree* unit) {
    unit->sr = SAMPLERATE; unit->writeHead = 0; unit->started = false;
    unit->anchor = unit->delay = unit->slicePos = 0.0; unit->sliceLen = 1.0;
    unit->readInc = 1.0; unit->repeatsLeft = 1; unit->panL = unit->panR = 0.7071f;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;

    double maxLen = gr::gr_clampd(ZIN0(MaxLen), 0.1, 30.0);
    unit->frames = (int)(maxLen * unit->sr) + 64;
    unit->buf = (float*)RTAlloc(unit->mWorld, (size_t)unit->frames * sizeof(float));
    unit->failed = (unit->buf == nullptr);
    if (unit->failed) { SETCALC(Scree_clear); ClearUnitOutputs(unit, 1); return; }
    memset(unit->buf, 0, (size_t)unit->frames * sizeof(float));
    SETCALC(Scree_next);
    ClearUnitOutputs(unit, 1);
}

static void Scree_Dtor(Scree* unit) { if (unit->buf) RTFree(unit->mWorld, unit->buf); }

PluginLoad(Scree) { ft = inTable; DefineDtorUnit(Scree); }
