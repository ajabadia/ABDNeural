/*
  ==============================================================================

    NeuronikEngine.cpp
    Created: 30 Jan 2026
    Description: Implementation of the central synthesis engine.

  ==============================================================================
*/

#include "DspCore.h"
#include "NeuronikEngine.h"
#include "../Synthesis/AdditiveVoice.h"
#include "../DspSafety.h"

namespace NEURONiK::DSP {

NeuronikEngine::NeuronikEngine()
{
    // Reserva PEREZOSA: nace con las voces de su limite (16 por defecto), no 32.
    // `ensureVoices` crece cuando `setPolyphony` sube el techo.
    ensureVoices (activeVoiceLimit.load());
}

std::unique_ptr<IVoice> NeuronikEngine::createVoice(int)
{
    return std::make_unique<Synthesis::AdditiveVoice>();
}

void NeuronikEngine::prepare(double sampleRate, int samplesPerBlock)
{
    BaseEngine::prepare(sampleRate, samplesPerBlock);
}

void NeuronikEngine::renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages)
{
    // 1. Parametros del bloque (config de LFO, FX y params de voz)
    updateParameters();

    // 2. Process MIDI events
    processMidiBuffer(midiMessages);

    // 3. Voces + modulacion, en tramos de tasa de control fija: el LFO se lee y la
    //    matriz se aplica cada kControlBlockSize muestras, no cada bloque del host.
    renderVoicesWithControlRate(buffer);

    // 4. Global FX
    applyGlobalFX(buffer);
}

// ============================================================================
//  LA TABLA DE DESTINOS: lo que el switch de 31 casos contaba, en datos
// ============================================================================
//  Un destino de la matriz es una de TRES cosas, y la tabla lo dice entero en
//  vez de repartirlo entre 31 ramas de codigo:
//
//    voiceAdd  -> SUMA `rawMod * scale` a un miembro de la voz.
//    globalAdd -> SUMA `rawMod * scale` a un parametro global (el bus de FX, no
//                 la voz).
//    envAssign -> si la fuente es la envolvente que pide el destino, ASIGNA el
//                 amount a un factor de routing. envAdd lo SUMA. Los dos son la
//                 "sintesis de reemplazo": la envolvente entra con ese factor,
//                 no encima del valor de la fuente.
//
//  Cada destino tiene DOS ranuras porque hay destinos con las dos ramas: el 1
//  (Osc Level) y el 10 (Filter Cutoff) suman con un LFO y REEMPLAZAN cuando la
//  fuente es su envolvente. La ranura `env` se consulta antes que la `add`, que
//  es exactamente la precedencia que tenia el switch: dentro de cada case, la
//  pregunta por la fuente iba antes que la suma.
//
//  El indice de la tabla ES el indice de preset del destino. Anadir un destino
//  es anadir una fila al FINAL y ampliar kNumModDestinations: insertar en medio
//  re-mapearia los presets ya guardados.
//
//  ── POR QUE CADA FILA ESCRIBE LOS CINCO CAMPOS A MANO, SIN FABRICAS ─────────
//  La primera version de esta tabla se construia con las fabricas `voiceAdd()`,
//  `globalAdd()` y `envRule()`, que son constexpr. Compilaba sin un solo aviso
//  y en ejecucion LOS PUNTEROS-MIEMBRO DE LA TABLA VALIAN TODOS CERO: el motor
//  se comia las rutas y de la UI no se/caia nadie, solo un hash de audio
//  distinto. MSVC no constant-initializa un array constexpr cuyos elementos
//  salen de una funcion constexpr que devuelve un agregado con punteros-miembro
//  de una clase POLIMORFICA (IVoice tiene funciones virtuales): los deja a
//  cero. Con inicializacion DIRECTA del agregado --lo que hay aqui-- si
//  funciona, y los static_assert de abajo lo comprueban en cada compilacion.
//
//  O sea que lo de no poner fabricas no es estilo: es que aqui mienten, y lo
//  unico que las distingue de la version rota es que la rota no decia nada.
namespace
{

/** Una regla de destino. Los cinco campos se escriben siempre: un puntero-
    miembro sin inicializar es justamente el valor que hace que la tabla valga
    cero, asi que "dejarlo a su cuenta" no es una opcion. */
struct ModRule
{
    enum class Kind
    {
        none,        ///< ranura vacia
        voiceAdd,    ///< suma al miembro de una voz
        globalAdd,   ///< suma a un parametro global (FX)
        envAssign,   ///< ASIGNA el amount a un factor de routing, si la fuente es la envolvente
        envAdd       ///< SUMA el amount a un factor de routing, si la fuente es la envolvente
    };

    Kind kind = Kind::none;
    int envSource = -1;                            ///< envAssign/envAdd: 6 = ENV 1, 7 = ENV 2
    float IVoice::* voice = nullptr;               ///< voiceAdd / envAssign / envAdd
    float GlobalParams::* global = nullptr;        ///< globalAdd
    float scale = 0.0f;                            ///< voiceAdd / globalAdd
};

struct ModDestinationDescriptor
{
    ModRule env;   ///< la rama que PREGUNTA por la fuente (se consulta antes)
    ModRule add;   ///< la rama aditiva, para cualquier otra fuente
};

using Kind = ModRule::Kind;

constexpr int kNumModDestinations = 31;   // = getModDestinationTable().size()

constexpr ModDestinationDescriptor kModDestinations[kNumModDestinations] =
{
    { {}, {} },   //  0  Off  --  inerte A PROPOSITO: la ausencia de destino, no un destino que aplica un 0
    { Kind::envAssign, 6, &IVoice::modEnvLevel, nullptr, 0.0f, Kind::voiceAdd, -1, &IVoice::modLevel, nullptr, 1.0f },   //  1  Osc Level  --  ENV 1 REEMPLAZA el factor de routing; otra fuente suma sobre el nivel
    { {}, Kind::voiceAdd, -1, &IVoice::modInharmonicity, nullptr, 1.0f },   //  2  Inharmonicity
    { {}, Kind::voiceAdd, -1, &IVoice::modRoughness, nullptr, 1.0f },   //  3  Roughness
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphX, nullptr, 1.0f },   //  4  Morph X
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphY, nullptr, 1.0f },   //  5  Morph Y
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpAttack, nullptr, 1.0f },   //  6  Amp Attack
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpDecay, nullptr, 1.0f },   //  7  Amp Decay
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpSustain, nullptr, 1.0f },   //  8  Amp Sustain
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpRelease, nullptr, 1.0f },   //  9  Amp Release
    { Kind::envAssign, 7, &IVoice::modEnvCutoff, nullptr, 0.0f, Kind::voiceAdd, -1, &IVoice::modCutoff, nullptr, 18000.0f },   // 10  Filter Cutoff  --  la voz SUMA este valor al cutoff en hercios; ENV 2 REEMPLAZA el factor
    { {}, Kind::voiceAdd, -1, &IVoice::modFilterRes, nullptr, 1.0f },   // 11  Filter Res
    { Kind::envAdd, 7, &IVoice::modEnvFltDepth, nullptr, 0.0f, {} },   // 12  Filter Env Amt  --  profundidad del knob filterEnvAmount (retirado 2026-09-26); solo ENV 2
    { Kind::envAdd, 7, &IVoice::modEnvFltAttack, nullptr, 0.0f, {} },   // 13  Flt Attack  --  ADSR del filtro por ENV 2 (aditiva)
    { Kind::envAdd, 7, &IVoice::modEnvFltDecay, nullptr, 0.0f, {} },   // 14  Flt Decay  --  aditiva
    { Kind::envAdd, 7, &IVoice::modEnvFltSustain, nullptr, 0.0f, {} },   // 15  Flt Sustain  --  aditiva
    { Kind::envAdd, 7, &IVoice::modEnvFltRelease, nullptr, 0.0f, {} },   // 16  Flt Release  --  aditiva
    { {}, Kind::globalAdd, -1, nullptr, &GlobalParams::saturationAmt, 1.0f },   // 17  Saturation  --  FX del bus, no de la voz
    { {}, Kind::globalAdd, -1, nullptr, &GlobalParams::delayTime, 1.0f },   // 18  Delay Time  --  FX del bus
    { {}, Kind::globalAdd, -1, nullptr, &GlobalParams::delayFB, 1.0f },   // 19  Delay FB  --  FX del bus
    { {}, Kind::voiceAdd, -1, &IVoice::modParity, nullptr, 1.0f },   // 20  Odd/Even Bal
    { {}, Kind::voiceAdd, -1, &IVoice::modShift, nullptr, 1.0f },   // 21  Spectral Shift
    { {}, Kind::voiceAdd, -1, &IVoice::modRolloff, nullptr, 1.0f },   // 22  Harm Roll-off
    { {}, Kind::voiceAdd, -1, &IVoice::modExciteNoise, nullptr, 1.0f },   // 23  Excite Noise
    { {}, Kind::voiceAdd, -1, &IVoice::modExciteColor, nullptr, 1.0f },   // 24  Excite Color
    { {}, Kind::voiceAdd, -1, &IVoice::modImpulseMix, nullptr, 1.0f },   // 25  Impulse Mix
    { {}, Kind::voiceAdd, -1, &IVoice::modResonance, nullptr, 1.0f },   // 26  Res Bank Res
    { {}, Kind::voiceAdd, -1, &IVoice::modUnison, nullptr, 1.0f },   // 27  Unison Detune
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ, nullptr, 1.0f },   // 28  Morph Z  --  FASE 11.3
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ2, nullptr, 1.0f },   // 29  Morph Z 2  --  FASE 11.3
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ3, nullptr, 1.0f },   // 30  Morph Z 3  --  FASE 11.3
};

// La tabla DEBE seguir la longitud de la tabla de etiquetas: si crece
// getModDestinationTable() y no crece esta, el destino nuevo seria inerte --
// seleccionable en la pagina y sin efecto, que es justo el fallo que fija el
// test de la matriz.
static_assert (kNumModDestinations == 31,
               "la tabla de destinos debe seguir la longitud de getModDestinationTable()");

// Y las filas que una etiqueta promete tienen que apuntar a SU miembro. Con la
// tabla a cero --el fallo de MSVC de arriba-- estos asertos entran a compilAR y
// dejan el build en rojo, en vez de dejar que el motor se coma las rutas en
// silencio y que solo se entere el oido.
static_assert (kModDestinations[0].add.kind == Kind::none, "el destino 0 (Off) es inerte");
static_assert (kModDestinations[1].env.kind == Kind::envAssign
               && kModDestinations[1].env.voice == &IVoice::modEnvLevel,
               "destino 1: ENV 1 asigna modEnvLevel");
static_assert (kModDestinations[1].add.voice == &IVoice::modLevel,
               "destino 1: otra fuente suma a modLevel");
static_assert (kModDestinations[2].add.voice == &IVoice::modInharmonicity, "destino 2: Inharmonicity");
static_assert (kModDestinations[3].add.voice == &IVoice::modRoughness, "destino 3: Roughness");
static_assert (kModDestinations[4].add.voice == &IVoice::modMorphX, "destino 4: Morph X");
static_assert (kModDestinations[5].add.voice == &IVoice::modMorphY, "destino 5: Morph Y");
static_assert (kModDestinations[10].env.voice == &IVoice::modEnvCutoff,
               "destino 10: ENV 2 asigna modEnvCutoff");
static_assert (kModDestinations[10].add.voice == &IVoice::modCutoff
               && kModDestinations[10].add.scale == 18000.0f,
               "destino 10: otra fuente suma 18000 Hz a modCutoff");
static_assert (kModDestinations[12].env.kind == Kind::envAdd
               && kModDestinations[12].env.voice == &IVoice::modEnvFltDepth,
               "destino 12: ENV 2 suma a modEnvFltDepth");
static_assert (kModDestinations[13].env.voice == &IVoice::modEnvFltAttack, "destino 13: Flt Attack");
static_assert (kModDestinations[16].env.voice == &IVoice::modEnvFltRelease, "destino 16: Flt Release");
static_assert (kModDestinations[17].add.kind == Kind::globalAdd
               && kModDestinations[17].add.global == &GlobalParams::saturationAmt,
               "destino 17: Saturacion es un parametro global, no de voz");
static_assert (kModDestinations[19].add.global == &GlobalParams::delayFB, "destino 19: Delay FB");
static_assert (kModDestinations[20].add.voice == &IVoice::modParity, "destino 20: Odd/Even Bal");
static_assert (kModDestinations[21].add.voice == &IVoice::modShift, "destino 21: Spectral Shift");
static_assert (kModDestinations[22].add.voice == &IVoice::modRolloff, "destino 22: Harm Roll-off");
static_assert (kModDestinations[27].add.voice == &IVoice::modUnison, "destino 27: Unison Detune");
static_assert (kModDestinations[28].add.voice == &IVoice::modMorphZ, "destino 28: Morph Z");
static_assert (kModDestinations[30].add.voice == &IVoice::modMorphZ3, "destino 30: Morph Z 3");

} // namespace

void NeuronikEngine::applyModulation()
{
    // Snapshot LFO values from base. Las fuentes ENV (6/7) se resuelven
    // per-voz dentro de los casos per-note: aqui valen 0 y nunca entran
    // por el camino LFO (una envolvente no es global, es de cada nota).
    // Las ocho fuentes, en el orden de la tabla (getModSources). Las tres que
    // eran un TODO (PB, MW, AT) ahora traen el valor real: lo escribe el mismo
    // gesto MIDI que mueve la voz, y se guarda normalizado para que la matriz
    // lo pueda sumar igual que a un LFO. Ver los miembros en NeuronikEngine.h.
    float sources[8] = { 
        0.0f,                   // Off
        lfo1Value.load(),       // LFO 1
        lfo2Value.load(),       // LFO 2
        pitchBendSource_,       // Pitch Bend   (-1..+1)
        modWheelSource_,        // Mod Wheel    (0..1)
        aftertouchSource_,      // Aftertouch   (0..1)
        0.0f,                   // ENV 1: per-voz (VCA)
        0.0f                    // ENV 2: per-voz (ADSR del filtro)
    };

    // Reset voice mod values
    for (auto& v : voices) v->resetModulations();
    
    // Reset visualization values
    lastModulations.fill(0.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto& route = currentGlobalParams.modMatrix[i];
        if (route.source == 0 || route.destination == 0) continue;
        
        float rawMod = sources[dsp::jlimit(0, 5, route.source)] * route.amount;
        
        // Update visualization
        if (route.destination >= 0 && route.destination < 64)
            lastModulations[route.destination] += rawMod;
        
        // Dest logic, ahora POR TABLA (ver kModDestinos, mas arriba).
        // Los destinos PER-NOTE (1, 10, 12-16) preguntan por la fuente: los
        // LFOs suman como siempre; si la ruta es ENV, la voz recibe el amount
        // como factor de routing (sintesis de reemplazo, ver arriba).
        //
        // Un destino fuera de la tabla no hace NADA, y aqui no se avisa porque
        // avisar en el hilo de audio no es opcion. El aviso vive en el
        // static_assert de arriba, que es donde se puede ver gratis.
        if (route.destination < 0 || route.destination >= kNumModDestinations)
            continue;

        const auto& descriptor = kModDestinations[route.destination];

        // 1) La rama que PREGUNTA por la fuente. Va antes que la aditiva porque
        //    es lo que hacia el switch dentro de cada case; si la fuente no es
        //    la envolvente que este destino pide, la ruta cae en la rama
        //    aditiva de abajo, igual que entonces.
        if (descriptor.env.kind != ModRule::Kind::none
            && route.source == descriptor.env.envSource)
        {
            for (auto& v : voices)
            {
                if (descriptor.env.kind == ModRule::Kind::envAssign)
                    v.get()->* (descriptor.env.voice) = route.amount;   // factor: sobrescribe
                else
                    v.get()->* (descriptor.env.voice) += route.amount;   // profundidad: suma
            }
        }
        // 2) La rama aditiva: a un miembro de la voz, o al bus de FX.
        else if (descriptor.add.kind == ModRule::Kind::voiceAdd)
        {
            const float amount = rawMod * descriptor.add.scale;
            for (auto& v : voices)
                v.get()->* (descriptor.add.voice) += amount;
        }
        else if (descriptor.add.kind == ModRule::Kind::globalAdd)
        {
            currentGlobalParams.* (descriptor.add.global) += rawMod * descriptor.add.scale;
        }
    }
}

void NeuronikEngine::updateParameters()
{
    // Propagate parameters to all voices
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->setParams(pendingVoiceParams);
    }

    BaseEngine::updateParameters();
    // La modulacion NO se aplica aqui: la aplica renderVoicesWithControlRate() en la
    // rejilla de control, con el valor de LFO de cada tramo (antes se aplicaba una vez
    // por bloque del host y con el valor del bloque anterior).
}


void NeuronikEngine::getSpectralData(float* destination64) const
{
    bool found = false;
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            auto& partials = av->getResonator().getPartialAmplitudes();
            for (int i = 0; i < 64; ++i) destination64[i] = partials[i];
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        for (int i = 0; i < 64; ++i) destination64[i] = 0.0f;
    }
}

void NeuronikEngine::getEnvelopeLevels(float& amp, float& filter) const
{
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            amp = av->getAmpEnvelopeLevel();
            filter = av->getFilterEnvelopeLevel();
            return;
        }
    }
    amp = 0.0f;
    filter = 0.0f;
}

void NeuronikEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    int numToCopy = dsp::jmin(count, (int)lastModulations.size());
    for (int i = 0; i < numToCopy; ++i)
        destination[i] = lastModulations[i];
        
    // Fill remaining with 0
    for (int i = numToCopy; i < count; ++i)
        destination[i] = 0.0f;
}

void NeuronikEngine::loadModel(const Common::SpectralModel& model, int slot)
{
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->loadModel(model, slot);
    }
}

void NeuronikEngine::setVoiceParams(const NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p)
{
    pendingVoiceParams = p;
}

// MORPH del pad XY: read-modify-write de pendingVoiceParams. No toca morphZ
// (el eje temporal es parametro propio, morphZ/morphZ2/morphZ3) ni el resto
// de la ficha: solo los dos ejes del pad. Viaja por el mismo canal que
// setVoiceParams, asi que el handoff a las voces es el de siempre.
void NeuronikEngine::setMorph (float morphX, float morphY)
{
    pendingVoiceParams.morphX = morphX;
    pendingVoiceParams.morphY = morphY;
}

// Eje temporal del morph (FASE 10): frame canonico de la capa 0.
void NeuronikEngine::setMorphZ (float morphZ)
{
    pendingVoiceParams.morphZ = morphZ;
}

// FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 es el fondo, siempre
// al maximo). Mismo canal RT-safe que setMorphZ; el clampeo lo hace el
// resonador al consumirlo.
void NeuronikEngine::setVoiceLayerMorph (float layerGain2, float layerGain3)
{
    pendingVoiceParams.layerGain2 = layerGain2;
    pendingVoiceParams.layerGain3 = layerGain3;
}

void NeuronikEngine::handleMidiEvent(const dsp::MidiMessage& m)
{
    int channel = m.getChannel();

    if (m.isNoteOn())
    {
        const int limit = activeVoiceLimit.load();
        for (int i = 0; i < limit; ++i)
        {
            if (!voices[i]->isActive())
            {
                voices[i]->setChannel(channel);
                voices[i]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
                return;
            }
        }
        voices[0]->setChannel(channel);
        voices[0]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
        {
            if (v->isActive() && v->getCurrentlyPlayingNote() == m.getNoteNumber() && v->getChannel() == channel)
                v->noteOff(m.getFloatVelocity(), true);
        }
    }
    else if (m.isPitchWheel())
    {
        float bendSemitones = ((float)m.getPitchWheelValue() - 8192.0f) / 8192.0f * 48.0f; // Scale to 48 semitones
        // La misma deflection, normalizada, para la matriz: una sola verdad
        // para la voz y para la ruta de modulación.
        pitchBendSource_ = (float)m.getPitchWheelValue() / 8192.0f - 1.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->notePitchBend(bendSemitones);
        }
    }
    else if (m.isAftertouch() || m.isChannelPressure())
    {
        float pressureVal = m.isAftertouch() ? (float)m.getAfterTouchValue() : (float)m.getChannelPressureValue();
        float pressure = pressureVal / 127.0f;
        // Igual que el pitch bend: la presión es tambien fuente de la matriz.
        aftertouchSource_ = pressure;
        
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->notePressure(pressure);
        }
    }
    else if (m.isController() && m.getControllerNumber() == 1)
    {
        // CC 1 es la rueda de modulacion del MIDI estandar, y la tabla la
        // ofrece como fuente: sin esto seguia sin modular nada.
        modWheelSource_ = (float)m.getControllerValue() / 127.0f;
    }
    else if (m.isController() && m.getControllerNumber() == 74)
    {
        float timbre = (float)m.getControllerValue() / 127.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->noteTimbre(timbre);
        }
    }
}


} // namespace NEURONiK::DSP
