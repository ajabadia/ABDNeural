/*
  ==============================================================================

    DspMidiBuffer.h  (SHIM de compatibilidad)

    La implementacion canonica vive en ABDSharedCode::DspCore
    (DspCore/DspMidiBuffer.h, namespace abd::dsp). Este fichero conserva la ruta
    de include historica mientras el proyecto consume el modulo compartido
    (Fase 1 [5/6]).

    NOTA: artefacto de sincronizacion, NO se mantiene a mano. Edita
    ABDSharedCode/DspCore/DspMidiBuffer.h.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspMidiBuffer.h"

namespace dsp = abd::dsp;
