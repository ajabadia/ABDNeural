/*
  ==============================================================================

    DspCore.h  (SHIM de compatibilidad)

    La implementacion canonica vive en ABDSharedCode::DspCore
    (DspCore/DspCore.h, namespace abd::dsp). Este fichero conserva la ruta de
    include historica de todo el motor (`#include "DspCore.h"`) mientras el
    proyecto consume el modulo compartido (Fase 1 [5/6], plan DRY transversal).

    NOTA: artefacto de sincronizacion, NO se mantiene a mano. Para tocar el
    sustrato (AudioBuffer, HeapBlock, SmoothedValue, FloatVectorOperations,
    ScopedNoDenormals, Random, Maths...) edita:

        ABDSharedCode/DspCore/DspCore.h

    Mismo patron que los shims de ABDMS2000 (`Source/DSP/Common/DSPUtils.h`,
    `Modulation/LFO.h`) sobre SynthCore.

    El alias de namespace de abajo es la clave del cambio sin churn: los 20+
    ficheros que decian `dsp::AudioBuffer` siguen diciendolo, y resuelven al
    namespace compartido. Se repite en cada shim a proposito (redeclarar un
    namespace-alias en el mismo ambito hacia el mismo namespace es legal) para
    que cada cabecera sea autocontenida: incluir solo `DspMidiMessage.h` y usar
    `dsp::MidiMessage` sigue funcionando.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspCore.h"

namespace dsp = abd::dsp;
