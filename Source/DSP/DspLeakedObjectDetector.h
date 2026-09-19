/*
  ==============================================================================

    DspLeakedObjectDetector.h  (SHIM de compatibilidad)

    La implementacion canonica vive en ABDSharedCode::DspCore
    (DspCore/DspLeakedObjectDetector.h, namespace abd::dsp): `dspLeakDetector` y
    `dspDeclareNonCopyableWithLeakDetector`, ports de los macros de JUCE.

    NOTA: artefacto de sincronizacion, NO se mantiene a mano. Edita
    ABDSharedCode/DspCore/DspLeakedObjectDetector.h.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspLeakedObjectDetector.h"

namespace dsp = abd::dsp;
