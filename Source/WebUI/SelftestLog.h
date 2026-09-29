/**
 * @file SelftestLog.h
 * @brief Donde cae el TRANSCRIPT del selftest, compartida por las dos superficies.
 *
 * Que pasa: el arnés (BridgeSelftest) escribe sus lineas por un `logFn` que cada
 * superficie construye a su medida. El plugin escribia en dos sitios —stdout y
 * un fichero acumulativo con marca de tiempo— y la bancada solo en stdout. Esa
 * asimetria costaba: al fallar una pasada de la bancada no habia nada que
 * comparar con la anterior, porque su transcript se reescribia desde cero cada
 * vez (lo hace el `> fichero` de build.bat) y no llevaba ni una marca de tiempo.
 *
 * Por que un log ACUMULATIVO y no uno por pasada: el fallo que importa casi
 * nunca es "el arnés va a la direccion equivocada", sino "esta tarde tardó 14 s
 * en arrancar y se le acabaron los 10 s de presupuesto". Eso solo se ve al
 * juxtaponer dos pasadas, y para juxtaponerlas hace falta que las dos esten en
 * el mismo fichero. Ademas el resumen (`Scripts/selftest_summary.ps1`) ya sabia
 * leer un log acumulativo, pero solo para el plugin: por eso tenia un modo
 * `stdout` entero, con reglas distintas, solo para la bancada.
 *
 * Por que una cabecera de corrida: `veredicto:` CIERRA una pasada, pero sin una
 * linea que la ABRA, comparar dos logs quiere decir diff de bloques sin poder
 * decir en que punto empieza cada uno. La cabecera lleva la hora de arranque y
 * por qué corre la pasada.
 *
 * Por que el formato de la marca de tiempo no se toca: `selftest_summary.ps1`
 * la parsea con `ParseExact(..., 'd MMM yyyy h:mm:sstt', ...)` para decidir si
 * la ultima corrida del log es vieja. Es la MISMA llamada de JUCE que hacia el
 * plugin (`toString (true, true)`), y por eso se queda aqui en vez de
 * "mejorarse": cambiarla haria que el resumen dejara de avisar de un log rancio
 * sin decir por que.
 *
 * Por que una variable de entorno POR SUPERFICIE y no una comun: si las dos
 * escribieran en el mismo fichero, el resumen del plugin acabaria resumiendo la
 * corrida de la bancada (lee la ULTIMA cerrada) y viceversa. Un log mezclado
 * daria dos veredictos que nadie pidio y ninguno serviria.
 *
 * Esta cabecera es la SSOT del formato. Cada superficie decide cuando escribir,
 * no como: si el plugin y la bancada compiten por el formato, el dia que uno
 * cambie el otro seguira escribiendo en el suyo y la comparacion dejara de
 * tener sentido en silencio.
 */

#pragma once

#include <juce_core/juce_core.h>

#include <iostream>

namespace NEURONiK::WebUI::SelftestLog
{
    /** Una superficie, con su variable de entorno y su fichero por defecto. */
    struct Surface
    {
        const char* environmentVariable;
        const char* defaultFileName;
    };

    /** El plugin (Standalone o dentro de un DAW). */
    inline const Surface& plugin()
    {
        static const Surface surface { "NEURONIK_SELFTEST_LOG", "neuronik-selftest.log" };
        return surface;
    }

    /** La bancada (WebPilotHost), que sirve la MISMA pagina por su cuenta. */
    inline const Surface& bench()
    {
        static const Surface surface { "NEURONIK_SELFTEST_LOG_BANCADA",
                                       "neuronik-selftest-bancada.log" };
        return surface;
    }

    /**
     * @brief Donde escribe esta superficie.
     * @details La variable de entorno manda si esta puesta. Por defecto NO se
     *          escribe junto al ejecutable: dentro de un DAW ese ejecutable es
     *          el del host (y su carpeta es de otra aplicacion), asi que el
     *          sitio honesto son los datos de usuario del sistema.
     */
    inline juce::File fileFor (const Surface& surface)
    {
        const auto fromEnvironment =
            juce::SystemStats::getEnvironmentVariable (surface.environmentVariable, {});

        if (fromEnvironment.isNotEmpty())
            return juce::File (fromEnvironment);

        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NEURONiK")
                   .getChildFile (surface.defaultFileName);
    }

    /** @brief La marca de tiempo; el formato que el resumen sabe leer. */
    inline juce::String stamp()
    {
        return juce::Time::getCurrentTime().toString (true, true);
    }

    /** @brief Una linea al stdout del proceso y al log, con marca de tiempo. */
    inline void write (const Surface& surface, const juce::String& line)
    {
        std::cout << line << std::endl;

        const auto file = fileFor (surface);

        file.getParentDirectory().createDirectory();
        file.appendText (stamp() + "  " + line + juce::newLine, false, false, nullptr);
    }

    /**
     * @brief La cabecera que ABRA una pasada, para que dos logs se comparen.
     * @param note Por que corre: el plugin dice si vino por `--selftest` o por la
     *             variable de entorno, y la bancada dice lo mismo. Sin esto, dos
     *             pasadas en el mismo log son indistinguibles salvo por la hora.
     */
    inline void beginRun (const Surface& surface, const juce::String& note)
    {
        // El separador va SIN el prefijo "[selftest] " a proposito: el resumen
        // descarta las lineas que no lo llevan, asi que la cabecera se ve
        // comparando el fichero y no altera el recuento de direcciones.
        const auto header = juce::String ("================ CORRIDA ") + stamp() + "  "
                              + note + " ================";

        std::cout << header << std::endl;

        const auto file = fileFor (surface);

        file.getParentDirectory().createDirectory();
        file.appendText (header + juce::newLine, false, false, nullptr);
    }
} // namespace NEURONiK::WebUI::SelftestLog
