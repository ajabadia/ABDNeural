# ============================================================================
# NEURONiK - Lista compartida de fuentes DSP (single-source, anti-drift)
#
# Consumida por DOS builds:
#   - CMakeLists.txt raiz (plugin nativo)
#   - wasm/CMakeLists.txt (build WebAssembly, Fase 5)
#
# Leccion ABDMS2000: su build WASM duplicaba la lista a mano y se quedo
# 4 ficheros atras del nativo (incluido el motor de sintesis). Una sola
# lista aqui hace imposible el drift.
#
# Rutas RELATIVAS a la raiz del proyecto (resueltas por cada consumidor).
# ============================================================================

set(NEURONIK_DSP_SOURCES
    Source/DSP/Runtime/DspEngineFacade.cpp
    Source/DSP/CoreModules/LFO.cpp
    Source/DSP/CoreModules/Oscillator.cpp
    Source/DSP/CoreModules/Resonator.cpp
    Source/DSP/CoreModules/Envelope.cpp
    Source/DSP/CoreModules/FilterBank.cpp
    Source/DSP/CoreModules/ResonatorBank.cpp
    Source/DSP/CoreModules/NeuronikEngine.cpp
    Source/DSP/CoreModules/NeurotikEngine.cpp
    Source/DSP/BaseEngine.cpp
    Source/DSP/Synthesis/AdditiveVoice.cpp
    Source/DSP/Synthesis/NeurotikVoice.cpp
)
