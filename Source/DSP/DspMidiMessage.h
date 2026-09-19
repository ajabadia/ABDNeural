/*
  ==============================================================================

    DspMidiMessage.h  (SHIM de compatibilidad)

    La implementacion canonica vive en ABDSharedCode::DspCore
    (DspCore/DspMidiMessage.h, namespace abd::dsp). Este fichero conserva la
    ruta de include historica mientras el proyecto consume el modulo compartido
    (Fase 1 [5/6]).

    NOTA: artefacto de sincronizacion, NO se mantiene a mano. Edita
    ABDSharedCode/DspCore/DspMidiMessage.h.

    Las comparaciones bit a bit contra juce::MidiMessage que fijan este puerto no
    pueden vivir en el modulo compartido (es JUCE-free por contrato): estan en
    Tests/MidiPortTest.cpp.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspMidiMessage.h"

namespace dsp = abd::dsp;
