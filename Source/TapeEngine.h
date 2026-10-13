// TapeEngine.h
// Motor de sonido "cinta / cassette" (wow, flutter, saturacion, tono, ruido).
// No depende de JUCE: se puede compilar y probar solo con un compilador de C++.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bombap
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// ---------------------------------------------------------------------------
// Parametros que llegan desde el plugin
// ---------------------------------------------------------------------------
struct Params
{
    int   mode     = 0;      // 0 = Tape, 1 = Cassette
    float drive    = 0.30f;  // 0..1  saturacion
    float wow      = 0.30f;  // 0..1  variacion lenta de pitch
    float flutter  = 0.30f;  // 0..1  variacion rapida de pitch
    float noise    = 0.20f;  // 0..1  hiss (siseo)
    float tone     = 0.50f;  // 0..1  0 = oscuro, 1 = brillante
    float mix      = 1.00f;  // 0..1  dry/wet
    float outputDb = 0.0f;   // dB
};

// ---------------------------------------------------------------------------
// Caracter de cada modo. Aqui se "afina" el sonido de Tape vs Cassette.
// ---------------------------------------------------------------------------
struct ModeSettings
{
    float wowRateHz;      // velocidad del wow
    float wowMaxMs;       // profundidad maxima del wow (ms de retardo)
    float flutterRateHz;  // velocidad del flutter
    float flutterMaxMs;   // profundidad maxima del flutter
    float driveMaxDb;     // saturacion maxima (dB de ganancia previa)
    float bias;           // asimetria de la saturacion (armonicos pares)
    float lpHz;           // corte pasa-bajos (con Tone al centro)
    float hpHz;           // corte pasa-altos
    float bumpHz;         // frecuencia del "head bump" (realce de graves)
    float bumpDb;         // cantidad de head bump
    float hissMax;        // nivel maximo de siseo
};

inline const ModeSettings& modeSettings (int mode)
{
    //                           wowHz wowMs fluHz fluMs  drvDb bias   lp      hp    bumpHz bumpDb hiss
    static const ModeSettings tape     { 0.55f, 1.6f, 9.0f, 0.10f, 18.f, 0.06f, 15000.f, 28.f,  85.f, 2.0f, 0.030f };
    static const ModeSettings cassette { 0.80f, 2.4f, 6.5f, 0.18f, 22.f, 0.10f,  9000.f, 55.f, 110.f, 1.5f, 0.080f };
    return mode == 1 ? cassette : tape;
}

// ---------------------------------------------------------------------------
// Filtro biquad (formulas RBJ), en double para que los graves queden limpios
// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    float process (float xf)
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    static double clampFreq (double f, double fs) { return std::min (std::max (f, 10.0), fs * 0.45); }

    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0;
        a1 = na1 / a0; a2 = na2 / a0;
    }

    void lowpass (double fs, double f, double q)
    {
        const double w0 = kTwoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    void highpass (double fs, double f, double q)
    {
        const double w0 = kTwoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    void peaking (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }
};

// ---------------------------------------------------------------------------
// Variacion aleatoria suave (valores al azar unidos con una curva Catmull-Rom).
// A diferencia de filtrar ruido blanco, su derivada es continua, asi que el
// pitch se mueve de forma fluida y no "tiembla".
// ---------------------------------------------------------------------------
struct SmoothRandom
{
    float c[4] = { 0, 0, 0, 0 };
    double phase = 0.0;
    double inc = 0.0;
    uint32_t rng = 1;

    void init (double rateHz, double fs, uint32_t seed)
    {
        inc = rateHz / fs;
        rng = seed;
        phase = 0.0;
        for (auto& v : c) v = nextRandom();
    }

    float nextRandom()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) static_cast<int32_t> (rng) * (1.0f / 2147483648.0f);
    }

    // devuelve un valor aproximadamente entre -1 y 1
    float next()
    {
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            c[0] = c[1]; c[1] = c[2]; c[2] = c[3]; c[3] = nextRandom();
        }
        const float t = (float) phase;
        const float c1 = 0.5f * (c[2] - c[0]);
        const float c2 = c[0] - 2.5f * c[1] + 2.0f * c[2] - 0.5f * c[3];
        const float c3 = 0.5f * (c[3] - c[0]) + 1.5f * (c[1] - c[2]);
        return ((c3 * t + c2) * t + c1) * t + c[1];
    }
};

// ---------------------------------------------------------------------------
// Motor
// ---------------------------------------------------------------------------
class TapeEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        nch = std::min (std::max (numChannels, 1), 2);

        baseDelay = (int) std::lround (0.004 * fs);   // 4 ms de latencia fija

        unsigned size = 1;
        while (size < (unsigned) (0.05 * fs))
            size <<= 1;

        for (int c = 0; c < 2; ++c)
        {
            ch[c].buf.assign (size, 0.0f);
            ch[c].mask = size - 1;
            ch[c].rng = 0x9E3779B9u * (unsigned) (c + 1) + 12345u;
        }

        smoothCoeff = (float) (1.0 - std::exp (-1.0 / (0.03 * fs)));

        reset();
        snapNext = true;
        filtersDirty = true;
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.buf.begin(), c.buf.end(), 0.0f);
            c.w = 0;
            c.hp.reset(); c.bump.reset(); c.lp1.reset(); c.lp2.reset();
            c.prevNoise = 0.0f;
        }
        wowPhase = 0.0;
        fluPhase = 0.25;
        drift.init (0.8, fs, 0xC0FFEEu);     // deriva lenta (wow aleatorio)
        fnoise.init (14.0, fs, 0xBADA55u);   // temblor rapido (flutter aleatorio)
    }

    int getLatencySamples() const { return baseDelay; }

    void setParams (const Params& p)
    {
        const ModeSettings& m = modeSettings (p.mode);
        cur = m;

        // saturacion: arrancamos en -6 dB para que Drive = 0 sea casi transparente
        const double driveDb = (double) p.drive * m.driveMaxDb - 6.0;
        tgtG     = (float) std::pow (10.0, driveDb / 20.0);
        tgtWowMs = p.wow * m.wowMaxMs;
        tgtFluMs = p.flutter * m.flutterMaxMs;
        tgtHiss  = m.hissMax * p.noise * p.noise;
        tgtMix   = std::min (std::max (p.mix, 0.0f), 1.0f);
        tgtOut   = (float) std::pow (10.0, (double) p.outputDb / 20.0);
        biasTanh = (float) std::tanh ((double) m.bias);

        if (p.mode != lastMode || p.tone != lastTone)
        {
            lastMode = p.mode;
            lastTone = p.tone;
            tone = p.tone;
            filtersDirty = true;
        }

        if (filtersDirty)
            updateFilters();

        if (snapNext)
        {
            gSm = tgtG; wowSm = tgtWowMs; fluSm = tgtFluMs;
            hissSm = tgtHiss; mixSm = tgtMix; outSm = tgtOut;
            snapNext = false;
        }
    }

    // io[c][i] se procesa "en sitio"
    void process (float* const* io, int numCh, int numSamples)
    {
        const int n = std::min (numCh, nch);
        const double wowInc = cur.wowRateHz / fs;
        const double fluInc = cur.flutterRateHz / fs;
        const float msToSamples = (float) (0.001 * fs);

        for (int i = 0; i < numSamples; ++i)
        {
            // suavizado de parametros
            gSm    += smoothCoeff * (tgtG     - gSm);
            wowSm  += smoothCoeff * (tgtWowMs - wowSm);
            fluSm  += smoothCoeff * (tgtFluMs - fluSm);
            hissSm += smoothCoeff * (tgtHiss  - hissSm);
            mixSm  += smoothCoeff * (tgtMix   - mixSm);
            outSm  += smoothCoeff * (tgtOut   - outSm);

            // moduladores (compartidos entre canales: la cinta es una sola)
            wowPhase += wowInc; if (wowPhase >= 1.0) wowPhase -= 1.0;
            fluPhase += fluInc; if (fluPhase >= 1.0) fluPhase -= 1.0;
            const float wowMod = 0.7f * (float) std::sin (kTwoPi * wowPhase) + 0.3f * drift.next();
            const float fluMod = 0.7f * (float) std::sin (kTwoPi * fluPhase) + 0.3f * fnoise.next();

            const float delay = (float) baseDelay + (wowMod * wowSm + fluMod * fluSm) * msToSamples;

            // compensacion de volumen de la saturacion
            const float comp = 1.0f / std::sqrt (gSm * std::tanh (gSm));

            for (int c = 0; c < n; ++c)
            {
                Channel& C = ch[c];
                const float x = io[c][i];

                C.buf[C.w] = x;

                const float wet0 = readHermite (C, delay);
                const float dry  = C.buf[(C.w - (unsigned) baseDelay) & C.mask];

                // saturacion tipo cinta (tanh con un poco de asimetria)
                float s = (std::tanh (gSm * wet0 + cur.bias) - biasTanh) * comp;

                // color: graves, head bump, agudos
                s = C.hp.process (s);
                s = C.bump.process (s);
                s = C.lp1.process (s);
                s = C.lp2.process (s);

                // hiss: ruido blanco con los agudos resaltados
                const float nz = C.white();
                s += (nz - C.prevNoise) * 0.5f * hissSm;
                C.prevNoise = nz;

                io[c][i] = (dry * (1.0f - mixSm) + s * mixSm) * outSm;

                C.w = (C.w + 1) & C.mask;
            }
        }
    }

private:
    struct Channel
    {
        std::vector<float> buf;
        unsigned mask = 0, w = 0;
        Biquad hp, bump, lp1, lp2;
        uint32_t rng = 1;
        float prevNoise = 0.0f;

        float white()
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return (float) static_cast<int32_t> (rng) * (1.0f / 2147483648.0f);
        }
    };

    static float clampf (float v, float lo, float hi) { return std::min (std::max (v, lo), hi); }

    // lectura con interpolacion Hermite de 4 puntos (suave, sin zumbidos)
    float readHermite (const Channel& C, float delay) const
    {
        const double rp = (double) C.w - (double) delay;
        const double fl = std::floor (rp);
        const float frac = (float) (rp - fl);
        const int i1 = (int) fl;

        const float ym1 = C.buf[(unsigned) (i1 - 1) & C.mask];
        const float y0  = C.buf[(unsigned) (i1)     & C.mask];
        const float y1  = C.buf[(unsigned) (i1 + 1) & C.mask];
        const float y2  = C.buf[(unsigned) (i1 + 2) & C.mask];

        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void updateFilters()
    {
        // Tone mueve el corte de agudos +/- 1 octava alrededor del valor del modo
        const double lpHz = cur.lpHz * std::pow (2.0, ((double) tone - 0.5) * 2.0);

        for (auto& c : ch)
        {
            c.hp.highpass (fs, cur.hpHz, 0.707);
            c.bump.peaking (fs, cur.bumpHz, 0.9, cur.bumpDb);
            c.lp1.lowpass (fs, lpHz, 0.5412);   // dos etapas = filtro Butterworth de 4 polos
            c.lp2.lowpass (fs, lpHz, 1.3066);
        }
        filtersDirty = false;
    }

    double fs = 44100.0;
    int nch = 2;
    int baseDelay = 176;
    Channel ch[2];

    ModeSettings cur = modeSettings (0);
    int   lastMode = -1;
    float lastTone = -1.0f;
    float tone = 0.5f;
    bool  filtersDirty = true;
    bool  snapNext = true;

    float tgtG = 1, tgtWowMs = 0, tgtFluMs = 0, tgtHiss = 0, tgtMix = 1, tgtOut = 1, biasTanh = 0;
    float gSm = 1, wowSm = 0, fluSm = 0, hissSm = 0, mixSm = 1, outSm = 1;
    float smoothCoeff = 0.001f;

    double wowPhase = 0.0, fluPhase = 0.25;
    SmoothRandom drift, fnoise;
};

} // namespace bombap
