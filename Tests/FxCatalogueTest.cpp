/*
  ==============================================================================

    FxCatalogueTest.cpp
    El catalogo de efectos de NEURONiK: que motor se empareja con que id del
    vocabulario compartido.

    QUE COMPRUEBA Y POR QUE ESTE TEST HACE FALTA SI LAS DOS TABLAS SON IGUALES DE
    LARGO. `fxNeuronikIdentities()` y `fxDefaultCatalogue()` se unen por indice,
    asi que una fila insertada en una y no en la otra daria a un motor el nombre
    y la familia de otro, y el efecto se veria: un modulo con el tema del coro y
    los knobs de la reverberacion. Un test que solo comparase los recuentos no
    lo veria jamas — dos tablas del mismo tamano y distinta permutacion pasan
    cualquier comprobacion de recuento. Por eso aqui cada motor se comprueba
    contra SU id y SU familia, uno a uno, y no la tabla entera contra si misma.

    Y LA TERCERA COMPROBACION ES LA QUE DUDE: que las reservas esten declaradas
    como reservas. Una reserva es un id que este producto ocupa y el contrato
    compartido todavia no lista; si alguien la rellena con un efecto alineado sin
    tocar el contrato, la pagina empezaria a pintar un efecto con la familia de
    otro. Se comprueba que el numero de reservas y el de filas alineadas siguen
    cuadrando con lo que el catalogo tiene, para que el numero no se isntale solo.

    No enlaza JUCE: el modulo compartido es JUCE-free por contrato (ver
    `FxCatalogExport.h`), y este test es una de las razones por las que eso se
    sostiene — si un dia alguien mete aqui una cabecera de ABDEep, esto no compila
    y lo dice en el sitio donde duele.

  ==============================================================================
*/

#include "../Source/DSP/FxCatalogue.h"

#include <cstdio>
#include <cstring>

using namespace NEURONiK::DSP;

namespace
{

int failures = 0;

void check (bool condition, const char* what)
{
    if (! condition)
    {
        std::printf ("  [FAIL] %s\n", what);
        ++failures;
    }
}

void checkEquals (const char* actual, const char* expected, const char* what)
{
    if (actual == nullptr || expected == nullptr || std::strcmp (actual, expected) != 0)
    {
        std::printf ("  [FAIL] %s (esperado \"%s\", \"%s\")\n", what,
                     expected != nullptr ? expected : "(null)",
                     actual   != nullptr ? actual   : "(null)");
        ++failures;
    }
}

void checkEquals (int actual, int expected, const char* what)
{
    if (actual != expected)
    {
        std::printf ("  [FAIL] %s (esperado %d, %d)\n", what, expected, actual);
        ++failures;
    }
}

} // namespace

int main()
{
    std::printf ("FxCatalogue: %d efectos\n\n", fxNeuronikCatalogueSize());

    // --- 1. Cada motor, con SU id y SU familia ------------------------------
    // El emparejamiento por indice, motor a motor. El nombre tecnico es la
    // clave: es lo que `fxFindEffect` busca, asi que si la tabla de identidades
    // se permuta, el fallo aparece aqui y no en un modulo con otro tema.
    struct Expected { const char* technical; int id; const char* family; bool aligned; };

    const Expected expected[] = {
        { "chorus",      10, "chorus",     true  },
        { "delay",       13, "delay",      true  },
        { "reverb",      57, "reverb",     true  },
        { "saturation",  58, "distortion", true  },
        { "schroeder",   59, "reverb",     true  },
        { "bbd",         36, "chorus",     true  },
        { "shelf",       60, "filter",     true  },
        { "phaser",       9, "modulation", true  },
    };

    const int count = fxNeuronikCatalogueSize();
    const int esperado = static_cast<int> (sizeof (expected) / sizeof (expected[0]));
    checkEquals (esperado, count, "el catalogo tiene las filas que el test espera");

    // Y el bucle se queda en lo que este test sabe mirar. Antes iba a `count` a
    // pelo: al sixth motor --o al septimo, o al que sea-- se salia de la tabla
    // `expected` y se caia accessing un `const char*` de la nada. Un test que
    // se cae no dice nada, y ademas tapa el fallo de verdad, que es que
    // faltaba una fila. Asi que el desajuste se ve como un `[FAIL]` y el resto
    // del test sigue dando su resultado.
    const int comprobables = esperado < count ? esperado : count;

    for (int i = 0; i < comprobables; ++i)
    {
        const auto entry = fxNeuronikEffectAt (i + 1);

        check (entry.effect != nullptr, "la fila del motor existe");
        check (entry.identity != nullptr, "la identidad existe");

        if (entry.effect == nullptr || entry.identity == nullptr)
            continue;

        checkEquals (entry.effect->name, expected[i].technical, "el motor tecnico");
        checkEquals (entry.identity->id, expected[i].id, "el id del vocabulario");
        checkEquals (entry.identity->family, expected[i].family, "la familia del tema");

        if (static_cast<int> (entry.identity->aligned) != static_cast<int> (expected[i].aligned))
        {
            std::printf ("  [FAIL] %s esta alineado (esperado %d, %d)\n",
                         entry.effect->name, expected[i].aligned,
                         static_cast<int> (entry.identity->aligned));
            ++failures;
        }
    }

    // --- 2. El 0 es bypass, siempre -----------------------------------------
    // La misma regla que `fxEffectAt`: el panel puede elegir "nada" sin que
    // ningun producto tenga que acordarlo.
    check (fxNeuronikEffectAt (0).effect == nullptr, "el indice 0 es bypass");
    check (fxNeuronikEffectAt (0).identity == nullptr, "el indice 0 no tiene identidad");
    check (fxNeuronikEffectAt (count + 1).effect == nullptr, "un indice pasado es nullptr");
    check (fxNeuronikEffectAt (-3).effect == nullptr, "un indice negativo es nullptr");

    // --- 3. Los ids son UNICOS y ninguno es el 0 ---------------------------
    // Un id repetido haria que dos motores pintaran el mismo numero, que es
    // justo el descuido que el contrato compartido ya documenta una vez (la
    // lista de nombres de ABDEep). Con ocho filas se comprueba aqui y no se
    // deja a un test de la pagina.
    for (int i = 0; i < comprobables; ++i)
    {
        const auto a = fxNeuronikEffectAt (i + 1);

        if (a.identity == nullptr)
            continue;

        check (a.identity->id != 0, "ningun efecto toma el id 0 del bypass");

        for (int j = i + 1; j < count; ++j)
        {
            const auto b = fxNeuronikEffectAt (j + 1);

            if (b.identity == nullptr)
                continue;

            if (a.identity->id == b.identity->id)
            {
                std::printf ("  [FAIL] \"%s\" y \"%s\" comparten el id %d\n",
                             a.effect->name, b.effect->name, a.identity->id);
                ++failures;
            }
        }
    }

    // --- 4. El ancho de mandos lo pide el MOTOR ----------------------------
    // El catalogo de este producto pide 4 (el BBD, la reverberacion de FreeVerb
    // y el Schroeder). El `kFxMaxParams` del modulo compartido (12) es el
    // maximo que el motor ACEPTA, no el que este producto necesita, y un bus de
    // 12 seria ocho parametros muertos por hueco en cada preset.
    int widest = 0;

    for (int i = 1; i <= count; ++i)
    {
        const auto entry = fxNeuronikEffectAt (i);

        if (entry.effect != nullptr && entry.effect->numParams > widest)
            widest = entry.effect->numParams;
    }

    checkEquals (widest, 4, "el motor mas ancho pide 4 mandos");
    check (widest <= abd::dsp::kFxMaxParams,
           "el ancho del catalogo cabe en el bus del motor");

    // --- 5. Todo mando tiene tabla y nombre --------------------------------
    // Un `params` a nullptr con `numParams` > 0 haria que la pagina pintara
    // knobs sin nombre ni rango. El motor lo admite; la pagina no.
    for (int i = 1; i <= count; ++i)
    {
        const auto entry = fxNeuronikEffectAt (i);

        if (entry.effect == nullptr)
            continue;

        if (entry.effect->numParams > 0)
            check (entry.effect->params != nullptr, "el efecto con mandos tiene tabla");

        for (int p = 0; p < entry.effect->numParams && entry.effect->params != nullptr; ++p)
        {
            const auto& spec = entry.effect->params[p];

            check (spec.name != nullptr && spec.name[0] != 0, "el mando tiene nombre");
            check (spec.maxValue > spec.minValue, "el mando tiene rango con los dos extremos");
            check (spec.skew > 0.0f, "el sesgo del mando es positivo");
        }
    }

    // --- 6. Las reservas estan declaradas como reservas -------------------
    // Hoy las ocho filas tienen su motor en el contrato compartido, asi que no
    // queda ninguna reserva: las que habia (el 1, el 50 y el 22) se rellenaron
    // al entrar el 57, el 58 y el 59, y despues el 60 y el 9. El numero sigue
    // aqui a proposito y sigue siendo cero a proposito: si aparece un motor
    // nuevo sin fila en el contrato, este test cae y obliga a decidir si es una
    // fila o una reserva, en vez de que el numero se instale solo.
    int aligned = 0;
    int reserved = 0;

    for (int i = 1; i <= count; ++i)
    {
        const auto entry = fxNeuronikEffectAt (i);

        if (entry.identity == nullptr)
            continue;

        if (entry.identity->aligned)
            ++aligned;
        else
            ++reserved;
    }

    checkEquals (aligned, 8, "los ocho motores alineados con el contrato compartido");
    checkEquals (reserved, 0, "ningun id reservado (sin fila todavia)");

    std::printf ("\n%d alineados, %d reservados, bus de %d mandos\n", aligned, reserved, widest);

    if (failures > 0)
    {
        std::printf ("\nFxCatalogueTest: %d problema(s)\n", failures);
        return 1;
    }

    std::printf ("\nFxCatalogueTest: OK\n");
    return 0;
}
