/*
  ==============================================================================

    PresetMigrationFx.cpp
    Created: 29 Sep 2026
    Description: Los doce ids planos de efectos -> el bus del hueco que les toca.

  ==============================================================================
*/

#include "PresetMigrationFx.h"

#include "PresetMigration.h"

#include "DSP/CoreModules/RhythmicDivision.h"
#include "DSP/FxCatalogue.h"
#include "State/ParameterDefinitions.h"

namespace NEURONiK::Serialization
{
namespace
{
    const juce::Identifier paramType { "PARAM" };
    const juce::Identifier idProperty { "id" };
    const juce::Identifier valueProperty { "value" };

    //--- Los doce ids, CONGELADOS ------------------------------------------
    // La foto de lo que habia antes del rack. Vive en el codigo y no se deduce
    // del layout porque para seis de ellos ya NO HAY parametro que leer: son
    // precisamente los que esta migracion viene a sustituir. Un id que se
    // olvidara aqui no daria error, daria un preset que pierde ese mando en
    // silencio al abrirlo, asi que la lista se mira en un test.
    //
    // Los ids con sufijo `_hueco` son los que el hueco publica. Se arman solos
    // cuando el layout los declara: una fila cuyo destino no existe NO se
    // aplica, y el id viejo se queda donde estaba (que es lo correcto mientras
    // ese hueco siga manejandose con mandos planos).
    constexpr const char* kRetiredSaturation  = "fxSaturation";

    constexpr const char* kChorusRate        = "fxChorusRate";
    constexpr const char* kChorusDepth       = "fxChorusDepth";
    constexpr const char* kChorusMix         = "fxChorusMix";

    constexpr const char* kDelayTime         = "fxDelayTime";
    constexpr const char* kDelayFeedback     = "fxDelayFeedback";
    constexpr const char* kDelaySync         = "fxDelaySync";
    constexpr const char* kDelayDivision     = "fxDelayDivision";

    constexpr const char* kReverbSize        = "fxReverbSize";
    constexpr const char* kReverbDamping     = "fxReverbDamping";
    constexpr const char* kReverbWidth       = "fxReverbWidth";
    constexpr const char* kReverbMix         = "fxReverbMix";

    //--- El rango APVTS VIEJO de cada id plano -------------------------------
    // POR QUE HAY QUE CONGELARLOS Y NO LEERLOS: `fxSaturation` ya no esta en el
    // layout, asi que no hay de donde leer su rango. Y para los que SI quedan
    // (coro, retardo, reverb) leer el rango VIGENTE seria una trampa: es el
    // rango de HOY, no el con el que se guardo el preset, y si alguien lo
    // cambia un dia el viaje de ida y vuelta dejaria de ser el inverso y cada
    // preset migrado moveria su mando de sitio sin decir nada.
    //
    // El sesgo va con el rango porque `NormalisableRange` lo lleva dentro: un
    // 0.55 aqui es el mismo 0.55 que declaraba el APVTS viejo.
    struct LegacyRange
    {
        float minValue;
        float maxValue;
        float skew;
    };

    constexpr LegacyRange kRangeUnit        {  0.00f,  1.00f, 1.00f };
    constexpr LegacyRange kRangeSaturation  {  0.00f,  1.00f, 1.00f };
    constexpr LegacyRange kRangeChorusRate  {  0.10f,  8.00f, 0.50f };
    constexpr LegacyRange kRangeDelayTime   {  0.01f,  2.00f, 0.50f };
    constexpr LegacyRange kRangeDelayFb     {  0.00f,  0.95f, 1.00f };

    /** El `NormalisableRange` viejo, para desnormalizar lo que guardo el preset. */
    juce::NormalisableRange<float> legacyRange (const LegacyRange& r)
    {
        return { r.minValue, r.maxValue };
    }

    //--- Los ids del bus, de una sola fuente ---------------------------------
    // De `ParameterDefinitions.h`, que es donde el layout los compone y donde
    // un `static_assert` ata la composicion a los literales. Aqui no hay ninguna
    // cadena escrita a mano: si el layout renombrara `fx1Param1` y esta
    // migracion no lo supiera, escribiria un id que el APVTS no declara y el
    // preset migrado perderia ese mando EN SILENCIO.
    juce::String busParamId (int slot, int param) { return State::fxBusParamId (slot, param); }
    juce::String busMixId (int slot)   { return State::fxBusFieldId (slot, "Mix"); }
    juce::String busGainId (int slot)  { return State::fxBusFieldId (slot, "Gain"); }
    juce::String busTypeId (int slot)  { return State::fxBusFieldId (slot, "Type"); }

    //--- Leer y escribir en el arbol ----------------------------------------
    double readParam (const juce::ValueTree& state, const juce::String& id, double fallback)
    {
        const auto child = state.getChildWithProperty (idProperty, id);
        return child.isValid() ? (double) child.getProperty (valueProperty) : fallback;
    }

    bool hasParam (const juce::ValueTree& state, const juce::String& id)
    {
        return state.getChildWithProperty (idProperty, id).isValid();
    }

    /** Escribe (o SOBRESCRIBE) un PARAM. Devuelve si ha escrito algo nuevo. */
    bool writeParam (juce::ValueTree& state, const juce::String& id, double value)
    {
        auto child = state.getChildWithProperty (idProperty, id);

        if (child.isValid())
        {
            child.setProperty (valueProperty, value, nullptr);
            return false;   // ya estaba: migrar dos veces no cuenta como escritura
        }

        juce::ValueTree fresh (paramType);
        fresh.setProperty (idProperty, id, nullptr);
        fresh.setProperty (valueProperty, value, nullptr);
        state.appendChild (fresh, nullptr);
        return true;
    }

    //==============================================================================
    /**
        La fila del catalogo que va en un hueco, para leer sus UNIDADES FISICAS.

        No se escribe el numero a mano: `fxDefaultTypeForSlot` es la MISMA tabla
        que usa el motor al preparar y que el APVTS usa para el default de
        `fx1Type`. Si esta fila escribe un numero y el motor da otro, el
        preset migrado llevaria el mando de un efecto al valor por defecto de
        otro, y sonaria distinto sin que nada lo dijera.
    */
    const abd::dsp::FxEffectInfo* rowOf (int slot) noexcept
    {
        const auto entry = NEURONiK::DSP::fxNeuronikEffectAt (NEURONiK::DSP::fxDefaultTypeForSlot (slot));
        return entry.effect;
    }

    /** El valor FISICO de un mando de la fila, a partir de un normalizado 0..1. */
    float physicalOf (int slot, int param, float normalised) noexcept
    {
        const auto* row = rowOf (slot);

        if (row == nullptr || row->params == nullptr || param < 0 || param >= row->numParams)
            return 0.0f;

        return abd::dsp::fxDenormalise (row->params[param], normalised);
    }

    /** El normalizado 0..1 de un mando de la fila, a partir de un valor fisico. */
    float normalisedOf (int slot, int param, float physical) noexcept
    {
        const auto* row = rowOf (slot);

        if (row == nullptr || row->params == nullptr || param < 0 || param >= row->numParams)
            return 0.5f;   // mando que esta fila no tiene: el centro, no un extremo

        return abd::dsp::fxNormalise (row->params[param], physical);
    }

    /** Un mando plano viejo travels: normalizado viejo -> fisico -> normalizado nuevo. */
    float remapKnob (int slot, int param, const LegacyRange& oldRange, double stored) noexcept
    {
        const auto range = legacyRange (oldRange);
        const float physical = range.convertFrom0to1 ((float) stored);
        return normalisedOf (slot, param, physical);
    }

} // namespace

juce::StringArray retiredFlatFxParameterIds()
{
    return { kRetiredSaturation,
             kChorusRate,  kChorusDepth,   kChorusMix,
             kDelayTime,   kDelayFeedback, kDelaySync, kDelayDivision,
             kReverbSize,  kReverbDamping, kReverbWidth, kReverbMix };
}

int migrateFlatFxToSlotBus (juce::ValueTree& state, const juce::AudioProcessor& processor)
{
    if (! state.isValid())
        return 0;

    const auto known = currentParameterIds (processor);
    int written = 0;

    // Un preset NUEVO ya trae los ids del bus, y uno viejo no trae ninguno. La
    // pregunta "¿esta ya migrado?" se hace por el TIPO del hueco, que es el
    // primero que se escribe y el unico que no se puede haber escrito solo: si
    // `fx1Type` esta, el preset paso por aqui. Sin esta comprobacion, abrir un
    // preset guardado con el bus y volver a migrarlo le pisaria el `fx1Type` con
    // el efecto por defecto, y el usuario perderia el que habia elegido.
    const auto alreadyOnBus = [&] (int slot) { return hasParam (state, busTypeId (slot)); };

    // El gate: una fila solo se aplica si su hueco esta declarado. Con el
    // layout de hoy solo el hueco 1 pasa, y las otras once filas no hacen nada
    // (y sus ids viejos NO se tocan, que es lo correcto: mientras el hueco 2
    // los maneje con mandos planos, siguen siendo los mandos de ese hueco).
    const auto slotMigrated = [&] (int slot)
    {
        return known.contains (busTypeId (slot));
    };

    //==============================================================================
    // HUECO 1: la saturacion. El unico migrado hoy.
    if (slotMigrated (0) && ! alreadyOnBus (0))
    {
        const auto amount = (float) readParam (state, kRetiredSaturation, 0.0);

        // EL MAPEO VIEJO, CONGELADO. `1 + 4 * amount` era la cuenta del
        // envoltorio `Saturation::setAmount` (Source/DSP/Effects/Saturation.h,
        // que ya no existe). Va escrita aqui porque es una ley HISTORICA del
        // preset, no una del motor: si alguien cambia el drive de la fila del
        // catalogo, esta migracion NO debe cambiar con el, o un preset migrado
        // sonaria distinto al abrirlo que al guardarlo.
        const float drive = 1.0f + amount * 4.0f;

        // El mando 1 del bus es el drive, y su valor se escribe NORMALIZADO
        // contra la fila: es el mismo viaje que hace `FxSlots::setPhysical`.
        written += writeParam (state, busParamId (0, 0), normalisedOf (0, 0, drive)) ? 1 : 0;

        // Y la mezcla, que antes NO era un mando: el envoltorio viejo era
        // INSERTO y se ponia solo en cuanto `amount > 0`. Se traduce a la
        // misma frase del bus, que es lo que hace el bypass por `mix = 0`
        // devolver la seca bit a bit.
        written += writeParam (state, busMixId (0), amount > 0.0f ? 1.0 : 0.0) ? 1 : 0;

        // La ganancia: el bus la tiene y el preset viejo no. Se deja el valor
        // por defecto (1.0 = a unidad) poniendolo explicitamente, para que el
        // preset no dependa de lo que el APVTS tenga puesto en ese momento. Un
        // preset migrado tiene que abrir IGUAL en un host donde el usuario
        // hubiera movido el mando antes de cargar.
        written += writeParam (state, busGainId (0), 1.0) ? 1 : 0;

        // El tipo, y sale de la MISMA tabla que el motor. Se escribe el ultimo
        // porque es el que marca el preset como migrado (arriba).
        written += writeParam (state, busTypeId (0),
                               NEURONiK::DSP::fxDefaultTypeForSlot (0)) ? 1 : 0;
    }

    //==============================================================================
    // HUECO 2: el coro. Rate, depth y el mix, que aqui SI era un mando del hueco.
    if (slotMigrated (1) && ! alreadyOnBus (1))
    {
        const auto rate  = (float) readParam (state, kChorusRate, 0.0);
        const auto depth = (float) readParam (state, kChorusDepth, 0.0);
        const auto mix   = (float) readParam (state, kChorusMix, 0.0);

        written += writeParam (state, busParamId (1, 0), remapKnob (1, 0, kRangeChorusRate, rate)) ? 1 : 0;
        written += writeParam (state, busParamId (1, 1), remapKnob (1, 1, kRangeUnit, depth)) ? 1 : 0;
        // El `chorusMix` de antes ES el `mix` del hueco: el_adapter le pasa
        // `mix = 1` y el slot lo pesa por el suyo (ver la cabecera de FxSlots.h,
        // donde se mide que las dos formulas coinciden en 2.1e-06).
        written += writeParam (state, busMixId (1), mix) ? 1 : 0;
        written += writeParam (state, busGainId (1), 1.0) ? 1 : 0;
        written += writeParam (state, busTypeId (1), NEURONiK::DSP::fxDefaultTypeForSlot (1)) ? 1 : 0;
    }

    //==============================================================================
    // HUECO 3: el retardo. Aqui esta el caso que NO es un simple viaje.
    if (slotMigrated (2) && ! alreadyOnBus (2))
    {
        const auto feedback = (float) readParam (state, kDelayFeedback, 0.0);
        const auto sync     = (int) readParam (state, kDelaySync, 0.0) == 1;
        const auto division = (int) readParam (state, kDelayDivision, 2.0);
        const auto bpm      = (float) readParam (state, State::IDs::masterBPM, 120.0);

        // El tiempo se resuelve AQUI y no se copia del preset, porque con sync
        // puesto el `fxDelayTime` guardado era un valor QUE EL HOST IGNORABA:
        // `fillGlobalParams` lo sustituye por los segundos de la division. Copiar
        // el guardado escribiria un tiempo que no es el que sonaba, y el preset
        // migrado delayaria otra vez al abrirlo. Con sync apagado si es el valor
        // guardado, y el unico caso en que se copia.
        const float seconds = sync
            ? (float) NEURONiK::DSP::Core::secondsForDivision (division, bpm)
            : legacyRange (kRangeDelayTime).convertFrom0to1 ((float) readParam (state, kDelayTime, 0.0));

        written += writeParam (state, busParamId (2, 0), normalisedOf (2, 0, seconds)) ? 1 : 0;
        written += writeParam (state, busParamId (2, 1), remapKnob (2, 1, kRangeDelayFb, feedback)) ? 1 : 0;

        // La mezcla del retardo antes NO era un mando: era fija (`.setMix(0.5)`).
        // Se escribe ese mismo 0.5, que es el punto que eligio el motor al
        // migrar (la cabecera de FxSlots.h explica que baja la seca 6 dB y por
        // que se acepta). Poner 0 dejaria el retardo mudo.
        written += writeParam (state, busMixId (2), 0.5) ? 1 : 0;
        written += writeParam (state, busGainId (2), 1.0) ? 1 : 0;
        written += writeParam (state, busTypeId (2), NEURONiK::DSP::fxDefaultTypeForSlot (2)) ? 1 : 0;
    }

    //==============================================================================
    // HUECO 4: la reverberacion. Los tres mandos del timbre y el nivel mojado.
    if (slotMigrated (3) && ! alreadyOnBus (3))
    {
        const auto size    = (float) readParam (state, kReverbSize, 0.0);
        const auto damping = (float) readParam (state, kReverbDamping, 0.0);
        const auto width   = (float) readParam (state, kReverbWidth, 0.0);
        const auto mix     = (float) readParam (state, kReverbMix, 0.0);

        written += writeParam (state, busParamId (3, 0), remapKnob (3, 0, kRangeUnit, size)) ? 1 : 0;
        written += writeParam (state, busParamId (3, 1), remapKnob (3, 1, kRangeUnit, damping)) ? 1 : 0;
        written += writeParam (state, busParamId (3, 2), remapKnob (3, 2, kRangeUnit, width)) ? 1 : 0;
        // El `levels` de la fila es el nivel mojado de FreeVerb (el wet), que es
        // lo que `fxReverbMix` movia. OJO: el wet NO es el `mix` del hueco, que
        // se queda en la mitad (o a cero, que es el bypass que hacia el
        // envoltorio viejo saltandose el bloque).
        written += writeParam (state, busParamId (3, 3), remapKnob (3, 3, kRangeUnit, mix)) ? 1 : 0;
        written += writeParam (state, busMixId (3), mix > 0.0f ? 0.5 : 0.0) ? 1 : 0;
        written += writeParam (state, busGainId (3), 1.0) ? 1 : 0;
        written += writeParam (state, busTypeId (3), NEURONiK::DSP::fxDefaultTypeForSlot (3)) ? 1 : 0;
    }

    return written;
}

} // namespace NEURONiK::Serialization
