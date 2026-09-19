/*
  ==============================================================================

    DspDebug.h  (SHIM de compatibilidad)

    La implementacion canonica vive en ABDSharedCode::DspCore
    (DspCore/DspDebug.h, namespace abd::dsp): `dspDbg`, port de DBG.

    NOTA: artefacto de sincronizacion, NO se mantiene a mano. Edita
    ABDSharedCode/DspCore/DspDebug.h.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspDebug.h"

namespace dsp = abd::dsp;
