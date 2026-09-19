/*
  ==============================================================================

    DspSafety.h
    Created: 30 Jan 2026 (como DSPUtils.h; renombrado el 2026-09-19)
    Description: Red de seguridad del motor para PARAMETROS: valida un valor
                 (NaN, infinito o fuera de rango) antes de que entre en la cadena
                 de audio y devuelve el fallback si no sirve.

    POR QUE EL NOMBRE. Este fichero se llamaba DSPUtils.h, el MISMO nombre que el
    de ABDSharedCode (SynthCore/DSPUtils.h, namespace abd::synth::DSPUtils), que
    es otra cosa: constantes, conversiones de unidades, waveshapers y un
    LinearSmoother. No habia codigo duplicado, solo homonimia — y con dos ficheros
    homonimos en la misma familia de proyectos, el dia que alguien ponga la carpeta
    SynthCore como include dir un `#include "DSPUtils.h"` se resuelve por ORDEN de
    include dirs: bug silencioso y dependiente del orden. El nombre tampoco
    describia el contenido ("utils" no es una red de seguridad).

    POR QUE NO ESTA (TODAVIA) EN UN MODULO COMPARTIDO. Lo evaluamos al renombrar
    (2026-09-19) y la conclusion fue: todavia no. El conjunto de funciones no se
    solapa con el de SynthCore (alli no hay validacion de parametros), asi que no
    hay duplicacion que eliminar, y este helper se apoya en el sustrato
    (dspDbg, dspAssert, dsp::jlimit), de modo que mudarlo obliga a elegir entre
    meter utilidades de producto en DspCore (que es "ports literales de JUCE") o
    hacer que SynthCore dependa de DspCore — hoy son independientes y los consume
    gente distinta (ABDMS2000/ABDDEep vs ABDNeural).

    Se unificara cuando se cumpla: (1) un segundo synth necesite validacion de
    parametros, y (2) el layout admita colgarlo sin crear una dependencia cruzada
    nueva.

    NO HAY SANEO DE BUFFERS AQUI, Y ES A PROPOSITO. Este fichero tenia
    `sanitizeAudioBuffer` (sustituir NaN/Inf por silencio en un buffer) con CERO
    llamadas; se borro el 2026-09-19. El motivo no es solo que estuviera muerta,
    sino que su politica es la que el motor NO usa. Donde el motor se enfrenta a un
    NaN de verdad (los lazos de realimentacion de las voces) la respuesta es
    DETECTAR Y REINICIAR LA VOZ:

        AdditiveVoice.cpp / NeurotikVoice.cpp:
            if (! isfinite (...)) { dspDbg ("NaN ... voice reset"); reset(); return false; }

    Eso arregla el ESTADO que diverge. Escribir 0 en el buffer de salida, en cambio,
    deja el lazo roto: seguiria produciendo NaN en las muestras siguientes (silencio
    sostenido y CPU gastada) y encima taparia el sintoma. Ponerlo en la salida del
    motor (`BaseEngine::applyGlobalFX`) seria lo mismo con un coste extra: un
    barrido `isfinite` por muestra sobre el buffer maestro en el hilo de audio. El
    otro caso real, los denormales, lo cubre `dsp::ScopedNoDenormals` en los lazos.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"
#include "DspDebug.h"

#include <cmath>

namespace NEURONiK::DSP {

/**
 * Validates and clamps an audio parameter to a safe range.
 * In Debug: Asserts if value is invalid.
 * In Release: Clamps silently and optionally logs warning.
 */
template<typename T>
inline T validateAudioParam(T value, T minVal, T maxVal, T fallback, const char* paramName) noexcept
{
    // Use std::isfinite to detect NaN and Infinity
    if (!std::isfinite(value) || value < minVal || value > maxVal)
    {
        // Aviso solo en Debug: dspDbg/dspAssert compilan a nada en Release
        // (misma puerta que JUCE_DEBUG), asi que la puerta explicita sobra.
        // ignoreUnused evita el C4100 de paramName cuando el aviso no existe.
        dsp::ignoreUnused (paramName);
        dspDbg ("WARNING: Invalid " << paramName << " (" << value
                                    << ") clamped to " << fallback);
        dspAssert (false);
        return fallback;
    }
    
    return dsp::jlimit(minVal, maxVal, value);
}

} // namespace NEURONiK::DSP
