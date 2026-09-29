/*
  ==============================================================================

    Vst3LoadTest.cpp
    Created: 28 Sep 2026
    Description: The compiled VST3 bundle, scanned and instantiated the way a DAW
                 does, against the parameter contract compiled from today's
                 sources.

                 This is the check that a plugin which BUILDS can still be dead on
                 disk: the bundle missing from the artefact tree, a load failure
                 that only shows up in the host, or — the interesting one — a
                 parameter list that has drifted from the contract because the
                 layout changed without the VST3 being rebuilt.

                 It is deliberately a SEPARATE PROCESS that knows nothing about
                 NEURONiKProcessor: the only thing shared with the plugin is the
                 .vst3 on disk. The expected parameters come from the same
                 descriptor sources the plugin is built from, materialised in a
                 throwaway APVTS (State::createLayoutApvts), so a stale bundle and
                 a changed source cannot both be right.

  ==============================================================================
*/

#include "../Source/State/ParameterDescriptors.h"
#include "../Source/State/ParameterDefinitions.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>

#ifndef NEURONIK_VST3_PATH
 #define NEURONIK_VST3_PATH ""
#endif

// El nombre de fabricante que el bundle DEBERIA declarar. Lo pasa CMake desde la
// MISMA variable que se metio en el bundle (`NEURONIK_MANUFACTURER_NAME`): si
// estuviera escrito aqui, el test compararia su propia constante contra si mismo
// y no fijaria nada. Lo que fija es el valor REAL que sale del escaneo del disco.
#ifndef NEURONIK_EXPECTED_MANUFACTURER
 #define NEURONIK_EXPECTED_MANUFACTURER ""
#endif

namespace
{
    using namespace NEURONiK;
    using namespace NEURONiK::State;

    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        std::cout << (condition ? "  [ok]   " : "  [FAIL] ") << description << '\n';

        if (! condition)
            ++failures;
    }

    /** The ids an APVTS actually holds, read the way preset state is read. */
    juce::StringArray idsInLayout (juce::AudioProcessorValueTreeState& apvts)
    {
        juce::StringArray ids;

        for (const auto& child : apvts.copyState())
            if (child.hasType ("PARAM"))
                ids.add (child.getProperty ("id").toString());

        return ids;
    }

    /** The bundle ROOT, from wherever the path points.
     *
     *  `NEURONIK_VST3_PATH` es el FICHERO (.vst3/x86_64-win/NEURONiK.vst3), no el
     *  directorio, y sin embargo el manifest vive en la raiz del bundle, dos
     *  niveles por encima. Se sube buscando el directorio terminado en ".vst3" en
     *  vez de contar niveles: contar niveles ata el test a la forma exacta de la
     *  ruta que le pase quien lo invoque, y el propósito de `argv[1]` es que
     *  corra contra una copia YA INSTALADA, cuya ruta es distinta.
     */
    juce::File bundleRootOf (const juce::File& path)
    {
        for (auto dir = path; dir != juce::File(); dir = dir.getParentDirectory())
            // `isDirectory()` importa: el BINARIO tambien se llama "NEURONiK.vst3",
            // y sin esta comprobacion la busqueda se pararia en el y devolveria el
            // fichero —donde no hay Contents/— en vez de subir al bundle.
            if (dir.isDirectory() && dir.getFileName().endsWith (".vst3"))
                return dir;

        return path;
    }

    /** One sentence on the first id that differs, so a failure is actionable. */
    juce::String describeFirstDifference (const juce::StringArray& expected,
                                          const juce::StringArray& actual)
    {
        for (const auto& id : expected)
            if (! actual.contains (id))
                return "falta " + id;

        for (const auto& id : actual)
            if (! expected.contains (id))
                return "sobra " + id;

        return {};
    }
}

int main (int argc, char* argv[])
{
    // GUI initialiser, not the core one: scanning a VST3 on Windows opens the
    // module through COM and JUCE's VST3 format wants a message manager that can
    // own a hidden window.
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // argv[1] lets the check run against an installed copy (a VST3 that works from
    // the build tree can still fail once it has been copied into a VST3 folder).
    const auto bundlePath = juce::String (argc > 1 ? argv[1] : NEURONIK_VST3_PATH);

    std::cout << "VST3 scan: " << bundlePath << '\n';

    const juce::File bundle (bundlePath);
    check (bundle.exists(), "el bundle existe en disco");

    if (! bundle.exists())
    {
        std::cout << "\nChecks failed (no bundle to scan).\n";
        return 1;
    }

    // ---- SCAN: the format has to recognise the file and describe the plugin.
    juce::AudioPluginFormatManager formatManager;
    formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::AudioPluginFormat* vst3Format = nullptr;

    for (auto* format : formatManager.getFormats())
        if (format != nullptr && format->getName() == "VST3"
             && format->fileMightContainThisPluginType (bundle.getFullPathName()))
        {
            vst3Format = format;
            break;
        }

    check (vst3Format != nullptr, "el formato VST3 reconoce el bundle");

    if (vst3Format == nullptr)
    {
        std::cout << "\nChecks failed (the file is not a VST3 this JUCE can read).\n";
        return 1;
    }

    juce::OwnedArray<juce::PluginDescription> types;
    vst3Format->findAllTypesForFile (types, bundle.getFullPathName());

    check (types.size() == 1,
           "el scan describe 1 plugin (encontrados: " + juce::String (types.size()) + ")");

    if (types.size() < 1)
    {
        std::cout << "\nChecks failed (nothing described inside the bundle).\n";
        return 1;
    }

    const auto& description = *types[0];
    std::cout << "         nombre=" << description.name
              << " uid=" << description.uniqueId
              << " formato=" << description.pluginFormatName
              << " fabricante=" << description.manufacturerName << '\n';

    check (description.name.contains ("NEURONiK"),
           "el plugin se llama NEURONiK (" + description.name + ")");

    // El FABRICANTE. Se comprueba por dos razones, y la segunda es la que
    // importaba: "yourcompany" es el valor POR DEFECTO de JUCE cuando
    // `juce_add_plugin` no declara el fabricante, llego hasta las listas de
    // plugins de los usuarios, y cada DAW lo cachea. Un simple "no esta vacio"
    // lo habria dado por bueno durante meses.
    check (description.manufacturerName == juce::String (NEURONIK_EXPECTED_MANUFACTURER)
               && description.manufacturerName.isNotEmpty(),
           "el plugin declara el fabricante esperado ("
               + description.manufacturerName + ", esperado "
               + juce::String (NEURONIK_EXPECTED_MANUFACTURER) + ")");

    check (! description.manufacturerName.containsIgnoreCase ("yourcompany"),
           "el fabricante NO es el valor por defecto de JUCE");

    // El codigo de fabricante NO cambia con el nombre: es lo que identifican los
    // DAWs, y cambiarlo haria que el plugin pareciera otro a quien ya lo tiene.
    // No se puede leer de `PluginDescription` (JUCE 8 no lo expone ahi), asi que
    // se comprueba donde el DAW lo busca de verdad: el moduleinfo.json del bundle.
    {
        const auto bundleRoot = bundleRootOf (bundle);
        juce::File moduleInfo (bundleRoot.getChildFile ("Contents/Resources/moduleinfo.json"));

        if (! moduleInfo.existsAsFile())
        {
            // En Windows el helper de JUCE escribe el manifest en Contents/ sin
            // Resources/, y mover el bundle entre carpetas lo puede dejar en uno u
            // otro. Se miran los dos antes de declarar que falta.
            const juce::File flat (bundleRoot.getChildFile ("Contents/moduleinfo.json"));

            if (flat.existsAsFile())
            {
                std::cout << "         (moduleinfo.json en Contents/, no en Resources/)\n";
                moduleInfo = flat;
            }
        }

        check (moduleInfo.existsAsFile(), "el bundle trae su moduleinfo.json");

        if (moduleInfo.existsAsFile())
        {
            // OJO: este manifest NO es JSON valido aunque se llame asi —el helper
            // de JUCE escribe comas colgantes y sin comillas en "Classes"— asi que
            // ni `parseXML` ni un parser de JSON lo leen. Se busca el texto de cada
            // `"Vendor"` a mano, que es justo lo que interesa comprobar.
            const auto text = moduleInfo.loadFileAsString();
            const auto expected = juce::String (NEURONIK_EXPECTED_MANUFACTURER);

            // El manifest de JUCE no lleva etiqueta-raíz con nombre: se
            // identifica por sus secciones ("Factory Info" y "Classes"). Sin
            // esto, un fichero equivocado con cero "Vendor" pasaria el recuento
            // de la linea siguiente por estar vacio.
            check (text.contains ("Factory Info") && text.contains ("Classes"),
                   "el manifest tiene las secciones que lo identifican");

            // TODOS los "Vendor", no solo el primero: el DAW lee el de cada clase,
            // y basta con que una se quede en "yourcompany" para que el plugin
            // aparezca en su lista con el nombre viejo. El recuento va en el
            // mensaje porque "ninguno coincide" sin decir cuantos habia no
            // distingue un manifiesto vacio de un fallo de verdad.
            auto vendorTotal = 0;
            auto wrongVendor = 0;
            juce::String firstWrong;

            // Se recorre linea a linea en vez de buscar offsets: cada "Vendor"
            // ocupa una linea entera (`"Vendor": "ABD Neural Audio",`) y asi el
            // fallo se puede señalar con la linea que lo causa, en vez de un
            // indice dentro de un texto plano que nadie va a leer. La linea
            // se recorta con la comilla como separador, no con un tokenizado
            // que se las COMIERA (y dejaria el valor sin sus comillas).
            for (const auto& line : juce::StringArray::fromTokens (text, "\n", "\r"))
            {
                const auto colon = line.indexOf (":");

                if (colon < 0 || ! line.substring (0, colon).contains ("Vendor"))
                    continue;

                ++vendorTotal;

                // El valor es lo que va entre la primera y la segunda comilla
                // despues de los dos puntos; la coma final se descarta al
                // recortar, asi que no hay que limpiarla a mano.
                const auto tail = line.substring (colon + 1);
                const auto openQuote = tail.indexOf ("\"");
                const auto closeQuote = tail.indexOf (openQuote + 1, "\"");

                const auto value = openQuote >= 0 && closeQuote > openQuote
                                       ? tail.substring (openQuote + 1, closeQuote).trim()
                                       : juce::String();

                if (value == expected)
                    continue;

                ++wrongVendor;

                if (firstWrong.isEmpty())
                    firstWrong = value;
            }

            std::cout << "         moduleinfo: " << (vendorTotal - wrongVendor) << '/'
                      << vendorTotal << " Vendor con el nombre esperado\n";

            check (vendorTotal > 0 && wrongVendor == 0,
                   "todos los Vendor del manifest llevan el nombre esperado ("
                       + juce::String (wrongVendor) + " de "
                       + juce::String (vendorTotal)
                       + (firstWrong.isEmpty() ? juce::String()
                                               : ", primero distinto: " + firstWrong) + ")");
        }
    }

    check (description.isInstrument,
           "el scan lo declara como instrumento (isInstrument)");

    // Los canales del SCAN no son los del plugin: se comprueban sobre la
    // instancia, que es lo que de verdad usa el host (0 aqui no significa nada).
    std::cout << "         canales que declara el scan: " << description.numInputChannels
              << " entrada / " << description.numOutputChannels << " salida\n";

    // ---- INSTANTIATE: the same call a host makes when it opens the plugin.
    juce::String loadError;
    auto scanned = formatManager.createPluginInstance (description, 48000.0, 512, loadError);

    check (scanned != nullptr,
           "el host instancia el plugin (createPluginInstance"
               + (loadError.isNotEmpty() ? ", error: " + loadError : juce::String()) + ")");

    if (scanned == nullptr)
    {
        std::cout << "\nChecks failed (the VST3 does not instantiate).\n";
        return 1;
    }

    std::cout << "         " << scanned->getName() << ", " << scanned->getNumPrograms()
              << " programa(s), " << scanned->getParameters().size()
              << " parametro(s) expuestos\n";

    check (scanned->getName().contains ("NEURONiK"),
           "el procesador instanciado es NEURONiK");
    check (scanned->getParameters().size() > 0, "expone parametros al host");

    // ---- PARAMETERS: the list on disk against the contract compiled today.
    auto expectedLayout = createLayoutApvts();
    auto& expectedApvts = *expectedLayout.apvts;

    const auto expectedIds = idsInLayout (expectedApvts);

    // The host ALSO gets entries that are NOT the plugin's, and they belong to
    // the wrapper, not to NEURONiK: the MIDI CC automation family ("MIDI CC
    // 15|126" and company) and the bypass parameter. Measured on the bundle built
    // today: 2080 + 1 over the 73 of the contract. They are filtered by NAME, not
    // by index or by count, so if the plugin ever published an entry called
    // "Bypass" it would come out as a difference instead of hiding behind the
    // filter.
    const auto midiCcPrefix = juce::String ("MIDI CC");
    const juce::StringArray wrapperEntries { "Bypass" };

    juce::StringArray scannedNames;
    auto midiCcEntries = 0;
    juce::StringArray filteredOut;

    for (auto* parameter : scanned->getParameters())
    {
        if (parameter == nullptr)
            continue;

        const auto name = parameter->getName (512).trim();

        if (name.startsWith (midiCcPrefix))
        {
            ++midiCcEntries;
            continue;
        }

        if (wrapperEntries.contains (name))
        {
            filteredOut.add (name);
            continue;
        }

        scannedNames.add (name);
    }

    juce::StringArray expectedNames;
    juce::String levelName;

    // El APVTS no expone una lista de ids: sale del estado (los hijos PARAM), que
    // es la misma fuente que usa el resto de la suite.
    for (const auto& id : expectedIds)
    {
        if (auto* parameter = expectedApvts.getParameter (id))
        {
            const auto name = parameter->getName (512).trim();
            expectedNames.add (name);

            if (name.equalsIgnoreCase ("master level"))
                levelName = name;
        }
    }

    std::cout << "         contrato: " << expectedNames.size() << " parametros ("
              << expectedIds.size() << " ids) | VST3: " << scannedNames.size()
              << " del plugin + " << midiCcEntries << " de automatizacion MIDI CC + "
              << filteredOut.size() << " del envoltorio ("
              << (filteredOut.isEmpty() ? juce::String() : filteredOut.joinIntoString (", "))
              << ")\n";

    check (scannedNames.size() == expectedNames.size(),
           "el VST3 expone tantos parametros del plugin como el contrato ("
               + juce::String (scannedNames.size()) + " vs "
               + juce::String (expectedNames.size()) + ")");

    const auto difference = describeFirstDifference (expectedNames, scannedNames);
    check (difference.isEmpty(),
           difference.isEmpty() ? juce::String ("los nombres coinciden uno a uno")
                                : "difieren: " + difference);

    // Every exposed parameter needs a name: an empty one is a blank cell in the
    // DAW's automation list, and it is invisible in a count.
    auto nameless = 0;

    for (const auto& name : scannedNames)
        if (name.isEmpty() || name.equalsIgnoreCase ("parameter"))
            ++nameless;

    check (nameless == 0,
           "ningun parametro expuesto llega al host sin nombre util ("
               + juce::String (nameless) + " sin)");

    // ---- ALIVE: a parameter is not a label. Move one through the SAME object the
    // host automates (setValueNotifyingHost, normalised) and read it back.
    //
    // El estado NO sirve para esto: por el borde del VST3 el estado del procesador
    // llega como blob opaco (el envoltorio lo envuelve), no como el arbol de
    // PARAM del APVTS. Por eso la lista de IDs se contrasta por NOMBRE, que es lo
    // que el host ve de verdad, y el movimiento se mide por el parametro.
    juce::AudioProcessorParameter* levelParameter = nullptr;

    for (auto* parameter : scanned->getParameters())
        if (parameter != nullptr && levelName.isNotEmpty()
             && parameter->getName (512).trim() == levelName
             && ! parameter->getName (512).trim().startsWith (midiCcPrefix))
        {
            levelParameter = parameter;
            break;
        }

    check (levelParameter != nullptr,
           "el host recibe el parametro \"" + levelName + "\" como automatizable");

    if (levelParameter != nullptr)
    {
        const auto before = levelParameter->getValue();
        constexpr float wanted = 0.25f;

        levelParameter->setValueNotifyingHost (wanted);
        const auto after = levelParameter->getValue();

        check (std::abs (after - wanted) < 0.01f,
               "el host escribe y relee " + levelName + " (" + juce::String (before, 3)
                   + " -> " + juce::String (after, 3) + ", pedido " + juce::String (wanted, 2) + ")");

        // STATE: what a DAW does when it saves the project. Round trip the opaque
        // blob the wrapper handed us and check the value survived it.
        juce::MemoryBlock block;
        scanned->getStateInformation (block);

        check (block.getSize() > 0,
               "el VST3 entrega estado al host (" + juce::String (block.getSize()) + " bytes)");

        levelParameter->setValueNotifyingHost (wanted);
        scanned->setStateInformation (block.getData(), (int) block.getSize());

        check (std::abs (levelParameter->getValue() - wanted) < 0.01f,
               "el estado del host devuelve el valor del parametro ("
                   + juce::String (levelParameter->getValue(), 3) + ")");

        levelParameter->setValueNotifyingHost (before);
    }

    // ---- READY: instantiated is not the same as runnable.
    scanned->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < 512; ++i)
        for (int ch = 0; ch < 2; ++ch)
            buffer.setSample (ch, i, 0.0f);

    scanned->processBlock (buffer, midi);

    check (true, "prepareToPlay + un bloque de silencio sin crashear ("
                     + juce::String (buffer.getNumChannels()) + " canales, "
                     + juce::String (buffer.getNumSamples()) + " muestras)");

    // A block full of NaN is what a host shows as a silent or screaming track,
    // and a count of parameters would never have noticed.
    auto nonFinite = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                ++nonFinite;

    check (nonFinite == 0,
           "el bloque de salida no trae NaN ni infinito (" + juce::String (nonFinite) + ")");

    scanned->releaseResources();
    scanned.reset();

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.")
              << " (" << failures << " fallo(s))\n";

    return failures == 0 ? 0 : 1;
}
