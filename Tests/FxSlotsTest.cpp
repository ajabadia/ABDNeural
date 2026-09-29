/*
  ==============================================================================

    FxSlotsTest.cpp
    LA MIGRACION DE LA CADENA GLOBAL DE NEURONiK AL SISTEMA DE HUECOS,medida.

    Que sustituye a este fichero el anterior (`DspEffectsParityTest.cpp`, que
    comparaba los envoltorios de producto contra una referencia congelada a 0
    ulps). Los cuatro envoltorios de `Source/DSP/Effects/` han desaparecido: los
    efectos ya no se envuelven uno a uno en el producto, se ponen en HUECOS del
    motor compartido `DspEffects/FxEngine.h`. Este test es el registro de lo que
    eso ha costado, y a la vez la bateria que queda en pie sobre la cadena
    nueva.

    LAS DOS CADENAS. `LegacyChain` es una copia CONGELADA de la cadena de antes
    —cuatro envoltorios con sus cuatro smoothers y sus cuatro leyes de
    mezclado, transcritas literalmente— y no se toca. `FxSlots` es la cadena
    nueva. Las dos se renderizan con la MISMA entrada determinista, el mismo
    troceado en bloques de 512 y el mismo guion de parametros, y se comparan
    ETAPA A ETAPA, porque cada etapa cambio por un motivo distinto y un solo
    numero de diferencia no distingue nada.

    QUE ES 0 ULPS Y QUE NO, Y POR QUE:

      1. SATURACION CON EL MANDO A CERO: 0 ULPS, en el bloque entero. El
         envoltorio viejo se saltaba la muestra cuando `drive <= 1.001`; el
         hueco nuevo se salta la mezcla con `mix = 0`, que devuelve la seca con
         `in·1 + mojado·0`, y eso son los mismos bits. Es la unica equivalencia
         EXACTA de las cuatro, y por eso se comprueba en el bloque entero y no
         "despues de que asiente".

      2. REVERBERACION CON EL MANDO A CERO: 0 ULPS, por el mismo motivo. El
         envoltorio viejo se saltaba el bloque entero; el hueco nuevo queda en
         `mix = 0`.

      3. CORO: 1e-5. El `chorusMix` de antes ES el `mix` del hueco —la cuenta
         sale, ver la nota de `FxSlots.h`— pero el viaje del mando por
         `fxNormalise`/`fxDenormalise` mete dos redondeos mas. Medido: 2.1e-06
         en estado estable, o sea que la banda esta puesta con una decada de
         margen y sigue mordiendo.

      4. SATURACION CON MANDO: 1e-5 por lo mismo (1.19e-07 medido).

      5. RETARDO Y REVERBERACION CON MANDO: NO hay banda, y no es una
         comodidad. Sus mezclados de antes (`in + eco·0.5`, y `mojado = mix·0.5`
         con `seca = 1 − mix·0.2`) NO SON PUNTOS DE LA RECTA que describe un
         hueco (`seca = 1 − mix`), y por eso no hay tolerancia que las acerque:
         medido, 2.4e-01 de pico en el retardo. Aqui lo que se comprueba es lo
         que si es exacto —la mezcla y la ganancia que el hueco)— y la
         diferencia se IMPRIME, que es donde queda registrado el cambio de
         sonido. Un test que pone una banda inventada alrededor de un cambio
         consentido no comprueba nada: solo documenta que no se ha mirado.

    LO QUE TAMPOCO CAMBIA Y SE COMPRUEBA IGUAL:

      - DETERMINISMO: dos objetos NUEVOS con el mismo guion dan el mismo
        resultado bit a bit. Los dos tienen que ser nuevos: comparar uno ya
        calentado contra otro recien construido mide que el primero tenga cola,
        no que el motor sea determinista.

      - INDEPENDENCIA DEL TROCEADO: el mismo bloque de audio pasado a 64 y a
        512 sale igual bit a bit. El motor de huecos trocea internamente, y los
        motores de este modulo son todos por muestra, asi que trocear no cambia
        ni un bit. Esto es lo que hacia falta a mano en los envoltorios.

      - COLA FINITA: saturacion + coro + retardo realimentado al 0.95 y una cola
        larga de silencio no producen NaN ni infinitos.

  ==============================================================================
*/

#include "DspCore.h"
#include "DspTypes.h"
#include "FxSlots.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int    kBlock      = 512;
constexpr int    kNumBlocks  = 96;                 // 1 s
constexpr int    kNumSamples = kBlock * kNumBlocks;
constexpr int    kSettle     = kBlock * 48;        // 0.5 s: cuando los cuatro
                                                   // smoothers de la cadena
                                                   // vieja ya han sentado

int gFailures = 0;
int gChecks = 0;

void check (bool ok, const char* what)
{
    ++gChecks;
    if (! ok)
    {
        std::printf ("[FALLO] %s\n", what);
        ++gFailures;
    }
}

void report (const char* what, float value)
{
    std::printf ("    %-46s %12.3e\n", what, value);
}

//==============================================================================
/** LCG: la entrada del test no depende de rand() ni del reloj. */
inline uint32_t nextLcg (uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return state;
}

/** El mismo contenido por los dos canales: una senal en la que el retardo, el
    coro y la reverberacion tienen con que trabajar (no es lo mismo que una
    entrada identica en L y R, que se queda sin imagen stereo). */
void makeInput (std::vector<std::vector<float>>& in)
{
    in.assign (2, std::vector<float> (kNumSamples));
    uint32_t state = 0x12345678u;

    for (int i = 0; i < kNumSamples; ++i)
    {
        const float t = (float) i / (float) kSampleRate;
        const float a = std::sin (2.0f * 3.14159265f * 110.0f * t);
        const float b = std::sin (2.0f * 3.14159265f * 220.0f * t) * 0.5f;
        const float c = std::sin (2.0f * 3.14159265f * 330.0f * t) * 0.25f;
        const float nz = ((float) (nextLcg (state) >> 8) / (float) (1 << 24) - 0.5f) * 0.05f;

        in[0][i] = (a + b + c) * 0.2f + nz;
        in[1][i] = (a + b + c) * 0.18f - nz;
    }
}
//==============================================================================
//  LA CADENA VIEJA, CONGELADA. Cuatro envoltorios de producto con sus cuatro
//  smoothers y sus cuatro leyes de mezclado, tal y como estaban en
//  Source/DSP/Effects/ antes de que los huecos compartidos los sustituyeran.
//  Se compara con ella; no se llama desde ningun sitio.
//==============================================================================
class LegacyChain
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        satDrive.reset (sampleRate, 0.02);

        delayTime.reset (sampleRate, 0.05);
        delayFb.reset (sampleRate, 0.02);
        delayLine.setSize (2, (int) (sampleRate * 2.0) + 1024);
        delayLine.clear();
        delaySize = delayLine.getNumSamples();
        delayWrite = 0;

        chorusRate.reset (sampleRate, 0.02);
        chorusDepth.reset (sampleRate, 0.02);
        chorusMix.reset (sampleRate, 0.02);
        chorusLine.setSize (2, (int) (sampleRate * 0.1));
        chorusLine.clear();
        chorusWrite = 0;
        chorusPhase = 0.0f;

        revSize.reset (sampleRate, 0.02);
        revDamp.reset (sampleRate, 0.02);
        revWidth.reset (sampleRate, 0.02);
        revMix.reset (sampleRate, 0.02);
        rev.setSampleRate (sampleRate);
    }

    void setParams (const NEURONiK::DSP::GlobalParams& p) noexcept
    {
        satDrive.setTargetValue (1.0f + p.saturationAmt * 4.0f);
        delayTime.setTargetValue (p.delayTime * (float) sr);
        delayFb.setTargetValue (dsp::jlimit (0.0f, 0.95f, p.delayFB));
        chorusRate.setTargetValue (p.chorusRate);
        chorusDepth.setTargetValue (p.chorusDepth);
        chorusMix.setTargetValue (p.chorusMix);
        revSize.setTargetValue (p.reverbSize);
        revDamp.setTargetValue (p.reverbDamping);
        revWidth.setTargetValue (p.reverbWidth);
        revMix.setTargetValue (p.reverbMix);
    }

    /** Una etapa. `stage` va de 0 (saturacion) a 3 (reverb). */
    void processStage (dsp::AudioBuffer<float>& b, int stage)
    {
        switch (stage)
        {
            case 0: stageSaturation (b); break;
            case 1: stageChorus (b);     break;
            case 2: stageDelay (b);      break;
            default: stageReverb (b);    break;
        }
    }

private:
    void stageSaturation (dsp::AudioBuffer<float>& b)
    {
        const int ch = b.getNumChannels(), n = b.getNumSamples();

        for (int s = 0; s < n; ++s)
        {
            const float drive = satDrive.getNextValue();
            if (drive > 1.001f)
                for (int c = 0; c < ch; ++c)
                    b.setSample (c, s, abd::dsp::Saturation::processSample (b.getSample (c, s), drive));
        }
    }

    void stageChorus (dsp::AudioBuffer<float>& b)
    {
        const int ch = b.getNumChannels(), n = b.getNumSamples();
        const int  size = chorusLine.getNumSamples();

        for (int s = 0; s < n; ++s)
        {
            const float rate = chorusRate.getNextValue();
            const float depth = chorusDepth.getNextValue();
            const float mix = chorusMix.getNextValue();
            const float inc = dsp::MathConstants<float>::twoPi * rate / (float) sr;
            const float mod = (abd::dsp::sin (chorusPhase) + 1.0f) * 0.5f;
            const float ds = (0.005f + mod * 0.025f * depth) * (float) sr;

            for (int c = 0; c < ch; ++c)
            {
                const float in = b.getReadPointer (c)[s];
                chorusLine.setSample (c % 2, chorusWrite, in);

                float readPos = (float) chorusWrite - ds;
                if (readPos < 0.0f) readPos += (float) size;

                const int i1 = (int) readPos;
                const int i2 = (i1 + 1) % size;
                const float f = readPos - (float) i1;
                const float eco = (1.0f - f) * chorusLine.getSample (c % 2, i1)
                                + f * chorusLine.getSample (c % 2, i2);

                b.getWritePointer (c)[s] = in * (1.0f - mix * 0.5f) + eco * mix * 0.5f;
            }

            chorusPhase += inc;
            if (chorusPhase >= dsp::MathConstants<float>::twoPi)
                chorusPhase -= dsp::MathConstants<float>::twoPi;
            if (++chorusWrite >= size) chorusWrite = 0;
        }
    }

    void stageDelay (dsp::AudioBuffer<float>& b)
    {
        dsp::ScopedNoDenormals noDenormals;
        const int ch = b.getNumChannels(), n = b.getNumSamples();

        for (int s = 0; s < n; ++s)
        {
            const float ds = delayTime.getNextValue();
            const float fb = delayFb.getNextValue();

            for (int c = 0; c < ch; ++c)
            {
                const float in = b.getReadPointer (c)[s];
                float r = (float) delayWrite - ds;
                while (r < 0.0f) r += (float) delaySize;
                if (r >= (float) delaySize) r = 0.0f;

                const int i1 = (int) r;
                const int i2 = (i1 + 1) % delaySize;
                const float f = r - (float) i1;
                const float tap = (1.0f - f) * delayLine.getSample (c, i1)
                                + f * delayLine.getSample (c, i2);

                delayLine.setSample (c, delayWrite, in + tap * fb);
                b.getWritePointer (c)[s] = in + tap * 0.5f;   // mezcla FIJA
            }

            if (++delayWrite >= delaySize) delayWrite = 0;
        }
    }

    void stageReverb (dsp::AudioBuffer<float>& b)
    {
        const int n = b.getNumSamples();
        const int ch = b.getNumChannels();

        if (revMix.getTargetValue() <= 0.002f && revMix.getCurrentValue() <= 0.002f)
        {
            revSize.skip (n); revDamp.skip (n); revWidth.skip (n); revMix.skip (n);
            return;
        }

        abd::dsp::Reverb::Parameters p;
        for (int i = 0; i < n; ++i)
        {
            p.roomSize = revSize.getNextValue();
            p.damping  = revDamp.getNextValue();
            p.width    = revWidth.getNextValue();
            p.wetLevel = revMix.getNextValue() * 0.5f;
            p.dryLevel = 1.0f - (revMix.getCurrentValue() * 0.2f);
            rev.setParameters (p);

            if (ch > 1) rev.processStereo (b.getWritePointer (0) + i, b.getWritePointer (1) + i, 1);
            else         rev.processMono (b.getWritePointer (0) + i, 1);
        }
    }

    double sr = 48000.0;
    dsp::LinearSmoothedValue<float> satDrive { 1.0f };

    dsp::LinearSmoothedValue<float> delayTime, delayFb;
    dsp::AudioBuffer<float> delayLine;
    int delayWrite = 0, delaySize = 0;

    dsp::LinearSmoothedValue<float> chorusRate { 1.0f }, chorusDepth { 0.2f }, chorusMix { 0.0f };
    dsp::AudioBuffer<float> chorusLine;
    int chorusWrite = 0;
    float chorusPhase = 0.0f;

    dsp::LinearSmoothedValue<float> revSize { 0.5f }, revDamp { 0.5f },
                               revWidth { 1.0f }, revMix { 0.0f };
    abd::dsp::Reverb rev;
};

//==============================================================================
/** Compara UNA etapa de las dos cadenas, y solo esa. */
struct StageResult { float maxDiff = 0.0f; float rmsOld = 0.0f; float rmsNew = 0.0f; int at = 0; };

/**
    Y POR QUE SE AISLA CADA ETAPA. La primera version de este test renderizaba
    las cuatro etapas en las dos cadenas y comparaba al final, y el resultado era
    un numero que no significaba nada: la reverb "diferia" en 1.5e-01 cuando las
    dos son la identidad, porque el retardo de delante habia cambiado y esa
    diferencia se colaba aqui. Un numero de diferencia entre dos cadenas solo
    dice algo si las dos han recibido EXACTAMENTE la misma entrada.

    Por eso antes de cada etapa se copia el estado de la cadena VIEJA a las dos,
    y la etapa se mide sola.
*/
StageResult compareStages (const NEURONiK::DSP::GlobalParams& p, int upToStage,
                           bool wholeBuffer = false)
{
    // La entrada, identica para las dos.
    std::vector<std::vector<float>> in;
    makeInput (in);

    dsp::AudioBuffer<float> oldBuf (2, kBlock);
    dsp::AudioBuffer<float> newBuf (2, kBlock);

    LegacyChain legacy;
    legacy.prepare (kSampleRate);

    NEURONiK::DSP::FxSlots fx;
    fx.prepare (kSampleRate, kBlock);

    StageResult result;
    float sumOld = 0.0f, sumNew = 0.0f;
    int   counted = 0;

    for (int blk = 0; blk < kNumBlocks; ++blk)
    {
        legacy.setParams (p);
        fx.updateFromGlobalParams (p);

        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < kBlock; ++s)
                oldBuf.setSample (c, s, in[c][blk * kBlock + s]);

        for (int stage = 0; stage <= upToStage; ++stage)
        {
            // Las dos entran en la etapa con lo mismo.
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < kBlock; ++s)
                    newBuf.setSample (c, s, oldBuf.getSample (c, s));

            legacy.processStage (oldBuf, stage);
            fx.engine().getSlot (stage).process (newBuf, kBlock);

            // Y el resultado de la etapa vieja es la entrada de la siguiente.
            if (stage < upToStage)
                for (int c = 0; c < 2; ++c)
                    for (int s = 0; s < kBlock; ++s)
                        oldBuf.setSample (c, s, newBuf.getSample (c, s));
        }

        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < kBlock; ++s)
            {
                const int i = blk * kBlock + s;
                if (! wholeBuffer && i < kSettle)
                    continue;   // la cadena vieja esta en rampa todavia

                const float o = oldBuf.getSample (c, s);
                const float n = newBuf.getSample (c, s);
                const float diff = std::fabs (o - n);

                if (diff > result.maxDiff) { result.maxDiff = diff; result.at = i; }
                sumOld += o * o;
                sumNew += n * n;
                ++counted;
            }
    }

    if (counted > 0)
    {
        result.rmsOld = std::sqrt (sumOld / (float) counted);
        result.rmsNew = std::sqrt (sumNew / (float) counted);
    }

    return result;
}

//==============================================================================
/**
    Pone el BUS del hueco 1 en el valor que el mando suelto de antes producia.

    POR QUE EXISTE ESTE HELPER Y POR QUE NO ES "EL VALOR QUE SALIA". Antes el
    mando `saturationAmt` hacia DOS cosas a la vez: el drive (`1 + 4*amt`) y la
    mezcla (`mix = 1` en cuanto `amt > 0`, o sea insercion). Desde 2026-09-29 el
    hueco tiene bus, y son dos mandos separados. Para que la comparacion contra
    la cadena CONGELADA siga siendo una comparacion —y no "el ancho de banda ha
    crecido porque ahora el mezclado es otro"— el bus se pone en el valor que
    reproduce el sonido viejo EXACTAMENTE: el drive normalizado que da un drive
    fisico de `1 + 4*amt` sobre la fila de la saturacion, y el mix a 1.

    Y ASI LA BANDA SE QUEDA EN 1e-5, que es la del redondeo del viaje
    normalizado <-> fisico. Un bus que NO pudiera reproducir el sonido viejo
    seria una perdida real, y esta es la comprobacion que lo dice.

    ESCRIBE LAS DOS COSAS A PROPOSITO, y no es redundancia. `p.saturationAmt` ya
    no lo lee nadie del motor (sigue en el struct solo por el ABI del layout, y
    por eso el puente no desplaza ningun indice), pero la cadena CONGELADA de
    este fichero —que es una copia del envoltorio viejo— si lo lee. Poner solo el
    bus dejaria a la referencia en silencio y a la cadena nueva saturando, y la
    comparacion mediria "una de las dos no hace nada" en vez de "las dos suenan
    igual". El campo es de la referencia, no del motor.
*/
void setSlot1ToLegacySaturation (NEURONiK::DSP::GlobalParams& p, float amount)
{
    const auto entry = NEURONiK::DSP::fxNeuronikEffectAt (NEURONiK::DSP::fxDefaultTypeForSlot (0));

    // El camino viejo, para la cadena congelada de este test.
    p.saturationAmt = amount;

    // Y el bus, para la cadena de huecos.
    p.fx[0].params[0] = amount <= 0.0f
                            ? 0.0f
                            : abd::dsp::fxNormalise (entry.effect->params[0], 1.0f + amount * 4.0f);
    p.fx[0].mix = amount > 0.0f ? 1.0f : 0.0f;
    p.fx[0].gain = 1.0f;
}

void testStageByStage (const NEURONiK::DSP::GlobalParams& base)
{
    struct Case { const char* name; int stage; float tolerance; bool wholeBuffer; };

    const Case cases[] = {
        { "saturacion, mando a CERO: 0 ulps",      0, 0.0f,  true  },
        { "saturacion, mando a 0.5: 1e-5",        0, 1e-5f, false },
        { "coro, mix 0.6: 1e-5",                  1, 1e-5f, false },
        { "reverb, mando a CERO: 0 ulps",         3, 0.0f,  true  }
    };

    for (const Case& c : cases)
    {
        NEURONiK::DSP::GlobalParams p = base;

        if (c.stage == 0)
            setSlot1ToLegacySaturation (p, (c.tolerance == 0.0f) ? 0.0f : 0.5f);
        if (c.stage == 1)
        { p.chorusMix = 0.6f; p.chorusRate = 1.0f; p.chorusDepth = 0.4f; }
        if (c.stage == 3)
            p.reverbMix = 0.0f;

        const StageResult r = compareStages (p, c.stage, c.wholeBuffer);

        std::printf ("  %s\n", c.name);
        if (! c.wholeBuffer)
            std::printf ("    RMS %.4f -> %.4f (despues de 0.5 s, cuando los smoothers ya han sentado)\n",
                         r.rmsOld, r.rmsNew);

        report ("diferencia maxima", r.maxDiff);
        check (r.maxDiff <= c.tolerance, c.name);
    }
}

//==============================================================================
/** El retardo y la reverb cambian de mezclado a proposito: se mide y se dice. */
void testIntendedChanges (const NEURONiK::DSP::GlobalParams& base)
{
    std::printf ("\n  Los dos que NO son el mismo mezclado (cambio consentido):\n");

    {
        NEURONiK::DSP::GlobalParams p = base;
        p.delayTime = 0.3f; p.delayFB = 0.4f;
        const StageResult r = compareStages (p, 2);
        std::printf ("  retardo: mojado y seco en paralelo al 50 %%\n");
        report ("diferencia maxima", r.maxDiff);
        report ("RMS vieja", r.rmsOld);
        report ("RMS nueva", r.rmsNew);
        check (r.maxDiff > 0.0f, "el retardo nuevo NO es el viejo (comprobacion de que el test no es vacio)");
    }

    {
        NEURONiK::DSP::GlobalParams p = base;
        p.reverbMix = 0.5f;
        const StageResult r = compareStages (p, 3);
        std::printf ("  reverb: el mando mueve el nivel mojado, el hueco mezcla al 50 %%\n");
        report ("diferencia maxima", r.maxDiff);
        report ("RMS vieja", r.rmsOld);
        report ("RMS nueva", r.rmsNew);
        check (r.maxDiff > 0.0f, "la reverb nueva NO es la vieja (comprobacion de que el test no es vacio)");
    }
}

//==============================================================================
/** Lo que la traduccion pone en los huecos: esto si es exacto. */
void testSlotPatch (const NEURONiK::DSP::GlobalParams& p)
{
    std::printf ("\n  El reparto de la cadena:\n");

    NEURONiK::DSP::FxSlots fx;
    fx.prepare (kSampleRate, kBlock);
    fx.updateFromGlobalParams (p);

    check (fx.engine().getRouting() == abd::dsp::FxRouting::Series, "el ruteo es serie");
    check (fx.engine().getMode() == abd::dsp::FxMode::Insert, "el modo es insercion");

    for (int i = 0; i < abd::dsp::kFxNumSlots; ++i)
    {
        const abd::dsp::FxEffectInfo* info = fx.engine().getSlot (i).getEffectInfo();
        check (info != nullptr, "cada hueco tiene un efecto");
        if (info == nullptr) continue;
        std::printf ("    hueco %d: %-12s  mix %.2f  ganancia %.2f\n",
                     i, info->name, fx.engine().getSlot (i).getMix(),
                     fx.engine().getSlot (i).getGain());
    }

    // El hueco 1 ya no sale de un mando suelto: su mezcla es la del BUS, que es
    // lo que se le pone en `setSlot1ToLegacySaturation`. Con el mando a cero la
    // mezcla es 0 y el hueco devuelve la seca bit a bit, que es el mismo bypass
    // de antes (la puerta `drive > 1.001` del envoltorio viejo).
    check (fx.engine().getSlot (0).getMix() == p.fx[0].mix,
           "la saturacion va en insercion, y a bypass con el bus a cero");
    check (fx.engine().getSlot (1).getMix() == p.chorusMix,
           "el chorusMix de antes es el mix del hueco, sin cambio");
    check (fx.engine().getSlot (2).getMix() == 0.5f, "el retardo va en paralelo al 50 %");
    check (fx.engine().getSlot (3).getMix() == (p.reverbMix > 0.0f ? 0.5f : 0.0f),
           "la reverb va en paralelo al 50 %, y a bypass con el mando a cero");
}

//==============================================================================
/** Dos objetos NUEVOS, mismo guion, mismos bits. */
void testDeterminism (const NEURONiK::DSP::GlobalParams& p)
{
    std::vector<float> a, b;

    for (int which = 0; which < 2; ++which)
    {
        NEURONiK::DSP::FxSlots fx;   // NUEVO en cada pasada
        fx.prepare (kSampleRate, kBlock);

        dsp::AudioBuffer<float> buf (2, kBlock);
        std::vector<float>& dst = (which == 0) ? a : b;

        for (int blk = 0; blk < kNumBlocks; ++blk)
        {
            fx.updateFromGlobalParams (p);
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < kBlock; ++s)
                    buf.setSample (c, s, 0.3f * std::sin (2.0f * 3.14159265f *
                                (110.0f + 40.0f * c) * ((float) (blk * kBlock + s) / (float) kSampleRate)));
            fx.process (buf, kBlock);
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < kBlock; ++s)
                    dst.push_back (buf.getSample (c, s));
        }
    }

    check (a.size() == b.size() && a.size() == (size_t) (kNumSamples * 2), "las dos pasadas rinden lo mismo");
    if (a.size() != b.size()) return;

    std::size_t diffs = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) ++diffs;

    check (diffs == 0, "dos motores nuevos dan el mismo audio bit a bit");
    if (diffs != 0) std::printf ("    %zu muestras de %zu difieren\n", diffs, a.size());
}

//==============================================================================
/** El mismo audio, troceado de otra manera: tiene que salir igual. */
void testBlockSizeIndependence (const NEURONiK::DSP::GlobalParams& p)
{
    std::vector<float> out[2];

    const int sizes[2] = { 64, 512 };
    for (int which = 0; which < 2; ++which)
    {
        NEURONiK::DSP::FxSlots fx;
        fx.prepare (kSampleRate, sizes[which]);

        dsp::AudioBuffer<float> buf (2, sizes[which]);
        out[which].assign ((std::size_t) kNumSamples * 2, 0.0f);

        for (int blk = 0; blk * sizes[which] < kNumSamples; ++blk)
        {
            const int n = sizes[which];
            fx.updateFromGlobalParams (p);
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < n; ++s)
                    buf.setSample (c, s, 0.3f * std::sin (2.0f * 3.14159265f *
                                (110.0f + 40.0f * c) * ((float) (blk * n + s) / (float) kSampleRate)));
            fx.process (buf, n);
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < n; ++s)
                    out[which][(std::size_t) c * kNumSamples + (blk * n + s)] = buf.getSample (c, s);
        }
    }

    check (out[0].size() == out[1].size(), "los dos troceados rinden el mismo numero de muestras");
    if (out[0].size() != out[1].size()) return;

    std::size_t diffs = 0;
    for (std::size_t i = 0; i < out[0].size(); ++i)
        if (out[0][i] != out[1][i]) ++diffs;

    check (diffs == 0, "el mismo bloque a 64 y a 512 sale bit a bit igual (el motor trocea)");
    if (diffs != 0) std::printf ("    %zu muestras de %zu difieren\n", diffs, out[0].size());
}

//==============================================================================
/** Cola larga de silencio con la realimentacion al tope: ni NaN ni infinito. */
void testFiniteTail (const NEURONiK::DSP::GlobalParams& base)
{
    NEURONiK::DSP::GlobalParams p = base;
    p.delayFB = 0.95f;
    p.chorusMix = 0.6f;
    p.reverbMix = 0.6f;
    setSlot1ToLegacySaturation (p, 0.5f);

    NEURONiK::DSP::FxSlots fx;
    fx.prepare (kSampleRate, 512);

    dsp::AudioBuffer<float> buf (2, 512);
    int bad = 0;

    for (int blk = 0; blk < kNumBlocks + 120; ++blk)   // + 0.25 s de silencio
    {
        fx.updateFromGlobalParams (p);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 512; ++s)
                buf.setSample (c, s, blk < kNumBlocks
                                         ? 0.3f * std::sin (2.0f * 3.14159265f * 220.0f *
                                                             ((float) (blk * 512 + s) / (float) kSampleRate))
                                         : 0.0f);
        fx.process (buf, 512);

        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 512; ++s)
            {
                const float v = buf.getSample (c, s);
                if (! (v == v) || v > 1.0e6f || v < -1.0e6f) ++bad;
            }
    }

    check (bad == 0, "la cola con retardo realimentado al 0.95 no produce NaN ni infinitos");
}

} // namespace

//==============================================================================
int main()
{
    std::printf ("NEURONiK — cadena global sobre el sistema de huecos\n\n");

    NEURONiK::DSP::GlobalParams base;   // los valores por defecto del plugin

    std::printf ("  1. El mapeo de los doce mandos a los huecos\n");
    testSlotPatch (base);

    std::printf ("\n  2. Las etapas que tienen que seguir sonando IGUALES\n");
    testStageByStage (base);

    testIntendedChanges (base);

    std::printf ("\n  3. Determinismo y troceado\n");
    testDeterminism (base);
    testBlockSizeIndependence (base);

    std::printf ("\n  4. Cola finita\n");
    testFiniteTail (base);

    if (gFailures != 0)
    {
        std::printf ("\n[FALLO] %d comprobaciones fallidas\n", gFailures);
        return 1;
    }

    std::printf ("\n[OK] DspEffects/FxSlots: %d comprobaciones\n", gChecks);
    return 0;
}
