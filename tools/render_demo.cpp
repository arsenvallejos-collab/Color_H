// Genera un loop boom bap sintetico y lo pasa por el motor (Tape y Cassette).
// Compilar: g++ -O2 -std=c++17 render_demo.cpp -o render_demo && ./render_demo
#include "../Source/TapeEngine.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace bombap;

static const double FS = 44100.0;

static uint32_t rngState = 7;
static float rnd() // -1..1
{
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return (float) static_cast<int32_t> (rngState) * (1.0f / 2147483648.0f);
}

static void addAt (std::vector<float>& out, const std::vector<float>& snd, double timeSec, float gain)
{
    size_t start = (size_t) (timeSec * FS);
    for (size_t i = 0; i < snd.size() && start + i < out.size(); ++i)
        out[start + i] += snd[i] * gain;
}

static std::vector<float> makeKick()
{
    std::vector<float> v ((size_t) (0.45 * FS));
    double ph = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        double t = (double) i / FS;
        double f = 48.0 + 95.0 * std::exp (-t * 32.0);
        ph += kTwoPi * f / FS;
        v[i] = (float) (std::sin (ph) * std::exp (-t * 7.0));
    }
    return v;
}

static std::vector<float> makeSnare()
{
    std::vector<float> v ((size_t) (0.35 * FS));
    float prev = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        double t = (double) i / FS;
        float n = rnd();
        float hp = n - 0.6f * prev; prev = n;
        v[i] = (float) (0.55 * hp * std::exp (-t * 16.0) + 0.45 * std::sin (kTwoPi * 190.0 * t) * std::exp (-t * 28.0));
    }
    return v;
}

static std::vector<float> makeHat (double decay)
{
    std::vector<float> v ((size_t) (0.25 * FS));
    float prev = 0, prev2 = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        double t = (double) i / FS;
        float n = rnd();
        float hp = n - 2.0f * prev + prev2; prev2 = prev; prev = n;
        v[i] = (float) (0.2 * hp * std::exp (-t * decay));
    }
    return v;
}

// acorde tipo Rhodes (notas sostenidas: aqui el wow y el flutter se escuchan muy bien)
static std::vector<float> makeChord (const std::vector<double>& freqs, double lengthSec)
{
    std::vector<float> v ((size_t) (lengthSec * FS));
    for (size_t i = 0; i < v.size(); ++i)
    {
        double t = (double) i / FS;
        double env = (1.0 - std::exp (-t * 120.0)) * std::exp (-t * 1.1);
        double s = 0;
        for (double f : freqs)
            s += std::sin (kTwoPi * f * t) + 0.35 * std::sin (kTwoPi * 2.0 * f * t) * std::exp (-t * 3.0)
               + 0.12 * std::sin (kTwoPi * 3.0 * f * t) * std::exp (-t * 5.0);
        v[i] = (float) (s * env * 0.12);
    }
    return v;
}

static void writeWav (const char* path, const std::vector<float>& mono)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    uint32_t dataSize = (uint32_t) (mono.size() * 2 * 2);
    uint32_t riffSize = 36 + dataSize;
    uint16_t fmt = 1, ch = 2, bits = 16, blockAlign = 4;
    uint32_t sr = (uint32_t) FS, byteRate = sr * 4, fmtSize = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riffSize, 4, 1, f); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); std::fwrite (&fmtSize, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f); std::fwrite (&sr, 4, 1, f);
    std::fwrite (&byteRate, 4, 1, f); std::fwrite (&blockAlign, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSize, 4, 1, f);
    for (float x : mono)
    {
        float c = std::max (-1.0f, std::min (1.0f, x));
        int16_t s = (int16_t) std::lround (c * 32767.0f);
        std::fwrite (&s, 2, 1, f); std::fwrite (&s, 2, 1, f);
    }
    std::fclose (f);
}

static std::vector<float> process (const Params& p, const std::vector<float>& in)
{
    TapeEngine e;
    e.prepare (FS, 1);
    std::vector<float> out = in;
    for (size_t i = 0; i < out.size(); i += 512)
    {
        float* ptr = out.data() + i;
        int n = (int) std::min<size_t> (512, out.size() - i);
        e.setParams (p);
        e.process (&ptr, 1, n);
    }
    return out;
}

static void stats (const char* name, const std::vector<float>& v)
{
    double peak = 0, sum = 0;
    for (float x : v) { peak = std::max (peak, (double) std::fabs (x)); sum += (double) x * x; }
    std::printf ("%-10s pico %.2f dBFS   rms %.2f dBFS\n", name,
                 20 * std::log10 (peak + 1e-12), 20 * std::log10 (std::sqrt (sum / (double) v.size()) + 1e-12));
}

int main()
{
    const double bpm = 90.0, beat = 60.0 / bpm, step = beat / 4.0;
    const int bars = 4;
    std::vector<float> mix ((size_t) ((bars * 4 * beat + 1.5) * FS), 0.0f);

    auto kick = makeKick(); auto snare = makeSnare();
    auto hatC = makeHat (70.0); auto hatO = makeHat (14.0);

    const double am7[] = { 220.0, 261.63, 329.63, 392.0 };
    const double fmaj7[] = { 174.61, 220.0, 261.63, 329.63 };
    auto chordA = makeChord (std::vector<double> (am7, am7 + 4), 2.2);
    auto chordF = makeChord (std::vector<double> (fmaj7, fmaj7 + 4), 2.2);

    for (int b = 0; b < bars; ++b)
    {
        double t0 = b * 4 * beat;
        addAt (mix, (b % 2 == 0) ? chordA : chordF, t0, 1.0f);
        const int kicks[] = { 0, 6, 10 };
        for (int k : kicks) addAt (mix, kick, t0 + k * step, 0.9f);
        addAt (mix, snare, t0 + 4 * step, 0.8f);
        addAt (mix, snare, t0 + 12 * step, 0.8f);
        for (int s = 0; s < 16; s += 2)
        {
            double swing = (s % 4 == 2) ? 0.025 : 0.0;
            addAt (mix, (s == 14) ? hatO : hatC, t0 + s * step + swing, (s % 4 == 0) ? 0.8f : 0.5f);
        }
    }

    // normaliza el original a -3 dBFS de pico
    float peak = 0; for (float x : mix) peak = std::max (peak, std::fabs (x));
    for (float& x : mix) x *= 0.708f / peak;

    Params tape;     tape.mode = 0;     tape.drive = 0.35f; tape.wow = 0.25f; tape.flutter = 0.20f; tape.noise = 0.15f; tape.tone = 0.50f; tape.mix = 1.0f;
    Params cassette; cassette.mode = 1; cassette.drive = 0.50f; cassette.wow = 0.45f; cassette.flutter = 0.40f; cassette.noise = 0.35f; cassette.tone = 0.40f; cassette.mix = 1.0f;

    auto outTape = process (tape, mix);
    auto outCas  = process (cassette, mix);

    stats ("original", mix);
    stats ("tape", outTape);
    stats ("cassette", outCas);

    writeWav ("demo_1_original.wav", mix);
    writeWav ("demo_2_tape.wav", outTape);
    writeWav ("demo_3_cassette.wav", outCas);
    return 0;
}
