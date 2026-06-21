// Moraine — detects grains in the live input by amplitude troughs, then transports
// and re-deposits them (after CDP's GRAIN suite). mode: 0 omit (keep some, drop the
// rest), 1 duplicate (repeat each), 2 reorder (shuffle within a window),
// 3 timewarp (open gaps between grains). Each emitted grain is windowed + panned.
#include "SC_PlugIn.h"
static InterfaceTable* ft;
#include "grains.hpp"

static const int kQCap = 256;     // grain queue depth
static const int kWMax = 32;      // reorder window

enum { In, MaxLen, Mode, Gate, MinHole, Amount, Pitch, kNumArgs };

struct Moraine : public Unit {
    float* buf; int frames, writeHead;
    // detector
    bool inGrain; int holeCount, grainStart;
    // grain queue (ring of spans)
    int qStart[kQCap], qLen[kQCap], qHead, qTail;
    // emitter
    bool hasGrain; int curStart, curLen; double playPos, gapLeft;
    int repeatsLeft, omitCounter;
    float panL, panR;
    // reorder window
    int rStart[kWMax], rLen[kWMax], rOrder[kWMax], rCount, rIdx;
    double sr; uint32 rng; bool failed;
};

static void Moraine_clear(Moraine* unit, int n) { ClearUnitOutputs(unit, n); }

static inline int qCount(Moraine* u) { return (u->qTail - u->qHead + kQCap) % kQCap; }
static inline void qPush(Moraine* u, int start, int len) {
    if ((u->qTail + 1) % kQCap == u->qHead) u->qHead = (u->qHead + 1) % kQCap; // drop oldest
    u->qStart[u->qTail] = start; u->qLen[u->qTail] = len; u->qTail = (u->qTail + 1) % kQCap;
}
static inline bool qPop(Moraine* u, int& start, int& len) {
    if (u->qHead == u->qTail) return false;
    start = u->qStart[u->qHead]; len = u->qLen[u->qHead]; u->qHead = (u->qHead + 1) % kQCap;
    return true;
}

// Load the next grain to emit according to mode. Returns false if none available.
static bool moraineLoad(Moraine* u, int mode, int amt) {
    int start, len;
    if (mode == 1) {                      // duplicate: replay current until repeats spent
        if (u->repeatsLeft > 0 && u->curLen > 0) { u->repeatsLeft--; u->playPos = 0.0; return true; }
        if (!qPop(u, start, len)) return false;
        u->repeatsLeft = amt;             // amt extra repeats
    } else if (mode == 2) {               // reorder: buffer a window, emit shuffled
        if (u->rIdx >= u->rCount) {
            int W = 2 + amt; if (W > kWMax) W = kWMax;
            u->rCount = 0;
            while (u->rCount < W && qPop(u, start, len)) { u->rStart[u->rCount] = start; u->rLen[u->rCount] = len; u->rCount++; }
            if (u->rCount == 0) return false;
            for (int i = 0; i < u->rCount; ++i) u->rOrder[i] = i;
            for (int i = u->rCount - 1; i > 0; --i) {      // Fisher-Yates
                int j = (int)(gr::gr_rand(u->rng) * (i + 1)); if (j > i) j = i;
                int t = u->rOrder[i]; u->rOrder[i] = u->rOrder[j]; u->rOrder[j] = t;
            }
            u->rIdx = 0;
        }
        int k = u->rOrder[u->rIdx++]; start = u->rStart[k]; len = u->rLen[k];
    } else if (mode == 0) {               // omit: keep 1 of every (amt+2)
        int outOf = amt + 2;
        do {
            if (!qPop(u, start, len)) return false;
            bool keep = (u->omitCounter % outOf) == 0;
            u->omitCounter = (u->omitCounter + 1) % 100000;
            if (keep) break;
        } while (true);
    } else {                              // timewarp: pop next, open a gap after it
        if (!qPop(u, start, len)) return false;
    }
    u->curStart = start; u->curLen = len; u->playPos = 0.0; u->hasGrain = true;
    float pan = 0.5f + (gr::gr_rand(u->rng) * 2.f - 1.f) * 0.4f;
    gr::gr_pan(pan, u->panL, u->panR);
    return true;
}

static void Moraine_next(Moraine* unit, int nSamples) {
    const float* in = IN(In);
    float* outL = OUT(0); float* outR = OUT(1);

    const int   mode = (int)gr::gr_clampf(ZIN0(Mode), 0.f, 3.f);
    const float gate = gr::gr_clampf(ZIN0(Gate), 0.0001f, 1.f);
    const double minHole = gr::gr_clampd(ZIN0(MinHole), 0.001, 0.5);
    const float amount = gr::gr_clampf(ZIN0(Amount), 0.f, 1.f);
    const float pitch = ZIN0(Pitch);
    const double rate = gr::gr_clampd(gr::gr_pitch_ratio(pitch), 0.0625, 16.0);
    const int minHoleSamp = (int)(minHole * unit->sr) > 1 ? (int)(minHole * unit->sr) : 1;
    const int amt = (mode == 2) ? (int)(amount * 10.f) : (int)(amount * 7.f);
    const double gapScale = (double)amount * 2.0;

    float* buf = unit->buf; const int F = unit->frames;

    for (int s = 0; s < nSamples; ++s) {
        // record + detect
        int idx = unit->writeHead;
        float x = gr::gr_sanitize(in[s]);
        buf[idx] = x;
        bool above = std::fabs(x) > gate;
        if (!unit->inGrain) {
            if (above) { unit->inGrain = true; unit->grainStart = idx; unit->holeCount = 0; }
        } else {
            if (above) unit->holeCount = 0;
            else if (++unit->holeCount >= minHoleSamp) {
                int gEnd = (idx - unit->holeCount + F) % F;
                int len = (gEnd - unit->grainStart + F) % F;
                if (len > 32) qPush(unit, unit->grainStart, len);
                unit->inGrain = false;
            }
        }
        unit->writeHead = (idx + 1) % F;

        // emit
        float v = 0.f;
        if (unit->gapLeft > 0.0) { unit->gapLeft -= 1.0; }
        else {
            if (!unit->hasGrain && !moraineLoad(unit, mode, amt)) { /* nothing to play */ }
            if (unit->hasGrain) {
                double rp = unit->curStart + unit->playPos;
                v = gr::gr_sinc(buf, F, 1, 0, rp, rate);
                v *= gr::gr_env((float)(unit->playPos / unit->curLen), 0.5f);
                unit->playPos += rate;
                if (unit->playPos >= unit->curLen) {
                    unit->hasGrain = false;
                    if (mode == 3) unit->gapLeft = (double)unit->curLen * gapScale;
                }
            }
        }
        outL[s] = gr::gr_sanitize(v * unit->panL);
        outR[s] = gr::gr_sanitize(v * unit->panR);
    }
}

static void Moraine_Ctor(Moraine* unit) {
    unit->sr = SAMPLERATE; unit->writeHead = 0;
    unit->inGrain = false; unit->holeCount = 0; unit->grainStart = 0;
    unit->qHead = unit->qTail = 0;
    unit->hasGrain = false; unit->curStart = 0; unit->curLen = 0; unit->playPos = 0.0;
    unit->gapLeft = 0.0; unit->repeatsLeft = 0; unit->omitCounter = 0;
    unit->panL = unit->panR = 0.7071f; unit->rCount = 0; unit->rIdx = 0;
    unit->rng = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF) | 1u;
    double maxLen = gr::gr_clampd(ZIN0(MaxLen), 0.1, 30.0);
    unit->frames = (int)(maxLen * unit->sr) + 64;
    unit->buf = (float*)RTAlloc(unit->mWorld, (size_t)unit->frames * sizeof(float));
    unit->failed = (unit->buf == nullptr);
    if (unit->failed) { SETCALC(Moraine_clear); ClearUnitOutputs(unit, 1); return; }
    memset(unit->buf, 0, (size_t)unit->frames * sizeof(float));
    SETCALC(Moraine_next);
    ClearUnitOutputs(unit, 1);
}

static void Moraine_Dtor(Moraine* unit) { if (unit->buf) RTFree(unit->mWorld, unit->buf); }

PluginLoad(Moraine) { ft = inTable; gr::gr_init_sinc(); DefineDtorUnit(Moraine); }
