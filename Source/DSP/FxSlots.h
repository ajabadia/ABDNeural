/*
  ==============================================================================

    FxSlots.h
    La cadena de efectos globales de NEURONiK, sobre el SISTEMA DE SLOTS del
    modulo compartido ABDSharedCode::DspEffects.

    QUE ES ESTE FICHERO. El motor de slots (`DspEffects/FxEngine.h`) y el
    catalogo (`DspEffects/FxDefaultCatalogue.h`) son del modulo compartido; lo
    que es de NEURONiK es TODO lo de este fichero: que efectos van en que hueco,
    en que orden, y como se traducen los doce mandos de `GlobalParams` a los
    mandos NORMALIZADOS de las filas del catalogo.

    POR QUE ESTA SEPARACION. El modulo compartido no puede saber que es
    NEURONiK: sus filas son unidades fisicas genericas (0.10..8 Hz de coro,
    0.01..2 s de retardo) porque las decide el producto, no el motor. Este
    fichero es el sitio donde se decide, y por eso es el unico que hay que
    tocar para cambiar el sonido de la cadena global.

    LOS TIPOS NO SE CAMBIAN EN VUELO. `updateFromGlobalParams` se llama desde el
    hilo de audio (los motores llaman a `updateParameters` al principio de su
    bloque), y `FxSlot::setType` CREA y DESTRUYE la instancia del efecto. Un
    `new` en el hilo de audio no es un detalle que se pueda meter aqui. Los
    cuatro tipos se fijan en `prepare` y no se vuelven a tocar; lo que cambia en
    caliente son los mandos, que si son baratos. Cuando el panel deje elegir el
    efecto de cada hueco, ese `setType` vivira en el hilo de mensajes, que es
    donde puede.

    LA MEZCLA DE CADA HUECO, que es la parte con mas criterio y la mas medida.
    El slot mezcla SIEMPRE igual: `seca·(1 − mix) + mojado·mix·ganancia`. Los
    cuatro efectos que venia aquí NO mezclaban asi:

      - la saturacion no mezclaba: `out = sat(in)`. Con `mix = 1` el slot hace
        exactamente eso, asi que va en INSERTO. Y con el mando a cero el slot
        esta en `mix = 0`, que da `in·1 + mojado·0 = in` BIT A BIT: el mismo
        bypass que hacia el envoltorio viejo con su puerta de `drive > 1.001`.
        Lo que se ha perdido es el ahorro de CPU, no el sonido.

      - el coro mapea 1:1, y no es casualidad. El motor mezcla
        `in·(1 − mix·0.5) + eco·mix·0.5` y el adaptador le pasa `mix = 1`, con
        lo que entrega `0.5·in + 0.5·eco`; el slot lo pesa por su `mix` y sale
        `in·(1 − mix) + mix·(0.5·in + 0.5·eco)` = `in·(1 − 0.5·mix) +
        eco·0.5·mix`, que es la MISMA formula con `mix` en vez de `mix` de
        motor. Medido: 2.1e-06 de diferencia en estado estable (el resto es el
        redondeo del viaje normalizado <-> fisico). El `chorusMix` de antes es,
        sin cambios, el `mix` del hueco.

      - el retardo ANTES era `out = in + 0.5·eco`, con la seca a unidad y el
        eco sumado: una mezcla FIJA, y el parametro `mix` que llevaba se
        ignoraba por completo. Eso NO es un punto de la recta que describe un
        slot (`seca = 1 − mix`), es un punto fuera de ella: con `mix = 0` la seca
        esta a 1 pero no hay eco, y con cualquier `mix > 0` la seca baja. Medido:
        con el hueco a `mix = 0.5` la diferencia es de 2.41e-01 de pico, o sea
        que no es un redondeo sino un cambio de mezclado. Aqui se elige el punto
        de la recta que se parece a lo que sonaba — mojado y seco al 50 % — y se
        deja escrito lo que cuesta: la seca del bus global baja 6 dB mientras
        el retardo este puesto. Es el mezclado en paralelo de toda la vida y es
        lo que hacen el resto de huecos de un sistema de slots.

      - la reverberacion era `mojado = mix·0.5`, `seca = 1 − mix·0.2`: DOS
        niveles con leyes DISTINTAS, que un solo mando de mezcla no puede
        escribir. El motor de FreeVerb tiene los dos dentro (por eso el
        adaptador declara su `levels` como parametro propio) y por eso aqui se
        reparte: el mando `fxReverbMix` mueve el nivel mojado de la fila y el
        `mix` del hueco se queda en la mitad. Con el mando a cero el hueco va a
        `mix = 0` y sale `in` sin tocar, que es lo que hacia el envoltorio
        viejo saltandose el bloque entero.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"
#include "FxCatalogue.h"
#include "DspTypes.h"

#include "DspEffects/FxDefaultCatalogue.h"
#include "DspEffects/FxEngine.h"

namespace NEURONiK::DSP {

//==============================================================================
/** La cadena global de NEURONiK: cuatro huecos con el motor de slots detras. */
class FxSlots
{
public:
    //--- Que efecto va en que hueco ----------------------------------------
    // Los indices son los del CATALOGO + 1, porque el 0 es bypass en
    // `fxEffectAt`. El orden es el de la cadena de antes: saturacion, coro,
    // retardo, reverberacion.
    static constexpr int kSlotSaturation = 0;
    static constexpr int kSlotChorus     = 1;
    static constexpr int kSlotDelay      = 2;
    static constexpr int kSlotReverb     = 3;

    /** El tipo de la fila del catalogo que va en el hueco `slot`. */
    static int typeForSlot (int slot) noexcept
    {
        // La cadena por defecto la decide el catalogo (`FxCatalogue.h`), que es
        // donde la lee tambien el APVTS para el default de `fx1Type`. Un solo
        // sitio: si el motor y el parametro tuvieran numeros distintos, el
        // preset nuevo sonaria distinto del queguarda el host.
        return fxDefaultTypeForSlot (slot);
    }

    //==============================================================================
    /** Prepara los cuatro huecos y monta la cadena por defecto. */
    void prepare (double sampleRate, int maxBlockSize) noexcept
    {
        const int numSlots = static_cast<int> (abd::dsp::kFxNumSlots);

        engine_.setCatalogue (abd::dsp::fxDefaultCatalogue(), abd::dsp::fxDefaultCatalogueSize());
        engine_.setRouting (abd::dsp::FxRouting::Series);
        engine_.setMode (abd::dsp::FxMode::Insert);
        engine_.prepare (sampleRate, 2, maxBlockSize);

        // Los tipos se fijan AQUI y no se vuelven a tocar (ver la nota de la
        // cabecera: `setType` crea y destruye, y esto corre en el hilo de
        // mensajes pero `updateFromGlobalParams` no). El `setType` de vuelta
        // despues del `prepare` del motor es lo que hace que el hueco conserve
        // el efecto: `prepare` recrea las instancias con el tipo que hubiera, y
        // en este caso es 0.
        for (int i = 0; i < numSlots; ++i)
            engine_.getSlot (i).setType (typeForSlot (i));
    }

    //==============================================================================
    /**
        Traduce los doce mandos de `GlobalParams` a los huecos.

        SE LLAMA DESDE EL HILO DE AUDIO, asi que aqui no hay ninguna creacion,
        ninguna destruccion y ningun `new`: solo `setParameter` y `setMix`, que
        son dos escrituras.
    */
    void updateFromGlobalParams (const GlobalParams& p) noexcept
    {
        //--- 0. Saturacion: el PRIMERO con BUS PROPIO (2026-09-29) ----------
        // A diferencia de los otros tres, este hueco ya no se despinta de los
        // doce mandos planos: lee `p.fx[kSlotSaturation]`, que son el bus del
        // hueco (mandos normalizados 0..1, ganancia y mezcla). Los mandos van
        // NORMALIZADOS, asi que aqui no hay unidades fisicas que escribir: el
        // viaje sesgo <-> fisico lo hizo el APVTS al publicar el parametro, y
        // `setParameter` habla normalizado.
        //
        // LO QUE CAMBIA DE SONIDO, y es a proposito (el resto no):
        //
        //   - Antes un solo mando hacia las dos cosas: `saturationAmt` era a la
        //     vez el `drive` (1 + 4*amt) y la mezcla (`mix = 1` en cuanto
        //     amt > 0, o sea INSERTO). Ahora hay dos mandos: `drive` y `mix`.
        //   - El bypass por `mix = 0` se conserva entero, y sigue devolviendo la
        //     seca BIT A BIT (`in*(1-0) + mojado*0`), que es lo que hacia la
        //     puerta `drive > 1.001` del envoltorio viejo.
        //   - Con el bus, cambiar el TIPO del hueco ya no esta prohibido por el
        //     hilo de audio: `setType` lo llama el procesador desde el hilo de
        //     mensajes, y aqui solo llegan mandos. Ver `setSlotType()`.
        updateSlotFromBus (kSlotSaturation, p.fx[kSlotSaturation]);

        //--- 1. Coro: el `mix` del hueco ES el `chorusMix` de antes ----------
        setPhysical (kSlotChorus, 0, p.chorusRate);
        setPhysical (kSlotChorus, 1, p.chorusDepth);
        engine_.getSlot (kSlotChorus).setMix (p.chorusMix);

        //--- 2. Retardo: paralelo al 50 %, sin mando de mezcla propio --------
        // El tiempo se recorta al tope del motor (2 s) dentro del adaptador, que
        // es el mismo tope que ponia `delay.prepare (sampleRate, sampleRate*2)`.
        setPhysical (kSlotDelay, 0, p.delayTime);
        setPhysical (kSlotDelay, 1, dsp::jlimit (0.0f, 0.95f, p.delayFB));
        engine_.getSlot (kSlotDelay).setMix (0.5f);

        //--- 3. Reverberacion: el `levels` de la fila es el mando de mezcla ---
        // Sin este mando la reverb no cabe en un solo `mix`, asi que la fila
        // declara su nivel mojado como parametro (`levels`) y el `mix` del hueco
        // se queda en la mitad. Con el mando a cero, `mix = 0`: la seca intacta.
        setPhysical (kSlotReverb, 0, p.reverbSize);
        setPhysical (kSlotReverb, 1, p.reverbDamping);
        setPhysical (kSlotReverb, 2, p.reverbWidth);
        setPhysical (kSlotReverb, 3, p.reverbMix);
        engine_.getSlot (kSlotReverb).setMix (p.reverbMix > 0.0f ? 0.5f : 0.0f);
    }

    //==============================================================================
    /**
        Pone un efecto en un hueco. ESTE es el `setType` que la cabecera de este
        fichero llevaba anos anunciando como "el dia que el panel deje elegir el
        efecto de cada hueco": crea y destruye la instancia, asi que solo puede
        llamarse desde el hilo de mensajes.

        Y HAY UNA SEGUNDA COSA QUE SOLUCIONA. Al cambiar de efecto, el motor crea
        una instancia nueva con los mandos en su valor por defecto, asi que los
        que tenia el efecto anterior se pierden. Por eso el hueco se reescribe
        entero justo despues, con los mandos que hay en el bus: el hueco queda
        en lo que el panel dice y no en lo que el motor se invento.

        @param slot   el hueco (0..kFxBusSlots-1)
        @param type   el indice del catalogo: 0 es bypass, 1..N el resto
        @param bus    los mandos que se dejan puestos tras el cambio
    */
    void setSlotType (int slot, int type, const FxSlotParams& bus) noexcept
    {
        if (slot < 0 || slot >= kFxBusSlots)
            return;

        engine_.getSlot (slot).setType (type);
        updateSlotFromBus (slot, bus);
    }

    /** Escribe un hueco desde su bus. Lo usan `setSlotType` y `updateFromGlobalParams`. */
    void updateSlotFromBus (int slot, const FxSlotParams& bus) noexcept
    {
        auto& fx = engine_.getSlot (slot);
        const auto* info = fx.getEffectInfo();

        if (info != nullptr && info->params != nullptr)
            for (int i = 0; i < info->numParams && i < kFxBusParams; ++i)
                fx.setParameter (i, bus.params[i]);

        fx.setGain (bus.gain);
        fx.setMix (bus.mix);
    }

    //==============================================================================
    /** Vacia el estado de audio de los cuatro huecos, sin tocar los mandos. */
    void reset() noexcept { engine_.reset(); }

    /** Procesa el bloque. `numSamples` es el tamaño real del bloque. */
    void process (dsp::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        engine_.process (buffer, numSamples);
    }

    //==============================================================================
    /** El motor, para el panel: tipos, mandos, ganancia y mezcla por hueco. */
    abd::dsp::FxEngine& engine() noexcept { return engine_; }
    const abd::dsp::FxEngine& engine() const noexcept { return engine_; }

private:
    /** Pone un mando de una fila pasandole su valor FISICO, que es como lo
        llevan los doce de `GlobalParams`. La fila decide el sesgo y el tope. */
    void setPhysical (int slot, int param, float physical) noexcept
    {
        abd::dsp::FxSlot& fx = engine_.getSlot (slot);
        const abd::dsp::FxEffectInfo* info = fx.getEffectInfo();
        if (info == nullptr || info->params == nullptr)
            return;   // hueco en bypass: el mando se guarda solo, sin motor

        if (param < 0 || param >= info->numParams)
            return;

        fx.setParameter (param, abd::dsp::fxNormalise (info->params[param], physical));
    }

    abd::dsp::FxEngine engine_;
};

} // namespace NEURONiK::DSP
