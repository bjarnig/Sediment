// Creep — a drunken-walk read-head over the live input (after CDP's DRUNK).
// A read locus wanders by random steps within an ambitus; each step reads a
// windowed grain from there, so playback creeps backward/forward through time.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kMaxGrains = 24;

enum { In, MaxAmbitus, Ambitus, Step, GrainDur, Overlap, Pitch, Pause, Spread, kNumArgs };

struct CreepGrain { bool active; double readPos, readInc, start, spanLen; float ampL, ampR; };

struct Creep : public Unit {
    CreepGrain g[kMaxGrains];
    float* line; int frames, writeHead;
    double locus, triggerPhase, sr;
    uint32 rng;
    bool failed;
};

static int creepFree(Creep* u) {
    int best = 0; double hi = -1.0;
    for (int i = 0; i < kMaxGrains; ++i) {
        if (!u->g[i].active) return i;
        double ph = u->g[i].spanLen > 0 ? u->g[i].readPos / u->g[i].spanLen : 1.0;
        if (ph > hi) { hi = ph; best = i; }
    }
    return best;
}

static void Creep_clear(Creep* unit, int n) { ClearUnitOutputs(unit, n); }

static void Creep_next(Creep* unit, int nSamples) {
    const float* in = IN(In);
    float* outL = OUT(0); float* outR = OUT(1);

    const double ambitus = gr::gr_clampd(ZIN0(Ambitus), 0.0, (double)unit->frames / unit->sr - 1.0);
    const float step    = gr::gr_clampf(ZIN0(Step), 0.f, 1.f);
    const double gdur   = gr::gr_clampd(ZIN0(GrainDur), 0.005, 1.0);
    const float overlap = gr::gr_clampf(ZIN0(Overlap), 0.f, 0.95f);
    const float pitch   = ZIN0(Pitch);
    const float pause   = gr::gr_clampf(ZIN0(Pause), 0.f, 1.f);
    const float spread  = gr::gr_clampf(ZIN0(Spread), 0.f, 1.f);
    const double rate = gr::gr_clampd(gr::gr_pitch_ratio(pitch), 0.0625, 16.0);
    const double spanLen = gdur * unit->sr;
    const double period = spanLen * (1.0 - (double)overlap);
    const double triggerInc = period > 1.0 ? 1.0 / period : 1.0;
    const double ambSamp = ambitus * unit->sr;
    const double minGap = spanLen * (rate > 1.0 ? rate : 1.0) + 64.0;

    float* line = unit->line; const int F = unit->frames;

    for (int s = 0; s < nSamples; ++s) {
        line[unit->writeHead] = gr::gr_sanitize(in[s]);
        unit->writeHead = (unit->writeHead + 1) % F;

        unit->triggerPhase += triggerInc;
        while (unit->triggerPhase >= 1.0) {
            unit->triggerPhase -= 1.0;
            // drunken walk of the locus within [0, ambitus]
            unit->locus += (gr::gr_rand(unit->rng) * 2.0 - 1.0) * (double)step * ambSamp;
            if (ambSamp > 1.0) unit->locus = gr::gr_wrap(unit->locus, ambSamp); else unit->locus = 0.0;
            if (gr::gr_rand(unit->rng) >= pause) {
                CreepGrain* gr = &unit->g[creepFree(unit)];
                double back = minGap + unit->locus;
                gr->active = true; gr->readPos = 0.0; gr->readInc = rate;
                gr->start = gr::gr_wrap((double)unit->writeHead - back, (double)F);
                gr->spanLen = spanLen;
                float pan = 0.5f + (gr::gr_rand(unit->rng) * 2.f - 1.f) * spread * 0.5f;
                gr::gr_pan(pan, gr->ampL, gr->ampR);
            }
        }

        float wL = 0.f, wR = 0.f; int active = 0;
        for (int i = 0; i < kMaxGrains; ++i) {
            CreepGrain* gr = &unit->g[i];
            if (!gr->active) continue;
            ++active;
            float v = gr::gr_sinc(line, F, 1, 0, gr->start + gr->readPos * gr->readInc, gr->readInc);
            float w = gr::gr_env((float)(gr->readPos / gr->spanLen), 0.5f);
            wL += v * gr->ampL * w; wR += v * gr->ampR * w;
            gr->readPos += 1.0;
            if (gr->readPos >= gr->spanLen) gr->active = false;
        }
        float norm = 0.7f / std::sqrt((float)(active > 1 ? active : 1));
        outL[s] = gr::gr_sanitize(wL * norm);
        outR[s] = gr::gr_sanitize(wR * norm);
    }
}

static void Creep_Ctor(Creep* unit) {
    unit->sr = SAMPLERATE; unit->triggerPhase = 0.0; unit->locus = 0.0; unit->writeHead = 0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    memset(unit->g, 0, sizeof(unit->g));
    double maxAmb = gr::gr_clampd(ZIN0(MaxAmbitus), 0.1, 30.0);
    unit->frames = (int)((maxAmb + 1.5) * unit->sr) + 64;   // ambitus + grain/gap headroom
    unit->line = (float*)RTAlloc(unit->mWorld, (size_t)unit->frames * sizeof(float));
    unit->failed = (unit->line == nullptr);
    if (unit->failed) { SETCALC(Creep_clear); ClearUnitOutputs(unit, 1); return; }
    memset(unit->line, 0, (size_t)unit->frames * sizeof(float));
    SETCALC(Creep_next);
    ClearUnitOutputs(unit, 1);
}

static void Creep_Dtor(Creep* unit) { if (unit->line) RTFree(unit->mWorld, unit->line); }

PluginLoad(Creep) { ft = inTable; gr::gr_init_sinc(); DefineDtorUnit(Creep); }
