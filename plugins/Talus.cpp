// Talus — granular feedback delay. Overlapping grains read a live-input delay line;
// their output is fed back into the line, so echoes accrete and pile up.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kMaxGrains = 48;

enum { In, MaxDelay, Delay, Density, Dur, Pitch, Feedback, Spread, kNumArgs };

struct TalusGrain { bool active; double readPos, readInc, start, phase, phaseInc; float ampL, ampR; };

struct Talus : public Unit {
    TalusGrain g[kMaxGrains];
    float* line; int frames, writeHead;
    float fbMono, fbHp;
    double triggerPhase, sr;
    uint32 rng;
    bool failed;
};

static int talusFree(Talus* u) {
    int best = 0; double hi = -1.0;
    for (int i = 0; i < kMaxGrains; ++i) {
        if (!u->g[i].active) return i;
        if (u->g[i].phase > hi) { hi = u->g[i].phase; best = i; }
    }
    return best;
}

static void Talus_clear(Talus* unit, int n) { ClearUnitOutputs(unit, n); }

static void Talus_next(Talus* unit, int nSamples) {
    const float* in = IN(In);
    float* outL = OUT(0); float* outR = OUT(1);

    const double delay = gr::gr_clampd(ZIN0(Delay), 0.001, (double)unit->frames / unit->sr - 0.01);
    const float density = gr::gr_clampf(ZIN0(Density), 0.f, 400.f);
    const double dur    = gr::gr_clampd(ZIN0(Dur), 0.005, 2.0);
    const float pitch   = ZIN0(Pitch);
    const float feedback = gr::gr_clampf(ZIN0(Feedback), 0.f, 1.f);
    const float spread  = gr::gr_clampf(ZIN0(Spread), 0.f, 1.f);
    const double triggerInc = (double)density / unit->sr;
    const double rate = gr::gr_clampd(gr::gr_pitch_ratio(pitch), 0.0625, 16.0);
    const double lenSamp = dur * unit->sr;
    const double minGap = 64.0 + (rate > 1.0 ? lenSamp * (rate - 1.0) : 0.0);
    const float wetGain = (float)(0.7 / std::sqrt(density * dur > 1.0 ? density * dur : 1.0));

    float* line = unit->line; const int F = unit->frames;

    for (int s = 0; s < nSamples; ++s) {
        unit->fbHp += 0.002f * (unit->fbMono - unit->fbHp);
        float fb = feedback * std::tanh(unit->fbMono - unit->fbHp);
        line[unit->writeHead] = gr::gr_sanitize(in[s] + fb);
        unit->writeHead = (unit->writeHead + 1) % F;

        unit->triggerPhase += triggerInc;
        while (unit->triggerPhase >= 1.0) {
            unit->triggerPhase -= 1.0;
            TalusGrain* gr = &unit->g[talusFree(unit)];
            double jitter = (double)spread * 0.3 * delay * unit->sr * gr::gr_rand(unit->rng);
            double back = delay * unit->sr + minGap + jitter;
            float pan = 0.5f + (gr::gr_rand(unit->rng) * 2.f - 1.f) * spread * 0.5f;
            gr->active = true; gr->readPos = 0.0; gr->readInc = rate;
            gr->start = gr::gr_wrap((double)unit->writeHead - back, (double)F);
            gr->phase = 0.0; gr->phaseInc = 1.0 / lenSamp;
            gr::gr_pan(pan, gr->ampL, gr->ampR);
        }

        float wL = 0.f, wR = 0.f, mono = 0.f;
        for (int i = 0; i < kMaxGrains; ++i) {
            TalusGrain* gr = &unit->g[i];
            if (!gr->active) continue;
            float v = gr::gr_cubic(line, F, 1, 0, gr->start + gr->readPos * gr->readInc);
            float w = gr::gr_window((float)gr->phase, 1.0f);
            float vw = v * w;
            wL += vw * gr->ampL; wR += vw * gr->ampR; mono += vw;
            gr->readPos += 1.0; gr->phase += gr->phaseInc;
            if (gr->phase >= 1.0) gr->active = false;
        }
        unit->fbMono = mono * wetGain;
        outL[s] = gr::gr_sanitize(wL * wetGain);
        outR[s] = gr::gr_sanitize(wR * wetGain);
    }
}

static void Talus_Ctor(Talus* unit) {
    unit->sr = SAMPLERATE; unit->triggerPhase = 0.0;
    unit->fbMono = unit->fbHp = 0.f; unit->writeHead = 0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    memset(unit->g, 0, sizeof(unit->g));

    double maxDelay = gr::gr_clampd(ZIN0(MaxDelay), 0.05, 30.0);
    unit->frames = (int)(maxDelay * unit->sr) + 64;
    unit->line = (float*)RTAlloc(unit->mWorld, (size_t)unit->frames * sizeof(float));
    unit->failed = (unit->line == nullptr);
    if (unit->failed) { SETCALC(Talus_clear); ClearUnitOutputs(unit, 1); return; }
    memset(unit->line, 0, (size_t)unit->frames * sizeof(float));
    SETCALC(Talus_next);
    ClearUnitOutputs(unit, 1);
}

static void Talus_Dtor(Talus* unit) { if (unit->line) RTFree(unit->mWorld, unit->line); }

PluginLoad(Talus) { ft = inTable; DefineDtorUnit(Talus); }
