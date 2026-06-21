// Silt — stochastic buffer grain cloud. Grain position, pitch and pan are each
// drawn from a selectable probability distribution: the distribution shapes the cloud.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kMaxGrains = 64;

enum { Bufnum, Density, Dur, Position, Scatter, Dist, DistParam, Pitch, kNumArgs };

struct SiltGrain { bool active; double readPos, readInc, start, phase, phaseInc; float ampL, ampR; };

struct Silt : public Unit {
    SiltGrain g[kMaxGrains];
    double triggerPhase, sr;
    uint32 rng;
};

static int siltFree(Silt* u) {
    int best = 0; double hi = -1.0;
    for (int i = 0; i < kMaxGrains; ++i) {
        if (!u->g[i].active) return i;
        if (u->g[i].phase > hi) { hi = u->g[i].phase; best = i; }
    }
    return best;
}

static void Silt_next(Silt* unit, int nSamples) {
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
    if (!bufData) { ClearUnitOutputs(unit, nSamples); return; }

    const float density = gr::gr_clampf(ZIN0(Density), 0.f, 1000.f);
    const double dur    = gr::gr_clampd(ZIN0(Dur), 0.001, 10.0);
    const float position = gr::gr_clampf(ZIN0(Position), 0.f, 1.f);
    const float scatter = gr::gr_clampf(ZIN0(Scatter), 0.f, 1.f);
    const int   dist    = (int)gr::gr_clampf(ZIN0(Dist), 0.f, 6.f);
    const float distP   = gr::gr_clampf(ZIN0(DistParam), 0.f, 1.f);
    const float pitch   = ZIN0(Pitch);
    const double triggerInc = (double)density / unit->sr;
    const float wetGain = (float)(0.7 / std::sqrt(density * dur > 1.0 ? density * dur : 1.0));

    for (int s = 0; s < nSamples; ++s) {
        unit->triggerPhase += triggerInc;
        while (unit->triggerPhase >= 1.0) {
            unit->triggerPhase -= 1.0;
            SiltGrain* gr = &unit->g[siltFree(unit)];
            float pj = gr::gr_distribution(dist, distP, gr::gr_rand(unit->rng)) * scatter;
            float pos = gr::gr_clampf(position + pj, 0.f, 1.f);
            float semis = pitch + gr::gr_distribution(dist, distP, gr::gr_rand(unit->rng)) * scatter * 24.f;
            float pan = 0.5f + gr::gr_distribution(dist, distP, gr::gr_rand(unit->rng)) * scatter * 0.5f;
            gr->active = true; gr->readPos = 0.0;
            gr->readInc = gr::gr_clampd(gr::gr_pitch_ratio(semis), 0.03125, 32.0);
            gr->start = (double)pos * (bufFrames - 1);
            gr->phase = 0.0; gr->phaseInc = 1.0 / (dur * unit->sr);
            gr::gr_pan(pan, gr->ampL, gr->ampR);
        }

        float wL = 0.f, wR = 0.f;
        for (int i = 0; i < kMaxGrains; ++i) {
            SiltGrain* gr = &unit->g[i];
            if (!gr->active) continue;
            float v = gr::gr_cubic(bufData, bufFrames, bufCh, 0, gr->start + gr->readPos * gr->readInc);
            float w = gr::gr_window((float)gr->phase, 1.0f);
            wL += v * gr->ampL * w; wR += v * gr->ampR * w;
            gr->readPos += 1.0; gr->phase += gr->phaseInc;
            if (gr->phase >= 1.0) gr->active = false;
        }
        outL[s] = gr::gr_sanitize(wL * wetGain);
        outR[s] = gr::gr_sanitize(wR * wetGain);
    }
}

static void Silt_Ctor(Silt* unit) {
    unit->sr = SAMPLERATE; unit->triggerPhase = 0.0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    memset(unit->g, 0, sizeof(unit->g));
    SETCALC(Silt_next);
    ClearUnitOutputs(unit, 1);
}

PluginLoad(Silt) { ft = inTable; DefineSimpleUnit(Silt); }
