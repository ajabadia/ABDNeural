/**
 * @file ColdStartWatch.h
 * @brief La politica de ARRANQUE EN FRIO, compartida por las dos superficies.
 *
 * Que pasa: WebView2, en frio (lo normal la primera vez que se abre el editor,
 * o tras un arranque del sistema con el disco ocupado), puede quedarse sin
 * pedir NINGUN recurso durante varios segundos. La pagina no aparece, el editor
 * se queda en blanco y el usuario —o el selftest— espera a un timeout sin
 * entender nada. La segunda superficie que sufria esto era la bancada, y su
 * remedio (volver a pedir la pagina cuando no hay ninguna senal en un rato) se
 * porta aqui para que el plugin tolere el mismo pico de carga.
 *
 * Por que NO reintenta siempre, y por que estos numeros: reintentar no es
 * gratis, la nueva navegacion cancela la que estaba en curso. Con la maquina
 * ocupada pero sana se midio el primer recurso a 7,1 s: un umbral de 6 s
 * reintentaba en una corrida que iba a cargar bien. A 10 s ese caso no se toca
 * y un arranque realmente atascado (nada servido en absoluto) sigue teniendo
 * tres oportunidades. El techo son 10 s x 3 = 30 s de arranque en el peor caso,
 * que es lo que hace falta para que un pico de carga se distinga de un cuelgue.
 *
 * Que se considera "senal": que la superficie haya servido AL MENOS un recurso,
 * o que la pagina haya anunciado estar lista. Deliberadamente NO cuentan
 * `readyState` ni "hay documento": una navegacion fallida deja igualmente un
 * documento en `interactive` (la pagina de error del navegador), asi que
 * contarlos hacia que el reintento ya habia funcionado y el proceso se rendia
 * en el plazo viejo. Medido en la bancada: con el proveedor sin servir nada, la
 * primera version daba el alta por buena a los 7 s y se rendia a los 28.
 *
 * Esta cabecera es la SSOT de la politica (los numeros y la decision). Cada
 * superficie decide COMO se reintenta —la bancada navega a la raiz del
 * proveedor, el plugin al mismo sitio por su via— porque eso es suyo; lo que no
 * puede ser suyo es el criterio, que si se duplicara acabaria siendo distinto en
 * cada una sin que nadie se entere.
 */

#pragma once

#include <algorithm>

namespace NEURONiK::WebUI::ColdStart
{

/** Sin ninguna senal durante este tiempo, se reintenta la navegacion. */
inline constexpr double stallMs = 10000.0;

/** Y como mucho estos reintentos: en el peor caso, 30 s de arranque. */
inline constexpr int maxRetries = 3;

/**
 * @brief El estado del vigilante. Uno por superficie, vivo todo el arranque.
 */
struct Watcher
{
    int retries = 0;          ///< reintentos ya gastados
    double lastSignalMs = 0.0; ///< cuando se vio la ultima senal (0 = ninguna)
};

/**
 * @brief Que hay que hacer en este tick, si algo.
 *
 * Devuelve `true` cuando toca reintentar la navegacion, y deja el estado ya
 * actualizado (reintentos gastados y reloj reiniciado) para que el llamante no
 * tenga que acordarse de esas dos lineas —que es justo donde dos copias de este
 * codigo acabarian divergiendo.
 *
 * @param watcher  estado vivo de esta superficie.
/// @param nowMs    reloj monótono en milisegundos.
/// @param hasSignal que la pagina ya dio señales de vida (un recurso servido, o
 *                  el anuncio de lista). Con señal, esta funcion no hace nada:
 *                  a partir de ahi el problema, si lo hay, no es de arranque.
/// @return true si el llamante debe volver a pedir la pagina.
 */
inline bool shouldRetry (Watcher& watcher, double nowMs, bool hasSignal) noexcept
{
    if (hasSignal)
    {
        watcher.lastSignalMs = nowMs;
        return false;
    }

    if (watcher.retries >= maxRetries)
        return false;

    // La primera llamada arranca con `lastSignalMs` a 0 y `nowMs()` tambien vale
    // 0 en un reloj que arranca con el proceso: sin este max, el primer tick
    // reintentaria en el instante cero, antes de que WebView2 haya podido pedir
    // nada. El `max` da al menos un `stallMs` de margen desde el primer tick real.
    if (std::max (nowMs, watcher.lastSignalMs) - watcher.lastSignalMs < stallMs)
        return false;

    ++watcher.retries;

    // El siguiente reintento espera OTRO stall entero desde ahora: si la
    // navegacion vuelve a perderse, no se reintenta en cadena.
    watcher.lastSignalMs = nowMs;

    return true;
}

} // namespace NEURONiK::WebUI::ColdStart
