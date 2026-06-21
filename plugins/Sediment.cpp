// Sediment — granular texture synthesizer (Mutable Instruments Sediment-inspired, original DSP).
// Phase 2: granular mode. Stereo in -> stereo out, internal recording buffer + grain pool.
#include "SC_PlugIn.h"
static InterfaceTable* ft;   // must precede sediment_dsp.hpp: its RTAlloc/RTFree expand to *ft
#include "sediment_dsp.hpp"
#include <cmath>

// Input order MUST match classes/Sediment.sc multiNew order. Sediment exposes
// macro controls (scatter/bloom/drift) rather than Clouds' knob-for-knob layout.
enum {
    Pitch, Position, Scatter, Bloom, Drift, Feedback, DryWet,
    Freeze, Mode, Trigger, InL, InR, kNumArgs
};

struct Sediment : public Unit {
    sediment::Engine eng;
    // one-pole-smoothed macro params (avoid zipper noise from block-rate updates)
    float sPitch, sPosition, sScatter, sBloom, sDrift, sDryWet;
    bool  allocFailed;
};

static const float kSmooth = 0.1f;

static inline float smooth(float& s, float target) {
    s += kSmooth * (target - s);
    return s;
}

static void Sediment_next(Sediment* unit, int nSamples) {
    sediment::Engine& e = unit->eng;
    const float* inL = IN(InL);
    const float* inR = IN(InR);
    float* outL = OUT(0);
    float* outR = OUT(1);

    // macros, smoothed
    const float basePitch = smooth(unit->sPitch,    sediment::cl_clampf(ZIN0(Pitch),  -48.f, 48.f));
    const float basePos   = smooth(unit->sPosition, sediment::cl_clampf(ZIN0(Position), 0.f, 1.f));
    const float scatter   = smooth(unit->sScatter,  sediment::cl_clampf(ZIN0(Scatter),  0.f, 1.f));
    const float bloom     = smooth(unit->sBloom,    sediment::cl_clampf(ZIN0(Bloom),    0.f, 1.f));
    const float drift     = smooth(unit->sDrift,    sediment::cl_clampf(ZIN0(Drift),    0.f, 1.f));
    const float dryWet    = smooth(unit->sDryWet,   sediment::cl_clampf(ZIN0(DryWet),   0.f, 1.f));

    e.frozen = ZIN0(Freeze) > 0.f;
    const float feedbackAmt = sediment::cl_clampf(ZIN0(Feedback), 0.f, 1.f);
    const int   mode  = (int)sediment::cl_clampf(ZIN0(Mode), 0.f, 2.f);

    // drift: a slow autonomous wander (two detuned sines) on position and pitch.
    const double dph = e.driftPhase;
    const float  posMod   = drift * 0.30f * std::sin((float)dph);
    const float  pitchMod = drift * 5.00f * std::sin((float)dph * 0.373f + 1.0f);
    e.driftPhase += 0.08 * (double)(sediment::CL_2PI) * nSamples / e.sr;  // ~0.08 Hz
    if (e.driftPhase > sediment::CL_2PI) e.driftPhase -= sediment::CL_2PI;

    // macro -> engine mapping. scatter drives the cloud's busyness/width/randomness;
    // bloom drives grain size and the space amount; drift moves position/pitch.
    const float position = sediment::cl_clampf(basePos + posMod, 0.f, 1.f);
    const float pitch    = basePitch + pitchMod;
    const float size     = bloom;       // grain length / segment / loop length
    const float texture  = scatter;     // window variation + position jitter
    const float spread   = scatter;     // stereo spread
    const float spaceAmt = bloom;       // FDN mix
    const float decay = 0.3f + 0.6f * bloom;
    const float damp  = 0.35f;

    // granular: grain rate from scatter. stretch: analysis speed from scatter.
    const double triggerRate  = std::pow(200.0, (double)scatter);
    const double triggerInc   = triggerRate / e.sr;
    const float  stretchSpeed = (float)std::pow(8.0, (double)scatter - 0.5);

    // dry/wet equal-power coefficients.
    const float angle = dryWet * (float)(sediment::CL_PI * 0.5);
    const float dryC = std::cos(angle), wetC = std::sin(angle);

    // normalize wet level. Granular: by expected grain overlap. Stretch: 50% Hann
    // overlap is ~constant power. Looping: near unity.
    const double lengthSec = 0.008 * std::pow(50.0, (double)size);
    const double overlap   = triggerRate * lengthSec;
    const float  wetGain   = (mode == 1) ? 0.5f
                           : (mode == 2) ? 0.8f
                           : (float)(0.7 / std::sqrt(overlap > 1.0 ? overlap : 1.0));

    const int bufFrames = e.bufFrames;
    float* buf = e.buf;

    for (int i = 0; i < nSamples; ++i) {
        // feedback: recirculate the last wet output into the recording, with a
        // DC/low cut and tanh saturation so the loop stays bounded.
        float fbInL = 0.f, fbInR = 0.f;
        if (feedbackAmt > 0.f) {
            unit->eng.fbHpL += 0.002f * (e.fbL - unit->eng.fbHpL);
            unit->eng.fbHpR += 0.002f * (e.fbR - unit->eng.fbHpR);
            fbInL = feedbackAmt * std::tanh(e.fbL - unit->eng.fbHpL);
            fbInR = feedbackAmt * std::tanh(e.fbR - unit->eng.fbHpR);
        }

        // record input (+ feedback) into the circular buffer (unless frozen)
        if (!e.frozen) {
            buf[e.writeHead * 2]     = sediment::cl_sanitize(inL[i] + fbInL);
            buf[e.writeHead * 2 + 1] = sediment::cl_sanitize(inR[i] + fbInR);
            e.writeHead = (e.writeHead + 1) % bufFrames;
        }

        // mode-specific wet generation
        float wetL = 0.f, wetR = 0.f;
        if (mode == 2) {
            e.loopingSample(position, size, pitch, spread, wetL, wetR);
            wetL *= wetGain; wetR *= wetGain;
        } else {
            if (mode == 1) {
                e.stretchTick(position, size, pitch, spread, stretchSpeed);
            } else {
                e.triggerPhase += triggerInc;
                while (e.triggerPhase >= 1.0) {
                    e.triggerGrain(position, size, pitch, texture, spread);
                    e.triggerPhase -= 1.0;
                }
            }
            // render active grains (granular + stretch share the grain pool)
            for (int g = 0; g < sediment::kMaxGrains; ++g) {
                sediment::Grain* gr = &e.grains[g];
                if (!gr->active) continue;
                double rp = gr->start + gr->readPos * gr->readInc;
                float sL = sediment::cl_read(buf, bufFrames, rp, 0);
                float sR = sediment::cl_read(buf, bufFrames, rp, 1);
                float mono = 0.5f * (sL + sR);
                float win = sediment::cl_window((float)gr->phase, gr->winK);
                wetL += mono * gr->ampL * win;
                wetR += mono * gr->ampR * win;
                gr->readPos += 1.0;
                gr->phase   += gr->phaseInc;
                if (gr->phase >= 1.0) gr->active = false;
            }
            wetL *= wetGain;
            wetR *= wetGain;
        }

        // diffuse the grain output, then add the (always-running) FDN space
        float dL, dR;
        e.diffuser.process(wetL, wetR, dL, dR);
        wetL = dL; wetR = dR;

        float rL, rR;
        e.space.process(wetL, wetR, rL, rR, decay, damp);
        wetL += spaceAmt * rL;
        wetR += spaceAmt * rR;

        // capture wet output for the next sample's feedback
        e.fbL = wetL; e.fbR = wetR;

        outL[i] = sediment::cl_sanitize(inL[i] * dryC + wetL * wetC);
        outR[i] = sediment::cl_sanitize(inR[i] * dryC + wetR * wetC);
    }
}

static void Sediment_next_failed(Sediment* unit, int nSamples) {
    ClearUnitOutputs(unit, nSamples);
}

static void Sediment_Ctor(Sediment* unit) {
    uint32 seed = (uint32)unit->mParent->mRGen->irand(0x7FFFFFFF);
    unit->allocFailed = !unit->eng.init(unit->mWorld, SAMPLERATE, seed);

    unit->sPitch = 0.f; unit->sPosition = 0.5f; unit->sScatter = 0.5f;
    unit->sBloom = 0.3f; unit->sDrift = 0.3f; unit->sDryWet = 0.5f;

    if (unit->allocFailed) {
        SETCALC(Sediment_next_failed);
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(Sediment_next);
    ClearUnitOutputs(unit, 1);
}

static void Sediment_Dtor(Sediment* unit) {
    unit->eng.release(unit->mWorld);
}

PluginLoad(Sediment) {
    ft = inTable;
    DefineDtorUnit(Sediment);
}
