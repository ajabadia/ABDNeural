/*
  ==============================================================================

    SpectralAnalyzerTest.cpp
    Created: 22 Sep 2026
    Description: El analizador del ModelMaker (multiframe + peak-picking +
                 offsets sub-bin) verificado como numero:

                   1. generateHarmonicTone (como el RoundTrip) -> detectPitch
                      -> analyze: tabla normalizada, fundamental dominante.
                   2. Un seno puro: el parcial medio debe caer sobre su bin
                      (offset ~ 0: el floor no inventa inharmonicidad).
                   3. Inharmonicidad REAL (offsets fijos en la fuente): el
                      analizador la reconstruye en frequencyOffsets — el
                      canal que antes era un TODO a cero.
                   4. Multiframe: un ataque fuerte al inicio NO domina ya el
                      modelo (las ventanas se reparten por todo el fichero).
                 5. La ESCALERA en los dos sentidos: la fundamental debil
                      con sub-octava fuerte (CZ-SWEP1) NO cae (el HPS
                      refinado pilla la nota del patch, 124 Hz) y su GEMELO
                      de fundamental ausente con serie continua SI cae una
                      octava (la regla x1/2). La bajada depende de la
                      cuantizacion del bin — ver el caso 4b.
                 6. El SUELO del estimador (50 Hz cuantizados al bin = 48.45 Hz:
                      E1 = 41.62 Hz queda fuera) y la via MANUAL del ModelMaker:
                      el estimador lee el 2o armonico (medicion negativa
                      pinneada) y con la f0 a mano el modelo y los frames
                      salen sobre la raiz real.
                 7. El modo REJILLA FIJA (declarado): con material que barre, el
                      seguimiento por ventana se apaga (la rejilla declarada en
                      todos los frames) y el modelo lo declara en gridFixed.
                 8. El AVISO de pitch del ModelMaker cita DOS cifras: la
                      desviacion (guardia) y el residuo de rejilla, con UNA
                      frase para el residuo (gridResidualNotice) que comparten
                      las tres superficies —la fila del aviso, el dialogo que
                      bloquea la exportacion y la sonda RealWav—. Aqui se
                      fija que las dos estan disponibles en el MISMO estado:
                      analyze() dispara la guardia SIN invalidar el residuo que
                      publico detectPitch sobre el mismo buffer (si lo
                      invalidara, el aviso diria "residuo n/d" con material
                      delante) y el residuo de un barrido cae en la banda alta
                      (el material no es UNA rejilla).

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <iostream>

namespace
{
    using namespace NEURONiK;

    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }
        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    /** Tono armonico 1/n como el del RoundTrip, con offsets e inharmonicidad
        opcionales y envolvente opcional (ataque fuerte al principio). */
    juce::AudioBuffer<float> generateTone (double sampleRate, float f0, int numSamples,
                                           float offsetPerPartial = 0.0f,
                                           bool strongAttack = false)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double sample = 0.0;

            for (int n = 1; n <= 8; ++n) // 8 parciales: los que el test examina
            {
                const auto decay = 1.0 / (double) n;
                // Parcial n en n*(f0+offset) Hz: offset real por parcial.
                const auto freq = 2.0 * juce::MathConstants<double>::pi * (double) n
                                    * ((double) f0 + (double) offsetPerPartial);
                sample += decay * std::sin (freq * t);
            }

            float value = (float) (0.6 * sample / 2.0);
            if (strongAttack)
            {
                // Los primeros 20 ms: transitorio 10x (un "golpe" de la fuente).
                if (i < (int) (0.02 * sampleRate))
                    value *= 10.0f;
            }
            left[i] = value;
            right[i] = value;
        }

        return buffer;
    }
}

int main()
{
    constexpr double sampleRate = 44100.0;
    constexpr float f0 = 440.0f;
    constexpr int numSamples = (int) sampleRate; // 1 s: varias ventanas

    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;

    std::cout << "SpectralAnalyzer: multiframe + peak-picking + offsets sub-bin\n";

    // --- 1. Tono armonico: pitcheo + tabla normalizada -----------------------
    std::cout << "\nTono armonico 1/n\n";
    {
        const auto source = generateTone (sampleRate, f0, numSamples);
        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - f0) < 10.0f,
               "detectPitch encuentra la fundamental: " + juce::String (detected, 2) + " Hz");

        const auto model = analyzer.analyze (source, sampleRate, detected);
        check (model.amplitudes[0] > 0.5f && model.amplitudes[0] <= 1.0f + 1.0e-4f,
               "tabla normalizada con fundamental dominante: " + juce::String (model.amplitudes[0], 3));

        // Un seno limpio muestreado en bins de 5.38 Hz: el pico local puede
        // caer hasta medio bin del nominal -> offset < 2.7 Hz (floor 0 si el
        // parcial queda bajo el suelo del ruido de ventana).
        // La propiedad que importa: root_detectado*n + offset_k debe caer
        // sobre la frecuencia REAL de la fuente (440*n). El offset cancela
        // el error residual de detectPitch (aqui ~1.4 Hz -> offset ~ -1.4*n)
        // y de paso codifica la desviacion verdadera: es el canal que los
        // motores renderizan como baseFreq*n + offset.
        bool reconstruye = true;
        for (int k = 0; k < 8; ++k)
        {
            const float n = (float) (k + 1);
            const float rendered = detected * n + model.frequencyOffsets[(size_t) k];
            if (std::abs (rendered - 440.0f * n) > 3.0f)
                reconstruye = false;
        }
        check (reconstruye, "root*n + offset reconstruye la frecuencia real de la fuente (<= 3 Hz)");
    }

    // --- 2. Inharmonicidad real reconstruida ---------------------------------
    std::cout << "\nInharmonicidad\n";
    {
        constexpr float realOffset = 6.0f; // Hz por parcial (cuerda rigida)
        const auto source = generateTone (sampleRate, f0, numSamples, realOffset);
        const auto model = analyzer.analyze (source, sampleRate, f0);

        // La fuente canta en n*(440+6) Hz; el modelo debe reproducirla:
        // root_detectado*n + offset_k ~ n*446 para los 8 parciales.
        bool losOcho = true;
        for (int k = 0; k < 8; ++k)
        {
            const float n = (float) (k + 1);
            const float rendered = f0 * n + model.frequencyOffsets[(size_t) k];
            if (std::abs (rendered - (440.0f + realOffset) * n) > 3.5f)
                losOcho = false;
        }
        check (losOcho, "los 8 parciales de la fuente inarmonica (446*n Hz) se reconstruyen (<= 3.5 Hz)");
    }

    // --- 3. Multiframe: el ataque ya no define el modelo ---------------------
    std::cout << "\nMultiframe\n";
    {
        const auto withAttack = generateTone (sampleRate, f0, numSamples, 0.0f, true);
        const auto withoutAttack = generateTone (sampleRate, f0, numSamples, 0.0f, false);

        const auto mWith = analyzer.analyze (withAttack, sampleRate, f0);
        const auto mWithout = analyzer.analyze (withoutAttack, sampleRate, f0);

        // Antes (una ventana al inicio) el transitorio 10x deformaba la tabla
        // entera. Con 6 ventanas repartidas, la diferencia debe ser pequena.
        double maxDiff = 0.0;
        for (int k = 0; k < 8; ++k)
            maxDiff = std::max (maxDiff,
                                (double) std::abs (mWith.amplitudes[(size_t) k]
                                                     - mWithout.amplitudes[(size_t) k]));
        check (maxDiff < 0.25,
               "el ataque fuerte apenas mueve la tabla (maxDiff=" + juce::String ((float) maxDiff, 3) + " < 0.25)");
    }

    // --- 4. Fundamental debil con sub-octava fuerte (CZ-SWEP1) ---------------
    std::cout << "\nFundamental debil (sub-octava fuerte)\n";
    {
        // Firma REAL del CZ-SWEP1 (medida por la sonda, serie de la raiz
        // 62): sub-octava 62 (0.39), fundamental 124 (1.0, dominante),
        // 186 (0.56), 248 practicamente vacio y migajas de banda ancha
        // encima — la nota del patch vive en 124 con un sub-oscilador
        // fuerte. El HPS clasico cae al sub-armonico (el producto en 62
        // multiplica los tres parciales reales que comparten).
        constexpr float f0Real = 124.0f;
        struct Partial { float freq; float amp; };
        constexpr Partial partials[] = {
            {  62.0f, 0.39f }, { 124.0f, 1.00f }, { 186.0f, 0.56f },
            { 248.0f, 0.05f }, { 372.0f, 0.06f },
        };
        juce::AudioBuffer<float> source (2, numSamples);
        source.clear();
        double peak = 0.0;
        for (const auto& p : partials)
            peak = juce::jmax (peak, (double) p.amp);
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double s = 0.0;
            for (const auto& p : partials)
                s += (double) p.amp
                        * std::sin (2.0 * juce::MathConstants<double>::pi
                                        * (double) p.freq * t);
            const float v = (float) (0.6 * s / (1.9 * peak));
            source.setSample (0, i, v);
            source.setSample (1, i, v);
        }

        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - f0Real) < 5.0f,
               "fundamental debil con sub-octava fuerte detecta la nota ("
                   + juce::String (detected, 2) + " Hz ~ "
                   + juce::String (f0Real, 1) + ")");

        // Y el modelo se analiza contra la raiz correcta: la ventana de
        // busqueda de cada parcial (mitad del espaciado - 1 bin) deja fuera
        // tanto la sub-octava de 62 como el medio-entero de 186, asi que la
        // tabla queda dominada por la fundamental, con el hueco de 248 casi
        // vacio (guardia de maximo local: sin fantasmas de faldas ajenas) y
        // el 3er parcial debil pero presente — el reparto REAL de la fuente.
        const auto model = analyzer.analyze (source, sampleRate, detected);
        check (model.amplitudes[(size_t) 0] > 0.8f
                   && model.amplitudes[(size_t) 0] > model.amplitudes[(size_t) 1]
                   && model.amplitudes[(size_t) 1] < 0.2f
                   && model.amplitudes[(size_t) 2] > 0.03f,
               "el modelo reproduce el reparto (fundamental dominante, hueco de "
                   "248 casi vacio: 124=" + juce::String (model.amplitudes[(size_t) 0], 3)
                   + " 248=" + juce::String (model.amplitudes[(size_t) 1], 3)
                   + " 372=" + juce::String (model.amplitudes[(size_t) 2], 3) + ")");

        // Y la via POR VENTANA (detectPitchFromSpectrum, la que alimenta
        // analyzeTemporal) tambien cae de pie sobre la firma del patch:
        // los frames detectan ~124 Hz y el modelo temporal no arrastra la
        // sub-octava a su trayectoria frameF0. Es la ruta que producia
        // 62.3 Hz en la ventana de cabecera antes del refinado.
        const auto temporal = analyzer.analyzeTemporal (source, sampleRate, detected, 4);
        check (temporal.numFrames() == 4, "el modelo temporal tiene los 4 frames pedidos");
        check (std::abs (temporal.f0At (0) - f0Real) < 5.0f
                   && std::abs (temporal.f0At (3) - f0Real) < 5.0f,
               "la via por ventana detecta la nota en frames 0 y 3 ("
                   + juce::String (temporal.f0At (0), 2) + " / "
                   + juce::String (temporal.f0At (3), 2) + " Hz ~ 124)");
    }

    // --- 4b. EL GEMELO: aqui la escalera SI baja al sub-armonico -----------
    //
    // Misma pinta que el caso 4 —fundamental debil por debajo de un 2f0
    // dominante— pero respuesta CONTRARIA: la serie continua hacia arriba
    // (193,5 y 258 poblados), asi que el periodo real es el del sub-armonico
    // y el HPS refinado SI tiene que bajar la escalera x1/2. Lo que separa
    // los dos casos es justo eso: en el SWEP1 la serie MUERE tras el 3er
    // parcial (248 casi vacio) y la puerta x2 (m4 < 0.25*m3) no deja al 2f0
    // quedarse con la raiz.
    //
    // MEDIDO (2026-09-25): quien decide la bajada es la REJILLA del FFT. La
    // regla x1/2 solo puede degradar el ancla UNA octava, y solo dentro de
    // +-60 cents de ancla/2: a 44,1 kHz / 8192 (bin 5,38 Hz) eso es ANCHO DE
    // UN BIN a ~62 Hz, asi que el ancla tiene que caer en un bin PAR. Por eso
    // la raiz de este caso es 64,5 Hz (bin 12, con el ancla en el 24) y no
    // 62: con el ancla en el bin 23 —las 62,3 Hz de la sub-octava del SWEP1
    // real— las dos candidatas (59,2 y 64,6) quedan a +-75 cents y la bajada
    // es INALCANZABLE. La cuantizacion es asimetrica: la regla de SUBIR
    // (bandas de +-25 %) no la sufre, la de BAJAR (ventana) si.
    {
        constexpr float f0Real = 64.5f;
        struct Partial { float freq; float amp; };
        constexpr Partial partials[] = {
            {  64.5f, 0.10f }, { 129.0f, 1.00f }, { 193.5f, 0.30f },
            { 258.0f, 0.40f },
        };
        juce::AudioBuffer<float> source (2, numSamples);
        source.clear();
        double peak = 0.0;
        for (const auto& p : partials)
            peak = juce::jmax (peak, (double) p.amp);
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double s = 0.0;
            for (const auto& p : partials)
                s += (double) p.amp
                        * std::sin (2.0 * juce::MathConstants<double>::pi
                                        * (double) p.freq * t);
            const float v = (float) (0.6 * s / (1.9 * peak));
            source.setSample (0, i, v);
            source.setSample (1, i, v);
        }

        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - f0Real) < 3.0f,
               "fundamental ausente con serie continua SI baja al sub-armonico ("
                   + juce::String (detected, 2) + " Hz ~ "
                   + juce::String (f0Real, 1) + ")");
        check (std::abs (detected - 2.0f * f0Real) > 30.0f,
               "y NO se queda en el 2f0 dominante ("
                   + juce::String (2.0f * f0Real, 1) + " Hz), que es lo que "
                   "propondria un HPS sin la regla x1/2");

        // El modelo se lee contra la raiz correcta: el parcial 1 es la
        // fundamental AUSENTE (debil pero presente) y el 2 el dominante — el
        // reparto de la fuente, al reves que en el caso 4 (alli n1 dominaba).
        const auto model = analyzer.analyze (source, sampleRate, detected);
        check (model.amplitudes[(size_t) 0] > 0.02f
                   && model.amplitudes[(size_t) 0] < 0.30f
                   && model.amplitudes[(size_t) 1] > 0.7f,
               "el modelo reparte como la fuente (fundamental debil, 2o "
                   "dominante: n1=" + juce::String (model.amplitudes[(size_t) 0], 3)
                   + " n2=" + juce::String (model.amplitudes[(size_t) 1], 3) + ")");

        // Y la via POR VENTANA cae igual: la bajada no puede ser un artefacto
        // de la ventana unica del HPS, tiene que sobrevivir a analyzeTemporal.
        const auto temporal = analyzer.analyzeTemporal (source, sampleRate, detected, 4);
        check (temporal.numFrames() == 4
                   && std::abs (temporal.f0At (0) - f0Real) < 3.0f
                   && std::abs (temporal.f0At (3) - f0Real) < 3.0f,
               "la via por ventana tambien baja al sub-armonico ("
                   + juce::String (temporal.f0At (0), 2) + " / "
                   + juce::String (temporal.f0At (3), 2) + " Hz ~ "
                   + juce::String (f0Real, 1) + ")");
    }

    // --- 5. Guardia de desviacion de pitch --------------------------------
    std::cout << "\nGuardia de desviacion de pitch\n";
    {
        // (a) Cuasi-monotonico: seno con vibrato suave (+-6 cents) en 220 Hz.
        //     La guardia NO dispara: el modelo estatico es representable.
        juce::AudioBuffer<float> steady (2, numSamples);
        steady.clear();
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            const double f = 220.0 * std::pow (2.0, 0.006 * std::sin (
                2.0 * juce::MathConstants<double>::pi * 0.8 * t));
            const double s = 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * f * t)
                                 + 0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 2.0 * f * t);
            const float v = (float) s;
            steady.setSample (0, i, v);
            steady.setSample (1, i, v);
        }
        analyzer.measurePitchDeviation (steady, sampleRate, 220.0f);
        check (analyzer.lastPitchGuardCents() == 0.0f,
               "material cuasi-monotonico: guardia a 0 ("
                   + juce::String (analyzer.lastPitchGuardCents(), 1) + " cents)");

        // (b) Barrido real: chirp exponencial de una octava (220 -> 440 Hz).
        //     Una sola rejilla no lo describe: la guardia DISPARA y el
        //     llamador sabe que el estatico saldria des-afinado.
        juce::AudioBuffer<float> sweepBuf (2, numSamples);
        sweepBuf.clear();
        {
            double phase = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                const double t = (double) i / sampleRate;
                const double f = 220.0 * std::pow (2.0, t);
                phase += 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
                const float v = (float) (0.5 * std::sin (phase)
                                             + 0.12 * std::sin (2.0 * phase));
                sweepBuf.setSample (0, i, v);
                sweepBuf.setSample (1, i, v);
            }
        }
        analyzer.measurePitchDeviation (sweepBuf, sampleRate, 220.0f);
        check (analyzer.lastPitchGuardCents() > 150.0f,
               "barrido de una octava dispara la guardia ("
                   + juce::String (analyzer.lastPitchGuardCents(), 0) + " cents)");

        // (c) GUARDIA + RESIDUO, el par que el ModelMaker enseña junto: el
        //     aviso "Pitch inestable (N cents) ... | residuo M cents (K picos)"
        //     lee las dos cifras del MISMO estado. El residuo lo publica
        //     detectPitch (el del material cargado, que es el que el analisis
        //     va a describir) y analyze() NO lo invalida al medir la guardia:
        //     si lo invalidara, el aviso diria "residuo n/d" con material
        //     delante. Y en un barrido el residuo tiene que caer en la banda
        //     ALTA: es lo que separa "una rejilla que se mueve" de "esto no es
        //     una rejilla".
        const float sweepRoot  = analyzer.detectPitch (sweepBuf, sampleRate);
        const auto  sweepModel = analyzer.analyze (sweepBuf, sampleRate, sweepRoot);
        const float sweepResid = analyzer.lastGridResidualCents();
        const int   sweepObs   = analyzer.lastGridObservations();
        const float sweepGuard = analyzer.lastPitchGuardCents();
        std::cout << "    barrido: raiz " << juce::String (sweepRoot, 2)
                  << " Hz | guardia " << juce::String (sweepGuard, 0)
                  << " cents | residuo " << juce::String (sweepResid, 1)
                  << " cents (" << sweepObs << " picos) | frames "
                  << sweepModel.frameCount << "\n";
        // El analisis ESTATICO no toca model.isValid (solo lo declaran
        // analyzeTemporal y buildLayeredModel); lo que SI declara es un unico
        // frame, y la guardia es la que dice que ese frame unico no basta.
        check (sweepModel.frameCount == 1 && sweepGuard > 150.0f,
               "guardia y residuo juntos: el analisis estatico da 1 frame y la "
               "guardia dispara (" + juce::String (sweepGuard, 0) + " cents)");
        check (sweepResid >= 0.0f && sweepObs > 0,
               "guardia y residuo juntos: el residuo sigue publicado tras analyze "
               "(" + juce::String (sweepResid, 1) + " cents, "
                   + juce::String (sweepObs) + " picos) — el aviso lo puede citar");
        check (sweepResid > 40.0f,
               "guardia y residuo juntos: y cae en la banda alta, el barrido no es "
               "UNA rejilla (" + juce::String (sweepResid, 1) + " cents, umbral 40)");

        //     Y el residuo se CITA con una sola frase, la del analizador: es la
        //     que escriben la fila del aviso, el dialogo que bloquea la
        //     exportacion y la sonda RealWav. Aqui se fija su forma —cifra y
        //     picos— y que sin material diga "n/d" en las tres.
        check (analyzer.gridResidualNotice()
                   == "residuo " + juce::String (sweepResid, 1) + " cents ("
                          + juce::String (sweepObs) + " picos)",
               "la frase del residuo sale del analizador: \""
                   + analyzer.gridResidualNotice() + "\"");

        const NEURONiK::ModelMaker::Analysis::SpectralAnalyzer sinMaterial;
        check (sinMaterial.gridResidualNotice() == "residuo n/d (material insuficiente)",
               "sin material la frase compartida es la misma: \""
                   + sinMaterial.gridResidualNotice() + "\"");
    }


    // --- 6. El suelo del estimador y la rejilla a mano (E1 = 41.62 Hz) -------
    std::cout << "\nSuelo del estimador (E1) y la via manual\n";
    {
        // 41.62 Hz (E1, la cuerda mas grave del bajo de 4 cuerdas) esta por
        // debajo del ancla del HPS: 50 Hz cuantizados al bin. El estimador lee
        // su 2o armonico. NO es una limitacion que se arregle bajando el suelo
        // —medido 2026-09-25: bajar el ancla a 35 Hz es un no-op, 16/16 modelos
        // del banco byte a byte identicos— ni abriendo la puerta de sub-octava
        // —medido: arregla E1 y ROMPE CZ-SWEP1 (62.6 Hz, una octava abajo)—.
        // La via es la f0 MANUAL del ModelMaker; esta seccion la verifica.
        constexpr float e1 = 41.62f;
        struct Partial { float freq; float amp; };
        // Firma de bajo: fundamental debil (0.35) con el armonico 2 dominante y
        // serie corta. Es el caso que la sonda midio con 501 cents de error
        // acustico cuando la rejilla la puso el estimador.
        constexpr Partial partials[] = {
            {  41.62f, 0.35f }, {  83.24f, 1.00f }, { 124.86f, 0.55f },
            { 166.48f, 0.15f }, { 249.72f, 0.10f },
        };
        juce::AudioBuffer<float> source (2, numSamples);
        source.clear();
        double peak = 0.0;
        for (const auto& p : partials)
            peak = juce::jmax (peak, (double) p.amp);
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double s = 0.0;
            for (const auto& p : partials)
                s += (double) p.amp
                        * std::sin (2.0 * juce::MathConstants<double>::pi
                                        * (double) p.freq * t);
            const float v = (float) (0.6 * s / (1.6 * peak));
            source.setSample (0, i, v);
            source.setSample (1, i, v);
        }

        const float floorHz =
            NEURONiK::ModelMaker::Analysis::SpectralAnalyzer::anchorFloorHz (sampleRate);
        check (std::abs (floorHz - 48.45f) < 0.2f,
               "el suelo del estimador es 50 Hz cuantizados al bin ("
                   + juce::String (floorHz, 2) + " Hz a 44.1 kHz)");

        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - 2.0f * e1) < 4.0f,
               "el estimador lee E1 una octava arriba (" + juce::String (detected, 2)
                   + " Hz): limitacion MEDIDA, no un fallo del analisis");

        // La rejilla a mano: el modelo sale sobre la raiz real (la fundamental
        // debil incluida, que es la que el estimador no puede ver).
        const auto manual = analyzer.analyze (source, sampleRate, e1);
        check (manual.amplitudes[(size_t) 0] > 0.25f
                   && manual.amplitudes[(size_t) 1] > manual.amplitudes[(size_t) 0]
                   && manual.amplitudes[(size_t) 2] > 0.4f,
               "con la f0 a mano el modelo reproduce la firma (41.62="
                   + juce::String (manual.amplitudes[(size_t) 0], 3) + " 83.24="
                   + juce::String (manual.amplitudes[(size_t) 1], 3) + " 124.86="
                   + juce::String (manual.amplitudes[(size_t) 2], 3) + ")");

        // Y el analisis temporal NO se va una octava arriba: por debajo del
        // suelo la rejilla del usuario manda en TODOS los frames (la f0 por
        // ventana es para seguir el pitch, y aqui no hay nada que seguir: el
        // estimador no llega).
        const auto temporal = analyzer.analyzeTemporal (source, sampleRate, e1, 4);
        check (temporal.numFrames() == 4
                   && std::abs (temporal.f0At (0) - e1) < 0.5f
                   && std::abs (temporal.f0At (3) - e1) < 0.5f,
               "analyzeTemporal con la rejilla a mano la respeta en todos los frames ("
                   + juce::String (temporal.f0At (0), 2) + " / "
                   + juce::String (temporal.f0At (3), 2) + " Hz)");

        // La politica es una FRONTERA, no un modo: por encima del suelo la f0
        // por ventana vuelve a estimarse (el seguimiento del caso 4 sigue vivo).
        const auto aboveFloor = analyzer.analyzeTemporal (source, sampleRate, 62.0f, 4);
        check (std::abs (aboveFloor.f0At (3) - 62.0f) > 0.5f,
               "por encima del suelo la f0 por ventana vuelve a estimarse (62 -> "
                   + juce::String (aboveFloor.f0At (3), 2) + " Hz)");

        // Y el ajuste por minimos cuadrados con la semilla del usuario (la via
        // del ModelMaker para pulir y REPORTAR el f0 escrito a mano) conserva la
        // raiz en vez de saltar de octava.
        const auto refined = analyzer.refineGrid (source, sampleRate, e1);
        check (std::abs (refined - e1) < 1.0f,
               "refineGrid con la semilla del usuario afina en vez de saltar de octava ("
                   + juce::String (refined, 2) + " Hz)");
    }


    // --- 7. El modo REJILLA FIJA (declarado) --------------------------------
    std::cout << "\nModo rejilla fija\n";
    {
        // El modo del ModelMaker: el usuario fija la rejilla y el analisis NO
        // sigue el pitch por ventana. Con material que SI barre, las tres
        // promesas del modo se comprueban de una vez: el seguimiento se apaga,
        // el modelo lo declara (gridFixed) y la rejilla declarada es la que se
        // usa en TODOS los frames.
        constexpr float f0Start = 220.0f;
        juce::AudioBuffer<float> sweep (2, numSamples);
        sweep.clear();
        {
            double phase = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const double t = (double) i / (double) numSamples;
                const double f = (double) f0Start * std::pow (2.0, t);   // una octava en el fichero
                phase += 2.0 * juce::MathConstants<double>::pi * f / sampleRate;

                // Serie corta para que el ajuste por minimos cuadrados tenga
                // observaciones (>= 4) en cada ventana.
                const float v = (float) (0.5 * std::sin (phase)
                                             + 0.25 * std::sin (2.0 * phase)
                                             + 0.15 * std::sin (3.0 * phase)
                                             + 0.10 * std::sin (4.0 * phase)
                                             + 0.08 * std::sin (5.0 * phase));
                sweep.setSample (0, i, v);
                sweep.setSample (1, i, v);
            }
        }

        // (a) Sin el modo: la trayectoria por ventana sigue el barrido (es lo
        //     que existe para los barridos) y el modelo NO se declara fijo.
        const auto tracked = analyzer.analyzeTemporal (sweep, sampleRate, f0Start, 4);
        check (! tracked.gridFixed,
               "sin el modo, el modelo NO declara rejilla fija");
        check (tracked.f0At (3) > 1.2f * f0Start,
               "sin el modo la f0 por ventana sigue el barrido ("
                   + juce::String (tracked.f0At (0), 1) + " -> "
                   + juce::String (tracked.f0At (3), 1) + " Hz)");

        // (b) Con el modo: la rejilla declarada en TODOS los frames, y el
        //     modelo lo dice (es la promesa del modo, no un detalle interno).
        const auto fixed = analyzer.analyzeTemporal (sweep, sampleRate, f0Start, 4, true);
        check (fixed.gridFixed, "con el modo, el modelo declara rejilla fija");
        check (std::abs (fixed.f0At (0) - f0Start) < 1.0e-3f
                   && std::abs (fixed.f0At (1) - f0Start) < 1.0e-3f
                   && std::abs (fixed.f0At (2) - f0Start) < 1.0e-3f
                   && std::abs (fixed.f0At (3) - f0Start) < 1.0e-3f,
               "con el modo el barrido NO se sigue: la rejilla declarada en los 4 frames");
        check (fixed.numFrames() == 4 && std::abs (fixed.frameSpanHz - f0Start) < 1.0e-3f,
               "y el modelo sigue siendo de 4 frames sobre la rejilla declarada");

        // (c) El estatico tambien declara la procedencia (el fichero dice de
        //     donde salio su rejilla: del usuario, no del estimador).
        check (analyzer.analyze (sweep, sampleRate, f0Start, true).gridFixed
                   && ! analyzer.analyze (sweep, sampleRate, f0Start).gridFixed,
               "el analisis estatico declara la rejilla fija solo cuando se pide");
    }

    std::cout << "\nRESULT: " << ((failures == 0) ? "OK" : "FAIL") << " (" << failures << " fallos)\n";
    return (failures == 0) ? 0 : 1;
}
