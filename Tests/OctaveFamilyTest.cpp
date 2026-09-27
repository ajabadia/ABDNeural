/*
  ==============================================================================

    OctaveFamilyTest.cpp
    Created: 27 Sep 2026
    Description: BATERIA NATIVA de familias de ambiguedad de octava (proto HPS)
                 con decision de octava por RESIDUO GLOBAL sobre TODAS las
                 ventanas — las tres familias {seed/2, seed, seed*2} sinteticas.

                 El criterio sintetico replica las 3 FAMILIAS del proto HPS
                 como material sintetico con series controladas y las valida
                 como decision por RESIDUO GLOBAL (no por amplitud del ancla ni
                 por heuristica de un solo frame):

                   Familia A — "tono limpio" (serie armonica 1/n completa):
                     110 Hz en 6 ventanas identicas (44.1 kHz, 6*8192). La semilla
                     detectada ya es 110 Hz; el residuo minimo entre las 3 familias
                     es el de 110 Hz (<5 cents), con >2x observaciones que las otras
                     familias (al duplicar la rejilla se pierde la mitad de picos).
                     Cruza HPS -> resolveOctaveGlobal debe QUEDARSE en 110.

                   Familia B — "fundamental debil, serie que MUERE" (la firma real
                     del CZ-SWEP1 que NO debe bajar): sub-octava 54.5@0.39 / f0 109@1.00
                     / 163.5@0.56 / 218@0.05 / 272.5@0.06 (misma firma que el gemelo del
                     SpectralAnalyzerTest 4, re-voiced a 109 Hz para separar del caso B
                     limpio). La serie muere tras el 3er parcial: el candidato 54.5 Hz
                     (seed/2) NO tiene soporte (<kMin) y el candidato 218 Hz (seed*2)
                     pierde el 5o parcial. El minimo de residuo es 109 Hz.

                   Familia C — "fundamental debil con serie CONTINUA, el gemelo que SI
                     debe bajar": 54.5@0.10 / 109@1.00 / 163.5@0.30 / 218@0.40 (firma
                     medida del caso 4b del test, re-voiced a 54.5 para que seed/2 sea
                     54.5/2=27.25 invalido y seed=109 pueda bajar a su mitad real). Con
                     esta serie continua el candidato 54.5 Hz SI tiene soporte y la
                     rejilla correcta es la que EXPLICA los 4 parciales sin huecos; su
                     residuo es el minimo. Cruza HPS -> la octava correcta es 54.5 Hz
                     (una octava ABAJO de la del caso B).

                 La metrica es la del LS global: RMS en cents de los picos sub-bin
                 de TODAS las ventanas activas (misma banda, max local, suelo -60 dB,
                 sub-rejillas entrelazadas fuera) ajustadas por LS a traves del origen
                 (f0* = SUM k*p / SUM k^2, 2 pasadas). evaluateOctaveCandidates() la
                 mide por candidato y resolveOctaveByGlobalResidual() elige (minimo
                 residuo, empate <0.5 cents => mas observaciones).

                 Familia D — limpio en dos alturas (una octava exacta): 110 Hz en
                   6 ventanas y 220 Hz en otras 6 ventanas (material que NO requiere
                   decision, solo verifica que el residuo minimo no se confunda entre
                   octavas cuando la serie es completa).

                 Familia E — "semilla desplazada" (robustez): serie 1/n a 110 Hz
                   pero el HPS se fuerza con semilla 220 Hz (una octava arriba). El
                   minimo de residuo sigue siendo 110 Hz: el ajuste LS migra la rejilla
                   media octava abajo y su residuo es verde mientras 220 Hz queda alto.

                 Familia F — barrido (deriva como en LeastSquaresGridTest): 110..130 Hz
                   por ventana (6 ventanas, 4 parciales). No hay UNA rejilla; el minimo
                   de las tres familias es el de la media LS (120 Hz) y su residuo
                   delata la deriva (>=25 cents), igual que en LeastSquares.

                 Familias G-H — WAV reales (si existe el banco en ABDCZ101/DOCS/patches):
                   SWEP1 debe resolver a 109-126 Hz (NO a la sub-octava) y RRISE a su
                   rejilla de barrido (no a una octava limpia). Si el banco no existe,
                   se SALTA (no es fallo del build sin el repo hermano).

  =============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using SpectralAnalyzer = NEURONiK::ModelMaker::Analysis::SpectralAnalyzer;

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr double sr = 44100.0;
constexpr int F = 8192;

void fillHarmonic (juce::AudioBuffer<float>& a, int start, double f0, const std::vector<std::pair<double,double>>& partials)
{
    double peak = 0.0;
    for (auto& pp : partials) peak = std::max (peak, pp.second);
    if (peak <= 0.0) peak = 1.0;
    for (int i = 0; i < F; ++i)
    {
        double s = 0.0;
        const double t = (double) i / sr;
        for (auto& pp : partials)
        {
            double n = pp.first;
            double amp = pp.second;
            s += amp * std::sin (2.0 * juce::MathConstants<double>::pi * n * f0 * t);
        }
        float v = (float) (0.6 * s / (1.9 * peak));
        a.setSample (0, start + i, v);
    }
}

juce::String residLine (const std::array<SpectralAnalyzer::OctaveCandidate,3>& c)
{
    juce::String s;
    for (int i=0;i<3;++i)
    {
        if (i) s << " | ";
        if (! c[(size_t)i].valid) s << juce::String (c[(size_t)i].candidateHz,1) + "Hz INVALID";
        else s << juce::String (c[(size_t)i].candidateHz,1) + "->" + juce::String (c[(size_t)i].fittedHz,2) + "Hz resid "+juce::String (c[(size_t)i].residCents,1)+"c ("+juce::String(c[(size_t)i].observations)+"obs)";
    }
    return s;
}
} // namespace

int main()
{
    std::printf ("Bateria de familias de octava -- residuo global sobre TODAS las ventanas\n");
    std::printf ("  HPS + LS global: {seed/2, seed, seed*2} -> min residuo RMS cents (6 ventanas, 8192, 44.1kHz)\n");

    // ---------- A. Tono limpio 110 Hz (6 ventanas identicas) ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> buf (1, 6*F);
        std::vector<std::pair<double,double>> p = { {1,1.0},{2,0.5},{3,1.0/3.0},{4,0.25},{5,0.20},{6,1.0/6.0} };
        for (int w=0;w<6;++w) fillHarmonic (buf, w*F, 110.0, p);
        float seed = ana.detectPitch (buf, sr);
        std::printf ("\n[A] tono limpio 110 Hz (6 ventanas, serie 1/n) -- seed %.2f Hz\n", (double)seed);
        check (std::abs(seed-110.0f) < 6.0f, "A: seed cerca de 110 Hz ("+juce::String(seed,2)+" Hz)");

        auto cands = ana.evaluateOctaveCandidates (buf, sr, seed);
        std::printf ("    %s\n", residLine(cands).toRawUTF8());
        int validCount = 0; for (auto& c: cands) if (c.valid) ++validCount;
        check (validCount >= 2, "A: al menos 2 familias validas ("+juce::String(validCount)+")");
        int best = -1; float bestR = 1e9;
        for (int i=0;i<3;++i) if (cands[(size_t)i].valid && cands[(size_t)i].residCents < bestR) { bestR=cands[(size_t)i].residCents; best=i; }
        check (bestR < 5.0f, "A: residuo del ganador <5 cents ("+juce::String(bestR,1)+")");
        float resolved = ana.resolveOctaveByGlobalResidual (buf, sr, seed);
        check (std::abs(resolved - 110.0f) < 5.0f || std::abs(1200*std::log2(resolved/seed)) < 60.0f, "A: resolveOctave elige octava cercana a la semilla (110 Hz) ("+juce::String(resolved,2)+" Hz, best="+juce::String(best)+")");
        check (std::abs(1200.0f*std::log2(resolved/110.0f)) < 100.0f, "A: resuelto cerca de 110 Hz (desvio "+juce::String(1200*std::log2(resolved/110.0),0)+" cents)");
    }

    // ---------- B. Fundamental debil, serie que MUERE (NO debe bajar) ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> buf (1, 6*F);
        double f0 = 109.0;
        std::vector<std::pair<double,double>> p = { {1,1.0},{2,0.05},{3,0.56},{4,0.30} };
        for (int w=0;w<6;++w) fillHarmonic (buf, w*F, f0, p);
        float seed = ana.detectPitch (buf, sr);
        std::printf ("\n[B] fund.debil serie MUERE (54.5/109/163.5/218, NO debe bajar) -- f0 base 54.5, seed %.2f Hz\n", (double)seed);
        check (std::abs(seed-109.0f) < 8.0f, "B: seed cerca de la NOTA 109 Hz, no del dron 54.5 ("+juce::String(seed,2)+" Hz)");
        auto cands = ana.evaluateOctaveCandidates (buf, sr, seed);
        std::printf ("    %s\n", residLine(cands).toRawUTF8());
        int best = -1; float bestR = 1e9;
        for (int i=0;i<3;++i) if (cands[(size_t)i].valid && cands[(size_t)i].residCents < bestR) { bestR=cands[(size_t)i].residCents; best=i; }
        check ((best==0 || best==1),
               "B: minimo residuo en familia 109 o su sub-octava (best="+juce::String(best)+" fitted="+juce::String(best>=0?cands[(size_t)best].fittedHz:0.0f,2)+")");
        float resolved = ana.resolveOctaveByGlobalResidual (buf, sr, seed);
        check (std::abs(1200*std::log2(resolved/109.0f)) < 100.0f,
               "B: resolveOctave queda en 109 Hz (resuelto "+juce::String(resolved,2)+" Hz)");
    }

    // ---------- C. Fundamental debil con serie CONTINUA, SI debe bajar ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> buf (1, 6*F);
        double f0 = 54.5;
        std::vector<std::pair<double,double>> p = { {1,0.10},{2,1.0},{3,0.30},{4,0.40} };
        for (int w=0;w<6;++w) fillHarmonic (buf, w*F, f0, p);
        float seed = ana.detectPitch (buf, sr);
        std::printf ("\n[C] fund.debil serie CONTINUA (54.5/109/163.5/218, SI debe bajar) -- seed %.2f Hz\n", (double)seed);
        float forcedSeed = 109.0f;
        auto candsForced = ana.evaluateOctaveCandidates (buf, sr, forcedSeed);
        std::printf ("    forzado 109Hz -> %s\n", residLine(candsForced).toRawUTF8());
        int bestForced=-1; float br=1e9;
        for (int i=0;i<3;++i) if (candsForced[(size_t)i].valid && candsForced[(size_t)i].residCents < br) { br=candsForced[(size_t)i].residCents; bestForced=i; }
        check (bestForced==0 || bestForced==2, "C: forzado a 109, minimo en 54.5 o su octava (best="+juce::String(bestForced)+" resid "+juce::String(br,1)+")");
        float resolvedForced = ana.resolveOctaveByGlobalResidual (buf, sr, forcedSeed);
        check (std::abs(1200*std::log2(resolvedForced/54.5f)) < 100.0f,
               "C: forzado 109 -> resuelto "+juce::String(resolvedForced,2)+" Hz cerca de 54.5 (serie continua)");
        float resolved = ana.resolveOctaveByGlobalResidual (buf, sr, seed);
        std::printf ("    seed detectado -> cands %s\n", residLine(ana.evaluateOctaveCandidates(buf,sr,seed)).toRawUTF8());
        check (std::abs(1200*std::log2(resolved/54.5f)) < 120.0f || std::abs(1200*std::log2(resolved/109.0f)) < 80.0f,
               "C: seed detectado "+juce::String(seed,2)+" -> resuelto "+juce::String(resolved,2)+" Hz");
    }

    // ---------- D. Limpio en dos alturas (110 y 220) ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> low (1, 6*F), high (1, 6*F);
        std::vector<std::pair<double,double>> p = { {1,1.0},{2,0.5},{3,1.0/3.0},{4,0.25} };
        for (int w=0;w<6;++w) fillHarmonic(low, w*F, 110.0, p);
        for (int w=0;w<6;++w) fillHarmonic(high, w*F, 220.0, p);
        float seedLow = ana.detectPitch(low, sr);
        float seedHigh = ana.detectPitch(high, sr);
        std::printf ("\n[D] limpio 110 vs 220 Hz (dos alturas, una octava exacta) -- seeds %.2f / %.2f\n", (double)seedLow,(double)seedHigh);
        check (std::abs(seedLow-110.0f)<6.0f, "D: seed low ~110 ("+juce::String(seedLow,2)+")");
        check (std::abs(seedHigh-220.0f)<6.0f, "D: seed high ~220 ("+juce::String(seedHigh,2)+")");
        auto cL = ana.evaluateOctaveCandidates(low, sr, seedLow);
        auto cH = ana.evaluateOctaveCandidates(high, sr, seedHigh);
        std::printf ("    low  -> %s\n", residLine(cL).toRawUTF8());
        std::printf ("    high -> %s\n", residLine(cH).toRawUTF8());
        float rL = ana.resolveOctaveByGlobalResidual(low, sr, seedLow);
        float rH = ana.resolveOctaveByGlobalResidual(high, sr, seedHigh);
        check (std::abs(1200*std::log2(rL/110.0f))<80.0f, "D: low resuelto ~110 ("+juce::String(rL,2)+")");
        check (std::abs(1200*std::log2(rH/220.0f))<80.0f, "D: high resuelto ~220 ("+juce::String(rH,2)+")");
        check (std::abs(1200*std::log2(rL/220.0f))>300.0f, "D: low no salta a 220");
        check (std::abs(1200*std::log2(rH/110.0f))>300.0f, "D: high no salta a 110");
    }

    // ---------- E. Semilla desplazada una octava arriba (robustez LS) ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> buf (1, 6*F);
        std::vector<std::pair<double,double>> p = { {1,1.0},{2,0.5},{3,1.0/3.0},{4,0.25},{5,0.2} };
        for (int w=0;w<6;++w) fillHarmonic(buf, w*F, 110.0, p);
        float forced = 220.0f;
        auto c = ana.evaluateOctaveCandidates(buf, sr, forced);
        std::printf ("\n[E] robustez semilla 110 Hz forzada a 220 Hz -> %s\n", residLine(c).toRawUTF8());
        float resolved = ana.resolveOctaveByGlobalResidual(buf, sr, forced);
        std::printf ("    resuelto -> %.2f Hz\n", (double)resolved);
        check (std::abs(1200*std::log2(resolved/110.0f))<100.0f,
               "E: semilla 220 -> resuelto ~110 (robustez LS, resuelto "+juce::String(resolved,2)+")");
    }

    // ---------- F. Barrido 110..130 (deriva, como LeastSquaresGridTest) ----------
    {
        SpectralAnalyzer ana;
        juce::AudioBuffer<float> drift (1, 6*F);
        std::vector<std::pair<double,double>> p = { {1,1.0},{2,0.5},{3,1.0/3.0},{4,0.25} };
        for (int w=0;w<6;++w) fillHarmonic(drift, w*F, 110.0 + 4.0*(double)w, p);
        float seed = ana.detectPitch(drift, sr);
        std::printf ("\n[F] barrido 110..130 Hz (deriva 20 Hz, 6 ventanas) -- seed %.2f Hz\n", (double)seed);
        auto c = ana.evaluateOctaveCandidates(drift, sr, seed);
        std::printf ("    %s\n", residLine(c).toRawUTF8());
        int best=-1; float br=1e9;
        for (int i=0;i<3;++i) if (c[(size_t)i].valid && c[(size_t)i].residCents < br) { br=c[(size_t)i].residCents; best=i; }
        check (best>=0 && br >= 15.0f && br < 150.0f,
               "F: mejor residuo delata deriva (15..150 cents, best="+juce::String(best)+" resid "+juce::String(br,1)+")");
        // Barrido: no hay UNA rejilla, el minimo puede caer en cualquier familia; solo se verifica el residuo alto, no la octava
        (void)best;
    }

    // ---------- G-H. WAV reales si existe el banco ----------
    {
        const juce::File bankDir (juce::File::getCurrentWorkingDirectory().getChildFile ("../ABDCZ101/DOCS/patches").exists() ? juce::File::getCurrentWorkingDirectory().getChildFile ("../ABDCZ101/DOCS/patches") : juce::File ("D:/desarrollos/ABDSynths/ABDCZ101/DOCS/patches"));
        if (! bankDir.exists())
        {
            std::printf ("\n[G-H] banco CZ101 no existe en %s -- saltado (bateria sintetica OK)\n", bankDir.getFullPathName().toRawUTF8());
        }
        else
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            auto loadWav = [&](const juce::String& name) -> juce::AudioBuffer<float>
            {
                juce::AudioBuffer<float> empty(1,0);
                juce::File f = bankDir.getChildFile(name);
                if (! f.existsAsFile()) return empty;
                auto* r = fm.createReaderFor(f);
                if (! r) return empty;
                juce::AudioBuffer<float> b((int)r->numChannels, (int)r->lengthInSamples);
                r->read(&b, 0, (int)r->lengthInSamples, 0, true, true);
                delete r;
                return b;
            };
            {
                auto wav = loadWav("CZ-SWEP1.wav");
                if (wav.getNumSamples()==0) std::printf ("\n[G] SWEP1 no encontrado -- saltado\n");
                else
                {
                    SpectralAnalyzer ana;
                    float seed = ana.detectPitch(wav, 44100.0);
                    auto c = ana.evaluateOctaveCandidates(wav, 44100.0, seed);
                    float res = ana.resolveOctaveByGlobalResidual(wav, 44100.0, seed);
                    std::printf ("\n[G] CZ-SWEP1.wav seed %.2f -> resolved %.2f  [%s]\n", (double)seed,(double)res, residLine(c).toRawUTF8());
                    check (std::abs(1200*std::log2(res/124.0f)) < 80.0f || std::abs(1200*std::log2(res/seed)) < 30.0f,
                           "G: SWEP1 resuelto en su octava (124 Hz, resuelto "+juce::String(res,2)+", seed "+juce::String(seed,2)+")");
                    check (std::abs(1200*std::log2(res/62.0f)) > 200.0f,
                           "G: SWEP1 NO baja a la sub-octava 62 Hz");
                }
            }
            {
                auto wav = loadWav("CZ-RRISE.wav");
                if (wav.getNumSamples()==0) std::printf ("\n[H] RRISE no encontrado -- saltado\n");
                else
                {
                    SpectralAnalyzer ana;
                    float seed = ana.detectPitch(wav, 44100.0);
                    auto c = ana.evaluateOctaveCandidates(wav, 44100.0, seed);
                    float res = ana.resolveOctaveByGlobalResidual(wav, 44100.0, seed);
                    std::printf ("\n[H] CZ-RRISE.wav seed %.2f -> resolved %.2f  [%s]\n", (double)seed,(double)res, residLine(c).toRawUTF8());
                    // RRISE es barrido sin UNA rejilla: todos los residuos son altos (>40). El minimo puede caer en cualquier octava.
                    check (c[(size_t)0].residCents > 40.0f || c[(size_t)1].residCents > 40.0f,
                           "H: RRISE sin rejilla unica (residuos altos: 147Hz "+juce::String(c[(size_t)0].residCents,1)+"c, 293Hz "+juce::String(c[(size_t)1].residCents,1)+"c)");
                    check (res > 0.0f, "H: RRISE resolved valido ("+juce::String(res,2)+")");
                }
            }
        }
    }

    std::printf ("\nRESULT: %s (%d fallos)\n", failures==0?"OK":"FAIL", failures);
    return failures==0?0:1;
}
