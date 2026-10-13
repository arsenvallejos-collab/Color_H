// Pruebas del motor. Compilar: g++ -O2 -std=c++17 engine_test.cpp -o engine_test
#include "../Source/TapeEngine.h"
#include <cstdio>
#include <vector>

using namespace bombap;

static int failures = 0;
static void check (bool ok, const char* what, double value)
{
    std::printf ("[%s] %s (%.4f)\n", ok ? "OK" : "FALLA", what, value);
    if (! ok) ++failures;
}

static std::vector<float> run (const Params& p, const std::vector<float>& in, double fs)
{
    TapeEngine e;
    e.prepare (fs, 1);
    e.setParams (p);
    std::vector<float> out = in;
    const int block = 512;
    for (size_t i = 0; i < out.size(); i += block)
    {
        float* ptr = out.data() + i;
        int n = (int) std::min<size_t> (block, out.size() - i);
        e.setParams (p);
        e.process (&ptr, 1, n);
    }
    return out;
}

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (to - from));
}

static std::vector<float> sine (double f, double amp, double fs, size_t n)
{
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (kTwoPi * f * (double) i / fs));
    return v;
}

int main()
{
    const double fs = 48000.0;
    const size_t N = (size_t) (fs * 10);

    // 1) Ruido: con silencio de entrada y Noise al maximo debe haber siseo a un nivel razonable
    {
        Params p; p.mode = 0; p.drive = 0; p.wow = 0; p.flutter = 0; p.noise = 1.0f; p.mix = 1.0f;
        auto out = run (p, std::vector<float> (N, 0.0f), fs);
        double r = rms (out, N / 2, N);
        check (r > 0.004 && r < 0.03, "Tape: siseo al maximo entre -48 y -30 dBFS", r);

        p.mode = 1;
        out = run (p, std::vector<float> (N, 0.0f), fs);
        double rc = rms (out, N / 2, N);
        check (rc > r * 1.5, "Cassette suena con mas siseo que Tape", rc);

        p.noise = 0.0f;
        out = run (p, std::vector<float> (N, 0.0f), fs);
        check (rms (out, N / 2, N) < 1e-6, "Noise = 0 -> silencio total", rms (out, N / 2, N));
    }

    // 2) Transparencia: todo en cero, Tone al centro: 1 kHz casi igual de fuerte
    {
        Params p; p.mode = 0; p.drive = 0; p.wow = 0; p.flutter = 0; p.noise = 0; p.mix = 1.0f; p.tone = 0.5f;
        auto in = sine (1000.0, 0.3, fs, N);
        auto out = run (p, in, fs);
        double ratioDb = 20.0 * std::log10 (rms (out, N / 2, N) / rms (in, N / 2, N));
        check (std::fabs (ratioDb) < 1.0, "Con todo en 0 el nivel a 1 kHz cambia menos de 1 dB", ratioDb);
    }

    // 3) Wow: el pitch de un tono de 1 kHz debe oscilar varios Hz
    auto pitchSpread = [&] (Params p, double& minF, double& maxF)
    {
        auto in = sine (1000.0, 0.3, fs, N);
        auto out = run (p, in, fs);
        minF = 1e9; maxF = 0;
        double lastZero = -1;
        for (size_t i = (size_t) (fs * 1.0) + 1; i < out.size(); ++i)
        {
            if (out[i - 1] < 0 && out[i] >= 0)
            {
                double z = (double) (i - 1) + (0 - out[i - 1]) / (double) (out[i] - out[i - 1]);
                if (lastZero > 0)
                {
                    double f = fs / (z - lastZero);
                    minF = std::min (minF, f); maxF = std::max (maxF, f);
                }
                lastZero = z;
            }
        }
    };

    {
        double mn, mx;
        Params p; p.mode = 0; p.drive = 0; p.wow = 1.0f; p.flutter = 0; p.noise = 0; p.mix = 1.0f;
        pitchSpread (p, mn, mx);
        check ((mx - mn) > 6.0 && (mx - mn) < 40.0, "Wow al maximo: el pitch de 1 kHz varia entre 6 y 40 Hz", mx - mn);

        p.wow = 0.0f;
        pitchSpread (p, mn, mx);
        check ((mx - mn) < 1.0, "Wow = 0 y Flutter = 0: el pitch no se mueve", mx - mn);

        p.flutter = 1.0f; p.mode = 1;
        pitchSpread (p, mn, mx);
        check ((mx - mn) > 4.0, "Flutter al maximo (Cassette): el pitch se mueve", mx - mn);
    }

    // 4) Estabilidad: ruido blanco fuerte, todo al maximo, no debe explotar ni dar NaN
    {
        Params p; p.mode = 1; p.drive = 1; p.wow = 1; p.flutter = 1; p.noise = 1; p.tone = 1; p.mix = 1; p.outputDb = 12;
        std::vector<float> in (N);
        uint32_t s = 1;
        for (auto& v : in) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; v = (float) (int32_t) s / 2147483648.0f; }
        auto out = run (p, in, fs);
        bool ok = true; double peak = 0;
        for (float v : out) { if (! std::isfinite (v)) ok = false; peak = std::max (peak, (double) std::fabs (v)); }
        check (ok, "Todo al maximo: sin NaN ni infinitos", peak);
        check (peak < 20.0, "Todo al maximo: pico razonable", peak);
    }

    // 5) Cambiar de modo y de sample rate no debe romper nada
    {
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Params p; p.mode = 0;
            auto in = sine (440.0, 0.4, sr, (size_t) sr);
            auto out = run (p, in, sr);
            bool ok = true;
            for (float v : out) if (! std::isfinite (v)) ok = false;
            char msg[96];
            std::snprintf (msg, sizeof msg, "Sample rate %.0f Hz funciona", sr);
            check (ok, msg, rms (out, out.size() / 2, out.size()));
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY PRUEBAS QUE FALLARON");
    return failures == 0 ? 0 : 1;
}
