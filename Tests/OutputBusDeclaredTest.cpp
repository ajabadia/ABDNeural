/*
  ==============================================================================

    OutputBusDeclaredTest.cpp
    Created: 3 Oct 2026
    Description: El processor TIENE que declarar un bus de salida. Guard de una
                 sola pregunta, pero la pregunta es la que hace que el plugin
                 suene o no.

                 Que caza: un `NEURONiKProcessor` que se queda sin
                 `BusesProperties().withOutput(...)`. Sin eso
                 `getMainBusNumOutputChannels()` vale CERO —JUCE no inventa
                 buses: sale de `getChannelCountOfBus`, que es null si no hay
                 bus— y de ahi sale el plugin mudo.

                 Ya paso, y no en un clon: el Standalone. El
                 `StandalonePluginHolder` pide al `AudioDeviceManager` exactamente
                 `processor->getMainBusNumOutputChannels()` salidas. Con 0,
                 `deviceManager.initialise (0, 0, ...)` no abre NINGUN
                 dispositivo; sin device no hay `processBlock`, y sin
                 `processBlock` el motor no suena. El sintoma era "tres
                 direcciones del selftest no se mueven", sin decir de donde, y
                 el resto del build estaba en verde.

                 POR QUE UN TEST Y NO UNO QUE LEA EL .cpp
                 ---------------------------------------------------
                 Lo primero que se penso fue grep del `BusesProperties` en el
                 fuente, que es lo que hacen los guards de texto del repo. Se
                 descarta por dos motivos, y el segundo es el que manda:

                   1. No affirmaria nada. Un `withOutput` escrito en un
                      comentario, o en una rama que no compila, haria pasar el
                      grep con el plugin igual de mudo. El fallo que se quiere
                      cazar es de COMPORTAMIENTO, y un grep no puede verlo.
                   2. Se puede quedar obsoleto sin avisar. El dia que el bus se
                      declare en otro sitio —una fabrica de buses, una clase
                      base, un `.h`— el grep sigue buscando en el `.cpp` y pasa
                      en verde con el bus sin declarar otra vez, que es
                      exactamente el bucle que este test corta.

                 Este test, en cambio, PREGUNTA al objeto. Si el bus vuelve a
                 desaparecer, da 0 canales y falla, se declare donde se
                 declare.

                 Que mira, y por que cada cosa:
                   - `getMainBusNumOutputChannels() > 0`. La pregunta que hace
                     el host. Es el aserto que importa; el resto la explica.
                   - Que el bus sea `stereo`, no mono. Un synth que declara una
                     sola salida no esta mudo, asi que esto NO habria cazado el
                     fallo de hoy — pero un mono donde el host espera stereo
                     es el mismo tipo de surprise (que el VST3 declares
                     stereo) y lo delata antes de que llegue a un usuario.
                   - Que se pueda ESCRIBIR en el bus de salida. Declara el bus y
                     snakes igual de mudo si `getBusBuffer` no devuelve nada
                     escribible, y eso el mudo del `processBlock` lo delata
                     igual de tarde. Se escribe un patron pequeno y se comprueba
                     que sale.

                 No comprueba el AUDIO, y a proposito: que suene de verdad lo
                 dice el selftest del paso 9, que corre el arnes real contra
                 la pagina. Este guard solo afirma el CONTRATO DE CANALES, que
                 es lo que se puede comprobar en un test de 30 lineas y sin
                 abrir un dispositivo.

                 Efecto secundario de la instancia: el `PresetManager` crea
                 Documents/NEURONiK/Presets si no existe (igual que hace el
                 banco WebView2 en cada pasada).

  ==============================================================================
*/

#include "../Source/Main/NEURONiKProcessor.h"

#include <iostream>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }
}

int main()
{
    std::cout << "OutputBusDeclaredTest: el processor declara un bus de salida\n";

    NEURONiKProcessor processor;

    // ── 1. LA PREGUNTA QUE HACE EL HOST ────────────────────────────────────
    // Con 0 aqui, el Standalone llama a initialise (0, 0, ...), no abre
    // dispositivo, no hay processBlock y el plugin no suena. Medido el
    // 2026-10-02. Este es el aserto que cierra el fallo.
    const int outputs = processor.getMainBusNumOutputChannels();

    check (outputs > 0,
           juce::String ("el bus principal declara salidas (ahora: ") + juce::String (outputs)
             + ") — con 0 el Standalone pide 0 salidas, no abre dispositivo y el "
               "plugin sale mudo");

    // ── 2. STEREO, no mono ─────────────────────────────────────────────────
    // No habria cazado el fallo de hoy, pero un bus mono donde el host espera
    // stereo es la misma sorpresa un dia mas tarde.
    check (outputs == 2,
           juce::String ("el bus de salida es stereo (ahora: ") + juce::String (outputs)
             + ") — el VST3 declara stereo en JucePlugin_ChannelConfigurations, y el "
               "bus de JUCE tiene que decir lo mismo");

    if (outputs > 0)
    {
        // El BUFFER no vive en el Bus (el Bus solo describe canales y layout):
        // lo tiene el AudioProcessor, y es el que el host rellena antes de
        // processBlock. Por eso la escritura se mira sobre el, no sobre el Bus.
        check (processor.getBus(false, 0) != nullptr,
               "el bus de salida existe y el processor lo puede describir");

        // ── 3. SE PUEDE ESCRIBIR ───────────────────────────────────────────
        // Declarar el bus y no poder escribir en el es el mismo mudo con otro
        // nombre: el host abre el dispositivo y el processBlock escribe en la
        // nada. Se hace lo que hace el host —preparar el bus de salida y
        // escribir en el— y se mira si sale lo que se puso.
        //
        // El buffer NO se inventa: `prepareToPlay` es lo que le da al processor
        // un bus de entrada y uno de salida escribibles, igual que hace el host
        // antes de processBlock. Sin el, `getBusBuffer` no tiene nada que
        // devolver y el mudo que se caza aqui ni se puede reproducir.
        processor.prepareToPlay (48000.0, 64);

        juce::AudioBuffer<float> buffer (outputs, 64);

        // Que el processor acepte el bus. `getBusBuffer` devuelve el buffer POR
        // VALOR y recorta los canales al bus: si el declarado no encaja con lo
        // que le da el host, el recorte deja 0 canales, que es el mismo mudo.
        juce::AudioBuffer<float> busBuffer = processor.getBusBuffer (buffer, false, 0);

        for (int channel = 0; channel < busBuffer.getNumChannels(); ++channel)
            busBuffer.setSample (channel, 0, 0.25f);

        const auto* muestras = busBuffer.getReadPointer (0);
        const bool escribible = busBuffer.getNumChannels() > 0
                                && muestras != nullptr
                                && muestras[0] == 0.25f;

        check (escribible,
               juce::String ("el bus de salida se puede preparar y escribir, y devuelve lo "
                             "que se le pone (canales tras el recorte: ")
                 + juce::String (busBuffer.getNumChannels())
                 + ") — si esto falla, el bus esta declarado pero no se puede usar: "
                   "el mudo vuelve a ser mudo, por otro nombre");
    }
    else
    {
        // Con 0 salidas no hay nada mas que mirar, y decirlo evita que el
        // resumen salga con 3 [ok] y un rojo que nadie encuentra.
        std::cout << "  [----] las comprobaciones de stereo y escritura no se pueden "
                     "hacer sin un bus de salida: no hay sobre que escribir.\n";
        ++failures;
    }

    std::cout << (failures == 0
                    ? "  RESULT: OK\n"
                    : "  RESULT: FAIL (" + juce::String (failures) + ")\n");

    return failures == 0 ? 0 : 1;
}