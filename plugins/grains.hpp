// Shared helpers for the Sediment granular family. static inline, prefixed gr_,
// local PI (never M_PI). Included after SC_PlugIn.h by each UGen .cpp.
#pragma once
#include <cmath>

namespace gr {

static const double G_PI  = 3.14159265358979323846;
static const double G_2PI = 6.28318530717958647692;

static inline float  gr_clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline double gr_clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float  gr_sanitize(float x) { return std::isfinite(x) ? x : 0.0f; }

static inline double gr_wrap(double x, double n) {
    if (n <= 0.0) return 0.0;
    x = std::fmod(x, n); if (x < 0.0) x += n; return x;
}

// xorshift32 in [0,1). Seed once from the parent RGen.
static inline float gr_rand(uint32& s) {
    uint32 x = s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s = x;
    return (float)((double)x / 4294967296.0);
}

static inline double gr_pitch_ratio(float semitones) { return std::pow(2.0, (double)semitones / 12.0); }

// Hann^k window at phase p in [0,1]. k=1 Hann, k>1 narrower, k<1 broader.
static inline float gr_window(float p, float k) {
    if (p < 0.0f || p > 1.0f) return 0.0f;
    float base = 0.5f - 0.5f * std::cos((float)G_2PI * p);
    return k == 1.0f ? base : std::pow(base, k);
}

// 4-point cubic read of one channel of an interleaved buffer (stride frames apart).
static inline float gr_cubic(const float* data, int frames, int stride, int ch, double pos) {
    if (!data || frames < 4) return 0.0f;
    pos = gr_wrap(pos, (double)frames);
    int i1 = (int)pos; double fr = pos - (double)i1;
    int i0 = (i1 - 1 + frames) % frames, i2 = (i1 + 1) % frames, i3 = (i1 + 2) % frames;
    float y0 = data[i0 * stride + ch], y1 = data[i1 * stride + ch];
    float y2 = data[i2 * stride + ch], y3 = data[i3 * stride + ch];
    float c0 = y1, c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * (float)fr + c2) * (float)fr + c1) * (float)fr + c0;
}

// Equal-power pan amplitudes for pan in [0,1].
static inline void gr_pan(float pan, float& l, float& r) {
    pan = gr_clampf(pan, 0.0f, 1.0f);
    float a = pan * (float)(G_PI * 0.5);
    l = std::cos(a); r = std::sin(a);
}

// Morphable grain window at phase p in [0,1]. shape 0 = percussive (peak at start),
// 0.5 = symmetric (Hann-like), 1 = reversed (peak at end). Zero at both ends.
static inline float gr_env(float p, float shape) {
    if (p < 0.0f || p > 1.0f) return 0.0f;
    float a = gr_clampf(shape, 0.02f, 0.98f);
    if (p < a) { float t = p / a; return 0.5f - 0.5f * std::cos((float)G_PI * t); }
    float t = (p - a) / (1.0f - a);
    return 0.5f + 0.5f * std::cos((float)G_PI * t);
}

// --- band-limited sinc interpolation (per-plugin LUT, built in PluginLoad) -----
static const int   GR_SINC_MAXARG = 16, GR_SINC_RES = 512;
static const int   GR_SINC_SIZE   = GR_SINC_MAXARG * GR_SINC_RES + 1;
static float       g_grSinc[GR_SINC_SIZE];
static bool        g_grSincInit = false;

static inline void gr_init_sinc() {
    if (g_grSincInit) return;
    g_grSinc[0] = 1.0f;
    for (int i = 1; i < GR_SINC_SIZE; ++i) {
        double x = (double)i / (double)GR_SINC_RES;
        g_grSinc[i] = (float)(std::sin(G_PI * x) / (G_PI * x));
    }
    g_grSincInit = true;
}
static inline float gr_sinclut(double w) {
    double aw = std::fabs(w);
    if (aw >= (double)GR_SINC_MAXARG) return 0.0f;
    double idx = aw * (double)GR_SINC_RES;
    int i0 = (int)idx; if (i0 >= GR_SINC_SIZE - 1) return 0.0f;
    float fr = (float)(idx - i0);
    return g_grSinc[i0] + fr * (g_grSinc[i0 + 1] - g_grSinc[i0]);
}
// Anti-aliased read of one channel; band-limit cutoff falls when transposing up.
static inline float gr_sinc(const float* data, int frames, int stride, int ch, double pos, double absRate) {
    if (!data || frames < 4) return 0.0f;
    pos = gr_wrap(pos, (double)frames);
    int center = (int)pos; double frac = pos - (double)center;
    if (absRate < 0.001) absRate = 0.001;
    double cutoff = absRate > 1.0 ? 1.0 / absRate : 1.0;
    int taps = 8; if (cutoff < 0.5) { taps = (int)(8.0 / cutoff); if (taps > 32) taps = 32; }
    int half = taps / 2; if (half < 2) half = 2;
    float sum = 0.f, norm = 0.f;
    for (int t = -half + 1; t <= half; ++t) {
        int idx = ((center + t) % frames + frames) % frames;
        double x = (double)t - frac;
        float coef = (float)cutoff * gr_sinclut(x * cutoff) * gr_sinclut(x / (double)half);
        sum += data[idx * stride + ch] * coef; norm += coef;
    }
    return std::fabs(norm) > 1e-6f ? sum / norm : 0.0f;
}

// --- wavecycle (zero-crossing span) detection ---------------------------------
// Index of the next sign change at or after i (-1 if none before the end).
static inline int gr_zc_next(const float* d, int frames, int stride, int ch, int i) {
    if (i < 0 || i >= frames - 1) return -1;
    bool s = d[i * stride + ch] >= 0.f;
    ++i;
    while (i < frames && ((d[i * stride + ch] >= 0.f) == s)) ++i;
    return i < frames ? i : -1;
}
// End frame of `cycles` wavecycles from `start` (each cycle = two sign changes); -1 past end.
static inline int gr_waveset_end(const float* d, int frames, int stride, int ch, int start, int cycles) {
    int i = start;
    for (int c = 0; c < cycles; ++c) {
        i = gr_zc_next(d, frames, stride, ch, i); if (i < 0) return -1;
        i = gr_zc_next(d, frames, stride, ch, i); if (i < 0) return -1;
    }
    return i;
}

// Shape a uniform deviate f in [0,1) to ~[-1,1] by distribution `which`, param a in [0,1].
// 0 linear, 1 cauchy, 2 logistic, 3 hyperbcos, 4 arcsine, 5 exponential, 6 sinus.
static inline float gr_distribution(int which, float a, float f) {
    float temp, c;
    a = gr_clampf(a, 0.0001f, 1.0f);
    switch (which) {
    case 1: c = std::atan(10.f * a); return ((1.f / a) * std::tan(c * (2.f * f - 1.f))) * 0.1f;
    case 2: c = 0.5f + (0.499f * a); c = std::log((1.f - c) / c);
            f = ((f - 0.5f) * 0.998f * a) + 0.5f; return std::log((1.f - f) / f) / c;
    case 3: c = std::tan(1.5692255f * a); temp = std::tan(1.5692255f * a * f) / c;
            temp = std::log(temp * 0.999f + 0.001f) * (-0.1447648f); return 2.f * temp - 1.0f;
    case 4: c = std::sin(1.5707963f * a); return std::sin((float)G_PI * (f - 0.5f) * a) / c;
    case 5: c = std::log(1.f - (0.999f * a)); return 2.f * (std::log(1.f - (f * 0.999f * a)) / c) - 1.f;
    case 6: return 2.f * a - 1.f;
    default: return 2.f * f - 1.f;
    }
}

} // namespace gr
