// Loess — micro-dust. A very dense cloud of sub-10ms grains read from a short
// live-input delay line, finely detuned and scattered into a continuous smear.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kMaxGrains = 96;

enum { In, Density, GrainDur, TimeSpread, Pitch, PitchSpread, kNumArgs };

struct LoessGrain { bool active; double readPos, readInc, start, phase, phaseInc; float ampL, ampR; };

struct Loess : public Unit {
    LoessGrain g[kMaxGrains];
    float* line; int frames, writeHead;
    double triggerPhase, sr;
    uint32 rng;
    bool failed;
};

static int loessFree(Loess* u) {
    int best = 0; double hi = -1.0;
    for (int i = 0; i < kMaxGrains; ++i) {
        if (!u->g[i].active) return i;
        if (u->g[i].phase > hi) { hi = u->g[i].phase; best = i; }
    }
    return best;
}

static void Loess_clear(Loess* unit, int n) { ClearUnitOutputs(unit, n); }

static void Loess_next(Loess* unit, int nSamples) {
    const float* in = IN(In);
    float* outL = OUT(0); float* outR = OUT(1);

    const float density = gr::gr_clampf(ZIN0(Density), 0.f, 2000.f);
    const double dur    = gr::gr_clampd(ZIN0(GrainDur), 0.001, 0.05);
    const float tspread = gr::gr_clampf(ZIN0(TimeSpread), 0.f, 1.f);
    const float pitch   = ZIN0(Pitch);
    const float pspread = gr::gr_clampf(ZIN0(PitchSpread), 0.f, 12.f);
    const double triggerInc = (double)density / unit->sr;
    const double base = 0.02 * unit->sr;
    const double range = ((double)unit->frames - base - 64.0) * (double)tspread;
    const float wetGain = (float)(0.6 / std::sqrt(density * dur > 1.0 ? density * dur : 1.0));

    float* line = unit->line; const int F = unit->frames;

    for (int s = 0; s < nSamples; ++s) {
        line[unit->writeHead] = gr::gr_sanitize(in[s]);
        unit->writeHead = (unit->writeHead + 1) % F;

        unit->triggerPhase += triggerInc;
        while (unit->triggerPhase >= 1.0) {
            unit->triggerPhase -= 1.0;
            LoessGrain* gr = &unit->g[loessFree(unit)];
            double back = base + 64.0 + range * gr::gr_rand(unit->rng);
            float semis = pitch + (gr::gr_rand(unit->rng) * 2.f - 1.f) * pspread;
            float pan = gr::gr_rand(unit->rng);
            gr->active = true; gr->readPos = 0.0;
            gr->readInc = gr::gr_clampd(gr::gr_pitch_ratio(semis), 0.25, 4.0);
            gr->start = gr::gr_wrap((double)unit->writeHead - back, (double)F);
            gr->phase = 0.0; gr->phaseInc = 1.0 / (dur * unit->sr);
            gr::gr_pan(pan, gr->ampL, gr->ampR);
        }

        float wL = 0.f, wR = 0.f;
        for (int i = 0; i < kMaxGrains; ++i) {
            LoessGrain* gr = &unit->g[i];
            if (!gr->active) continue;
            float v = gr::gr_sinc(line, F, 1, 0, gr->start + gr->readPos * gr->readInc, gr->readInc);
            float w = gr::gr_window((float)gr->phase, 1.0f);
            wL += v * gr->ampL * w; wR += v * gr->ampR * w;
            gr->readPos += 1.0; gr->phase += gr->phaseInc;
            if (gr->phase >= 1.0) gr->active = false;
        }
        outL[s] = gr::gr_sanitize(wL * wetGain);
        outR[s] = gr::gr_sanitize(wR * wetGain);
    }
}

static void Loess_Ctor(Loess* unit) {
    unit->sr = SAMPLERATE; unit->triggerPhase = 0.0; unit->writeHead = 0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    memset(unit->g, 0, sizeof(unit->g));
    unit->frames = (int)(0.3 * unit->sr) + 64;     // short delay line
    unit->line = (float*)RTAlloc(unit->mWorld, (size_t)unit->frames * sizeof(float));
    unit->failed = (unit->line == nullptr);
    if (unit->failed) { SETCALC(Loess_clear); ClearUnitOutputs(unit, 1); return; }
    memset(unit->line, 0, (size_t)unit->frames * sizeof(float));
    SETCALC(Loess_next);
    ClearUnitOutputs(unit, 1);
}

static void Loess_Dtor(Loess* unit) { if (unit->line) RTFree(unit->mWorld, unit->line); }

PluginLoad(Loess) { ft = inTable; gr::gr_init_sinc(); DefineDtorUnit(Loess); }
