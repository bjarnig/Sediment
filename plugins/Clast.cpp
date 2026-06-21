// Clast — wavecycle grain cloud. Each grain spans N wavecycles (zero-crossing
// spans) of the source buffer, so grain length follows the source's own period:
// pitch-synchronous granulation. Overlapped, windowed, per-grain pitch/pan.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kMaxGrains = 32;

enum { Bufnum, Cycles, Density, Position, Scan, Pitch, Spread, Shape, kNumArgs };

struct ClastGrain { bool active; double readPos, readInc, start, spanLen; float ampL, ampR; };

struct Clast : public Unit {
    ClastGrain g[kMaxGrains];
    double triggerPhase, scanPos, sr;
    uint32 rng;
};

static int clastFree(Clast* u) {
    int best = 0; double hi = -1.0;
    for (int i = 0; i < kMaxGrains; ++i) {
        if (!u->g[i].active) return i;
        double ph = u->g[i].spanLen > 0 ? u->g[i].readPos / u->g[i].spanLen : 1.0;
        if (ph > hi) { hi = ph; best = i; }
    }
    return best;
}

static void Clast_next(Clast* unit, int nSamples) {
    float* outL = OUT(0); float* outR = OUT(1);

    float fbufnum = ZIN0(Bufnum);
    const float* bufData = nullptr; int bufFrames = 0, bufCh = 1;
    if (fbufnum >= 0.f) {
        uint32 bi = (uint32)fbufnum;
        if (bi < unit->mWorld->mNumSndBufs) {
            SndBuf* b = unit->mWorld->mSndBufs + bi;
            if (b->data && b->frames > 0) { bufData = b->data; bufFrames = b->frames; bufCh = b->channels; }
        }
    }
    if (!bufData || bufFrames < 8) { ClearUnitOutputs(unit, nSamples); return; }

    const int   cycles  = (int)gr::gr_clampf(ZIN0(Cycles), 1.f, 64.f);
    const float density = gr::gr_clampf(ZIN0(Density), 0.f, 800.f);
    const float position = gr::gr_clampf(ZIN0(Position), 0.f, 1.f);
    const float scan    = gr::gr_clampf(ZIN0(Scan), -1.f, 1.f);
    const float pitch   = ZIN0(Pitch);
    const float spread  = gr::gr_clampf(ZIN0(Spread), 0.f, 1.f);
    const float shape   = gr::gr_clampf(ZIN0(Shape), 0.f, 1.f);
    const double triggerInc = (double)density / unit->sr;
    const double rate = gr::gr_clampd(gr::gr_pitch_ratio(pitch), 0.03125, 32.0);
    const double scanInc = (double)scan * 2.0;

    for (int s = 0; s < nSamples; ++s) {
        unit->scanPos += scanInc;

        unit->triggerPhase += triggerInc;
        while (unit->triggerPhase >= 1.0) {
            unit->triggerPhase -= 1.0;
            ClastGrain* gr = &unit->g[clastFree(unit)];
            double jit = (gr::gr_rand(unit->rng) * 2.0 - 1.0) * spread * 0.1 * bufFrames;
            double raw = gr::gr_wrap((double)position * bufFrames + unit->scanPos + jit, (double)bufFrames);
            int start = gr::gr_zc_next(bufData, bufFrames, bufCh, 0, (int)raw);
            if (start < 0) start = (int)raw;
            int end = gr::gr_waveset_end(bufData, bufFrames, bufCh, 0, start, cycles);
            double span = (end > start) ? (double)(end - start) : 0.02 * unit->sr;
            gr->active = true; gr->readPos = 0.0; gr->readInc = rate;
            gr->start = (double)start; gr->spanLen = span;
            float pan = 0.5f + (gr::gr_rand(unit->rng) * 2.f - 1.f) * spread * 0.5f;
            gr::gr_pan(pan, gr->ampL, gr->ampR);
        }

        float wL = 0.f, wR = 0.f; int active = 0;
        for (int i = 0; i < kMaxGrains; ++i) {
            ClastGrain* gr = &unit->g[i];
            if (!gr->active) continue;
            ++active;
            float v = gr::gr_sinc(bufData, bufFrames, bufCh, 0, gr->start + gr->readPos, gr->readInc);
            float w = gr::gr_env((float)(gr->readPos / gr->spanLen), shape);
            wL += v * gr->ampL * w; wR += v * gr->ampR * w;
            gr->readPos += gr->readInc;
            if (gr->readPos >= gr->spanLen) gr->active = false;
        }
        float norm = 0.7f / std::sqrt((float)(active > 1 ? active : 1));
        outL[s] = gr::gr_sanitize(wL * norm);
        outR[s] = gr::gr_sanitize(wR * norm);
    }
}

static void Clast_Ctor(Clast* unit) {
    unit->sr = SAMPLERATE; unit->triggerPhase = 0.0; unit->scanPos = 0.0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    memset(unit->g, 0, sizeof(unit->g));
    SETCALC(Clast_next);
    ClearUnitOutputs(unit, 1);
}

PluginLoad(Clast) { ft = inTable; gr::gr_init_sinc(); DefineSimpleUnit(Clast); }
