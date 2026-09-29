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

    LOS CUATRO HUECOS, Y UN SOLO CAMINO. Antes de 2026-09-29 el hueco 1 se
    rellenaba por el BUS (`p.fx[0]`) y los otros tres por sus mandos planos, con
    una traduccion distinta para cada uno. Desde ese dia los cuatro leen
    `p.fx[slot]` y la traduccion es una sola: el bus va NORMALIZADO y lo
    entrega tal cual. Lo que se lee de abajo no es como rellena hoy el motor, es
    POR QUE los defaults del hueco son los que son (ver
    `FxCatalogue.h::fxDefaultSlotParam`), que es la unica parte de la cadena que
    no sale de la fila.

    LA MEZCLA, que es la parte con mas criterio y la mas medida. El slot mezcla
    SIEMPRE igual: `seca·(1 − mix) + mojado·mix·ganancia`. Los cuatro efectos que
    venia aquí NO mezclaban asi:

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
    // El tipo lo elige el USUARIO (es el parametro `fxNType` del APVTS) y se
    // queda apuntado aqui, no solo puesto en el hueco.
    //
    // POR QUE HACE FALTA APUNTARLO, y no es que el `prepare` lo pise. `prepare`
    // es lo que hace `prepareToPlay`, que el host llama cuando cambia la tasa de
    // muestreo, cuando abre una ventana o cuando carga un estado: sin este
    // `types_`, un `prepare` devolvia los cuatro huecos a la cadena por defecto
    // y el efecto que el usuario habia elegido se iba sin decir nada. El sintoma
    // es el peor de los possibles --suena bien hasta que tocas algo-- y por eso
    // el tipo se guarda en el unico sitio que sobrevive al `prepare`.
    FxSlots() noexcept
    {
        for (int i = 0; i < kFxBusSlots; ++i)
            types_[i] = typeForSlot (i);
    }

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
    /** Prepara los cuatro huecos y vuelve a poner el tipo que tuvieran. */
    void prepare (double sampleRate, int maxBlockSize) noexcept
    {
        const int numSlots = static_cast<int> (abd::dsp::kFxNumSlots);

        engine_.setCatalogue (abd::dsp::fxDefaultCatalogue(), abd::dsp::fxDefaultCatalogueSize());
        engine_.setRouting (abd::dsp::FxRouting::Series);
        engine_.setMode (abd::dsp::FxMode::Insert);
        engine_.prepare (sampleRate, 2, maxBlockSize);

        // El `setType` de vuelta, DESPUES del `prepare` del motor, es lo que hace
        // que el hueco conserve el efecto: `prepare` recrea las instancias con
        // el tipo que hubiera, y en un hueco recien nacido ese tipo es 0. Se
        // reaplica el de `types_`, que el usuario fija con `setSlotType` y que
        // desde 2026-09-29 no se pierde al cambiar la tasa.
        for (int i = 0; i < numSlots; ++i)
            engine_.getSlot (i).setType (types_[i]);
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
        //--- LOS CUATRO, POR EL MISMO CAMINO (2026-09-29) ---------------------
        // Antes el hueco 1 venia por el bus y los otros tres por sus mandos
        // planos, cada uno con su traduccion. Ahora los cuatro leen `p.fx[slot]`
        // y no hay traduccion: el bus va NORMALIZADO, que es como habla
        // `setParameter`, y el sesgo lo aplica la fila. El viaje sesgo <->
        // fisico ocurre en los dos sitios que son dueños de las unidades (el
        // APVTS al publicar el parametro, y la migracion de presets al abrir
        // uno viejo), y no aqui, que ya no tiene unidades fisicas que escribir.
        //
        // LO QUE HA CAMBIADO DE SONIDO, y es a proposito en los tres casos:
        //
        //   - La saturacion ya no es el unico hueco con dos mandos donde antes
        //     habia uno: el drive y la mezcla se separaron en 2026-09-29, y el
        //     bypass por `mix = 0` se conservo entero (devuelve la seca BIT A
        //     BIT, `in*(1-0) + mojado*0`, que es la puerta `drive > 1.001` del
        //     envoltorio viejo).
        //   - El retardo deja de estar clavado al 50 % y la reverberacion deja
        //     de decidir su mezcla con `reverbMix > 0`: los dos son el `mix` del
        //     hueco, y con el a cero los dos hacen el bypass bit a bit, que es
        //     justo lo que hacia el envoltorio viejo (el retardo no, que
        //     antes no tenia forma de apagarse; el cambio esta medido y
        //     aceptado en `FxSlotsTest`).
        //   - Cambiar el TIPO de un hueco ya no esta prohibido por el hilo de
        //     audio en ninguno de los cuatro: `setType` lo llama el procesador
        //     desde el hilo de mensajes, y aqui solo llegan mandos. Ver
        //     `setSlotType()`.
        for (int i = 0; i < kFxBusSlots; ++i)
            updateSlotFromBus (i, p.fx[i]);
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

        // Se guarda el indice TAL COMO LLEGO, incluso si no vale: un tipo
        // fuera de catalogo deja el hueco en bypass (es lo que hace `setType`),
        // y guardarlo tal cual deja que el `prepare` vuelva a dejarlo en bypass
        // en vez de resucitar el efecto que el hueco tenia antes del error.
        types_[slot] = type;
        updateSlotFromBus (slot, bus);
    }

    /** El tipo que tiene puesto un hueco: el indice del catalogo, 0 = bypass. */
    int getSlotType (int slot) const noexcept
    {
        return (slot >= 0 && slot < kFxBusSlots) ? types_[slot] : 0;
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
    // NO HAY `setPhysical` anymore, y no se ha movido a otro sitio: el paso de
    // unidades fisicas a normalizadas lo hacen los dos sitios que son duenos de
    // las unidades, que son el APVTS (al publicar el parametro) y
    // `PresetMigrationFx.cpp` (al abrir un preset viejo). Dejar una tercera
    // copia aqui seria un camino mas por el que un mando puede entrar, y este
    // es el fichero donde se nota: el motor no sabria cual de los dos mando.
    abd::dsp::FxEngine engine_;

    int types_[kFxBusSlots] {};   ///< el tipo pedido, para que `prepare` no lo pierda
};

} // namespace NEURONiK::DSP
