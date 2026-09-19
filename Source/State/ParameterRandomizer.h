/*
  ==============================================================================

    ParameterRandomizer.h
    Created: 19 Sep 2026
    Description: El RANDOMIZE del instrumento, fuera del panel (ticket 8.2).

                 ESTABA EN `ParameterPanel` (el panel nativo) y ahi tenia dos
                 problemas que este modulo resuelve de raiz:

                 1. mezclaba unidades: convertia el valor actual a REAL, el
                    sorteo a NORMALIZADO y los promediaba con `jmap`. Para
                    cualquier parametro cuyo rango real no sea 0..1 (cutoff en
                    Hz, tiempos en segundos, BPM...) el resultado caia fuera de
                    0..1 y `setValueNotifyingHost` lo clavaba en el maximo. Ahora
                    se mezcla en unidades REALES y se convierte a normalizado una
                    sola vez, al final.
                 2. duplicaba los rangos del contrato a mano y a ciegas: cada
                    parametro llevaba sus literales (0.8..1.3, 200..8000) sin
                    comprobar que estuvieran dentro de su propio rango. Aqui la
                    intencion musical sigue siendo la misma, pero se RECORTA al
                    rango real del parametro, y el test comprueba que cada
                    intencion cabe y que cada id existe.

                 La tabla de abajo no es una copia del APVTS: es la INTENCION
                 musical (que se randomiza, entre que extremos y quien lo
                 congela). Los rangos vienen del parametro; la intencion, de aqui.

                 Reglas que se respetan:
                   - sin entropia de sistema (regla WASM 7A): el `juce::Random`
                     entra por parametro, asi que el test es determinista;
                   - nada de escribir a ciegas: si el sorteo deja el valor donde
                     estaba, no se cuenta ni se notifica (a fuerza 0 no se mueve
                     nada);
                   - los congelados (`freezeResonator`/`freezeFilter`/
                     `freezeEnvelopes`) se leen del APVTS, no de la UI.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace NEURONiK::State
{

/**
 * @brief Que congelado (`freeze*`) protege un parametro del RANDOMIZE.
 * @details Los FX comparten el congelado de filtro a proposito (no hay boton
 *          propio): es la decision que ya tomaba el panel nativo y se conserva.
 */
enum class FreezeGroup
{
    none,        //!< nunca congelado (parametro de motor, no de timbre)
    resonator,   //!< freezeResonator: el banco espectral, el unísono y el morph
    filter,      //!< freezeFilter: filtro + los FX que entran en el sorteo
    envelopes    //!< freezeEnvelopes: la envolvente de amplitud y el nivel
};

/** @brief Un parametro del sorteo: intencion musical + quien lo congela. */
struct RandomizeTarget
{
    const char* id;         //!< id del APVTS (unico literal de id en este modulo)
    float minValue;         //!< extremo inferior de la INTENCION (unidades reales)
    float maxValue;         //!< extremo superior de la INTENCION (unidades reales)
    FreezeGroup freeze;     //!< grupo que lo protege
};

/** @brief La tabla del RANDOMIZE, en orden de declaracion. */
const std::vector<RandomizeTarget>& getRandomizeTargets();

/** @brief Estado de los tres congelados, tal como los ve el APVTS. */
struct FreezeFlags
{
    bool resonator = false;
    bool filter = false;
    bool envelopes = false;
};

/** @brief Lee los tres `freeze*` del APVTS (0.5 es el umbral, como en JUCE). */
FreezeFlags readFreezeFlags (const juce::AudioProcessorValueTreeState& apvts);

/** @brief Fuerza del sorteo: el parametro `randomStrength` (0..1, 1 = sorteo entero). */
float readRandomizeStrength (const juce::AudioProcessorValueTreeState& apvts);

/**
 * @brief Aplica el RANDOMIZE al APVTS.
 * @details Para cada objetivo no congelado, sortea un valor real dentro de la
 *          intencion (recortada al rango del parametro), lo mezcla con el actual
 *          segun `strength` EN UNIDADES REALES y escribe el resultado normalizado.
 * @param strength 0..1; se recorta. A 0 no mueve nada.
 * @param random   generador inyectado (determinista en tests, sin entropia global).
 * @returns cuantos parametros cambiaron de valor.
 */
int applyRandomize (juce::AudioProcessorValueTreeState& apvts, float strength, juce::Random& random);

} // namespace NEURONiK::State
