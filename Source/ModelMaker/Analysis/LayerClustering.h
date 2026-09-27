/*
  ==============================================================================

    LayerClustering.h
    Created: 25 Sep 2026
    Description: FASE 11.2 — clustering de parciales por FORMA DE ENVOLVENTE.

                 Separa los indices de rejilla (n = 1..64 de la serie n*f0) en
                 CAPAS cuyo rasgo comun es la FORMA TEMPORAL de su envolvente,
                 no su magnitud ni su posicion en el espectro. Es el mecanismo
                 offline del analisis temporal (SpectralAnalyzer::analyzeTemporal)
                 para material con varias capas espectrales sobre UNA rejilla
                 (caso real: CZ-RRISE). Diseno completo:
                 DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD, seccion 3.

                 Es un modulo PURO: sin JUCE audio, sin estado y determinista
                 (misma entrada => misma salida), para que el test de trazas
                 sinteticas sea la bateria de no-regresion del algoritmo.

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "../../../Source/Common/SpectralModel.h"

namespace NEURONiK::ModelMaker::Analysis
{

/** Resultado de la separacion en capas + los descriptores medidos.
    El indice de capa 0 ocupa la RAIZ del modelo (compatibilidad v2: un lector
    viejo suena solo esa capa) y es la mas PERSISTENTE —soporte medio mas alto,
    y a igual soporte mas energia—: el drone es el cuerpo del sonido y la voz
    pasa por encima. Ver el paso 6 de clusterTraces y el test de CZ-RRISE. */
struct LayerClustering
{
    /** Parciales del modelo (SpectralModel::amplitudes). */
    static constexpr int kMaxTraces = 64;
    static constexpr int kMaxLayers = NEURONiK::Common::SpectralModel::kMaxLayers;
    static constexpr int kMaxFrames = NEURONiK::Common::SpectralModel::kMaxFrames;

    /** Suelo de actividad: por debajo, el frame no cuenta como soporte (es el
        mismo -60 dB de kPartialFloor del analizador). */
    static constexpr float kActivityFloor = 1.0e-3f;

    /** Un indice necesita energia en >= 2 frames PRODUCTIVOS para ser traza
        (con un solo frame no hay forma que comparar). */
    static constexpr int kMinActiveFrames = 2;

    /** Corte del dendrograma: dos grupos se fusionan mientras su afinidad
        media sea >= este valor (afinidad = 1 - distancia, ver abajo). */
    static constexpr float kAffinityCut = 0.55f;

    /** Corte PROPIO para EnvelopeCosine (2026-09-27, calibrado). Barrido
        0.10..0.90 sobre los DOS juegos del test: sinteticas (drone-voz
        0.49, voz-voz adyacente 0.44, disjuntas 0.00) y reales CZ-RRISE
        9 ventanas (drone-n7 0.69, drone-n15 0.55, n7-n15 0.09) NINGUNO
        separa ambos. Sinteticas piden corte >0.49 para no fusionar
        drone-voz Y <=0.44 para mantener la voz junta; reales piden
        >0.69 Y <=0.09: huecos INVERTIDOS y sin interseccion. La
        aglomeracion + clamp a 3 + guardia delgada colapsan el resto
        (medido: sinteticas 6->3->1, reales 2->1 a 0.55 y 3->1 a >=0.70).
        El valor 0.60 es el intento dedicado (centro entre 0.49 y 0.69)
        y TAMPOCO separa: sinteticas 1 capa, reales 1 capa. El coseno no
        falla por el corte sino por ceguera al SOPORTE (traza plana vs
        parcial). Ver plan sec 3.3 y test seccion E. */
    static constexpr float kEnvelopeCosineCut = 0.60f;

    /** Guardia de degeneracion (plan seccion 3.7). */
    static constexpr int   kMinLayerTraces = 2;
    static constexpr float kMinLayerEnergy = 0.10f;

    /** Persistencia por debajo de la cual una capa floja es ADEMAS episodica
        (y por tanto reabsorbible). "Floja" sola no basta: con la voz 20x mas
        fuerte que el drone (test de robustez), el drone baja del 10 % de la
        energia y una guardia puramente energetica lo reabsorbe en la voz — la
        capa 0 pasaria a ser el barrido y un lector v2 viejo (que oye la raiz)
        oiria justo lo que no es el cuerpo del sonido, que es la razon de ser
        del orden por persistencia. Una capa floja pero que suena en TODO el
        fichero ES el fondo: se queda, y su nivel vive en las amplitudes
        medidas (la mezcla es fiel).

        El valor lo fijan las dos medidas del test de trazas: la capa
        reabsorbible del caso "floja" son DOS bumps de 0.02 en 4 de 8 frames
        (soporte 0.50) y el drone que debe sobrevivir al x20 suena en 8 de 8
        (soporte 1.00); el corte tiene que caer entre las dos, y 0.75 es la
        linea de "el cuerpo del sonido" (menos de tres cuartos del fichero es
        un tramo, no un fondo). */
    static constexpr float kPersistentSupport = 0.75f;

    int layerCount = 1;
    std::array<int, kMaxTraces> layerOfTrace {};      // por parcial: capa (0 = inactivo)
    std::array<float, kMaxLayers> layerEnergy {};     // suma de amplitudes de la capa
    std::array<int, kMaxLayers> layerTraces {};       // parciales activos de la capa

    // Descriptores medidos por traza (los publica la UI/sonda 11.4/11.5 y los
    // comprueba el test de trazas sinteticas; no deciden nada por si solos).
    std::array<bool, kMaxTraces> active {};
    std::array<float, kMaxTraces> supportFraction {}; // frames activos / frames totales
    std::array<float, kMaxTraces> entropy {};         // entropia temporal normalizada 0..1
};

/** METRICA de afinidad del clustering (2026-09-26). El plan (seccion 3.3)
    pide el COSENO entre envolventes; el modulo usa DESCRIPTORES por defecto
    por una razon MEDIDA y no estetica, pero la letra del plan queda
    SELECCIONABLE aqui —vive en el modulo, no en el llamador— para poder
    medirla, compararla y usarla donde si discrimina.

    Que cambia cada una, en una linea:
      - Descriptors: 1 - distancia en el plano (fraccion de soporte, entropia
        temporal). Mira CUANTO y COMO reparte la energia la traza. Invariante a
        la posicion temporal: dos crestas en ventanas distintas (los armonicos
        que migran) caen en la MISMA capa.
      - EnvelopeCosine: el coseno de las dos envolventes. Ciego a la magnitud
        pero tambien al SOPORTE: una traza plana (el drone) correlaciona con
        cualquier traza parcial, y dos trazas de soportes DISJUNTOS salen con
        coseno 0 (por eso el coseno separa bien n7 de n15 y mal al drone).

    Las DOS caras estan MEDIDAS en el test (seccion E), y el coseno pierde en
    las dos: sobre las trazas sinteticas del criterio de aceptacion (la voz
    ocupa 2 de 8 frames) su afinidad con el drone (0.49) NO llega al corte, no
    fusiona nada, y el clamp a 3 + la guardia de degeneracion colapsan el
    resultado en 1 capa igualmente; sobre las trazas REALES de 9 ventanas del
    plan fusiona de MAS (drone y n7 dan 0.69 >= 0.55) y tambien da 1 capa. El
    audio de punta a punta con el coseno: 1 capa (con descriptores, 2). El
    coseno no falla por su forma —separa n7 de n15, de soportes disjuntos— sino
    por su ceguera al SOPORTE: el drone, plano, correlaciona con todo lo que
    dure parte del fichero, y lo que no correlaciona tampoco llega al corte.

    CALIBRACION 2026-09-27 (corte propio): barrido 0.10..0.90 en ambos juegos
    con su corte DEDICADO (kEnvelopeCosineCut = 0.60). Resultado NEGATIVO:
    NINGUN corte separa drone/voz en los DOS juegos a la vez —sinteticas
    0.49 vs 0.44 y reales 0.69 vs 0.09 piden huecos invertidos sin
    interseccion—, con el dedicado sinteticas 1 capa y reales 1 capa
    (descriptores dan 2 y 2). El coseno queda disponible pero NO es el
    defecto (plan sec 3.3, sec 3 para el dedicado). */
enum class LayerMetric
{
    Descriptors,     // por defecto: 1 - distancia en (soporte, entropia)
    EnvelopeCosine   // la metrica literal del plan, seccion 3.3
};

/** Afinidad COSENO entre dos trazas temporales: la metrica literal del plan
    (seccion 3.3). Se expone para poder MEDIR que no es la que decide por
    defecto, y para ELEGIRLA (LayerMetric::EnvelopeCosine): una traza
    plana (drone) correlaciona con cualquier traza de soporte parcial, y en el
    RRISE real el drone y el armonico n7 dan 0.69 (dos capas distintas).
    Con su corte DEDICADO (kEnvelopeCosineCut = 0.60) sigue sin separar
    (plan sec 3.3, test E). */
inline float envelopeCosine (const float* a, const float* b, int numFrames)
{
    double dot = 0.0, na = 0.0, nb = 0.0;

    for (int f = 0; f < numFrames; ++f)
    {
        dot += (double) a[f] * (double) b[f];
        na  += (double) a[f] * (double) a[f];
        nb  += (double) b[f] * (double) b[f];
    }

    if (na <= 0.0 || nb <= 0.0)
        return 0.0f;

    return (float) std::clamp (dot / std::sqrt (na * nb), 0.0, 1.0);
}

/** Afinidad entre dos descriptores: 1 - distancia euclidea en el plano
    (fraccion de soporte, entropia temporal), las dos en [0,1].

    POR QUE NO EL COSENO: el coseno es ciego a la magnitud pero tambien al
    SOPORTE, asi que dos trazas que no comparten frames pueden salir parecidas
    (plan seccion 8: "subir el corte a 0.65" no lo arregla, lo empeora) y una
    traza plana correlaciona con todas. La forma que el plan describe —"el drone
    es una recta, la voz es una cresta que sube y muere"— se lee mejor en dos
    numeros que en una correlacion: CUANTO del fichero esta activo y COMO de
    repartida (entropia) esta la energia dentro de ese soporte. Ambos son
    invariantes a la posicion temporal, que es justo lo que hace falta: los
    armonicos de la voz del RRISE son crestas en ventanas DISTINTAS y aun asi
    forman UNA capa (el plan seccion 8: la migracion la lleva el emparejador
    por indice, no el clustering). */
inline float descriptorAffinity (const LayerClustering& c, int traceA, int traceB)
{
    const float ds = c.supportFraction[(size_t) traceA] - c.supportFraction[(size_t) traceB];
    const float dh = c.entropy[(size_t) traceA]         - c.entropy[(size_t) traceB];

    // EL SOPORTE PESA MAS QUE LA ENTROPIA (kSupportWeight). Con los dos ejes a
    // peso 1 las trazas REALES del CZ-RRISE no se separan por poco: el drone
    // n1 (1.00, 0.97) contra el grupo de voz {n7, n15} (0.56, 0.79)/(0.67,
    // 0.76) da distancias 0.48 y 0.39, y la afinidad MEDIA del grupo contra el
    // drone sale 0.56: justo por encima del corte, asi que el drone se
    // absorbe. Pesar el soporte x2 —"sonar todo el fichero o sonar un tramo
    // es lo que el oido oye; la entropia solo matiza como se reparte la
    // energia dentro del tramo"— deja esa afinidad media en 0.42 sin mover el
    // resto de repartos (el test de trazas sinteticas es la medida).
    constexpr float kSupportWeight = 2.0f;

    return std::clamp (1.0f - std::sqrt (kSupportWeight * ds * ds + dh * dh), 0.0f, 1.0f);
}

/**
 * Separa los parciales en capas por forma de envolvente.

 * @param traces  matriz row-major traza-mayor: traces[t * numFrames + f] es la
 *                amplitud del parcial t en el frame f (ya normalizada y con el
 *                suelo aplicado por el llamador).
 * @param numTraces  parciales (clampeado a kMaxTraces).
 * @param numFrames  frames de la pelicula (clampeado a kMaxFrames).
 * @param floor      suelo de actividad por frame (por defecto kActivityFloor).
 * @param metric     afinidad con la que se fusionan las trazas (ver LayerMetric).

 * Camino: descriptores -> aglomerativo por afinidad media -> medoide (colapsa
 * las cadenas de single-linkage sin volver a cortar) -> clamp a kMaxLayers ->
 * guardia de degeneracion -> orden (la capa 0 es la mas persistente).
 *
 * Sin eje temporal (numFrames < kMinActiveFrames) o con menos de 2 trazas
 * activas devuelve 1 capa: el modelo estatico NUNCA se parte (asi el camino
 * de una sola capa queda bit a bit como estaba).
 */
inline LayerClustering clusterTraces (const float* traces, int numTraces, int numFrames,
                                      float floor = LayerClustering::kActivityFloor,
                                      LayerMetric metric = LayerMetric::Descriptors)
{
    LayerClustering out;
    out.layerCount = 1;
    out.layerOfTrace.fill (0);

    const int n      = std::clamp (numTraces, 0, LayerClustering::kMaxTraces);
    const int frames = std::clamp (numFrames, 0, LayerClustering::kMaxFrames);

    if (traces == nullptr || n <= 0 || frames < LayerClustering::kMinActiveFrames)
        return out;

    // 1. Descriptores + actividad -------------------------------------------
    std::vector<int> activeIndex;
    activeIndex.reserve ((size_t) n);

    for (int t = 0; t < n; ++t)
    {
        const float* tr = traces + (size_t) t * (size_t) frames;
        int support = 0;
        double sum = 0.0;

        for (int f = 0; f < frames; ++f)
            if (tr[f] >= floor)
            {
                ++support;
                sum += (double) tr[f];
            }

        out.supportFraction[(size_t) t] = (float) support / (float) frames;

        float h = 0.0f;

        if (support > 1 && sum > 0.0)
        {
            double acc = 0.0;

            for (int f = 0; f < frames; ++f)
                if (tr[f] >= floor)
                {
                    const double p = (double) tr[f] / sum;
                    acc -= p * std::log (p);
                }

            h = (float) (acc / std::log ((double) support));
        }

        out.entropy[(size_t) t] = h;
        out.active[(size_t) t]  = support >= LayerClustering::kMinActiveFrames;

        if (out.active[(size_t) t])
            activeIndex.push_back (t);
    }

    const int m = (int) activeIndex.size();

    if (m < LayerClustering::kMinLayerTraces)
        return out;

    // La metrica se elige AQUI y no en el llamador: todo el camino (media,
    // medoide, orden) consume esta lambda, asi que cambiar de metrica no toca
    // ni una linea del resto del algoritmo.
    const auto affinity = [&out, &activeIndex, traces, frames, metric] (int i, int j)
    {
        const int ta = activeIndex[(size_t) i];
        const int tb = activeIndex[(size_t) j];

        if (metric == LayerMetric::EnvelopeCosine)
            return envelopeCosine (traces + (size_t) ta * (size_t) frames,
                                   traces + (size_t) tb * (size_t) frames, frames);

        return descriptorAffinity (out, ta, tb);
    };

    const auto traceEnergy = [traces, frames, &activeIndex] (int i)
    {
        const float* tr = traces + (size_t) activeIndex[(size_t) i] * (size_t) frames;
        double e = 0.0;

        for (int f = 0; f < frames; ++f)
            e += (double) tr[f];

        return e;
    };

    // 2. Aglomerativo (afinidad MEDIA) hasta bajar del corte ----------------
    std::vector<std::vector<int>> clusters ((size_t) m);

    for (int i = 0; i < m; ++i)
        clusters[(size_t) i] = { i };

    const auto averageAffinity = [&affinity] (const std::vector<int>& a, const std::vector<int>& b)
    {
        double acc = 0.0;
        int count = 0;

        for (int i : a)
            for (int j : b)
            {
                acc += (double) affinity (i, j);
                ++count;
            }

        return count > 0 ? (float) (acc / (double) count) : 0.0f;
    };

    const auto mergeBestPair = [&clusters, &averageAffinity] (float cut) -> bool
    {
        float best = -1.0f;
        int bi = -1, bj = -1;

        for (size_t i = 0; i < clusters.size(); ++i)
            for (size_t j = i + 1; j < clusters.size(); ++j)
            {
                const float a = averageAffinity (clusters[i], clusters[j]);

                if (a > best)
                {
                    best = a;
                    bi = (int) i;
                    bj = (int) j;
                }
            }

        if (bi < 0 || best < cut)
            return false;

        clusters[(size_t) bi].insert (clusters[(size_t) bi].end(),
                                      clusters[(size_t) bj].begin(),
                                      clusters[(size_t) bj].end());
        clusters.erase (clusters.begin() + bj);
        return true;
    };

    const float cut = (metric == LayerMetric::EnvelopeCosine ? LayerClustering::kEnvelopeCosineCut : LayerClustering::kAffinityCut);
    while (mergeBestPair (cut)) {}

    // 3. Medoide: colapsa las cadenas del single-linkage sin volver a cortar -
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        std::vector<int> medoid (clusters.size(), 0);

        for (size_t c = 0; c < clusters.size(); ++c)
        {
            float bestSum = -1.0f;

            for (int member : clusters[c])
            {
                double acc = 0.0;

                for (int other : clusters[c])
                    acc += (double) affinity (member, other);

                if ((float) acc > bestSum)
                {
                    bestSum = (float) acc;
                    medoid[c] = member;
                }
            }
        }

        std::vector<std::vector<int>> next (clusters.size());
        bool moved = false;

        for (size_t c = 0; c < clusters.size(); ++c)
            for (int member : clusters[c])
            {
                float best = -1.0f;
                int bestCluster = (int) c;

                for (size_t d = 0; d < clusters.size(); ++d)
                {
                    const float a = affinity (member, medoid[d]);

                    if (a > best)
                    {
                        best = a;
                        bestCluster = (int) d;
                    }
                }

                next[(size_t) bestCluster].push_back (member);
                moved = moved || bestCluster != (int) c;
            }

        clusters.clear();

        for (auto& c : next)
            if (! c.empty())
                clusters.push_back (c);

        if (! moved)
            break;
    }

    // 4. Clamp a kMaxLayers (la aglomeracion sigue, sin corte) --------------
    while (clusters.size() > (size_t) LayerClustering::kMaxLayers)
        if (! mergeBestPair (-1.0f))
            break;

    // 5. Guardia de degeneracion: la capa sin soporte se reabsorbe -----------
    std::vector<std::pair<double, std::vector<int>>> ordered;

    for (auto& c : clusters)
    {
        double e = 0.0;

        for (int i : c)
            e += traceEnergy (i);

        ordered.emplace_back (e, c);
    }

    const auto sortByEnergy = [&ordered]
    {
        std::stable_sort (ordered.begin(), ordered.end(),
                          [] (const auto& a, const auto& b) { return a.first > b.first; });
    };

    sortByEnergy();

    /** Soporte medio de un grupo (se usa en la guardia y en el orden final). */
    const auto meanSupport = [&out, &activeIndex] (const std::vector<int>& cluster)
    {
        double acc = 0.0;

        for (int i : cluster)
            acc += (double) out.supportFraction[(size_t) activeIndex[(size_t) i]];

        return cluster.empty() ? 0.0 : acc / (double) cluster.size();
    };

    if (! ordered.empty() && (int) ordered.front().second.size() < LayerClustering::kMinLayerTraces)
        return out;   // la dominante no tiene soporte: no hay capas que separar

    for (bool changed = true; changed && ordered.size() > 1; )
    {
        changed = false;

        double total = 0.0;

        for (const auto& layer : ordered)
            total += layer.first;

        for (size_t c = 1; c < ordered.size() && ! changed; ++c)
        {
            const bool thin = (int) ordered[c].second.size() < LayerClustering::kMinLayerTraces;
            // Floja Y episodica (ver kPersistentSupport): una capa floja que
            // suena en todo el fichero es el fondo, no una degeneracion.
            const bool weak = ordered[c].first < (double) LayerClustering::kMinLayerEnergy * total
                              && meanSupport (ordered[c].second) < LayerClustering::kPersistentSupport;

            if (! (thin || weak))
                continue;

            // Al grupo MAS AFIN (no necesariamente la dominante: absorberse en la
            // dominante borraria justo la capa que el clustering acaba de ver).
            float best = -1.0f;
            size_t bestC = 0;

            for (size_t d = 0; d < ordered.size(); ++d)
            {
                if (d == c)
                    continue;

                const float a = averageAffinity (ordered[c].second, ordered[d].second);

                if (a > best)
                {
                    best = a;
                    bestC = d;
                }
            }

            ordered[bestC].second.insert (ordered[bestC].second.end(),
                                          ordered[c].second.begin(),
                                          ordered[c].second.end());
            ordered[bestC].first += ordered[c].first;
            ordered.erase (ordered.begin() + (long) c);
            changed = true;
        }

        sortByEnergy();
    }

    // 6. Orden de las capas: la 0 (la RAIZ, lo que oye un lector v2 viejo) es
    //    la mas PERSISTENTE —el drone es el cuerpo del sonido y la voz pasa por
    //    encima—; a igual soporte manda la energia. Con las trazas reales de
    //    CZ-RRISE la voz suma MAS energia que el drone (ver el test), asi que
    //    ordenar por energia pondria el barrido en la raiz.
    std::stable_sort (ordered.begin(), ordered.end(),
                      [&meanSupport] (const auto& a, const auto& b)
                      {
                          const double sa = meanSupport (a.second);
                          const double sb = meanSupport (b.second);

                          if (std::abs (sa - sb) > 1.0e-6)
                              return sa > sb;

                          return a.first > b.first;
                      });

    // 7. Resultado: capa 0 = la mas persistente ----------------------------
    out.layerCount = (int) ordered.size();

    for (int l = 0; l < out.layerCount; ++l)
    {
        out.layerEnergy[(size_t) l] = (float) ordered[(size_t) l].first;
        out.layerTraces[(size_t) l] = (int) ordered[(size_t) l].second.size();

        for (int i : ordered[(size_t) l].second)
            out.layerOfTrace[(size_t) activeIndex[(size_t) i]] = l;
    }

    return out;
}

/** Sobrecarga de conveniencia con un solo vector row-major. */
inline LayerClustering clusterTraces (const std::vector<float>& traces, int numTraces, int numFrames,
                                      float floor = LayerClustering::kActivityFloor,
                                      LayerMetric metric = LayerMetric::Descriptors)
{
    return clusterTraces (traces.data(), numTraces, numFrames, floor, metric);
}

} // namespace NEURONiK::ModelMaker::Analysis
