# NEURONiK Handoff

> **Este documento es un registro corrido (append-only):** las entradas nuevas se añaden al final,
> fechadas. Las secciones temáticas de la primera mitad reflejan el arranque de la migración
> (**2026-09-16**) y varias quedaron superadas por decisiones posteriores —las marcadas como
> «histórico» y las que ya tienen entrada propia más abajo—. Estado vigente: la última entrada de
> este documento y la **Fase 8** de `ROADMAP.md`.
>
> El [índice navegable](#índice-navegable) de abajo agrupa las entradas por tema y enlaza las
> secciones canónicas; sin él, buscar cualquier cosa en este fichero es un `grep`.

## Índice (navegable)

Este fichero es un registro corrido de ~8.000 líneas y 172 entradas: este índice las agrupa por
tema para no tener que recorrerlo entero. Los enlaces apuntan a los encabezados reales del
fichero (anclas estándar de Markdown), así que navegan en cualquier visor. Las entradas marcadas
«histórico» en su propio texto reflejan el arranque de la migración y están superadas por las
posteriores.

**Regla de lectura:** lo vigente está en las secciones canónicas y en las últimas entradas;
lo de la primera mitad del fichero cuenta cómo se llegó, no qué es cierto hoy.

### Canon (vigente: leer esto primero)

- [SECCION CANONICA — las 15 direcciones del selftest: indice, AGUJA, barras ENV, medidor-PANIC y MIDI-CC](#seccion-canonica-las-15-direcciones-del-selftest-indice-aguja-barras-env-medidor-panic-y-midi-cc) — las 15 direcciones del arnés, con su índice de orden de corrida y todas las subsecciones
- [SECCION CANONICA — VOLVER (direccion 1b-bis): el retorno del salto ENV -> MATRIZ](#seccion-canonica-volver-direccion-1b-bis-el-retorno-del-salto-env---matriz) — VOLVER (1b-bis) en sección propia
- [2026-09-27 — PENDIENTES: todo lo que quedó abierto en esta sesion, con su dueño y su coste](#2026-09-27-pendientes-todo-lo-que-quedó-abierto-en-esta-sesion-con-su-dueño-y-su-coste) — pendientes que quedaron abiertos el 27/09, con dueño y coste

### Estado y punto de partida (2026-09-16/18, referencia)

- [Contexto](#contexto)
- [Repositorio](#repositorio)
- [Línea base compilada](#línea-base-compilada)
- [Prueba DSP actual](#prueba-dsp-actual)
- [Arquitectura actual relevante](#arquitectura-actual-relevante) — arquitectura relevante
- [Decisión arquitectónica actual](#decisión-arquitectónica-actual)
- [Frontera DSP creada](#frontera-dsp-creada)

### Construcción, pruebas y reglas de trabajo

- [Procedimiento de build](#procedimiento-de-build)
- [Reglas de comparación](#reglas-de-comparación)
- [Persistencia de estado (Fase 4 cerrada, 2026-09-17)](#persistencia-de-estado-fase-4-cerrada-2026-09-17)
- [Reglas de trabajo vigentes (desde la migración a Next.js, 2026-09-16)](#reglas-de-trabajo-vigentes-desde-la-migración-a-nextjs-2026-09-16)
- [Criterio para no desperdiciar trabajo web](#criterio-para-no-desperdiciar-trabajo-web)
- [2026-09-28 — REVISION DE CALIDAD del codigo de la sesion: un bug real (CZ101) y una regla triplicada](#2026-09-28-revision-de-calidad-del-codigo-de-la-sesion-un-bug-real-cz101-y-una-regla-triplicada) — revisión de calidad del código: un bug real y una regla triplicada
- [2026-09-28 — mientras haya OTRO hilo en el arbol, los commits van por partes](#2026-09-28-mientras-haya-otro-hilo-en-el-arbol-los-commits-van-por-partes) — con otro hilo en el árbol, los commits van por partes
- [2026-09-28 — el resumen final de build.bat dice QUE superficie fallo](#2026-09-28-el-resumen-final-de-buildbat-dice-que-superficie-fallo) — el resumen final de build.bat dice QUÉ superficie falló
- [2026-09-28 — la bancada tolera un arranque en frio del WebView2 (reintenta, y espera mas)](#2026-09-28-la-bancada-tolera-un-arranque-en-frio-del-webview2-reintenta-y-espera-mas) — la bancada tolera un arranque en frío del WebView2
- [2026-09-28 — la regresion visual del lienzo puede correr sola (Windows, en cada push)](#2026-09-28-la-regresion-visual-del-lienzo-puede-correr-sola-windows-en-cada-push) — la regresión visual del lienzo puede correr sola

### Contrato de parámetros, presets y puente

- [Contrato de parámetros (generado)](#contrato-de-parámetros-generado)
- [Conexión de parámetros divergentes: implementado (2026-09-16)](#conexión-de-parámetros-divergentes-implementado-2026-09-16)
- [Documentación de parámetros](#documentación-de-parámetros)
- [Presets por el bridge (2026-09-17): protocolo v1 aditivo](#presets-por-el-bridge-2026-09-17-protocolo-v1-aditivo) — presets por el bridge (protocolo v1 aditivo)
- [Bridge de parámetros JUCE <-> WebUI (2026-09-16)](#bridge-de-parámetros-juce---webui-2026-09-16)

### DSP y motor (de-JUCE, efectos, determinismo)

- [Fase 1 [4/6] — la frontera MIDI deja de ser JUCE (2026-09-18)](#fase-1-46-la-frontera-midi-deja-de-ser-juce-2026-09-18)
- [Fase 1 [6/6] — el motor deja de incluir JUCE (2026-09-18)](#fase-1-66-el-motor-deja-de-incluir-juce-2026-09-18)
- [Fase 1 [5/6] — el motor deja de depender de juce::Reverb (2026-09-18)](#fase-1-56-el-motor-deja-de-depender-de-jucereverb-2026-09-18)
- [Arreglo de tiempo real: el jitter de entropía y el placeholder de `Resonator` (2026-09-18)](#arreglo-de-tiempo-real-el-jitter-de-entropía-y-el-placeholder-de-resonator-2026-09-18) — jitter de entropía y placeholder de Resonator
- [Migración de los efectos a DspEffects — chorus, delay y saturación (2026-09-19)](#migración-de-los-efectos-a-dspeffects-chorus-delay-y-saturación-2026-09-19)
- [Arreglo de la rampa de la reverb: duraba 960 BLOQUES, no 20 ms (2026-09-19)](#arreglo-de-la-rampa-de-la-reverb-duraba-960-bloques-no-20-ms-2026-09-19) — la rampa de la reverb duraba 960 bloques
- [Tasa de control fija del motor: 64 muestras (2026-09-19)](#tasa-de-control-fija-del-motor-64-muestras-2026-09-19) — tasa de control fija de 64 muestras
- [Matematica determinista en el sustrato: sin/atan sin libm (2026-09-19)](#matematica-determinista-en-el-sustrato-sinatan-sin-libm-2026-09-19) — matemática determinista sin libm
- [2026-09-21 (s): auditoria de los dos motores — normalizacion, guardia y catch-up](#2026-09-21-s-auditoria-de-los-dos-motores-normalizacion-guardia-y-catch-up) — auditoría de los dos motores
- [2026-09-26 (an): las voces del motor, perezosas (16/8 en vez de 32 fijas)](#2026-09-26-an-las-voces-del-motor-perezosas-168-en-vez-de-32-fijas) — las voces del motor, perezosas (16/8)

### WASM, worklet y paridad

- [2026-09-17 (d) — Fase 5: DSP real compilado a WASM (smoke test Node en verde)](#2026-09-17-d-fase-5-dsp-real-compilado-a-wasm-smoke-test-node-en-verde) — el DSP real compilado a WASM
- [2026-09-18 — AudioWorklet del piloto (Fase 5, segundo hito)](#2026-09-18-audioworklet-del-piloto-fase-5-segundo-hito) — el AudioWorklet
- [2026-09-18 (b) — Paridad bit-exacta WASM<->nativo (Fase 5, tercer hito, CERRADO)](#2026-09-18-b-paridad-bit-exacta-wasm-nativo-fase-5-tercer-hito-cerrado)
- [Matriz de paridad WASM por sample rate y tamaño de bloque, y el hallazgo que destapa (2026-09-19)](#matriz-de-paridad-wasm-por-sample-rate-y-tamaño-de-bloque-y-el-hallazgo-que-destapa-2026-09-19) — matriz de paridad por sample rate y bloque
- [2026-09-26 (ao): modulo WASM reconstruido y worklet sincronizado](#2026-09-26-ao-modulo-wasm-reconstruido-y-worklet-sincronizado) — módulo WASM reconstruido y worklet sincronizado
- [2026-09-26 — fase 11.3 en el camino WASM: el motor del navegador suma las capas](#2026-09-26-fase-113-en-el-camino-wasm-el-motor-del-navegador-suma-las-capas)
- [2026-09-28 — canal `neuronik:voice`: los ocho knobs de envolvente POR FIN llegan al motor local](#2026-09-28-canal-neuronikvoice-los-ocho-knobs-de-envolvente-por-fin-llegan-al-motor-local) — canal `neuronik:voice`: los ocho knobs al motor local
- [ctest 51/51 y el bus del hueco ya viaja al navegador (2026-09-29)](#ctest-5151-y-el-bus-del-hueco-ya-viaja-al-navegador-2026-09-29) — el ctest completo al 100%, los 7 fallos del 85% atribuidos, y lo que queda del `.wasm`

### Arquitectura de la página y modo local

- [Andamiaje vainilla de la WebUI (Fase 8, 2026-09-19)](#andamiaje-vainilla-de-la-webui-fase-8-2026-09-19) — andamiaje vainilla de la WebUI (Fase 8)
- [Shell vainilla de la UI (base de 8.2, 2026-09-19) — hecha antes de 8.1 a propósito](#shell-vainilla-de-la-ui-base-de-82-2026-09-19-hecha-antes-de-81-a-propósito)
- [Politica de audio fijada en codigo + motor del worklet portado (8.1, 2026-09-19)](#politica-de-audio-fijada-en-codigo-motor-del-worklet-portado-81-2026-09-19) — política de audio fijada en código
- [8.2 — el lienzo unico: los 70 controles en una pantalla (2026-09-19)](#82-el-lienzo-unico-los-70-controles-en-una-pantalla-2026-09-19) — el lienzo único: los 70 controles
- [La bancada sirve la página del PLUGIN (y `--pilot-page` para la retirada) (2026-09-19)](#la-bancada-sirve-la-página-del-plugin-y---pilot-page-para-la-retirada-2026-09-19)
- [Retirada del piloto: las tres SSOT se mudan y la bancada pierde su snapshot (2026-09-19)](#retirada-del-piloto-las-tres-ssot-se-mudan-y-la-bancada-pierde-su-snapshot-2026-09-19)
- [2026-09-27 — el smoke del MODO LOCAL, automatizado (Playwright + Chromium) y el bug que caza](#2026-09-27-el-smoke-del-modo-local-automatizado-playwright-chromium-y-el-bug-que-caza) — el smoke del modo local (Playwright + Chromium)
- [8.1, paso 2c: el selftest de cuatro direcciones pasa al editor, y la bancada usa el MISMO (2026-09-19)](#81-paso-2c-el-selftest-de-cuatro-direcciones-pasa-al-editor-y-la-bancada-usa-el-mismo-2026-09-19) — el selftest pasa al editor

### Arnés del selftest, dirección por dirección

- [La dirección MATRIZ: el selftest corre con el cajón abierto y la matriz en uso (2026-09-19)](#la-dirección-matriz-el-selftest-corre-con-el-cajón-abierto-y-la-matriz-en-uso-2026-09-19)
- [2026-09-26 — ZRING: el anillo morphZ por el camino nativo (telemetria frame.modulation[28])](#2026-09-26-zring-el-anillo-morphz-por-el-camino-nativo-telemetria-framemodulation28)
- [2026-09-27 — el arco del anillo morpheZ con SIGNO: la semionda negativa se ve](#2026-09-27-el-arco-del-anillo-morphez-con-signo-la-semionda-negativa-se-ve) — el arco del anillo morph-Z con SIGNO
- [2026-09-27 — commit 157fa16: AGUJA (la octava direccion), build.bat nopause y las causas raices](#2026-09-27-commit-157fa16-aguja-la-octava-direccion-buildbat-nopause-y-las-causas-raices) — AGUJA (fases 0-3)
- [2026-09-27 — nopause, el parseo release y la retencion por coherencia de AGUJA](#2026-09-27-nopause-el-parseo-release-y-la-retencion-por-coherencia-de-aguja) — la retención por coherencia de AGUJA
- [2026-09-27 — AGUJA fase 4: el PANIC del medidor, medido (el clic apaga el medidor y silencia las agujas)](#2026-09-27-aguja-fase-4-el-panic-del-medidor-medido-el-clic-apaga-el-medidor-y-silencia-las-agujas)
- [2026-09-27 — las barras ENV del cajón de la MATRIZ: el nivel de cada envolvente en SU fila](#2026-09-27-las-barras-env-del-cajón-de-la-matriz-el-nivel-de-cada-envolvente-en-su-fila)
- [2026-09-27 — las dos copias de cada barra ENV son GEMELAS (resumen vs cajon, misma fila y frame)](#2026-09-27-las-dos-copias-de-cada-barra-env-son-gemelas-resumen-vs-cajon-misma-fila-y-frame)
- [2026-09-27 — VOLVER: el retorno del salto ENV -> MATRIZ se mide (la direccion 1b-bis)](#2026-09-27-volver-el-retorno-del-salto-env---matriz-se-mide-la-direccion-1b-bis)
- [2026-09-27 — VOLVER extendida: la regla de CANCELACION, medida E2E](#2026-09-27-volver-extendida-la-regla-de-cancelacion-medida-e2e) — VOLVER extendida: la regla de cancelación
- [2026-09-27 — VOLVER en el arnes: encaje tras ENV-RUTAS, el settle de modales y el cierre por cajon abierto](#2026-09-27-volver-en-el-arnes-encaje-tras-env-rutas-el-settle-de-modales-y-el-cierre-por-cajon-abierto) — VOLVER en el arnés: el encaje y el settle
- [2026-09-27 — ESQUINA: la calle de vuelta del pad entra en el arnes (la direccion 7b)](#2026-09-27-esquina-la-calle-de-vuelta-del-pad-entra-en-el-arnes-la-direccion-7b)
- [2026-09-27 — RESUMEN-RUTAS, la esquina clicable y la ayuda de gestos corregida: las tres caras del mismo viaje](#2026-09-27-resumen-rutas-la-esquina-clicable-y-la-ayuda-de-gestos-corregida-las-tres-caras-del-mismo-viaje)
- [2026-09-28 — MIDI-CC: la direccion 10b del arnes, y el hueco del puente que hacia falta para medirla](#2026-09-28-midi-cc-la-direccion-10b-del-arnes-y-el-hueco-del-puente-que-hacia-falta-para-medirla) — MIDI-CC, la dirección 10b
- [2026-09-28 — NEURONIK_SELFTEST_BUDGET: el presupuesto del selftest sube sin recompilar](#2026-09-28-neuronik_selftest_budget-el-presupuesto-del-selftest-sube-sin-recompilar) — el presupuesto del selftest sin recompilar
- [2026-09-28 — el guion de 1b/1b-bis/1b-ter escrito como lo que es: UNA cadena por datos](#2026-09-28-el-guion-de-1b1b-bis1b-ter-escrito-como-lo-que-es-una-cadena-por-datos) — el guion de 1b/1b-bis/1b-ter: una cadena por datos

### Agujas, barras, medidor y UI viva

- [2026-09-27 — la aguja del MODO NAVEGADOR tiene verificacion visual E2E (WebUI/needle-probe)](#2026-09-27-la-aguja-del-modo-navegador-tiene-verificacion-visual-e2e-webuineedle-probe) — la aguja del modo navegador tiene verificación visual (needle-probe)
- [2026-09-28 — las agujas SOSTENIDAS tienen foto de referencia: el unico bloque visual con audio](#2026-09-28-las-agujas-sostenidas-tienen-foto-de-referencia-el-unico-bloque-visual-con-audio)
- [2026-09-28 — needle-probe entra en ctest como tercer E2E de Playwright, con sus dos puertos](#2026-09-28-needle-probe-entra-en-ctest-como-tercer-e2e-de-playwright-con-sus-dos-puertos)
- [2026-09-27 — el medidor de voces es un BOTON PANIC: tooltip dinamico y el role de a11y arreglado](#2026-09-27-el-medidor-de-voces-es-un-boton-panic-tooltip-dinamico-y-el-role-de-a11y-arreglado) — el medidor de voces es un BOTÓN PANIC
- [2026-09-22 (v): ENV 1 y ENV 2 como fuentes de la matriz — la envolvente del filtro sale del armario](#2026-09-22-v-env-1-y-env-2-como-fuentes-de-la-matriz-la-envolvente-del-filtro-sale-del-armario)
- [2026-09-25 (ag): ENVOLVENTES — tamanos, destinos junto a cada grafica y el plan de N slots](#2026-09-25-ag-envolventes-tamanos-destinos-junto-a-cada-grafica-y-el-plan-de-n-slots) — ENVOLVENTES: tamaños y destinos
- [2026-09-27 — el DUAL-PATH de las envolventes (bridge nativo vs meter WASM) y el unsubscribe de los onWorklet*](#2026-09-27-el-dual-path-de-las-envolventes-bridge-nativo-vs-meter-wasm-y-el-unsubscribe-de-los-onworklet) — el dual-path de las envolventes y el unsubscribe de los onWorklet*

### Pad XY, aro morph-Z y ranuras A-D

- [8.3, paso 1: el pad XY dibujado (2026-09-20)](#83-paso-1-el-pad-xy-dibujado-2026-09-20)
- [2026-09-23 (o): el anillo de morphZ en el pad XY — el tercer eje gana su superficie](#2026-09-23-o-el-anillo-de-morphz-en-el-pad-xy-el-tercer-eje-gana-su-superficie)
- [2026-09-25 (aa): el pad XY y el anillo morph-Z verificados EN VIVO con CZ-SWEP1 — y los cuatro fallos que la verificación destapó](#2026-09-25-aa-el-pad-xy-y-el-anillo-morph-z-verificados-en-vivo-con-cz-swep1-y-los-cuatro-fallos-que-la-verificación-destapó) — pad XY y aro verificados EN VIVO
- [2026-09-25 (af): el anillo morphZ del pad, verificado en el navegador (destino 28)](#2026-09-25-af-el-anillo-morphz-del-pad-verificado-en-el-navegador-destino-28) — el anillo morphZ verificado en el navegador
- [2026-09-26 — el pad XY, el anillo morphZ y las ranuras, sin host (modo local del navegador)](#2026-09-26-el-pad-xy-el-anillo-morphz-y-las-ranuras-sin-host-modo-local-del-navegador) — pad XY, aro y ranuras sin host
- [2026-09-27 — el anillo del pad BAILA sin host (ruta local de la MATRIZ a Morph Z)](#2026-09-27-el-anillo-del-pad-baila-sin-host-ruta-local-de-la-matriz-a-morph-z)
- [8.2 (b) — los slots de modelo A–D: la carga la hace el host y contesta TARDE (2026-09-19)](#82-b-los-slots-de-modelo-ad-la-carga-la-hace-el-host-y-contesta-tarde-2026-09-19) — los slots de modelo A-D
- [2026-09-27 — la pagina RECUERDA sus ranuras de modelo sin host (memoria local)](#2026-09-27-la-pagina-recuerda-sus-ranuras-de-modelo-sin-host-memoria-local) — la página recuerda sus ranuras
- [2026-09-27 — OLVIDAR: una ranura se vacia de verdad, con el aviso en la misma linea que el fallo](#2026-09-27-olvidar-una-ranura-se-vacia-de-verdad-con-el-aviso-en-la-misma-linea-que-el-fallo) — OLVIDAR: una ranura se vacía de verdad
- [2026-09-27 — la MATRIZ tiene conmutador: la ruta local del pad, apagable y con LFO a elegir](#2026-09-27-la-matriz-tiene-conmutador-la-ruta-local-del-pad-apagable-y-con-lfo-a-elegir) — la MATRIZ tiene conmutador de ruta local
- [2026-09-27 — el distintivo VIVO también vive en el lienzo, y pulsarlo abre su cajón](#2026-09-27-el-distintivo-vivo-también-vive-en-el-lienzo-y-pulsarlo-abre-su-cajón) — el distintivo vivo también en el lienzo
- [2026-09-27 — los distintivos vivos también saben de MOTORES: el modo 'active'](#2026-09-27-los-distintivos-vivos-también-saben-de-motores-el-modo-active) — los distintivos vivos con motores
- [2026-09-25 (ak): el distintivo del cajón de la MATRIZ, vivo (`setHeader`)](#2026-09-25-ak-el-distintivo-del-cajón-de-la-matriz-vivo-setheader)

### UX, accesibilidad y controles compartidos

- [Familia de controles compartidos en ABDSharedAssets (2026-09-16)](#familia-de-controles-compartidos-en-abdsharedassets-2026-09-16)
- [Teclado MIDI compartido en la página (2026-09-17, Fase 4)](#teclado-midi-compartido-en-la-página-2026-09-17-fase-4)
- [2026-09-27 — la ayuda contextual de gestos del cajón de MODELOS y su encaje con los tooltips cortos](#2026-09-27-la-ayuda-contextual-de-gestos-del-cajón-de-modelos-y-su-encaje-con-los-tooltips-cortos)
- [2026-09-28 — revision de gestos no obvios: la ayuda GLOBAL de gestos, y lo que NO hacia falta](#2026-09-28-revision-de-gestos-no-obvios-la-ayuda-global-de-gestos-y-lo-que-no-hacia-falta)
- [2026-09-28 — la pista de teclado de las esquinas, en el tooltip Y con UNA sola fuente](#2026-09-28-la-pista-de-teclado-de-las-esquinas-en-el-tooltip-y-con-una-sola-fuente)
- [2026-09-28 — la fila del LCD: el D-pad estaba cortado a media altura](#2026-09-28-la-fila-del-lcd-el-d-pad-estaba-cortado-a-media-altura) — la fila del LCD: el D-pad cortado
- [2026-09-27 — regresion VISUAL del lienzo: una referencia por ficha, y el umbral del hermano Resulto ciego](#2026-09-27-regresion-visual-del-lienzo-una-referencia-por-ficha-y-el-umbral-del-hermano-resulto-ciego) — regresión visual del lienzo

### ModelMaker: análisis temporal, capas y rejilla

- [2026-09-23 (m): FASE 10 completa — el ModelMaker ya produce modelos temporales (Neuron-parity)](#2026-09-23-m-fase-10-completa-el-modelmaker-ya-produce-modelos-temporales-neuron-parity) — Fase 10: modelos temporales
- [2026-09-23 (n): sonda CLI del ModelMaker — el análisis verificado en producción](#2026-09-23-n-sonda-cli-del-modelmaker-el-análisis-verificado-en-producción)
- [2026-09-23 (p): HPS refinado y guardia de maximo local — el estimador no cae a sub-armonicos ni fabrica parciales](#2026-09-23-p-hps-refinado-y-guardia-de-maximo-local-el-estimador-no-cae-a-sub-armonicos-ni-fabrica-parciales) — HPS refinado y guardia de máximo local
- [2026-09-23 (q): 10.6 cerrada — frames con f0 por frame, y RRISE resulto ser material en capas](#2026-09-23-q-106-cerrada-frames-con-f0-por-frame-y-rrise-resulto-ser-material-en-capas)
- [2026-09-25 (w): Fase 11.2 — el clustering por forma de envolvente (y las tres medidas que cambiaron el diseño del plan)](#2026-09-25-w-fase-112-el-clustering-por-forma-de-envolvente-y-las-tres-medidas-que-cambiaron-el-diseño-del-plan)
- [2026-09-25 (x): el suelo de E1 — decidido con medidas: la vía es la f0 MANUAL (el suelo no se toca)](#2026-09-25-x-el-suelo-de-e1-decidido-con-medidas-la-vía-es-la-f0-manual-el-suelo-no-se-toca)
- [2026-09-25 (dd): el formato ralo de parciales — DISEÑADO (fase 11.6), con las medidas delante](#2026-09-25-dd-el-formato-ralo-de-parciales-diseñado-fase-116-con-las-medidas-delante)
- [2026-09-25 (ad): la pestana CAPAS del ModelMaker (Fase 11.5)](#2026-09-25-ad-la-pestana-capas-del-modelmaker-fase-115) — la pestaña CAPAS
- [2026-09-25 (ah): el indicador de rejilla del ModelMaker, clicable](#2026-09-25-ah-el-indicador-de-rejilla-del-modelmaker-clicable) — el indicador de rejilla clicable
- [2026-09-27 — el indicador de rejilla del ModelMaker es un MODELO PURO con su test](#2026-09-27-el-indicador-de-rejilla-del-modelmaker-es-un-modelo-puro-con-su-test) — el indicador de rejilla como modelo puro
- [2026-09-27 — el indicador de rejilla se alcanza y acciona con el TECLADO](#2026-09-27-el-indicador-de-rejilla-se-alcanza-y-acciona-con-el-teclado) — el indicador de rejilla con teclado
- [2026-09-26 — la metrica del clustering, seleccionable (y el coseno, medido)](#2026-09-26-la-metrica-del-clustering-seleccionable-y-el-coseno-medido) — la métrica del clustering, seleccionable
- [2026-09-26 — rejillas entrelazadas en la sonda: el parcial 4 de SWEP1 no es un error de 212 cents](#2026-09-26-rejillas-entrelazadas-en-la-sonda-el-parcial-4-de-swep1-no-es-un-error-de-212-cents)
- [2026-09-25 (aj): los rangos del residuo del banco CZ101, fijados en el ctest](#2026-09-25-aj-los-rangos-del-residuo-del-banco-cz101-fijados-en-el-ctest) — rangos del residuo fijados en el ctest
- [2026-09-26 (am): la huella del modelo de 25 KB, medida y con presupuesto](#2026-09-26-am-la-huella-del-modelo-de-25-kb-medida-y-con-presupuesto)

## Estado de la instrumentación del arranque (bancada WebView2)

La bancada `NEURONiK_WebPilotHost` sirve `WebUI/dist` —la misma página que embebe el plugin— y
mide su arranque real en un log. Es lo único que queda con nombre del piloto: el target, su exe y
su `.cpp` conservan el nombre porque renombrarlos toca CMake, `build.bat`, `start.bat` y dos
tests, y no era parte de este encargo (ver la sección de la retirada, al final).

```bash
cd ABDNeural
"./build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe" --auto-quit
```

- `--auto-quit` cierra el host en cuanto el panel está listo, para poder medir sin interacción.
- Salida: `pilot-startup.log` junto al ejecutable (se añade un bloque por ejecución) y stdout.
- Qué mide: construcción del backend WebView2, `document.readyState` interactivo, panel en el DOM,
  `window.__pilotReady` (lo publica el store de la página al terminar de aplicarse el snapshot),
  recursos servidos con sus bytes y rutas no resueltas.
- El marcador `window.__pilotReady` lo publica `WebUI/src/contracts/paramStore.js` (lo pone
  `WebUI/src/app.js` en marcha). Los nombres "panel in DOM" y "react ready" del log son del piloto:
  la métrica es la misma y cambió el dueño del marcador.

Resultados (Release, 2026-09-16):

```text
options built       535 - 622 ms
panel in DOM       1232 - 1386 ms
react ready        1366 - 1518 ms
recursos            8 peticiones, 476 KB, 1 fallo (favicon.ico)
```

El perfil WebView2 borrado no cambia el resultado de forma apreciable. El coste dominante previo a
nuestro código es la construcción del backend (~0,55 s).

## Contexto

NEURONiK es un sintetizador propio basado en JUCE/C++. Tiene un motor híbrido de síntesis aditiva/resonante, una segunda ruta Neurotik, parámetros APVTS, presets, MIDI mapping y una interfaz JUCE hardware-inspired.

El objetivo futuro es conservar Standalone/VST3/AU y añadir una interfaz web reutilizable, primero en navegador/WebView2 y posteriormente con un posible backend WASM para el mismo núcleo DSP.

Fecha de apertura de este handoff: **2026-09-16** (crece por entradas fechadas; la última está al final).

## Repositorio

```text
ABDNeural/
```

Repositorio original:

```text
https://github.com/ajabadia/ABDNeural
```

No se ha modificado todavía `ABDMS2000` ni `ABDSharedCode` como parte de esta iniciativa.

## Línea base compilada

La copia de referencia está en:

```text
ABDNeural/build-reference/
```

Artefactos principales:

```text
ABDNeural/build-reference/NEURONiK_artefacts/Release/Standalone/NEURONiK.exe
ABDNeural/build-reference/NEURONiK_artefacts/Release/VST3/NEURONiK.vst3
```

El Standalone ha sido comprobado manualmente por el usuario y se considera la referencia funcional actual.

El build también generó el ejecutable de prueba:

```text
ABDNeural/build-reference/Release/NEURONiK_DSPReferenceTest.exe
```

## Prueba DSP actual

Archivo:

```text
Tests/DSPReferenceTest.cpp
```

Target CMake:

```text
NEURONiK_DSPReferenceTest
```

Registro CTest:

```cmake
enable_testing()
add_test(NAME NEURONiK_DSPReferenceTest COMMAND NEURONiK_DSPReferenceTest)
```

La prueba prepara `NeuronikEngine` a 48 kHz, envía MIDI note-on, renderiza un bloque de 512 muestras y verifica una señal no nula y una voz activa.

Último resultado conocido:

```text
peak = 0.0901112
rms  = 0.0360038
1/1 tests passed
```

Comando de validación (o simplemente `build.bat`, que hace todo y termina con pausa):

```bash
cmake --build build-reference --config Release --target NEURONiK_DSPReferenceTest
ctest --test-dir build-reference -C Release --output-on-failure
```

`build.bat` encadena configuración de CMake, regeneración del contrato, Standalone + VST3, host
del piloto, exportación de la WebUI y la suite completa. Acepta un directorio de build como
argumento (por defecto `build-reference`).

Arranque de la versión web (`start.bat`, mismo patrón que ABDMS2000, menú 1-3):

1. **Bancada WebView2** — lanza "NEURONiK Web Pilot.exe" (bridge bidireccional con el
   plugin; es la web "de verdad", como el Vite 8384 en ABDMS2000). Ése es el nombre del exe
   porque el target conserva el del piloto: retirado el piloto, esto es la bancada de la WebUI.
2. **Solo WebUI en navegador** — sirve `WebUI/dist` en `http://localhost:8399` con
   `npx serve`; sin JUCE la página queda en LOCAL MODE (útil para depurar la página a solas).
3. **Selftest del bridge** — ejecuta el host con `--selftest` (E2E automático NATIVO->JS y
   JS->NATIVO, imprime resultado y cierra solo; exit 0 = OK).

Antes de lanzar la opción 1 o 3 debe existir el host (compilar con `build.bat`); el propio
`start.bat` comprueba artefactos y avisa si faltan. Termina con pausa para poder leer la
salida (regla de trabajo de este proyecto).

## Arquitectura actual relevante

### Procesador JUCE

```text
Source/Main/NEURONiKProcessor.cpp
Source/Main/NEURONiKProcessor.h
```

El procesador contiene:

- `juce::AudioProcessorValueTreeState apvts`;
- selección entre `NeuronikEngine` y `NeurotikEngine`;
- sincronización APVTS → motor mediante `synchronizeEngineParameters()`;
- inyección de MIDI desde la UI;
- persistencia de estado y presets;
- visualización de envolventes, LFO y espectro.

### Parámetros

```text
Source/State/ParameterDefinitions.h
```

Este archivo es actualmente la definición central de IDs, rangos, defaults y elecciones de parámetros.

No se debe crear todavía una segunda lista independiente de parámetros para la web.

### Motor

```text
Source/DSP/ISynthesisEngine.h
Source/DSP/IVoice.h
Source/DSP/BaseEngine.h
Source/DSP/CoreModules/NeuronikEngine.*
Source/DSP/CoreModules/NeurotikEngine.*
Source/DSP/Synthesis/AdditiveVoice.*
Source/DSP/Synthesis/NeurotikVoice.*
```

Dependencias JUCE detectadas en el DSP:

- `juce::AudioBuffer<float>`;
- `juce::MidiBuffer`;
- `juce::MidiMessage`;
- `juce::LinearSmoothedValue`;
- `juce::Random`;
- `juce::AudioBuffer` en efectos;
- `juce::Reverb`;
- SIMD JUCE;
- utilidades `jlimit`, `jmin`, `MathConstants`.

## Decisión arquitectónica actual

Se ha elegido **extracción progresiva**, no desacoplamiento completo inmediato.

Primera frontera prevista:

```text
JUCE APVTS / MidiBuffer / AudioBuffer
              ↓
        adaptador JUCE
              ↓
     fachada DSP progresiva
              ↓
       futuro adaptador WASM
```

Durante la primera etapa se mantendrán temporalmente tipos JUCE donde eliminarlos implique riesgo. Se extraerán primero eventos, parámetros y la fachada de procesamiento; la sustitución completa de buffers y efectos queda para una fase posterior.

## Frontera DSP creada

Se ha añadido una primera fachada progresiva:

```text
Source/DSP/Runtime/DspEvent.h
Source/DSP/Runtime/DspEngineFacade.h
Source/DSP/Runtime/DspEngineFacade.cpp
```

La fachada expone eventos propios (`NoteOn`, `NoteOff`, pitch bend, presión y timbre) y adapta temporalmente a `juce::MidiMessage`/`juce::AudioBuffer` por dentro. Todavía no elimina dependencias JUCE; su objetivo es establecer el contrato que podrán usar el adaptador JUCE y el futuro adaptador WASM.

La prueba `NEURONiK_DSPReferenceTest` ya pasa a través de esta fachada.

## Próximo trabajo recomendado (histórico, 2026-09-16)

> Superada por las entradas fechadas posteriores: el bridge existe y está verificado en las dos
> direcciones, el piloto se creó y luego se retiró, Vite ganó el A/B y el plan vigente es la
> **Fase 8** de `ROADMAP.md`.

1. Lanzar `build.bat`: compila el host con el bridge y añade `NEURONiK_ParameterBridgeTest` a la
   suite (8 tests). Verificación interactiva que solo puede hacerse con la ventana delante:
   mover un slider de la tira nativa debe mover el control web y viceversa.
2. Añadir `setParameter` y estructuras de parámetros a la fachada.
3. Validar note-off y eventos expresivos mediante pruebas.
4. Crear una conversión explícita de APVTS a un modelo de parámetros común.
5. Mantener un adaptador JUCE que produzca exactamente la misma salida.
6. Añadir pruebas de:
   - note-on/note-off;
   - cambio de `masterLevel`;
   - cambio de `morphX/morphY`;
   - presets;
   - selección de `engineType`.
7. Crear un piloto aislado de Next.js con un solo panel y estado simulado.
8. Probar exportación estática y carga en WebView2 antes de migrar más UI.
9. Solo si el piloto supera el punto de decisión, iniciar el wrapper WASM y la primera pantalla web conectada.

## Piloto Next.js creado

El experimento aislado está en:

```text
WebPilot/
```

Incluye un panel mínimo con estado simulado para:

```text
masterLevel
morphX
morphY
engineType
```

Configuración relevante:

```text
WebPilot/package.json
WebPilot/next.config.mjs
WebPilot/app/page.jsx
WebPilot/app/layout.jsx
WebPilot/app/globals.css
```

La configuración utiliza `output: 'export'` y `trailingSlash: true`, por lo que la salida estática se genera en:

```text
WebPilot/out/
```

El build validado ha sido:

```bash
cd ABDNeural/WebPilot
pnpm install --ignore-workspace
pnpm build
```

Resultado conocido: Next.js 16.3.5 genera la ruta estática `/` correctamente. `node_modules`, `.next` y `out` están excluidos mediante `WebPilot/.gitignore`.

El piloto todavía no conecta parámetros reales, WASM ni audio. Se ha añadido un host JUCE/WebView2 independiente:

```text
Source/WebPilotHost.cpp
```

Target CMake:

```text
NEURONiK_WebPilotHost
```

Ejecutable generado:

```text
build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe
```

El host sirve directamente `WebPilot/out/` mediante un `ResourceProvider`, habilita el backend WebView2 y no sustituye `NEURONiKEditor`.

## Verificación del piloto en WebView2 (2026-09-16)

Confirmado manualmente: la ventana `NEURONiK Web Pilot` carga el panel Next.js y los controles
responden (`Master Level`, `Morph X`, `Morph Y`, `Engine Type`, `STATIC EXPORT`, contador y JSON de estado).

Cadena validada sin hacks de plataforma:

```text
next build (output: 'export')
    → WebPilot/out/
    → ResourceProvider JUCE (backend WebView2)
    → https://juce.backend/
```

Detalle relevante: las rutas absolutas que emite Next (`/_next/static/chunks/*.js`) se resuelven
contra el origen `https://juce.backend/`, que es el que `WebBrowserComponent` intercepta, por lo que
no hace falta reescribir el `index.html` ni forzar `basePath`.

El proveedor de recursos se ha endurecido después de la verificación:

- la raíz de la página se localiza por ruta de compilación (`NEURONiK_WEBUI_DIR`) y, si falla,
  buscando `WebUI/dist` hacia arriba desde el ejecutable y desde el directorio de trabajo;
  `NEURONIK_WEBUI_DEV_DIR` la puede apuntar a cualquier exportación (mismo override que el plugin);
- CMake **ya no falla** en configure por esta carpeta: hasta la retirada del piloto exigía
  `WebPilot/out/index.html` (y llevaba su export embebido, `NEURONiK_WebPilotAssets`); hoy la
  bancada sirve `WebUI/dist` desde disco y no embebe página;
- si el documento no se encuentra, se sirve una página de diagnóstico legible en lugar de una ventana en blanco.

Nota operativa: el `.exe` del host queda bloqueado mientras la ventana está abierta, así que hay que
cerrarla antes de recompilar.

## Bridge de parámetros JUCE <-> WebUI (2026-09-16)

El host del piloto ya no es solo un visor: refleja un APVTS real en ambos sentidos por el canal de
eventos de JUCE 8 y lleva una tira nativa de comparación.

```text
Source/WebUI/ParameterBridge.{h,cpp}   protocolo bidireccional (sin WebView2: testeable con una lambda)
Source/WebPilotHost.cpp                transporte + APVTS real (createLayoutApvts) + tira nativa (bancada)
WebUI/src/bridge/bridgeCore.js         transporte JS (window.__JUCE__.backend; modo local sin JUCE)
WebUI/src/app.js                       panel conectado, con fases de gesto y estado normalizado
Tests/ParameterBridgeTest.cpp          protocolo completo contra el APVTS del contrato
Tests/webviewBridgeDirectionTest.mjs   guard de dirección (compartido con ABDSharedCode)
```

(Las dos rutas JS de esta tabla decían `WebPilot/lib/bridge.js` y `WebPilot/app/page.jsx` hasta la
retirada del piloto: son los ficheros que se portaron a `WebUI/src/`, y desde 8.4 viven ahí.)

Puntos clave de diseño (el detalle completo está en `DOCS/PILOT_RETIRED.md`, sección del bridge —
ese documento es el registro del piloto y se conserva por esto mismo):

- El APVTS reflejado es `State::createLayoutApvts()`: el layout exacto del plugin (70 parámetros).
- **Salida por sondeo** (`publishPendingChanges`, timer de 30 ms) en lugar de listeners de
  parámetros: elimina por construcción el modo de fallo "listener duplicado / evento perdido" y
  permite diferir por valor (un parámetro puesto al valor que ya tenía no viaja).
- **Sin eco**: lo que llega de JS actualiza `lastReported`, y el sondeo no se lo devuelve.
- **Gestos**: begin/change/end desde JS; si la página se recarga en mitad de un arrastre, el host
  cierra el gesto abierto (listener `pageLoaded` + `closeOpenGestures`).
- **Tolerancia**: mensajes malformados e IDs desconocidos se cuentan en `Stats`, nunca lanzan.
- `value` en el cable SIEMPRE normalizado 0..1; `real`/`text` viajan solo nativo -> JS.
- El host sirve `juce.js` sin interferir (integración nativa activada: sin ella no hay
  `window.__JUCE__` y ambas direcciones mueren).
- La tira nativa del host usa `SliderAttachment` reales sobre los mismos IDs: mover un lado debe
  mover el otro. Verificación interactiva pendiente del próximo `build.bat`.

Validado sin compilar el plugin: `next build` en verde, el smoke test del transporte JS (ids y
payloads en ambas direcciones, dispose sin fugas) y el guard de dirección sobre los 98 ficheros de
`Source/`. El test `NEURONiK_ParameterBridgeTest` compila y corre con `build.bat`.

### El protocolo, como contrato versionado (2026-09-16)

El formato del cable está fijado en `WebUI/contracts/bridge-protocol.json` (versionado en git,
versión 1) con especificación completa en `DOCS/BRIDGE_PROTOCOL.md`. Misma mecánica que el
contrato de parámetros: el fichero no se genera, se edita, y dos tests anti-drift lo comparan con
lo que cada lado ejecuta:

```text
NEURONiK_BridgeProtocolContractTest   C++: JSON vs literales compilados de ParameterBridge.h
NEURONiK_BridgeProtocolJs (node)      JS: JSON vs bridge.js + formas de mensaje reales (backend simulado)
```

Fijado por ellos: los tres event ids (`event`, `nativeEvent`, `pageLoaded`), las acciones
(`syncAllParams`, `parameterChanged`, `requestState`), las fases de gesto, la convención de
escala (`value` normalizado, `real`/`text` solo nativo->JS) y los comportamientos (sin eco,
salida por sondeo, gestos siempre cerrados, entrada tolerante, modo local). La política de
versiones está en `BRIDGE_PROTOCOL.md`: cambio aditivo opcional no la incrementa; renombrar un
literal o cambiar la escala, sí.

Ambos tests están en verde, y con ellos la suite sube a **10 tests** en el próximo `build.bat`
(8 C++ + 2 node).

**Validado por el usuario (build.bat completo, 2026-09-16 18:11): 10/10 tests, plugin recompilado
byte-idéntico (md5 f15de044…) y host del piloto con el bridge sirviendo la página (8 recursos,
478 KB, panel en DOM a 1645 ms, ready a 2384 ms; el 2,4 s vuelve a ser perfil WebView2 frío tras
recompilar — en caliente la serie documentada está en 886-1166 ms).**

### Verificación bidireccional automatizada: `--selftest` (2026-09-16 18:26)

El host del piloto incluye un modo que ejecuta el doble E2E del bridge sobre el canal real de
WebView2 (mismo recorrido que una prueba manual con el ratón):

```text
"NEURONiK Web Pilot.exe" --selftest
  [selftest] NATIVE -> JS: native masterLevel = 0.25, page slider = 0.25 -> OK
  [selftest] JS -> NATIVE: page slider set to 0.75, native masterLevel = 0.7500 -> OK
  [selftest] RESULT: OK        (exit code 0)
```

- NATIVO -> JS: `setValueNotifyingHost(0.25)` en el APVTS y, 400 ms después, lectura del `value`
  del primer slider de la página (0.25 exacto).
- JS -> NATIVO: dispatch de un evento `input` real sobre el slider (lo que dispara un arrastre de
  usuario) con valor 0.75 y, tras el ciclo React -> bridge -> poller, lectura del parámetro nativo
  (0.7500 exacto).
- `--selftest` implica `--auto-quit`; el exit code (0/1) es el veredicto. En este modo el selftest
  es quien cierra la ventana, no el sondeo de arranque.
- **`build.bat` lo ejecuta como paso 10/10** (tras la suite de tests; era 8/8 antes de que el WASM
  entrara como 3/10 y la WebUI del plugin como 4/10), solo si el host compiló y
  existe `WebPilot\out`. Un fallo del selftest marca el build como CON ERRORES. Se omite con
  `build.bat noselftest`. La ventana del piloto parpadea unos 3 segundos: es el selftest.
- **Verificación manual del usuario (misma sesión): confirmada.** Captura del host con la página
  en `BRIDGE LIVE` y la tira nativa mostrando exactamente los mismos valores (0.67/0.564/0.399 en
  ambos lados, 261 actualizaciones de parámetro). El punto de ROADMAP queda cerrado.

## Reglas de trabajo vigentes (desde la migración a Next.js, 2026-09-16)

1. **Sin monolitos**: ningún fichero nuevo por encima de ~300 líneas; si crece, se divide.
2. **DRY**: la matemática/interacción compartida vive en un módulo común (p. ej.
   `drag-core.js`), nunca duplicada entre controles o paneles.
3. **Tests al cerrar cada paso relevante**: JS con vitest, C++ con ctest; un paso sin su
   test no cuenta como terminado.
4. **Sin NTFS junctions en nada nuevo**: la reutilización va por paquetes npm/pnpm
   (workspace `@abdsynths/*` o `file:`); los junctions existentes son legacy.

## Familia de controles compartidos en ABDSharedAssets (2026-09-16)

NEURONiK arranca la migración de UI como **primer consumidor** de la familia de controles
compartidos del paquete `@abdsynths/shared` (`ABDSharedAssets/`). Documentación completa:
`ABDSharedAssets/COMPONENTS.md`. Resumen operativo:

- **Familia**: `Knob`, `Slider`, `Toggle` (+ `Wheel` preexistente) en
  `ABDSharedAssets/components/`, contrato común (constructor + `setValue/getValue/destroy`,
  `onChange` solo en ediciones de usuario), interacción DRY vía `drag-core.js`.
- **Skins**: el ASPECTO es intercambiable por synthe. Una skin es un **mapa de renderers por
  tipo** (`{knob, slider, toggle}`); `applySkin()` despacha por `CONTROL_KIND` y cae al renderer
  'vector' cuando falta un tipo. Incluidas: `vector` (SVG/CSS sin assets), `ms2000`
  (extraída de ABDMS2000), `junio` (sprites PNG extraídos de ABDJUNiO601, en
  `ABDSharedAssets/assets/junio/`). `registerSkin()` para skins de proyecto.
- **Demo única**: `ABDSharedAssets/demo/demo.html` **sección 8** (familia completa, 3 skins,
  colores del toggle junio, toggle momentary). `demo/proto/` fue **deprecada y eliminada**:
  su única aportación (cascada de tema de 3 niveles) está documentada en
  `docs/INTEGRATION_GUIDE.md` §5 bis y no mostraba ningún componente que la demo principal
  no tuviera. `npm run demo` sirve la raíz del paquete → abrir `/demo/demo.html`.
- **Tests**: `pnpm test` en ABDSharedAssets (vitest+jsdom) — 24/24 en verde: contrato,
  clamping, onChange/setValue, gestos, despacho de skins, fallback, destroy sin fugas.
- **Consumo sin junctions**: paquete pnpm (`workspace:*`) o `file:`; los controles no saben
  nada de JUCE/bridge/React — el envoltorio React es quien conecta con el contrato.

### Wrappers React en el WebPilot (2026-09-16)

Primera integración real de la familia en una pantalla Next.js del plugin:

- `WebPilot/lib/controls.jsx` — `useSharedControl(Clase, props)`: monta el control
  imperativo una vez por instancia (efecto ligado a la clase, no a las props), React ->
  `control.setValue()` (programático, **sin eco** de onChange), callbacks de usuario vía
  refs estables (`handlersRef`), `destroy()` en el cleanup. Wrappers: `ParamKnob`,
  `ParamSlider`, `ParamToggle` (bool como 0/1 en el cable) y `ParamChoice` (select nativo,
  índice N viaja como N/(count-1), el encoding discreto del APVTS).
- `WebPilot/lib/paramValue.js` — plomería de valores sobre el contrato: `realFromNormalized`
  / `normalizedFromReal` (misma matemática NormalisableRange que el host, con snap de
  intervalo), `displayText` (percent en rangos 0..1 sin unidad, unidades reales en el resto),
  `describeParam` (view-model del contrato; null para IDs desconocidos → error visible).
- `WebPilot/lib/useParameterControls.js` — glue de página: estado normalizado + snapshot
  inicial + push al bridge con fases de gesto (`begin`/`change`/`end`), modo local si no hay
  `window.__JUCE__`.
- `app/page.jsx` — reescrito sobre el hook y los wrappers. **`masterLevel` conserva
  `input[type=range]` nativo a propósito**: el `--selftest` del host lo conduce con
  `querySelector('input[type=range]')`; el resto (morphX, morphY, engineType) usa la familia
  compartida. Bug corregido de pasada: la página anterior convertía real→normalizado con
  `fromNormalized` (la inversa); fallaría con skew≠1 o rangos no 0..1.
- Consumo: `"@abdsynths/shared": "file:../../ABDSharedAssets"` en WebPilot/package.json,
  instalado con `pnpm install --ignore-workspace` (el workspace raíz capturaría el install).
  El paquete exportó además `"./components"` (barrel) y `tokens.css` dejó de `@import`ar
  Google Fonts (rompía `output: 'export'` sin red).
- Validado: **15/15 tests nuevos** (`pnpm test` en WebPilot: unitarios de paramValue + guard
  de contrato de página) y `pnpm build` en verde con el CSS de la familia embebido. E2E
  re-verificado con `--selftest` tras la corrección del drag (ver abajo).

### Corrección de interacción: drag 1:1 en la familia compartida (2026-09-16 21:45)

**Síntoma informado por el usuario**: los sliders morphX/morphY "iban a toda velocidad" al
arrastrar (masterLevel, el `input` nativo, iba bien). **Causa raíz** en `ABDSharedAssets/
components/drag-core.js` (afecta a TODA la familia: knob, slider y wheel): el delta se medía
**desde el origen del arrastre** pero los controles lo aplican como **incremento** sobre el
valor ya actualizado — el valor se componía y la velocidad crecía cuadráticamente con el
número de eventos de movimiento. Silencioso en tests porque los drags sintéticos iban en un
solo `pointermove`.

**Fix**: el delta es ahora **relativo al movimiento anterior** (`lastX/lastY` actualizados en
cada `pointermove`); el valor sigue al puntero 1:1 (N eventos de d px = N*d px de recorrido).
Test de regresión nuevo en `tests/controls.test.js` (drag en 4 movimientos → valor exacto);
suite en **25/25**. Requiere re-servir/re-exportar cualquier WebUI que embebiera `drag-core`.


## Paso 1 ejecutado: ParameterPanel real + assets embebidos (2026-09-16, por validar)

Escrito SIN compilar (acuerdo de turno: compila el usuario con `build.bat`). Estado en disco:

- **Host con el plugin de verdad.** `PilotComponent` instancia `NEURONiKProcessor` (miembro
  propio, declarado DESPUÉS de `browser` y ANTES de `bridge` — el orden de destrucción importa:
  el panel y el bridge mueren antes que el procesador). Fuera `State::createLayoutApvts()`: el
  bridge puentea `processor.getAPVTS()`. El selftest consulta `processor.getAPVTS()` igual que
  antes consultaba el APVTS de juguete — protocolo y página no cambian.
- **Panel real en vez de tira.** `NativeStrip` eliminado; abajo del navegador vive ahora el
  `NEURONiK::UI::ParameterPanel` real (pestaña GENERAL: envolvente, unison, freeze, RANDOM,
  selector de motor, MidiLearners). Altura 240 px (la tira eran 120). El `timerCallback` ya no
  refresca etiquetas (las pintaba la tira); el panel se repinta con sus propios timers.
- **CMake del host.** Compila las mismas fuentes que el plugin (`${NEURONIK_SOURCES}`, sin el
  `.rc` para no duplicar VERSIONINFO), patrón igual que el Standalone. Enlaza además
  `juce_audio_basics`, `juce_data_structures`, `juce_dsp` y `NEURONiK_Common`. Include dirs de
  Main/UI/Panels/Browser/DSP/State/Serialization añadidos.
- **Assets WebUI embebidos (apuntado por el usuario y hecho).** `juce_add_binary_data
  (NEURONiK_WebPilotAssets)` con el GLOB de `WebPilot/out/**` (CONFIGURE_DEPENDS, excluyendo
  `_not-found` para no duplicar identificadores). `loadPilotResource` sirve DISCO primero y
  cae a `BinaryData` si el exe está solo (`loadEmbeddedResource`, match por sufijo/basename);
  define `NEURONIK_HAS_PILOT_ASSETS`. El informe del log imprime `[embedded fallback: N]`.
  Esta es la vía de servicio del VST3 final.

**Correcciones tras la primera compilación (2026-09-16):**

- `C2065 JucePlugin_Name` (NEURONiKProcessor.cpp:418): el target gui-app no genera macros de
  plugin → `JucePlugin_Name="NEURONiK"` clavado en `target_compile_definitions` del host
  (mismo valor que el vcxproj del plugin).
- `C3861 loadEmbeddedResource`: estaba definida DESPUÉS de `loadPilotResource` → declaración
  adelantada dentro de la clase (el orden de definición ya no importa).
- `C2039 getNumResources` / `C2664 getNamedResourceOriginalFilename(i)`: esas APIs NO existen
  en el `BinaryData` generado. La real: `BinaryData::namedResourceListSize` +
  `namedResourceList[]` (nombres mangled), y el payload se pide por NOMBRE de recurso
  (`getNamedResource(resourceName, size)`), nunca por ruta; el nombre original se saca con
  `getNamedResourceOriginalFilename(resourceName)` para el match por sufijo/basename.
- `C2039 fromLastCharacterOfDelimiter`: no existe en `juce::String` → `fromLastOccurrenceOf
  ("/", false, true)`.

**Bug preexistente destapado por el piloto (corregido 2026-09-16):** TODA la telemetría de UI
se sincronizaba SOLO dentro de `processBlock` (`uiAttack…uiFRelease`, `uiMorphX/Y`). En el
plugin da igual (siempre hay audio), pero el host del piloto NO tiene callback de audio →
XYPad y EnvelopeVisualizer nativos se quedaban congelados. Fix: extraído a
`NEURONiKProcessor::refreshUiTelemetryFromApvts()` (público, thread-safe: atomics + loads del
APVTS); `processBlock` lo sigue llamando cada bloque y el host lo consulta en su timer.
La telemetría derivada del MOTOR (espectral, LFO, envolventes de salida) sigue donde estaba:
solo existe tras render.

Observaciones auditadas SIN cambiar (por si parecen bugs y no lo son):
- `morphX/morphY` son 0..1 con default 0 — "esquina modelo A", no centro. El DSP hace
  `lerp(modelA, modelB, morphX)` con `jlimit(0,1)`: legal por diseño. El fallback local 0.5
  del XYPad es inofensivo (lo sobrescribe el source al instante). El centro real del pad
  depende de qué modelos cargue el preset.
- El XYPad nativo invierte Y de forma consistente (pintado, ratón y thumb con el mismo
  `1 - y`): correcto.

**3ª compilación (2026-09-16 23:03) — HOST ENLAZA Y PASA:** juce_audio_utils resolvió los
LNK2019, selftest OK con host recompilado, 10/10. NOTA: los fixes de telemetría (23:05) son
posteriores a los .obj (22:57) → necesitan UNA pasada más (rápida: 2 ficheros). El lado WebUI
(hook normalizado) SÍ quedó dentro (paso 5 regeneró out/).

**4ª compilación (2026-09-16 23:43, build.bat completo): 10/10 tests + selftest OK, telemetría
dentro (exe 23:43 > fixes 23:05) y el exit code del selftest YA llega al proceso — verificado
en ambos caminos: `--selftest` → exit 0; `--selftest-force-fail` (drill temporal) →
`RESULT: FAIL`, `reason=selftest-forced-fail`, exit 1.** La pasada aun así imprimió
RESULTADO: OK pese a un fallo real en el paso 5 (incidencia y endurecimiento abajo).

**Incidencia del paso 5 (RESUELTA): snapshot viejo de `@abdsynths/shared` en node_modules.**
El barrel instalado ya exportaba XYPad pero le faltaba `components/xypad.js` (copia a medias
de la sesión cortada) → `next build` murió con `Module not found: Can't resolve './xypad.js'`.
El XYPad SÍ está completo en ABDSharedAssets: `xypad.js` (23:11), 12 tests propios, suite
**37/37** en verde, exportado en el barrel y usado en la demo (sección 8). Resolución:
`cd WebPilot && pnpm install --ignore-workspace` refresca los deps `file:` desde el paquete
fuente. **Lección:** tocar `ABDSharedAssets` exige re-install en cada consumidor `file:`, no
basta re-exportar; el snapshot de node_modules puede quedarse a medias si la sesión muere.

**Endurecido build.bat (el paso 5 ya no es blando):** un fallo de `pnpm build` marca
`WEBUI_BUILD_FAILED`, `EXIT_CODE=1` y el paso 8 OMITE el selftest. Antes: aviso y el selftest
corría contra `WebPilot\out` ANTERIOR — en esta pasada celebró un OK con la WebUI vieja.

**Pendiente del checklist del paso 1:** (a) visual: RANDOM nativo mueve morphX/Y en página y
slider de página mueve VOLUME nativo; (c) XYPad nativo sigue a la página. (b) fallback
embebido **VALIDADO (2026-09-17, ver sección propia más abajo)**. Drill
`--selftest-force-fail`: RETIRADO (2026-09-17) tras validar; el flag ya no fuerza nada y se
degraja a un `--selftest` normal.

**Fix de revisión (2026-09-16, por compilar): el exit code del selftest nunca salía del exe.**
`selftestPassed` moría en el `PilotComponent`: ni `finish()` ni `systemRequestedQuit()` lo
publicaban, así que el proceso salía SIEMPRE con 0 y la puerta del paso 8/8 de `build.bat`
(hoy es el 10/10: el WASM entró después como 3/10 y la WebUI del plugin como 4/10, ver "Procedimiento de build")
(`if !ERRORLEVEL! neq 0`) era decorativa — un selftest FAIL se habría celebrado como
`[OK] Bridge verificado`. Arreglado en `Source/WebPilotHost.cpp`: `g_selftestExitCode`
(atómico, -1 = sin veredicto) lo escribe `selftestFinish()` y `PilotApplication::
systemRequestedQuit()` lo aplica con `setApplicationReturnValue(code)`. Sin selftest
(`--auto-quit` o cierre manual) el default 0 se conserva. Validación:

```bat
"build-reference\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe" --selftest
echo %ERRORLEVEL%   :: debe ser 0

"build-reference\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe" --selftest-force-fail
echo %ERRORLEVEL%   :: debe ser 1 (simulacro temporal, imprime selftest-forced-fail)
```

El simulacro `--selftest-force-fail` (RETIRADO 2026-09-17 tras validar) saltaba el E2E y
publicaba el veredicto FAIL en el primer tick: comprobó que el exit 1 llegaba al proceso sin
depender de que el bridge fallara de verdad. En `--auto-quit`/cierre manual el exit sigue
siendo 0.
**VALIDADO (2026-09-16 23:5x): ambos caminos, exit 0 y exit 1.** Revalidado 2026-09-17 con el
host corregido del fallback (ambos en verde).

## Ejercicio del fallback embebido — VALIDADO (2026-09-17), con 2 bugs arreglados

Ejercicio: renombrar `WebPilot/out`, lanzar `--selftest`, restaurar. Resultado final:
**`[embedded fallback: 8]` (497 KB), E2E del bridge OK en ambos sentidos sobre la WebUI
embebida, exit 0**; con disco 8/496 KB; y `--selftest-force-fail` sigue publicando exit 1.

- **Bug 1 — fuga de exit code en timeout:** un selftest que moría por `timeout` sin veredicto
  salía con 0. `finish()` ahora publica FAIL si el selftest acaba sin veredicto
  (`selftestFinish()` graba el código ANTES de llamar a `finish`, así que nunca lo pisa).
- **Bug 2 — el snapshot embebido servía la página 404 como `index.html` (causa raíz del
  timeout):** el glob de CMake excluía `_not-found` pero NO la ruta `404/`, y
  `404/index.html` ganó el identificador `index.html` (juce_add_binary_data usa UN identificador
  por basename, orden alfabético). Síntoma perverso: `resources served: 7 (460 KB)` con **cero
  misses** — pedía 7 y le servían 7, pero el documento era el not-found (sin `.panel`).
  Arreglo en `CMakeLists.txt`: excluir del snapshot `_not-found`, `404.html` y `404/…`; el
  matcher (`loadEmbeddedResource`) conserva el guard para que una ruta de error jamás
  responda a una petición normal. Lección: tras tocar el snapshot, verificar que
  `originalFilenames` no tiene duplicados (`grep '"index.html"' BinaryData1.cpp` → 1).

Checklist restante del paso 1: (a) visual: RANDOM nativo mueve morphX/Y en página, slider de
página mueve VOLUME nativo; (c) con telemetría dentro: XYPad nativo sigue a la página.
(b) fallback embebido: HECHO.

## Presets por el bridge (2026-09-17): protocolo v1 aditivo

La página ya lista, carga y guarda presets. Decisiones de diseño:

- **Wire (aditivo a v1, no hay bump de versión):** JS->nativo `listPresets`, `loadPreset{name}`,
  `savePreset{name}`; nativo->JS `presetList{presets,current}` y `presetError{operation,detail}`.
  Un load correcto cierra los gestos abiertos de la página y resincroniza TODO el estado
  (snapshot completo + presetList); un save correcto responde presetList.
- **Nombres no fiables:** `isUnsafePresetName` rechaza vacío, >100 chars, separadores, `..`,
  punto inicial y bordes con espacio → `presetError`, nunca un file write.
- **`PresetController` (interfaz en ParameterBridge.h):** el bridge posee el wire; el host
  inyecta un adaptador sobre el `PresetManager` del plugin (puntero NO owned: el procesador
  le sobrevive). Así `ParameterBridgeTest` prueba el protocolo con un backend falso, sin
  tocar `Documents/NEURONiK/Presets`.
- **El adaptador comprueba la existencia ANTES de cargar:** `loadPreset()` del PresetManager
  hace no-op silencioso si el fichero no existe; sin ese check un preset inexistente
  parecería cargar bien. `savePreset` verifica el fichero tras escribir.
- **Stats nuevos:** `presetsLoaded`, `presetsSaved`, `presetErrors`.
- **UI:** barra en `page.jsx` (select de carga + input/SAVE); deshabilitada en modo local.
  El hook expone `presetState/presetError/listPresets/loadPreset/savePreset` y pide la lista
  en el mount (tras `requestState`).
- **Tests:** sección 8 de `ParameterBridgeTest` (list/load/traversal/save/fail/no-backend),
  literales nuevos en el contrato anti-drift C++ y mjs, suite vitest del hook (42/42).
  Trampa encontrada: la sección 7 del test desmonta el sender con `setSender({})`; la sección
  8 debe reinstalar `recorder.sender()` o la respuesta no llega y el test segfaulta al indexar.
- **Pendiente de oído/vista:** cargar/guardar de verdad contra `Documents/NEURONiK/Presets`
  desde la ventana abierta (la barra está: seleccionar preset o escribir nombre y SAVE).

## Pantalla GENERAL en Next.js (2026-09-17, Fase 7)

Pestañas **BRIDGE/GENERAL** dentro de UNA página (`page.jsx`): el snapshot embebido sirve por
basename y una segunda ruta colisionaría con `index.html` (bug del 404 ya sufrido). La GENERAL
refleja la pestaña GENERAL nativa (IDs de `ParameterPanel.cpp`): motor, ADSR (gráfico SVG puro,
proporciones por tiempo real con piso mínimo, sustain a ancho fijo), unison, RANDOM y los tres
freeze. Puntos finos:

- **Un solo hook para ambas pestañas** (`SCREEN_PARAMETER_IDS = PILOT + GENERAL`): el estado es
  el mismo normalizado 0..1, así que un preset o un gesto nativo se ve en la pestaña que esté
  abierta. `ParamKnob`/`ParamToggle` pintan SU propia etiqueta (ParamControlShell): en GENERAL no
  se añade heading propio, solo un readout en unidades reales (`.control-value`).
- **Footer**: la validación es la del hook (`contractErrors`, validador NORMALIZADO). El anterior
  `validateState(controls.map(...), parameters)` comparaba estado normalizado contra rangos en
  unidades reales → errores falsos en cuanto entraron floats con min≠0 (envAttack 0.001).
- **pageContract.test.js** fija: `masterLevel` sigue siendo el `input[type=range]` nativo que el
  selftest conduce, `ParamSlider`/`ParamChoice` en la BRIDGE, `value={normalized}` y
  `handleChange(control.id…)` — todos los checks siguen en verde (vitest 42/42).
- **Selftest con 3er chequeo E2E (GENERAL)**: tras las dos direcciones de `masterLevel`, el
  selftest empuja `envAttack` (normalizado 0.5) desde nativo y lee el estado de la página desde
  el JSON del footer: los 11 ids de la GENERAL deben estar presentes y numéricos, y `envAttack`
  debe valer 0.5. El veredicto del selftest exige ahora las TRES direcciones
  (`selftestNativeToJsOk && selftestJsToNativeOk && selftestGeneralOk`).
- **CSS**: tabs/grupos/ADSR con tokens del tema (`--accent`, `--line`, `--muted`), sin frameworks
  (regla del skill JUCE_hybrid: vanilla CSS dentro del WebView).

**2ª compilación — enlace (previsto en el punto 1 del checklist):** 4x `LNK2019` sobre
`juce::MidiKeyboardComponent`/`KeyboardComponentBase` (los usa NEURONiKEditor, no PresetBrowser
como sospechábamos): `MidiKeyboardComponent` vive en `juce_audio_utils` → añadida al target del
host. Lección: la lib que falta no la indica el símbolo, hay que saber en qué módulo JUCE vive
cada componente.

**Trampa de pipeline corregida (build.bat):** el paso 4 era "blando" (aviso y continúa) y el
selftest del paso 8 corría igual con el exe VIEJO del host — un host sin recompilar daba
`RESULTADO: OK` engañoso porque el selftest solo prueba el bridge y la WebUI se carga en
caliente desde disco. Ahora el build recuerda `HOST_BUILD_FAILED` y, si el host no compiló,
**omite el selftest y sale con error**. Si un día quieres compilar solo el plugin, usa
`set WITH_SELFTEST=0` o compila los targets a mano.

## Teclado MIDI compartido en la página (2026-09-17, Fase 4)

La tab **KEYS** monta el teclado compartido `@abdsynths/midi-keyb` (el MISMO paquete que
consume ABDMS2000, evolucionado a v0.2.0 — nada de forks locales) + las ruedas de pitch/mod.
Puntos de diseño:

- **Wire (aditivo a v1):** JS->nativo `midiNoteOn{note,velocity}`, `midiNoteOff{note}`,
  `midiPitchBend{value -1..+1}`, `midiModWheel{value 0..1}`, `midiPanic` (sin campos);
  nativo->JS `midiNoteState{held[], pitchBend, modWheel}` (~6 Hz desde el timer del host).
  Campos fuera de rango → `stats.midiRejected`, jamás llegan al motor. Sin backend MIDI los
  actions se aceptan y se descartan en silencio.
- **`MidiController` (interfaz en ParameterBridge.h, patrón PresetController):** el bridge
  posee el wire; el host inyecta `MidiInjectionAdapter` sobre los `injectNoteOn/Off/`
  `injectPitchBend/injectController/requestAllNotesOff` del procesador — el MISMO camino
  lock-free (FIFO) que usa el teclado del editor nativo.
- **¡El piloto SUENA!:** `juce::AudioProcessorPlayer` + `AudioDeviceManager` con el dispositivo
  por defecto. Sin esto `processBlock` nunca corría en el host: las notas morían en el FIFO sin
  oírse y la telemetría del motor quedaba congelada. Orden de miembros: processor ANTES de
  deviceManager/player; el callback se desmonta en el destructor antes de que muera el
  procesador.
- **Notas mantenidas reales:** máscara de 128 bits (4×`atomic<uint32>`) plegada en
  `processBlock` desde el MIDI ya filtrado por canal (hardware + inyectado, ambas fuentes).
  Acumulativa entre bloques (el FIFO se vacía cada bloque; sin la máscara `held` parpadearía).
  `allNotesOff` del motor la pone a cero (el pánico no deja resaltes fantasma). Lectura por
  `getHeldNotes()` con bucle de desplazamiento (sin `<bit>`: el host compila C++17).
- **Feedback sin eco (API v0.2 del paquete):** `setPitchBend/setModWheel` mueven la rueda con
  `Wheel.setValue(n, false)` tras un gate `suppressWheelCallbacks` — visual sin re-disparar
  `onPitchBend/onModWheel` como si fuera input del usuario. `notesOffVisual` existe pero NO se
  usa en el loop de telemetría a propósito: el teclado mantiene su propio estado de pulsación y
  un repaint desde telemetría pelearía con el dedo del usuario. Nota fina: el slider del wheel
  es `min=-8192 max=8191` (¡centro 0, no 8192!) — el mapeo del feedback es
  `v>0 ? v*8191 : v*8192`.
- **Página (`KeysTab` en page.jsx):** contenedores por id (`#piano-keyboard`, ruedas) escritos
  con `innerHTML` y teclado creado UNA vez por mount de la tab (callbacks via `refs` para no
  recrearlo). PANIC de la página llama a `keyboard.panic()` (client-side, suelta sus teclas) +
  `sendMidiPanic()` (nativo, para hardware/DAW). `window.__pilotSendMidi` expone el camino de
  envío al host para el selftest.
- **Selftest 4ª dirección (MIDI):** la página envía noteOn/noteOff de la 60 por SU propio
  camino (`__pilotSendMidi`) y el host verifica la máscara de notas; en paralelo empuja
  CC1=64 nativo y lee el slider del wheel (`#mod-wheel-container .kbd-wheel-slider`, escala
  0..127 → normalizada) tras cambiar a la tab KEYS. 100% asíncrono (evaluateJavascript +
  Timers; el valor del wheel viaja en un `atomic<float>` miembro — las lambdas hermanas no
  pueden capturarse entre sí). Veredicto: las CUATRO direcciones.
- **Empaquetado pnpm (lección):** `pnpm install` desde WebPilot se colaba en el workspace raíz
  de la suite (`pnpm-workspace.yaml` de ABDSynths NO lista ABDNeural/WebPilot). Solución:
  WebPilot es ahora workspace anidado propio que incluye `ABDSharedAssets` y
  `ABDSharedCode/MidiKeyboard` como miembros (el `@abdsynths/shared@workspace:*` interno del
  paquete de teclado obliga a ello). Turbopack necesita `turbopack.root = raíz de la suite` en
  `next.config.mjs` (los symlinks resuelven fuera de WebPilot) y vitest necesita
  `server.fs.allow` para el setup compartido. El sprite `assets/bender.png` de la rueda se
  copió a `WebPilot/public/assets/` (el snapshot embebido lo sirve).
- **Tests:** sección 9 de `ParameterBridgeTest` (routing/rangos/panic/sin-backend), literales
  MIDI en el contrato anti-drift C++ y mjs, vitest del bridge (formas exactas + midiNoteState)
  y del hook (`__pilotSendMidi` + `midiState`, limpieza en unmount). 46/46 en verde.
- **VALIDADO por el usuario (2026-09-17):** el teclado suena y se comporta. El primer vistazo
  mostró el keybed colapsado a una tira negra — culpable: el CSS del CONTENEDOR (`.keys-strip`)
  no existía en la página (el keybed estira sus teclas a 100% de la altura del contenedor);
  el componente compartido estaba bien. Fix en `117ac33` y re-validado.

**Qué validar cuando compile** (en orden):

1. Que el host enlaza (primera vez con el plugin entero dentro; si falta un símbolo, añadir la
   lib JUCE que pida — candidata: `juce_audio_utils` si PresetBrowser toca file choosers).
2. `--selftest` en verde (el E2E no cambió: `masterLevel` sigue siendo el `input` nativo).
3. A la vista: mover un knob del panel nativo REAL (p. ej. ATTACK) y ver que la página no
   cambia (no es de las 4 que muestra) pero RANDOM sí debe mover morphX/morphY en la página;
   y mover un slider de la página debe verse en el knob VOLUME del panel.
4. Con la carpeta `WebPilot/out` renombrada, el host debe seguir cargando la UI (fallback
   embebido) y el log debe contar recursos con `[embedded fallback: N]`.

## Migrar la primera pantalla real a Next.js: qué falta (2026-09-16)

El piloto ya valida la cadena completa (Next.js estático + WebView2 + bridge bidireccional +
contrato versionado). Lo que sigue es migrar UI real del plugin. Candidato natural: la pestaña
**GENERAL** (`ParameterPanel`, `Source/UI/ParameterPanel.{h,cpp}`, 274 líneas), porque es la que
usa el contrato de parámetros tal cual (sliders/choices sobre IDs) sin piezas nativas especiales.

En orden, lo que falta:

1. **Doble host temporal (lo primero y más barato).** Dentro del `WebPilotHost`, sustituir la tira
   nativa de comparación por un `ParameterPanel` real del plugin (necesita `NEURONiKProcessor&`, no
   solo el APVTS: los botones de acción del panel leen el procesador). Verificación: mover un
   slider nativo del panel real mueve la página, y viceversa. Si esto pasa, la migración es
   mecánica.
2. **Transporte de acciones de panel.** El contrato de parámetros cubre sliders/choices, pero
   `ParameterPanel` también dispara acciones (randomize, preset save/load). Añadir al protocolo
   (versión 2, aditiva) un mensaje `panelAction { name, args? }` JS->nativo y registrar las
   acciones soportadas en el contrato JSON.
3. **Reutilizar el tema.** `ThemeManager` expone colores (surface, text, accent…). Exportarlos al
   bridge (un `themeChanged` nativo->JS o variables CSS inyectadas) para que la pantalla web y la
   nativa no diverjan visualmente.
4. **Componentes web equivalentes.** Mapear los controles custom JUCE (XYPad, LcdDisplay,
   EnvelopeVisualizer, SpectralVisualizer) a React. Para la pestaña GENERAL no hace falta ninguno;
   son la fase 2 de la migración (y probablemente canvas, no DOM).
5. **Estado del editor vs estado del plugin.** La página web hoy refleja el APVTS. Faltan:
   restaurar `window.__pilotReady` tras recarga (ya funciona) y decidir qué pasa con parámetros
   uiOnly (randomStrength) — hoy viajan y no tienen efecto DSP: documentar en el contrato que son
   "efectivos solo vía acciones".
6. **Decisión formal Next.js** (punto de decisión del ROADMAP) con los números reales de esta
   sesión: bundle 476 KB / 8 peticiones, arranque 886-1166 ms en caliente, y el coste del runtime
   (~572 KB crudo / 172 KB gzip) como suelo conocido.

No-bloqueantes pero a tener en cuenta: el host mide `document interactive` ~3.2 s en la primera
ejecución tras recompilar (perfil de WebView2 frío; en caliente vuelve a <1.2 s); si la migración
huele lenta, repetir la medición con el perfil caliente antes de culpar al framework.

## Cobertura de tests del lado web (auditoría 2026-09-16)

- **Hueco encontrado:** `WebPilot/lib/bridge.js` (la contrapartida JS del protocolo versionado)
  no tenía NINGÚN test: su filtrado de mensajes, modo local y dispose solo se ejercitaban en el
  selftest E2E. `parameters.js` además sin cubrir `defaultState`/`validateState`/
  `describeControl`/`divergentParameters`.
- **Relleno:** `tests/bridge.test.js` (9 tests contra un backend falso de `window.__JUCE__`:
  formato de cable exacto JS->nativo, `pageLoaded` en su propio event id, routing
  nativo->JS, mensajes malformados ignorados sin lanzar, dispose sin fugas, backend que
  lanza no tira la página) y `tests/parametersState.test.js` (11: siembra de defaults por
  tipo, validación de estado, divergencias, resumen de pantalla).
- **Suite WebPilot: 15 -> 35 tests.** Suite completa de la sesión: 10 C++ (ctest) + 35 web
  + 25 ABDSharedAssets.

## Contrato de parámetros (generado)

El contrato que consume la WebUI se genera desde el propio APVTS:

```text
Source/State/ParameterDescriptors.h/.cpp         (descriptores)
Source/State/ParameterDescriptorExport.h/.cpp    (emisión determinista)
Tests/ParameterExportTool.cpp                    (CLI)
Tests/ParameterDescriptorTest.cpp                (regresión)
WebPilot/generated/parameters.generated.{json,js,d.ts}
WebPilot/lib/parameters.js                       (adaptador de la UI)
```

Puntos clave de diseño:

- Los descriptores se leen del layout real mediante un `AudioProcessor` mínimo de sondeo
  (`LayoutProbe`) y un APVTS temporal: los IDs, rangos, intervalos, `skew`, defaults y listas de
  opciones no pueden divergir del plugin.
- `ParameterLayout` no es iterable en JUCE 8 (sus parámetros son privados), de ahí el sondeo.
  El probe y el APVTS viven solo durante la llamada porque APVTS arranca un temporizador.
- Leer los descriptores requiere un `MessageManager` inicializado. En herramientas de consola se
  crea un `juce::ScopedJuceInitialiser_GUI`.
- La generación es determinista (sin fechas), por lo que el test puede comparar los ficheros
  versionados con una exportación nueva.

Comandos:

```bash
cd ABDNeural
cmake --build build-reference --config Release --target NEURONiK_ParameterExport
./build-reference/Release/NEURONiK_ParameterExport.exe WebPilot/generated
ctest --test-dir build-reference -C Release --output-on-failure
```

Resultado actual: 70 parámetros, 1 ID declarado pero no enrutado (`oscPitchCoarse`).

### `WebPilot/generated/` se versiona (decisión del 2026-09-16)

Los tres artefactos (~70 KB de texto: 36 KB JSON, 32 KB JS, 1,6 KB `.d.ts`) **sí entran en git**:

1. `NEURONiK_ParameterDescriptorTest` los compara contra una exportación nueva, así que tenerlos
   versionados convierte cualquier divergencia en un **diff revisable**. Si se ignoraran, en un
   clone limpio el test fallaría hasta ejecutar la exportación y la garantía anti-drift no valdría
   nada fuera de esta máquina.
2. Los importa `lib/parameters.js` y el build de Next, de modo que el repositorio queda
   autocontenido (salvo `node_modules`).
3. Son deterministas y pequeños: no generan ruido de diff.

Se regeneran en el paso 2/7 de `build.bat`. `WebPilot/.gitignore` lleva un comentario explícito
para que nadie añada una regla que los ignore. `git check-ignore` confirma que no están ignorados.

El caso "fichero ausente" del test falla con un mensaje que indica que hay que ejecutar
`NEURONiK_ParameterExport`, así que una pérdida accidental se detecta y se explica sola.

El adaptador `WebPilot/lib/parameters.js` añade lookup, recorte a rango, ajuste a intervalo,
conversión `0..1` con la misma matemática de `skew` que JUCE, formato de valor, estado por defecto
y validación del contrato. El panel del piloto ya no contiene rangos ni defaults escritos a mano.

## Conexión de parámetros divergentes: implementado (2026-09-16)

### Estado

```text
implemented 65 · uiOnly 4 · notRouted 1 · notInLayout 1
```

Conectado en esta pasada:

- `midiChannel` → filtrado de entrada (`Source/Main/MidiChannelFilter.h/.cpp`).
- `allNotesOff()` nuevo en `ISynthesisEngine` (una implementación en `BaseEngine`) y disparado
  con un flag atómico al cambiar de canal.
- `masterBPM` y `lfo1/2SyncMode`/`RhythmicDivision` → tempo sync real en ambos LFO.
- `fxDelaySync`/`fxDelayDivision` → tiempo del delay en duración de nota.
- `fxChorusRate`/`Depth` y `fxReverbSize`/`Damping`/`Width` → `setParameters()` completo.
- `velocityCurve` → `Source/Main/VelocityCurve.h/.cpp`, aplicado a los note-on entrantes.
- `midiThru` → `NEEDS_MIDI_OUTPUT TRUE` y limpieza del búfer de salida cuando está apagado.

Tests nuevos:

```text
NEURONiK_MidiChannelFilterTest   20 comprobaciones
NEURONiK_LfoSyncTest             19 comprobaciones
NEURONiK_VelocityCurveTest       36 comprobaciones
NEURONiK_PresetRoundTripTest     31 comprobaciones  (verificado con build.bat 2026-09-16)
```

Los tres registrados en CTest: la suite pasa 6/6 sin hardware (build.bat, 2026-09-16, pasos 1-7 con
ModelMaker omitido por diseño).

`NEURONiK_DSPReferenceTest` cubre además el camino de pánico: tras `allNotesOff()` las voces
dejan de ser activas cuando termina el release (≈4,4 s con el release de 500 ms por defecto, ya que
la envolvente decae de forma multiplicativa hasta `1e-4`). Se expone como `DspEngineFacade::allNotesOff()`
para que la frontera web futura tenga el mismo pánico.

### Cambio de comportamiento a tener en cuenta

1. **Semántica del sync del LFO**: `getSyncedRateHz()` dividía mal (multiplicaba), así que "1/8"
   daba un ciclo cada dos negras. Ahora la división es longitud de nota. Nada llamaba a esa ruta,
   así que solo afecta a presets con `Tempo Sync` guardado, que antes no hacían nada.
2. **Aftertouch/pitch bend/CC74 ya no tratan el canal 1 como "todos"**. Es más correcto, pero si
   algún controlador dependía del comportamiento antiguo, se notará.
3. **`midiChannel` ahora silencia MIDI de otros canales**, incluido el del host. El teclado en
   pantalla sigue sonando siempre.
4. **Los efectos completos responden** (`chorus rate/depth`, `reverb size/damping/width`,
   `delay sync/division`). Con los defaults del APVTS el sonido no cambia, porque coinciden con
   los valores iniciales de los smoother del DSP; se pide de todos modos una validación de oído.

### Parámetros sin consumidor: decididos (2026-09-16)

| ID | Decisión | Por qué |
|---|---|---|
| `velocityCurve` | Conectado | Default `Linear` = identidad, así que ningún preset cambia de respuesta |
| `midiThru` | Conectado, opt-in | El control ahora manda: apagado (default) el plugin no emite MIDI al host |
| `unisonEnabled` | Control retirado de `ParameterPanel` | La puerta habría silenciado el unison de todos los presets (default off). El parámetro se conserva para que carguen |
| `harmMix` | **Retirado del layout** | Nadie lo leía y no había diseño de sonido detrás. Mantener un parámetro muerto solo compensa si alguien lo lee; retirarlo exige migrar los presets, y esa migración existe ahora |

Nuevo cambio de comportamiento a tener en cuenta, además de los anteriores:

5. **`midiThru` apagado significa apagado**: antes el búfer de entrada se devolvía siempre, con
   lo que el toggle no servía de nada. Quien contara con ese eco accidental (por ejemplo, para
   encadenar otro instrumento) debe activar el parámetro. La ficha del plugin pasa a declarar
   salida MIDI, que es lo que el runtime ya anunciaba con `producesMidi()`.
6. **La curva de velocidad se aplica al teclado en pantalla**: es intencionado (la curva es
   respuesta del instrumento, no de un puerto), y con `Linear` no cambia nada.
7. **`harmMix` ya no existe como parámetro** (el total baja de 71 a 70). Los presets guardados
   que lo llevan cargan igual: los hijos desconocidos se ignoran y además se eliminan del estado
   al cargar, de modo que un re-guardado no vuelve a escribir el id muerto.
8. **Un parámetro ausente de un preset vuelve a su default**, no conserva el valor que tuviera
   cargado. Es comportamiento del APVTS, y es el deseable para presets antiguos: no heredan
   valores de otro preset. Está fijado con una prueba.

### Migración de presets

`Source/Serialization/PresetMigration.{h,cpp}`: antes de `replaceState()` se retiran los hijos
`<PARAM>` cuyo `id` no esté entre los parámetros actuales, y también el `METADATA` del fichero
(metadato del preset, no estado del plugin; si se quedaba dentro, se duplicaba al reescribir).
La función es pura sobre un `ValueTree`, así que se prueba sin tocar ficheros del usuario.

`Tests/PresetRoundTripTest.cpp` cubre: round trip de valores reales (incluido un rango con `skew`
y dos `choice`), `unisonEnabled` (retirado de la UI pero vivo) que sigue cargando, preset legado
con `harmMix` más un id futuro inexistente, re-guardado sin ids muertos, etiquetas que sobreviven,
fichero inexistente, XML malformado y preset sin parámetros.

**Neutralidad sonora, hecha prueba (2026-09-16).** "Un preset antiguo suena igual" es una
afirmación por parámetro, así que en lugar de dejarla a oído se comprueba: se guardan dos ficheros
idénticos salvo por un hijo `<PARAM id="harmMix">`, se cargan ambos y se comparan los **70 valores**
del estado resultante como texto canónico ordenado. Deben ser exactamente iguales:

```text
canonicalParameterState(load(limpio)) == canonicalParameterState(load(con harmMix))
```

Es una garantía más fuerte que escuchar un preset, porque cubre los 70 parámetros y no una
impresión. La parte que sigue siendo auditiva es la comparación de la pareja de EXEs (ver
`Versiones compiladas\`); en esta máquina no hay presets guardados
(`Documents\NEURONiK\Presets` no existe), así que hay que crear uno con el build antiguo primero.

## Detalle histórico de la propuesta (referencia)

### A. `midiChannel` — filtrado de entrada

El canal se lee en `handleMidiEvent` solo para etiquetar la voz, nunca para descartar. Cambio
propuesto en `NEURONiKProcessor::processBlock()`, justo después de inyectar la FIFO de la UI y
antes de `renderNextBlock()`:

```cpp
// miembro reutilizado: juce::MidiBuffer channelFilteredMidi;
const int channelSetting = juce::roundToInt (apvts.getRawParameterValue (IDs::midiChannel)->load());
if (channelSetting > 0)
{
    channelFilteredMidi.clear();                       // conserva capacidad entre bloques
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const bool isSystemMessage = message.getChannel() == 0;   // sysex, clock, ...
        if (isSystemMessage || message.getChannel() == channelSetting)
            channelFilteredMidi.addEvent (message, metadata.samplePosition);
    }
    midiMessages.swapWith (channelFilteredMidi);
}
```

Puntos a resolver en el mismo cambio:

1. **Notas colgadas al cambiar de canal**: añadir `virtual void allNotesOff() = 0` a
   `ISynthesisEngine` e implementarlo por motor (cada uno tiene sus voces). Llamarlo desde
   `parameterChanged` cuando cambie `midiChannel`. Como extra, habilita un botón de pánico.
2. **Inconsistencia existente**: el aftertouch y el CC74 usan
   `(v->getChannel() == channel || channel == 1)`, es decir, el canal 1 actúa como "todos". Con
   filtrado real eso sobra: una instancia en canal 3 no debería responder a aftertouch de canal 1.
3. **MIDI learn**: cuando se implemente, debe ignorar el filtro (o avisar), o no se podrá mapear
   un controlador que emita en otro canal.

Riesgo: bajo. Coste estimado: 1–2 h + pruebas.

### B. `masterBPM` y sync de LFO — 4 cambios pequeños

El LFO ya soporta tempo sync; solo falta el cableado.

1. `GlobalParams`: añadir `double bpm = 120.0;` (hoy no existe ningún campo de tempo).
2. `BaseEngine::updateParameters()`: aplicar la configuración completa del LFO, no solo
   waveform/rate/depth:

```cpp
const double bpm = currentGlobalParams.bpm;
auto applyLfo = [bpm] (Core::LFO& lfo, const GlobalParams::LFOParams& p)
{
    lfo.setWaveform (static_cast<Core::LFO::Waveform> (p.waveform));
    lfo.setRate (p.rateHz);
    lfo.setDepth (p.depth);
    lfo.setSyncMode (p.syncMode == 0 ? Core::LFO::SyncMode::Free
                                     : Core::LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (bpm);
    lfo.setRhythmicDivision (quarterNotesForDivisionIndex (p.rhythmicDivision));
};
applyLfo (lfo1, currentGlobalParams.lfo1);
applyLfo (lfo2, currentGlobalParams.lfo2);
```

3. Tabla de divisiones, en un único sitio, reutilizable después por el delay:

```cpp
// mismo orden que {"1/1","1/2","1/4","1/8","1/16","1/32","1/4t","1/8t","1/16t"}
static constexpr float kDivisionInQuarterNotes[] = {
    4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f
};
```

4. `synchronizeEngineParameters()`: rellenar `bpm`, `lfo1.syncMode`, `lfo1.rhythmicDivision` y
   los equivalentes de `lfo2`. Los dos bloques del motor duplican ~20 líneas: conviene extraer un
   `applyGlobalParams()` antes de añadir una tercera copia.

Detalles importantes:

- **Rango a alinear**: el parámetro admite 20–400 BPM pero `LFO::setTempoBPM` valida 10–300. Si no
  se toca, a 350 BPM el LFO usará 300 en silencio. Recomendado: ampliar el LFO a 400.
- **Seguridad de presets**: el default de `lfo1/2SyncMode` es `Free` (índice 0), así que conectar
  el sync **no cambia el sonido** de ningún preset existente salvo que ya tuviera `Tempo Sync`
  guardado, que hasta ahora no hacía nada.
- **Efecto colateral deseado**: `fxDelaySync`/`fxDelayDivision` pueden conectarse después con la
  misma tabla; hoy el delay siempre interpreta su tiempo en segundos.

Prueba propuesta (sin hardware): en `Tests/DSPReferenceTest.cpp`, con `syncMode = TempoSync`,
`division = 1/4` y `bpm = 120`, el periodo del LFO debe ser ≈0.5 s; y una prueba de regresión en
modo `Free` que confirme que `rateHz` sigue mandando.

Riesgo: bajo (plomería aditiva). Coste estimado: 2–4 h con pruebas. Validación final auditiva.

## Documentación de parámetros

El inventario actual de parámetros DSP está en:

```text
DSP_PARAMETERS.md
```

Incluye rangos, defaults, conversiones de tiempo, parámetros por motor y divergencias conocidas entre la definición APVTS y la sincronización actual. Debe actualizarse antes de convertir ese contrato en TypeScript.

## Criterio para no desperdiciar trabajo web

El primer objetivo web no es migrar ABDMS2000 ni toda la interfaz de NEURONiK. Es validar este circuito mínimo:

```text
Next.js export estático
        ↓
panel pequeño
        ↓
modelo de parámetros simulado
        ↓
WebView2
```

La decisión de continuar con Next.js se tomará después de comprobar empaquetado, recursos, ciclo de vida y complejidad. Si falla, la alternativa será React/Vite y la arquitectura de parámetros deberá seguir siendo reutilizable.

## Procedimiento de build

El proyecto usa CMake y busca JUCE en `C:/JUCE` o mediante `JUCE_PATH`.

### `build.bat` (recomendado)

Un solo comando hace el ciclo completo y **termina siempre con pausa**, tanto si sale bien como si
falla, para poder copiar la salida:

```bat
build.bat                    :: 10 pasos: contrato + WASM + WebUI del plugin + plugin + piloto + tests + selftest
build.bat build              :: lo mismo, en un directorio de build limpio
build.bat modelmaker         :: además compila ModelMaker (incrementa Source\ModelMaker\Version.h)
build.bat build modelmaker   :: build limpio incluyendo ModelMaker
build.bat tests              :: modo rápido: solo contrato + suite (ni plugin ni WebUI)
build.bat noselftest         :: omite el E2E del bridge (paso 10)
build.bat nowasm             :: omite el WASM del worklet (por defecto, si falla, aborta el build)
build.bat nextui             :: WebUI del piloto con Next en vez de Vite (referencia)
```

Pasos que ejecuta, en orden:

```text
1/10  cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release
2/10  NEURONiK_ParameterExport + regeneracion de WebPilot\generated
3/10  build_wasm.bat: WASM + paridad + smoke + sync del worklet (aborta el build si falla)
4/10  WebUI: pnpm build              -> WebUI\dist  (la interfaz QUE EMBIBE EL PLUGIN)
5/10  NEURONiK_Standalone + NEURONiK_VST3   (embeben WebUI\dist en el enlace)
6/10  WebPilot: pnpm build            -> WebPilot\out (el piloto; se omite si no hay node_modules)
7/10  NEURONiK_WebPilotHost           (si falla, solo avisa; embebe WebPilot\out)
8/10  NEURONiK_ModelMaker             (solo con 'modelmaker', ver abajo)
9/10  compilacion de los 17 tests + ctest --output-on-failure
10/10 selftest del bridge del piloto  (se omite con 'noselftest'; ver arriba)
```

**El orden es la dependencia, no un gusto:** el `.wasm` lo produce 3/10, la WebUI lo copia a su
`dist` por el `publicDir` en 4/10, y el **plugin lo embebe en el enlace** en 5/10
(`juce_add_binary_data` sobre `WebUI\dist\*`). Con el plugin antes, se quedaba dentro el bundle y
el DSP de la pasada anterior (sintoma mudo: suena "el de antes"). Lo mismo vale para el host del
piloto, que embebe `WebPilot\out`.

Dos guards de staleness por la misma razon, y los dos abortan:

- 3/10/4/10: si `WebUI\dist\worklet\neuronik_dsp.wasm` falta o **no coincide** con
  `build-wasm\neuronik_dsp.wasm`, el plugin sonaria con un DSP viejo.
- 6/10/7/10: el mismo chequeo sobre `WebPilot\out\worklet` para la bancada del piloto.

Artefactos:

```text
<dir>\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe      (con WebUI\dist embebida)
<dir>\NEURONiK_artefacts\Release\VST3\NEURONiK.vst3            (con WebUI\dist embebida)
<dir>\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe
<dir>\Release\NEURONiK_ModelMaker.exe                  (solo con 'modelmaker')
```

### La interfaz del plugin (`WebUI/`): la construye `build.bat` y la sirve el plugin

**Desde 8.1 (2026-09-19) la pagina ES la interfaz del plugin**: `NEURONiKEditor` monta
`Source/WebUI/NeuronikWebView.h`, que sirve `WebUI/dist` desde una copia **embebida** en el
binario. Por eso `build.bat` la construye en 4/10, antes de compilar el plugin, y por eso
`WebUI/dist` ya no es "una carpeta que no consume nadie".

El piloto React (`WebPilot/out`, 6/10 y 7/10) sigue vivo **solo mientras 8.1 no cierre el paso
2c**, porque su bancada es la unica implementacion del selftest de cuatro direcciones. Despues se
retira (ver `ROADMAP.md`, "Retirada del piloto").

```bash
cd ABDNeural/WebPilot && pnpm install   # WebUI es miembro del workspace anidado de WebPilot
cd ../WebUI && pnpm test                # 96 tests (vitest + jsdom)
cd ../WebUI && pnpm dev                 # navegador, modo local (sin host) — el bucle de iteracion
cd ../WebUI && pnpm build               # -> WebUI/dist  (lo mismo que hace el paso 4/10)
```

Los assets compartidos y el worklet no se duplican: el `publicDir` de Vite apunta a
`WebPilot/public`, asi que `dist\worklet\` sale con el procesador y el `.wasm` que acaba de
compilar el paso 3/10; y los estilos de `@abdsynths/shared` los **empaqueta Vite en el bundle**
(no se piden al disco en tiempo de ejecucion). Los selectores que el `--selftest` lee de la
pagina estan documentados y probados en `WebUI/README.md` (y en `WebUI/tests/`).

**Iterar la interfaz sin recompilar el plugin** (override de desarrollo, apagado por defecto):

```bat
set NEURONIK_WEBUI_DEV_DIR=D:\desarrollos\ABDSynths\ABDNeural\WebUI\dist
```

Con esa variable puesta, el plugin sirve la pagina desde disco: `pnpm build` (menos de un
segundo) y recargar, en vez de recompilar el plugin entero por cada retoque de CSS. Sin la
variable, el binario sirve su copia embebida y **no toca el disco** (cero rutas grabadas: el
VST3 funciona copiado a cualquier sitio).

El paso lento es el plugin: varios minutos la primera vez (compila JUCE y las dos variantes,
Standalone y VST3) y
bastante menos después, porque solo recompila lo que cambie. Si solo hay que tocar pruebas, es más
rápido pedir el target concreto:

```bash
cmake --build build-reference --config Release --target NEURONiK_PresetRoundTripTest
ctest --test-dir build-reference -C Release --output-on-failure
```

### ModelMaker: fuera del build por defecto (decisión)

`NEURONiK_ModelMaker` **no** se compila salvo que se pida con `modelmaker`. Motivos:

1. **Modifica un fichero versionado.** El target arrastra `add_dependencies(... UpdateVersion)`, que
   ejecuta `Scripts/update_version.ps1` e incrementa `NEURONIK_MODELMAKER_VERSION_SUB` en
   `Source/ModelMaker/Version.h` (fichero trackeado por git, ahora en 0.1.28). Si estuviera en el
   camino por defecto, cada build diario ensuciaría el árbol y forzaría recompilaciones.
2. **Es otro entregable.** Aplicación WIN32 independiente; no forma parte del plugin ni del
   contrato, así que mezclarla con el build del sintetizador hace más lento el ciclo normal.
3. **Históricamente se compilaba en otro directorio** (`build_neuronik`, ver más abajo), así que
   tampoco es que la caché de `build-reference` lo espere.

Pero tampoco conviene olvidarla: comparte con el plugin `Oscillator.cpp`, `Resonator.cpp` y
`NEURONiK_Common`, de modo que un cambio en el núcleo DSP puede romperla sin que nadie se entere.
De ahí que exista el modo opt-in: úsalo tras tocar esos ficheros.

### Builds guardadas

Las copias de entregables se guardan en:

```text
Versiones compiladas\
```

Convención de nombre: `<Producto>_<AAAA-MM-DD>_<HHMM>_<motivo>.exe`.

### Pareja de referencia (2026-09-16): antes y después de retirar `harmMix`

```text
NEURONiK_2026-09-16_1444_pre-harmMix-retiro.exe     5.376.512 bytes
md5 e6d6c239f9bc8d16de1bc1cfc79d53a2     Standalone 2026-09-16 14:44

NEURONiK_2026-09-16_1517_post-harmMix-retiro.exe    5.383.168 bytes
md5 f15de044138afd3d1a61d5e8256cbd05     Standalone 2026-09-16 15:17
```

La segunda es el build validado con `build.bat`:

```text
100% tests passed out of 6  (DSPReference, MidiChannelFilter, VelocityCurve,
                             LfoSync, ParameterDescriptor, PresetRoundTrip)
contrato: 70 parametros · 65 wired · 4 UI only · 1 not routed · 1 not in the layout
```

Comprobado también después del último `build.bat` (2026-09-16 15:4x): el md5 del Standalone
**sigue coincidiendo** con esta copia, así que es una referencia válida y no un binario de una
revisión anterior.

Sirven como A/B de oído: entre las dos solo cambian la retirada de `harmMix`, la curva de
velocidad, el MIDI thru opt-in y las sincronizaciones ya documentadas. Con los defaults, la
diferencia audible esperada en presets existentes es **ninguna**; en el A/B, el default de
`velocityCurve` es `Linear` (identidad) y el de `midiThru` es off (antes el búfer se devolvía
siempre, lo que solo se nota si el host leía el MIDI del plugin).

### Build post-wrappers (2026-09-16 21:30)

Primera build con la familia de controles compartidos en la WebUI del piloto (wrappers React
`ParamSlider`/`ParamChoice` + `useParameterControls`; ver sección "Wrappers React en el
WebPilot"). Validada con `build.bat` completo: 10/10 tests + selftest del bridge OK
(NATIVO->JS y JS->NATIVO).

```text
NEURONiK_2026-09-16_2130_post-wrappers.exe                 5.383.168 bytes (Standalone)
NEURONiK_WebPilotHost_2026-09-16_2130_post-wrappers.exe    3.453.440 bytes (host del piloto)
```

Nota: el Standalone quedó **byte-idéntico** a la copia de las 15:17 (los wrappers viven en la
WebUI, no en el plugin); el binario que cambia es el host del piloto, y la WebUI nueva es la
que `build.bat` exporta a `WebPilot\out` (paso 5/8) — el host la carga desde ahí en caliente.

Detalles:

- Los `.exe` están cubiertos por `.gitignore`, así que estas copias no entran en el repositorio.
- Convención sugerida: guardar una copia **antes** de un cambio que pueda alterar el sonido y otra
  después, con el motivo en el nombre.
- `Versiones compiladas\NEURONiK_ModelMaker\` contiene un **acceso directo** (no el binario). Se
  rehizo el 2026-09-16: apunta a `build-reference\Release` (donde `build.bat modelmaker` deja
  `NEURONiK_ModelMaker.exe`) y su descripción recuerda cómo generarlo. El anterior apuntaba a
  `D:\desarrollos\ABDNeural\build_neuronik\Release`, ruta de cuando el proyecto vivía en otra
  carpeta.

### `build.ps1` (eliminado el 2026-09-16)

Estaba desfasado: construía **todos** los targets en `build/` (por tanto incluía ModelMaker y
tocaba `Version.h`), no ejecutaba pruebas y no pausaba. Se comprobó que ningún script lo
referenciaba y se eliminó; `build.bat` lo sustituye por completo. La eliminación queda como cambio
pendiente de commitear (el fichero estaba trackeado).

### Nota sobre `UpdateVersion`

El sistema de build tiene un target `UpdateVersion` que modifica automáticamente
`Source/ModelMaker/Version.h`. Revisar `git status` después de compilar y descartar cualquier
incremento accidental que no forme parte de la tarea.

## Persistencia de estado (Fase 4 cerrada, 2026-09-17)

`NEURONiK_StatePersistenceTest` (procesador REAL, mismas fuentes que el host): roundtrip
de sesión DAW completo — editar parámetros + mapping MIDI en A, getStateInformation,
setStateInformation en B (instancia fresca), y comparar: 0 diferencias en el APVTS
completo, mapping restaurado, y contratos de robustez (null/basura/estado extranjero
ignorados sin corromper el estado vivo; estado de build antigua con ids extra aplica lo
conocido y conserva defaults).

**Bug real destapado por el test:** `MidiMappingManager::saveToValueTree` terminaba con
`v.getOrCreateChildWithName("MIDIMAPPINGS") = midiNode;` — la asignación de ValueTree en
JUCE **no copia contenido, re-referencia objetos** (semántica de puntero compartido). El
nodo quedaba vacío y TODO mapping de MIDI Learn se perdía silenciosamente al guardar la
sesión del DAW (los presets no estaban afectados: `saveToValueTree` solo lo llama
`getStateInformation`). Fix: `removeChild` + `appendChild`, con comentario explicando la
trampa para que nadie "simplifique" de vuelta.

**Pipeline:** `build.bat` reordenado — la WebUI ANTES del host (hoy 5/9 y 6/9; eran 4 y 5 cuando
el WASM todavía iba fuera del build), porque el
host embebe `WebPilot/out` en el enlace (`juce_add_binary_data`); compilar el host antes
dejaraba dentro el bundle de la pasada anterior. El test nuevo (`NEURONiK_StatePersistenceTest`)
entró en el paso 7.

## Reglas de comparación

Antes de cambiar el DSP:

- ejecutar `NEURONiK_DSPReferenceTest`;
- conservar `build-reference`;
- compilar el nuevo Standalone en otra carpeta si es necesario;
- comparar primero compilación y RMS/peak;
- comprobar manualmente el mismo preset en el EXE de referencia y en el nuevo.

No borrar ni sobrescribir `build-reference` hasta generar una nueva referencia validada.

## Decisiones pendientes (histórico, 2026-09-16)

> Resueltas desde entonces: el stack es **JS vanilla + Vite** (Next.js retirado el 2026-09-19),
> el primer panel migrado fue GENERAL y el lienzo 8.2 ya cubre los 70 parámetros, y los presets
> viajan por el **protocolo v1 del bridge** (2026-09-17). Lo que queda por decidir vive en la
> **Fase 8** de `ROADMAP.md`.

- Si la interfaz web piloto se hará inicialmente con React/Vite o Next.js estático.
- Qué panel será el primero en migrar.
- Si el primer WASM incluirá ambos motores (`Neuronik` y `Neurotik`) o solo uno.
- Qué formato de preset común se utilizará entre APVTS, web y WASM.
- Qué componentes de `ABDSharedCode` se extraerán sin acoplarlos a Next.js.

## No hacer todavía (histórico, 2026-09-16)

> Frenos del arranque de la migración. Los vigentes los fija la **Fase 8** de `ROADMAP.md`
> (p. ej. solo este proyecto —ABDMS2000 no se toca— y no revivir las homonimias retiradas, ver
> 8.5). Dos se siguen cumpliendo por diseño: el APVTS sigue siendo la SSOT de parámetros y los
> IDs no se duplican a mano (viajan en el contrato generado).

- No reescribir todo `NEURONiKProcessor`.
- No eliminar APVTS.
- No duplicar los IDs de parámetros en JavaScript.
- No migrar toda la interfaz JUCE antes de validar un panel piloto.
- No añadir Next.js al proyecto principal sin probar antes exportación estática y WebView2.
- No comparar solo por oído: mantener pruebas automatizadas de audio.

## 2026-09-17 (b): Siembra de juce::Random + bomba de paréntesis en build.bat

**Siembra Random (regla 7A del skill JUCE hybrid — preparación WASM):**
- `LFO` ahora toma semilla explícita por constructor (antes: `getMillisecondCounter`, no determinista; y el miembro pasaba por el ctor por defecto, que busca entropía del sistema).
- `BaseEngine` instancia `lfo1`/`lfo2` con semillas distintas (S&H decorrelacionado).
- `NeurotikVoice(int voiceIndex)` siembra determinista y única por voz (con semilla idéntica, el ruido de excitación sería idéntico entre voces en unison: artefacto audible).
- `ParameterPanel::randomizeParameters` usa instancia local sembrada con reloj en vez de `getSystemRandom()` (UI, no RT; pero mismo principio).

**Bug del pipeline (explicaba builds que morían en silencio):** en la rama de error del paso 4 había un `(staleness)` sin escapar dentro de un bloque `if (...)`. cmd parsea el bloque entero aunque la condición sea falsa: el `)` cerraba el bloque prematuramente y el script moría con "No se esperaba . en este momento" justo al terminar el paso 4 — sin llegar nunca al `pause` final. Escapado como las demás líneas. Cada pasada ahora deja además `build-last-run.log` (envoltorio PowerShell Tee-Object, UTF-8, exit code propagado).

## 2026-09-17 (c): Fase 6 — spike Vite contra-piloto (A/B cerrado)

**Qué es `WebPilotVite/`**: contra-piloto que compila la MISMA página que Next
(`WebPilot/app/page.jsx` + `lib/`, importados 1:1 — cero copias) con Vite en vez de
Next. Solo añade: `index.html`, `src/main.jsx` (entry que importa los CSS globales que
en Next llevaba el layout), `src/main.css` (reset mínimo) y `vite.config.js`.
Es miembro del workspace anidado de WebPilot (`pnpm-workspace.yaml`), así que los
paquetes compartidos (@abdsynths/shared, @abdsynths/midi-keyb) resuelven igual.

**Veredicto técnico: Vite gana en todo lo medible.**

| Métrica | Next (next build) | Vite (vite build) |
|---|---|---|
| Bundle en disco | 854 KB / 10 recursos | **471 KB / 4 recursos** (-45%) |
| Tiempo de build | 15-25 s | **2-6 s** |
| Selftest del host | RESULT: OK, exit 0 | **RESULT: OK, exit 0** (mismo árbitro) |
| Fallback embebido | 0 | 0 (con el fix de publicDir) |
| Config extra | turbopack.root a la suite (obligatorio) | root local (resuelve symlinks solo) |

**A/B hecho con swap de directorios** (`out` ↔ `out-vite`) sobre el MISMO exe del host,
modo disco y modo embebido (rebuild del target del host). El arranque (~7.2-7.9 s react
ready) está dominado por el arranque frío de WebView2 y resultó empardado: el ahorro
real del bundle Vite se verá en el parseo JS y en memoria, no en el primer paint del
host de prueba.

**Trampas del camino:**
1. `vite.config.js` con `--config` relativo + `pnpm --filter` falla (duplicación de
   ruta). Sin `--config`: Vite encuentra vite.config.js en su cwd.
2. Con raíz elevada a la suite (como hicimos en Turbopack) Rollup exige `input`
   explícito y los `/src/...` absolutos del HTML se resuelven contra la raíz. Con raíz
   local todo funciona sin trucos — NO es necesaria la raíz-suite en Vite.
3. `publicDir` por defecto es `<root>/public`: el sprite `bender.png` del wheel vive en
   `WebPilot/public/` y había que apuntarlo a mano (si no, el fallback embebido se
   come 1 recurso y el A/B del snapshot queda cojo).

**Cómo construirlo**: `cd WebPilot && pnpm --filter @abdsynths/web-pilot-vite build`
(salida: `WebPilot/out-vite/`). Para probarlo en el host: swap `out` ↔ `out-vite` y
relanzar; para embeberlo, rebuild del target `NEURONiK_WebPilotHost` con el swap hecho.

**Pendiente de decisión**: el switch definitivo (migrar `build.bat` paso 4 a Vite y
dejar Next solo como referencia, o mantener ambos). El piloto quedó funcional en ambos.

## 2026-09-17 (d): Switch del paso 4 a Vite — Next queda como referencia

- `build.bat` (paso 4 entonces, **5/9 hoy**) ahora construye con **Vite** por defecto:
  `pnpm --filter @abdsynths/web-pilot-vite build`, salida DIRECTA a `WebPilot/out`
  (la ruta que consumen el snapshot embebido y el selftest — nada cambia de sitio).
  `emptyOutDir` deja `out/` solo con ficheros del motor activo: no se mezclan restos.
- **`build.bat nextui`** conserva la ruta Next intacta (misma página, otro empaquetador)
  para comparaciones futuras: `build.bat nextui [modelmaker|noselftest|tests|<dir>]`.
- `vite.config.js`: outDir a `../WebPilot/out` (antes `out-vite`, que ya no existe).
- Validado: pasada completa por defecto (Vite, 4 recursos/445 KB, selftest OK, 11/11
  ctest), pasada `nextui noselftest` (Next OK) y restauración canónica por defecto.

## 2026-09-17 (e): Fase 1 — frontera DSP separada (paridad bit-exacta)

**Qué se hizo:**
- `DspTypes.h` NUEVO: `GlobalParams` extraído del header de `ISynthesisEngine` a un
  header POD **sin JUCE** (única dependencia: el propio fichero). `ISynthesisEngine.h`
  lo incluye hacia atrás — nadie más cambió.
- `DspEngineFacade.h` ya no incluye juce_*.h (solo `DspTypes.h` + `DspEvent.h`, con
  forward-declaration del engine). El `#include <juce_audio_basics>` vive solo en el
  `.cpp`, donde también vive el adaptador `Event`→`MidiMessage`: la separación de
  tipos de evento del transporte JUCE es real (un host WASM nunca ve juce_*).
- API de parámetros en la fachada: `setGlobalParams` / `setPolyphony` (delegan en el
  engine, que hace el handoff RT-safe).
- Contrato de `ISynthesisEngine` documentado EN la cabecera: ciclo de vida,
  restricciones RT de `renderNextBlock`, hilos de los getters, y qué tipos cruzan
  la frontera.
- `DSPReferenceTest` ampliado con **paridad de frontera**: mismo motor, misma
  secuencia de notas (on → 8 bloques → off → 12 de cola) por la ruta JUCE directa
  (`renderNextBlock` + `MidiBuffer`) y por la ruta fachada (punteros crudos +
  `Runtime::Event`); comparación **muestra a muestra sin tolerancia**. Resultado:
  bit-exacta en 2×512 muestras (peak 0.216). Viable porque el DSP por defecto es
  determinista: semillas constantes (siembra del 2026-09-17) y `entropyAmount=0`
  (el jitter del resonador usa `getMillisecondCounter` pero está cortado a <0.001 —
  si algún día se activa por defecto, ese path necesitará sembrado determinista).

**Estado de la frontera:** `DspTypes.h` + `Runtime/*` compilan sin JUCE; el motor
interno (`BaseEngine`, voces, efectos) sigue usando juce_* por decisión de diseño
transicional (casilla abierta a propósito en el roadmap, se ataca con la Fase 5/WASM).

## 2026-09-17 (d) — Fase 5: DSP real compilado a WASM (smoke test Node en verde)

- `wasm/CMakeLists.txt` + `build_wasm.bat`: compilan el DSP REAL (sin port) sobre la
  frontera de la Fase 1 (DspEngineFacade). JUCE 8.0.12 con em++ requiere: (a) entorno
  de Visual Studio ANTES que emsdk en el PATH (bootstrap de juceaide; el bat lo auto-arma
  via vswhere), (b) generador Ninja (el generador VS no soporta el toolchain em++),
  (c) `-includeemscripten.h` global (bug de juce_SystemStats_wasm.cpp en 8.0.12: usa
  emscripten_get_now() sin incluir <emscripten.h>), (d) parche minimo en
  C:/JUCE/modules/juce_core/native/juce_ThreadPriorities_native.h (rama JUCE_WASM con
  tabla a 0, igual que LINUX; auditoria del hash original en /tmp/juce_patch_audit.txt —
  replicar el parche si se actualiza JUCE).
- `DspSources.cmake` (nuevo): lista DSP single-source compartida nativo<->WASM.
  Leccion ABDMS2000 aplicada (su WASM duplicaba la lista a mano y sufrio drift: le
  faltaban 4 fuentes del nativo, incluido SynthEngine.cpp).
- `SIMDWrapper.h` dual: nativo sigue con juce::dsp::SIMDRegister (intacto); WASM usa
  fallback escalar de 4 lanes con API identica (JUCE no define SIMDRegister bajo
  Emscripten: JUCE_USE_SIMD=0 y forzarlo choca con #error interno). `Resonator.cpp`
  ya no usa SIMDRegister directamente (pasa por simdGreaterThanOrEqual del wrapper).
  Paridad nativa intacta: DSPReferenceTest bit-exacta en verde (11/11).
- `Source/Wasm/NeuronikWasmBridge.cpp`: ABI C plana (convencion ABDMS2000) hablando
  con DspEngineFacade; layouts de GlobalParams/modMatrix por offsetof (JS nunca
  hardcodea offsets); static_assert de Event = 24 bytes standard-layout.
- Smoke test Node (`Tests/neuronik_wasm_smoke.mjs`): binario entregado via hook
  instantiateWasm (el glue ES6 con ENVIRONMENT=web,worker usa fetch sobre
  import.meta.url y no sabe leer file://), HEAPF32/HEAP32 en EXPORTED_RUNTIME_METHODS.
  Verifica: audio finito no nulo (peak 0.53), voz activa tras noteOn, drain de release
  tras allNotesOff (contrato: dispara releases RT-safe; la cola exponencial de 200 ms
  cruza el umbral de Idle ~1.4-1.9 s), exit 0.
- Artefactos: build-wasm/neuronik_dsp.js (13 KB) + .wasm (89 KB), ES6+MODULARIZE,
  listos para AudioWorklet. Quedan en la Fase 5: worklet JS, paridad WASM<->nativo
  (el DSP es determinista: bit-exacta alcanzable) y wire-up de presets/params en UI.

## 2026-09-18 — AudioWorklet del piloto (Fase 5, segundo hito)
- `WebPilot/public/worklet/neuronik-worklet.js`: AudioWorkletProcessor que
  instancia el módulo WASM real (glue ES6 importado; binario entregado por
  processorOptions como ArrayBuffer clonado — el scope del worklet no tiene
  fetch al mundo de la página) y renderiza via neuronikProcess. Eventos MIDI
  apilados a Runtime::Event (24 B), GlobalParams escritos por índice de campo
  (bpm f64 aparte), telemetría de voces/LFO cada ~21 ms.
- `WebPilot/lib/audioParams.js`: mapeo contrato -> índices de GlobalParams
  (21 campos reachables; bpm sin contrato aún). Conversion normalizada->real
  con la MISMA matemática del panel nativo (fromNormalized). Test de contrato
  (`tests/audioParams.test.js`): defaults C++ vs contrato, discretos, skew.
- `WebPilot/lib/audioWorkletEngine.js`: ciclo de vida del AudioContext en la
  página (botón SOUND ON = gesto de usuario), sync de parámetros por snapshot
  completo (cubre ediciones locales Y snapshots nativos del bridge), notas/
  wheels/panic en camino dual (bridge + worklet).
- Sincronización de artefactos: `sync_wasm.bat` / `pnpm --filter
  @abdsynths/web-pilot-vite sync:wasm` copia build-wasm/ -> public/worklet/
  (ejecutar tras cada build_wasm.bat; el .wasm se sirve desde la exportación).
- Validación: vitest 55/55, build Vite 4.1 s (306 KB JS), smoke Node del
  módulo servido (peak 0.53, drain OK), selftest del host exit 0 con la página
  nueva, build nativo + ctest 11/11.
- Pendiente de la Fase 5: wire-up de presets en la vía web.

## 2026-09-18 (b) — Paridad bit-exacta WASM<->nativo (Fase 5, tercer hito, CERRADO)
- `Tests/WasmParityTest.cpp` (target `NEURONiK_WasmParityTest` en CMake, junto a
  DSPReferenceTest): ejecuta 4 escenarios sobre la MISMA frontera
  (DspEngineFacade) que consume el puente WASM y vuelca el canal izquierdo a
  `build-wasm/parity-native.json`: A_neuronik_default (32 bloques),
  B_neurotik_default (32), C_fx_panico (48: panico a mitad y cola de
  reverb/delay), D_modmatrix (24: modMorphX con case 4).
- `Tests/neuronik_wasm_parity.mjs`: instancia el módulo WASM real y compara
  muestra a muestra con distancia en ulps (double fract32) + presupuesto por
  escenario. bpm se escribe partido en dos mitades de 32 bits (el modulo no
  exporta HEAPF64). `--strict` exige 0 ulps en TODO (para CI sin tolerancia).
- Resultado medido: **A/B/D bit-exactos a 0 ulps** (osciladores, envolventes,
  modMatrix — la libm coincide); C difiere en 2 de 6144 muestras a 16 ulps
  (~3e-8, -150 dBFS): divergencia MSVC vs musl de 1 ulp amplificada por el
  feedback del delay en la cola. Presupuesto de C documentado en 16 ulps;
  A/B/D exigen 0 (cualquier dif alli es regresion real).
- Integracion: `build_wasm.bat` pasa a 5 pasos (genera referencia nativa ->
  compila WASM -> paridad Node -> smoke). build.bat valida con exit 0.
- Falta el wire-up de presets en la via web (ultimo pendiente de la Fase 5).

## 2026-09-18 (c) — Wire-up de presets en la vía web (Fase 5, CIERRE)
- Un preset es estado APVTS **más** hasta 4 slots de SpectralModel (64
  parciales) entre los que morphea el Resonator; los modelos nunca viven en el
  APVTS, así que el snapshot de parámetros no los llevaba. Camino completo:
  - Puente WASM: `neuronikLoadModel(slot, engineType, ptr128floats, isValid)`
    (el modelo cuelga del engine concreto, no de la fachada).
  - Bridge nativo: interfaz `NativeModelController` + mensaje aditivo
    `modelsState` que viaja pegado a cada `syncAllParams` y tras cada
    `loadPreset`; el host adapta el processor con una vista file-backed
    (`modelPath<slot>`, la misma fuente que recarga un cambio de engine).
  - Página: `pushModelsToWorklet` + efecto que re-aplica los modelos tras cada
    cambio de engine del worklet (el switch reconstruye el engine y los slots
    vuelven a defaults hasta que la página los re-envía).
- Worklet: mensaje `neuronik:models` (buffer heap compartido de 128 floats por
  slot) + fix del `instantiateWasm` (method-shorthand + `.bind` nunca parseó:
  el worklet no podía cargar; ahora arrow function con this léxico).
- Paridad: nuevo escenario E (`E_modelo_espectral`, slot 0 con constantes
  exactas 2^-k y offsets 2^-7 para evitar doble redondeo f32/f64 entre MSVC y
  V8) — bit-exacto a 0 ulps. Presupuestos: A/B/D/E=0, C=16 (libm de la cola).
- Validación: build.bat RESULTADO OK (11/11 ctest, selftest bidireccional OK),
  vitest 56/56, contrato de protocolo en verde (gemelos C++ y JS),
  build_wasm.bat 5 pasos con paridad incluida.
- **FASE 5 CERRADA** — la vía web suena con el DSP real y consume presets
  completos (parámetros + timbre). Siguiente en el roadmap: quitar juce_* del
  motor interno y las decisiones abiertas (¿WASM con ambos motores?, formato
  de preset común).

## Fase 1 (de-JUCE del motor) — en curso

- Commit A (`5e24271`): `DspCore.h` con ports literales de jmin/jmax/jmap/jlimit,
  MathConstants, ignoreUnused, ScopedNoDenormals (máscara MXCSR 0x8040) y
  LinearSmoothedValue. Swap mecánico en Source/DSP. Validado bit-exacto.
- Commit B+C: port de **AudioBuffer** (HeapBlock, setSize/allocateData,
  setDataToReferTo, addFrom/copyFrom/clear, FloatVectorOperations escalares
  literales sin FMA) + vista zero-copy en la frontera (processor, test).
  `ISynthesisEngine::renderNextBlock` ya toma `dsp::AudioBuffer`.
- **Lección del arnés (bug latente del test desenterrado por el port):** el
  test limpiaba el buffer JUCE subyacente mientras el motor escribía vía la
  vista `dsp::AudioBuffer`: dos flags `isClear` desincronizados => clear() en
  no-op desde el bloque 2 => residuos entre bloques (21x el nivel). Era
  invisible en el baseline (heap fresco = páginas a cero). Diagnóstico con
  motores sobre memoria envenenada 0xAA/0x00: el motor es determinista. Fix:
  limpiar SIEMPRE a través del mismo objeto que recibe las escrituras.
- Fix de portabilidad: HeapBlock del port necesita move ctor/assign (clang/
  emscripten lo exige; MSVC era laxo). Paridad intacta.
- Validación: build.bat RESULTADO OK, paridad WASM A/B/D/E=0 ulps y C=16
  (presupuesto), smoke OK, vitest 56/56 tras sync_wasm.

## Fase 1 [4/6] — la frontera MIDI deja de ser JUCE (2026-09-18)

**Qué se hizo:** el motor ya no conoce el transporte MIDI de JUCE. Solo queda el
adaptador del lado host.

- `Source/DSP/DspMidiMessage.h` NUEVO: `dsp::MidiMessage`, port LITERAL del
  subconjunto que el motor interpreta (note on/off, pitch wheel, aftertouch de
  canal y polifónico, CC). Mismos cuerpos y defaults que JUCE, incluido el
  detalle que más importa: **`isNoteOff()` trata note-on con velocity 0 como
  note-off** (default de JUCE) y `getFloatVelocity()` sale del byte, no del
  float de entrada. Fuera del alcance a propósito: SysEx, meta y realtime.
- `Source/DSP/DspMidiBuffer.h` NUEVO: `dsp::MidiBuffer`, port de
  `juce::MidiBuffer` con el mismo empaquetado (`[int32 pos][uint16 size][bytes]`)
  y la misma semántica de inserción (ordenado por posición, port literal de
  `findEventAfter`; los empates quedan FIFO). `ensureSize()` + `clear()` que
  conserva la capacidad = cero asignaciones en el hilo de audio.
- `dsp::roundToInt` en `DspCore.h`: port literal del truco de doble precisión de
  JUCE (empates **al par**, no como `std::lround`). Es lo que cuantiza la
  velocity de nota en `floatValueToMidiByte`, así que sin él el port habría
  cambiado el sonido en los empates.
- `Source/DSP/Runtime/JuceMidiAdapter.h` NUEVO: frontera
  `juce::MidiBuffer` → `dsp::MidiBuffer` para hosts JUCE. Copia los bytes crudos
  (cero re-cuantización), conserva orden y posiciones, y descarta lo que el
  motor no interpreta (SysEx/meta/realtime y >3 bytes). `#error` explícito si
  se incluye bajo `__EMSCRIPTEN__`.
- `DspEngineFacade` deja de incluir `juce_audio_basics`: construye
  `dsp::MidiMessage` con las mismas factorías y **reutiliza un `dsp::MidiBuffer`
  miembro** (antes creaba un buffer local por bloque → asignaba en el hilo de
  audio). La frontera `Runtime/*` es ya 100 % libre de JUCE.
- `Source/Main/NEURONiKProcessor`: traduce a `engineMidiBuffer` cada bloque
  (miembro reutilizado). El resto del procesador (filtro de canal, curva de
  velocidad, máscara de notas, MIDI thru) sigue con su `juce::MidiBuffer`: es el
  lado host y ahí JUCE es lo correcto.
- Limpiezas de paso: `IVoice.h` incluye `DspCore.h` (antes obtenía
  `dsp::AudioBuffer` por rebote de `juce_audio_basics`); eliminado
  `Source/DSP/Synthesis/ResonatorSound.h`, que era **código muerto** (cero
  referencias) y el único `juce_audio_processors` del árbol DSP.
- `Tests/MidiPortTest.cpp` NUEVO (target `NEURONiK_MidiPortTest`, en ctest y en
  el paso 7 de `build.bat`): anti-drift contra los originales — barrido de 2.849
  patrones de bytes comparando TODOS los predicados/getters, 1.824 casos de las
  seis factorías comparando bytes crudos, 2.541 valores de velocity (incluye el
  empate 63.5), `roundToInt` en semienteros, `getMidiNoteInHertz` en las 128
  notas, semántica de `dsp::MidiBuffer` (orden, posiciones, empates, `clear()`) y
  del adaptador (qué se copia, qué se descarta, reutilización del destino).

**Verificación ejecutada (2026-09-18, todo en esta máquina):**

```text
build_wasm.bat (5 pasos)                    EXIT 0 → paridad A/B/D/E = 0 ulps,
                                            C = 16 (presupuesto), smoke peak=0.53199
cmake --build ... NEURONiK_Standalone       EXIT 0 (procesador + editor + plugin)
ctest --test-dir build-reference -C Release 12/12 (11 previos + MidiPortTest)
pnpm --filter @abdsynths/web-pilot-vite build  EXIT 0 (bundle de la WebUI)
NEURONiK_WebPilotHost + --selftest          EXIT 0 → 4/4 direcciones:
                                            NATIVO->JS, JS->NATIVO, GENERAL y MIDI
```

La paridad no se movió por el port: los picos nativos de los cinco escenarios
son idénticos a los de antes (A 0.531995, B 0.006339, C 0.528966, D 0.553536,
E 0.434755) y A/B/D/E siguen bit-exactos. El selftest es el que valida de punta
a punta el lado host del paso: la página manda note on/off de la 60 y el host
lee la máscara de notas (su línea `MIDI: ... -> OK` pasa por
`NEURONiKProcessor` → `copyToDspMidiBuffer` → `dsp::MidiBuffer` → motor).
Falta la pasada completa de `build.bat`, que repite todo lo anterior en un solo
comando (y solo añade el bundle embebido del host y el informe de pasos).

**Lo que aún queda de JUCE en el motor** (pasos 5/6 y 6/6):

| Dependencia | Dónde |
|---|---|
| `juce::Reverb` | `Source/DSP/Effects/Reverb.h` |
| `juce::dsp::SIMDRegister` | `Source/DSP/Utils/SIMDWrapper.h` (rama nativa; WASM ya usa el fallback escalar) |
| `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` | 10 clases del DSP (incluidos `BaseEngine.h`, `LFO.h`, los efectos y las voces) |
| `JUCE_DEBUG`/`DBG`/`jassertfalse` | `Source/DSP/DSPUtils.h`, `AdditiveVoice.cpp`, `NeurotikVoice.cpp` |
| includes `juce_core`/`juce_audio_basics` residuales | `Envelope.h`, `FilterBank.h`, `Oscillator.h`, `Resonator.h`, `ResonatorBank.h`, `RhythmicDivision.h`, `Saturation.h`, `Chorus.h`, `Delay.h`, `NeurotikVoice.h` (varios son vestigiales: ya no usan ningún símbolo `juce::`) |

## Arreglo de tiempo real: el jitter de entropía y el placeholder de `Resonator` (2026-09-18)

Dos cosas que marcó la auditoría del motor, ya cerradas:

1. **`Resonator::prepareEntropy` ya no asigna en el hilo de audio.** Hacía
   `ampJitterBuffer.resize(...)` dentro del callback cuando la entropía estaba
   activa (guardado por `entropyAmount < 0.001f`, y la entropía es 0 por defecto,
   pero es exactamente la regla ZERO ALLOCATIONS). Ahora los buffers se reservan
   UNA vez con `Resonator::prepareJitterBuffers(maxBlockSize)`, que llama
   `AdditiveVoice::prepare()`; `prepareEntropy` solo rellena y deja
   `jitterLength = min(numSamples, capacidad)`. Si un host entrega un bloque mayor
   que el preparado, el jitter se recicla por módulo (determinista) en vez de
   reservar: con el bloque dentro de lo reservado `sampleIdx % jitterLength ==
   sampleIdx`, así que el audio es idéntico (paridad A/B/D/E = 0 ulps y picos
   nativos sin cambios).
2. **Fuera el placeholder `Resonator::processSample()` sin argumento.** Devolvía
   `processSample(0)` («This won't work as is») y ModelMaker lo llamaba de verdad:
   con entropía activa y sin jitter preparado leía fuera del vector. Ahora hay una
   sola sobrecarga indexada, ModelMaker pasa su `i`, y la ruta de entropía exige
   `jitterLength > 0` (si nadie preparó el jitter queda inerte en vez de tocar
   memoria inválida).

## Fase 1 [6/6] — el motor deja de incluir JUCE (2026-09-18)

- `DspDebug.h` NUEVO: `dspDbg(...)`, port de `DBG` con la misma puerta
  (`! defined (NDEBUG)` = `JUCE_DEBUG`): en Release no compila ni evalúa la
  expresión. Escribe en stderr (el motor no tiene Logger).
- `DspLeakedObjectDetector.h` NUEVO: `dspLeakDetector` /
  `dspDeclareNonCopyableWithLeakDetector(Class)`, port de
  `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` + `juce::LeakedObjectDetector`.
  **Como en JUCE, el detector solo existe en Debug** (en Release el macro deja
  únicamente el borrado de copia), así que el binario Release no engorda.
  `DspCore.h` lo incluye al final para que el macro siga estando disponible donde
  antes lo ponía juce_core.
- Macro de JUCE sustituido en las 9 clases del DSP que lo usaban (`BaseEngine`,
  `LFO`, los dos motores, `Chorus`, `Delay`, `Saturation`, `Reverb`,
  `NeurotikVoice`).
- `DSPUtils.h`: `JUCE_DEBUG`/`DBG`/`jassertfalse` → `dspDbg`/`dspAssert`, sin la
  puerta explícita (el macro ya es un no-op en Release). `dsp::ignoreUnused
  (paramName)` mata además los **8 avisos C4100** que rompían la puerta de «0
  avisos»: el plugin y el host del piloto compilan ahora **limpios**.
- Fuera los includes `juce_core`/`juce_audio_basics` **vestigiales** (ninguno de
  esos ficheros usaba ya un símbolo `juce::`): `Envelope.h`, `FilterBank.h`,
  `Oscillator.h`, `Resonator.h`, `ResonatorBank.h`, `RhythmicDivision.h`, `LFO.h`,
  `Saturation.h`, `Chorus.h`, `Delay.h`, `NeurotikVoice.h`, `DSPUtils.h`,
  `BaseEngine.h`, `NeuronikEngine.h`, `NeurotikEngine.h`. `Resonator.h` recupera
  `<cstdint>`/`<vector>`, que obtenía por rebote.
- `Tests/LfoSyncTest.cpp` incluye `juce_core` explícitamente: usaba `juce::String`
  en sus ayudantes apoyándose en el include del DSP.

**Lo único que queda de JUCE en `Source/DSP`** (a propósito) — `juce::Reverb`
se portó en el paso 5/6, ver la sección siguiente:

| Dependencia | Dónde | Por qué |
|---|---|---|
| `juce::dsp::SIMDRegister` (rama nativa) | `Utils/SIMDWrapper.h` | es la implementación nativa real, no un vestigio; WASM ya usa el fallback escalar |
| `juce::MidiBuffer` → `dsp::MidiBuffer` | `Runtime/JuceMidiAdapter.h` | es la frontera del host JUCE, por diseño |

**Verificación (2026-09-18):** `build_wasm.bat` EXIT 0 (paridad A/B/D/E = 0 ulps,
C = 16, smoke peak 0.53199), `ctest` 12/12, Standalone y host del piloto EXIT 0 con
**0 avisos**, `--selftest` 4/4 direcciones, y `NEURONiK_ModelMaker` EXIT 0
(comprueba el cambio de `processSample`; su `Version.h` se restauró).

## Fase 1 [5/6] — el motor deja de depender de juce::Reverb (2026-09-18)

- `Effects/DspReverb.h` NUEVO: port libre de JUCE de `juce::Reverb` (Freeverb,
  JUCE 8.0.12, `juce_audio_basics/utilities/juce_Reverb.h`): 8 comb filters en
  paralelo + 4 all-pass en serie por canal, con los tunings y el stereo spread
  idénticos. `Effects/Reverb.h` queda como envoltorio de producto (mapeo
  mix→wet/dry y suavizado), de modo que el efecto puro es reutilizable tal cual.
  **Ya no queda ninguna dependencia de `juce_audio_basics` en `Source/DSP/Effects`.**
- `dspUndenormalise` (en `DspCore.h`) NUEVO: port de `JUCE_UNDENORMALISE` con una
  decisión de política documentada. El macro de JUCE está definido **solo en x86**
  y **no es un no-op**: `(x + 0.1f) - 0.1f` redondea dos veces, aplasta los
  denormales (su propósito) y **perturba los valores normales en ~2 ulps de 1.0**.
  `juce::Reverb` lo aplica a `last` y a `temp` en cada muestra de cada comb
  filter, así que el mismo `juce::Reverb` **calculaba distinto en nativo y en
  WASM**. El port es no-op uniforme: la paridad bit a bit nativo↔WASM es la
  invariante del motor, y los denormales en x86 ya los cubre
  `dsp::ScopedNoDenormals`, que es determinista porque cambia el modo de la FPU y
  no las muestras.
- **La puerta de paridad se endurece: los cinco escenarios a 0 ulps.** El
  escenario `C_fx_panico` (delay + chorus + reverb a mix no nulo + pánico)
  medía 16 ulps y estaban atribuidos a libm (`sin`/`exp` en el feedback del delay)
  desde la Fase 5. **No era libm: era `JUCE_UNDENORMALISE`.** Con el port, C mide
  0 ulps y su presupuesto vuelve a 0 en `Tests/neuronik_wasm_parity.mjs` (la
  historia queda escrita en la cabecera del test, para que nadie vuelva a
  atribuirlo a libm).
- **Bug latente del port de `dsp::HeapBlock` corregido**: `clear` y `allocate`
  tomaban **bytes** y JUCE toma **elementos** (`sizeof(ElementType) * numElements`).
  Era invisible porque sus únicos consumidores eran `AudioBuffer` (que no usa
  `HeapBlock`) y `DspMidiBuffer` (`HeapBlock<uint8_t>`: elementos == bytes).
  `juce::Reverb` sí lo expone: `buffer.clear((size_t) bufferSize)` sobre un
  `HeapBlock<float>` con la semántica de bytes dejaba 3/4 del buffer de cada comb
  filter sin inicializar y realimentaba basura —la reverb divergía a ~1e35 en
  pocos bloques, y por eso el primer `<reverb>.reset()` no bastaba para que dos
  renders fueran idénticos—. Corregido en `DspCore.h` con la nota de la
  discrepancia; `DspMidiBuffer` no cambia de comportamiento (uint8).
- `build_wasm.bat`: nuevo paso **[6/6]** que ejecuta `sync-wasm.mjs`. El drift de
  artefactos (worklet servido con el DSP de la pasada anterior) ya había ocurrido
  dos veces y no se detecta en el código porque los `.js`/`.wasm` se versionan.

**Tests nuevos** (el mismo fuente se compila dos veces, con y sin el define):

- `NEURONiK_DspReverbParityTest`: la política enviada (no-op uniforme) contra
  `juce::Reverb` con presupuesto documentado, más determinismo (dos renders bit
  idénticos, con `reset()` de por medio) y una cola larga de silencio sin NaN ni
  infinitos.
- `NEURONiK_DspReverbJucePolicyTest`: el mismo fuente con
  `-DDSP_UNDENORMALISE_JUCE_POLICY=1` (réplica de `JUCE_UNDENORMALISE`) exigiendo
  **0 ulps**. Esto es lo que prueba que el port es literal: la única diferencia
  con `juce::Reverb` es la política, y queda cuantificada.

**Medido** (MSVC x64 Release, 48 kHz, 24 bloques de 512, estéreo y mono): modo
JUCE **0 ulps** y 0.000e+00 en las 36.864 muestras comparadas; modo enviado
maxDiff **2.384e-07** (exactamente 2 ulps de 1.0) y ≤ 5.632 ulps en cola de
magnitud pequeña. La condición de arquitectura es propia (`DSP_HOST_IS_X86`),
**no `JUCE_INTEL`**: un header JUCE-free no puede depender de un macro que solo
existe si antes se incluyó JUCE —el primer intento hacía que la política de JUCE
nunca se activara y el test del port literal pasaba en falso—.

**Verificación (2026-09-18):** `build_wasm.bat` (6 pasos) EXIT 0 → paridad
**A/B/C/D/E = 0 ulps** (C incluido) y smoke peak 0.53199; `ctest` **14/14**;
host del piloto EXIT 0 con `--selftest` **4/4** direcciones (NATIVO↔JS, GENERAL,
MIDI); `NEURONiK_WebPilotHost` compila con 0 avisos nuevos. `juce::Reverb` ya
solo aparece en el test que lo usa como referencia.

## Migración de los efectos a DspEffects — chorus, delay y saturación (2026-09-19)

Continuación de la Fase 1 [5/6]: la reverb abrió el módulo compartido
`ABDSharedCode::DspEffects` (`DspEffects/DspReverb.h`) y ahora se mueven los tres
efectos que todavía vivían solo en el synth. La partición es la misma en los tres:
**motor puro en el módulo compartido, política de producto en el envoltorio.**

- `DspEffects/DspChorus.h` NUEVO: chorus estéreo de línea retardada modulada (LFO
  de fase, 5ms..30ms, lectura interpolada lineal, mezcla wet/dry). API por
  muestra: `processSample (canal, x, depth, mix)` + `advance (rateHz)`.
- `DspEffects/DspDelay.h` NUEVO: retardo estéreo realimentado (buffer circular de
  2 canales, lectura interpolada, escritura de `input + delayed * feedback`). API
  por muestra: `processSample (canal, x, delayInSamples, feedback)` +
  `advanceWritePosition()`.
- `DspEffects/DspSaturation.h` NUEVO: la forma del soft-clipping
  (`atan (x * drive) * 0.63661977236f`) como utilidad estática sin estado (su
  "estado" en el producto era el smoother del drive, que es política).
- `Source/DSP/Effects/{Chorus,Delay,Saturation}.h` pasan a ser envoltorios de
  producto: suavizado de parámetros (20ms/50ms), mapeo (segundos -> muestras,
  amount -> drive, recorte del feedback a 0.95), puerta de denormales y mezcla.
  **La API pública no cambia**, así que `BaseEngine` no toca una línea.

**Por qué por muestra y no por bloque (la decisión que sostiene la paridad).** Los
smoothers de los tres efectos se leían *dentro* del bucle de muestras, así que un
`processBlock` con parámetros por bloque habría movido rate/depth/mix/tiempo/
feedback en cada muestra: otra salida. Por eso el motor compartido expone la
muestra y el consumidor aporta la política, y no al revés. La contrapartida es un
contrato de llamada explícito (recorrer los canales y luego avanzar una vez por
muestra), documentado en la cabecera de cada motor junto al `channel % 2`
heredado del original.

**Nota (denormales).** El `ScopedNoDenormals` del delay lo sigue abriendo el
consumidor: la API compartida es por muestra y no tiene nivel de bloque. El
original lo abría en su `processBlock`, así que está exactamente en el mismo
sitio.

**Desviación documentada.** `dsp::Chorus::prepare()` pone el puntero de
escritura a 0. El original no lo hacía: con un `prepare()` en caliente (cambio de
sample rate) el puntero podía quedar por encima del buffer nuevo y la primera
escritura se salía del rango. En el uso normal (un `prepare` antes de procesar) el
puntero ya valía 0, de modo que no cambia ningún resultado definido. `reset()`, en
cambio, sigue sin tocarlo (solo vacía el buffer), como el original.

**Test nuevo:** `Tests/DspEffectsParityTest.cpp` -> `NEURONiK_DspEffectsParityTest`
(registrado en `ctest` y en la lista de targets de `build.bat`). Aquí no hay
implementación ajena contra la que comparar (esto no es un port de JUCE), así que
el test lleva una **referencia congelada**: la copia literal de los tres efectos
tal y como estaban antes de moverse. Mismo guion de parámetros por bloque, misma
entrada determinista (LCG, sin reloj ni `rand`), y se exigen **0 ulps** muestra a
muestra. Cubre: chorus estéreo/mono/3 canales (el `channel % 2` del motor), delay
estéreo/mono, saturación por bloque (incluido el primer bloque con drive
exactamente 1.0, que pasa por la puerta de bypass del producto) y por muestra,
determinismo entre dos objetos nuevos, y una cola de 200 bloques de silencio con
fb 0.95 sin NaN ni infinitos.

**Medido** (48 kHz, 24 bloques de 512): **0 ulps en las 7 comparaciones** (147.456
muestras) con MSVC x64 Release (`/O2 /W4`) y con MinGW g++ 10.3
(`-O2 -DNDEBUG -Wall -Wextra`), **0 avisos** con los dos compiladores.

**Verificación:** pendiente de la pasada completa de `build.bat` (los 17 targets
de ctest, que ahora incluyen estos dos tests).

## Sustrato: dsp::AudioBuffer contra juce::AudioBuffer (2026-09-19)

`Tests/AudioBufferParityTest.cpp` -> `NEURONiK_AudioBufferParityTest` (ctest y
lista de targets de `build.bat`). Cierra el hueco que dejó la corrección del
helper: el port de `AudioBuffer` era el único consumidor del sustrato sin test
contra su original (los otros dos ports, `MidiMessage`/`MidiBuffer`, ya estaban
pinados en `MidiPortTest`, y la reverb tiene el suyo). La comparación vive aquí
porque el módulo compartido es JUCE-free por contrato.

- **Paridad de datos.** El mismo guion sobre los dos tipos (setSize con sus
  banderas, clear por canal, setSample/addSample, applyGain, applyGainRamp,
  addFrom/copyFrom y sus variantes con rampa, reverse, makeCopyOf, getMagnitude,
  getRMSLevel) deja las mismas muestras bit a bit, la misma magnitud/RMS y la
  misma bandera `hasBeenCleared()`.
- **Estructura de punteros (el guard de la rama que estuvo muerta).** Barrido de 1
  a 40 canales preguntando dónde vive el array de canales: dentro del objeto hasta
  **31** y en el heap desde 32, igual que JUCE (`numChannels < 32`). Con el helper
  devolviendo 1 la frontera medida era **0** en el port y 31 en JUCE: ese
  desacuerdo es exactamente lo que delata el test.
- **Memoria externa, movimiento y copia.** Un buffer que referencia canales del
  que llama: tras moverse (constructor y asignación) sigue escribiendo en esos
  canales y su array de punteros vive en el DESTINO; el constructor de copia
  COMPARTE la memoria externa (documentado en JUCE) y `makeCopyOf()` en cambio se
  queda con memoria propia. Los cuatro comportamientos se comparan con JUCE.

**Medido** (MSVC x64 Release, JUCE 8.0.12 real, 2 canales x 512 muestras): 0 ulps
en muestras, magnitud y RMS; frontera de `preallocatedChannelSpace` dsp=31
juce=31; 0 avisos de compilación.

**Control negativo del guard:** el `static_assert` de `DspCoreTests` con el
helper **anterior** (parámetro por valor) no compila — comprobado a propósito en
un fichero aparte —, así que la regresión no puede volver en silencio.

## Renombrado: DSPUtils.h -> DspSafety.h, y las homonimias de la familia (2026-09-19)

Al evaluar la homonimia entre el `DSPUtils.h` de este repo y el de
`ABDSharedCode/SynthCore` (fruto del refactor DRY transversal), la conclusión fue
**renombrar sí, unificar todavía no**. El motivo completo, y las tres condiciones
para unificar, están escritos en la cabecera del fichero renombrado.

- `Source/DSP/DSPUtils.h` -> `Source/DSP/DspSafety.h`: mismo contenido, mismo
  namespace (`NEURONiK::DSP`), mismas firmas. El nombre nuevo describe lo que es
  (validación de parámetros + saneo de NaN/Inf), encaja con la convención `Dsp*`
  de los módulos compartidos (si algún día se muda, no vuelve a renombrarse) y
  elimina el nombre que compartía con `SynthCore/DSPUtils.h`.
- **8 ficheros** actualizan el include (todos los `.cpp` de `Source/DSP` que la
  usaban): `CoreModules/{LFO,FilterBank,Resonator,ResonatorBank,NeuronikEngine,NeurotikEngine}.cpp`
  y `Synthesis/{AdditiveVoice,NeurotikVoice}.cpp`.
- **No se unifica** con SynthCore: no hay código duplicado (aquel no tiene
  validación de parámetros ni saneo de buffers), y estos helpers se apoyan en el
  sustrato (`AudioBuffer`, `dspDbg`, `dspAssert`, `jlimit`), así que mudarlos
  obliga a meter utilidades de producto en DspCore o a que SynthCore dependa de
  DspCore — dos módulos que hoy son independientes y que consume gente distinta.
- `sanitizeAudioBuffer` tenía **0 llamadas** y se **borró**: su política es la que
  el motor no usa (ver el bloque siguiente).

**Verificado:** los 8 `.cpp` sintaxis-limpios con MSVC `/W4` (0 errores, 0 avisos;
los que tiran de JUCE con los includes de módulo reales) y los dos que son
JUCE-free también con GCC `-Wall -Wextra` (0 avisos). Cero referencias de código o
build al nombre antiguo (`grep` en `*.h/*.cpp/*.txt/*.cmake/*.bat`).

**Inventario de homonimias que queda.** Comparando nombres de fichero entre
`ABDNeural/Source/DSP` y los módulos compartidos salen 7 coincidencias, y solo dos
son problemas reales:

| Coincidencia | ¿Problema? |
|---|---|
| `DspCore.h`, `DspDebug.h`, `DspLeakedObjectDetector.h`, `DspMidiBuffer.h`, `DspMidiMessage.h` | **No**: son los shims de este repo. Misma entidad con dos caminos, deliberado y documentado. |
| `DSPUtils.h` | Sí, y queda **resuelto** con este renombrado. |
| `LFO.h` | Sí, **abierto**: `CoreModules/LFO.h` (NEURONiK, `NEURONiK::DSP::Core`, ondas Sine/Triangle/SawUp/SawDown/Square/S&H con Free/TempoSync) y `SynthCore/LFO.h` (el de MS2000/ABDEep, `abd::synth`, ondas MS2000 con `syncNoteIdx` de la spec SysEx) son dos LFO distintos con el mismo nombre de fichero, y en ABDMS2000 hay un tercer `LFO.h` que es shim al de SynthCore. Misma clase de riesgo que el de DSPUtils; el renombrado de uno de los dos (o la convergencia real en un LFO parametrizable) es decisión aparte, porque las dos APIs no son un superconjunto la una de la otra. |

**Y `sanitizeAudioBuffer` se borra, no se conecta (2026-09-19).** Estaba sin usar,
pero el motivo de fondo es otro: **su política es la contraria a la que el motor ya
aplica**. Donde el motor se topa con un NaN de verdad (los lazos de realimentación
de las voces) la respuesta es *detectar y reiniciar la voz*:

```cpp
// AdditiveVoice.cpp / NeurotikVoice.cpp
if (! isfinite (...)) { dspDbg ("NaN ... voice reset"); reset(); return false; }
```

Eso arregla el **estado** que diverge. `sanitizeAudioBuffer` solo escribía 0 en el
buffer de salida: el lazo seguiría roto y produciendo NaN en las muestras
siguientes (silencio sostenido y CPU gastada), además de tapar el síntoma. Ponerlo
en `BaseEngine::applyGlobalFX` sería eso mismo pagando un barrido `isfinite` por
muestra sobre el buffer maestro **en el hilo de audio**. El otro caso real, los
denormales, ya lo cubre `dsp::ScopedNoDenormals` en los lazos. El razonamiento
queda escrito en la cabecera de `DspSafety.h`, junto al de por qué el fichero no se
unifica todavía con SynthCore. Se va con él el `#include <limits>`, que solo él
usaba.

**Verificado:** los 8 `.cpp` que incluyen `DspSafety.h` pasan `cl` `/W4 /O2`
sintaxis-limpios (**0 errores, 0 avisos**), y la cabecera suelta más los dos TUs
JUCE-free (`LFO.cpp`, `FilterBank.cpp`) también con GCC `-Wall -Wextra -Wpedantic`
(**0 avisos**). `grep` de `sanitizeAudioBuffer` en el repo: solo la mención
histórica de este documento.

**De paso, un bug latente del sustrato compartido.** El aviso de GCC
`-Wsizeof-pointer-div` que apareció al escribir el test venía de
`dsp::numElementsInArray` (`DspCore.h`): el port tomaba el array POR VALOR, así que
el array decaía a puntero en la llamada y la función devolvía
`sizeof(Type*) / sizeof(Type)` — 1 en sus tres consumidores, que le pasan
`preallocatedChannelSpace` (un `Type* [32]`). La comprobación `numChannels < 32` se
evaluaba como `numChannels < 1`, de modo que la rama de la memoria PREASIGNADA de
`AudioBuffer` era inalcanzable: `allocateChannels` hacía un `malloc` por buffer
sobre memoria externa (exactamente el que JUCE evita ahí: "blow up things like
Pro-Tools") y el constructor/asignación de movimiento iban siempre por la rama de
aliasar en vez de copiar al hueco propio. Corregido a la forma de JUCE
(`Type (&)[N]`, devuelve N), con ruta testigo en `DspCoreTests`
(`testNumElementsInArray`, con un `static_assert` que no compila si el array vuelve
a decaer). Verificado con MSVC `/W4` y GCC `-Wall -Wextra`: 55 comprobaciones OK y
0 avisos en los dos (el aviso de GCC desaparece).

**Efecto en ABDMS2000:** ninguno. Su `CMakeLists.txt` ya hace `add_subdirectory`
de ABDSharedCode y `DspEffects` es INTERFACE (solo ruta de include), así que esto
es aditivo; y el synth no usa hoy ningún efecto del módulo (ni reverb tiene).
Cuando quiera reutilizarlos, `ABDShared::DspEffects` ya está en el grafo.

---

## Matriz de paridad WASM por sample rate y tamaño de bloque, y el hallazgo que destapa (2026-09-19)

Cierra el último punto sin verificar de la Fase 5. Antes la paridad se medía en **una
sola** pareja (48 kHz / 128), que no dice nada sobre los otros sample rates ni sobre
el buffer que use el host.

**Qué se hizo**

- `Tests/WasmParityTest.cpp` pasa de un caso a una matriz de **9**: 44.1/48/96 kHz ×
  64/128/512 muestras de bloque. La clave es que la duración de cada escenario es la
  MISMA en los nueve — el número de bloques se reescala sobre una referencia de 128
  (32 → 64 bloques con bloque 64, → 8 con bloque 512) y el pánico se reescala igual —
  así que la comparación es de contenido musical y no de número de llamadas.
- El JSON pasa a `{"referenceBlockSize":128, "cases":[{sampleRate, blockSize,
  scenarios:[...]}], "blockSizeInvariance":[...], "blockSizeDependentScenarios":[...]}`.
- `Tests/neuronik_wasm_parity.mjs` recorre la matriz entera: reinicializa el módulo con
  cada pareja (`_neuronikInit(sampleRate, blockSize)`) y compara cada caso contra su
  propia referencia. El calendario (`blocks`, `panicAtBlock`) **se lee de la
  referencia**, no se duplica: era la forma más fácil de comparar dos cosas distintas
  sin darse cuenta. 9 casos × 5 escenarios = 45 comparaciones / 184.320 muestras.

**El hallazgo: la salida depende del tamaño de bloque en dos rutas**

El test mide también, en nativo, si los tres tamaños de bloque dan la misma señal
(misma duración ⇒ deberían ser bit-exactos si el DSP es por muestra):

| Escenario | 44.1 kHz | 48 kHz | 96 kHz |
|---|---|---|---|
| A_neuronik_default | **bit-exacto** (0/8192) | **bit-exacto** | **bit-exacto** |
| B_neurotik_default | **bit-exacto** (0/8192) | **bit-exacto** | **bit-exacto** |
| E_modelo_espectral | **bit-exacto** (0/6144) | **bit-exacto** | **bit-exacto** |
| C_fx_panico | depende (maxAbs 2.7e-1) | depende (3.0e-1) | depende (2.6e-1) |
| D_modmatrix | depende (maxAbs 8.7e-3) | depende (8.7e-3) | depende (3.1e-3) |

Las dos causas, localizadas:

1. **Modulación por bloque** — `BaseEngine::applyGlobalFX` llama a
   `lfo1.processBlock(numSamples)` / `lfo2.processBlock(numSamples)`, y
   `LFO::processBlock` devuelve **un** valor por llamada (avanza la fase `increment *
   (numSamples - 1)` y lo mantiene). La matriz de modulación es entonces una escalera
   cuyo paso es el tamaño de bloque. Coherente con lo medido: en D la primera
   diferencia cae EXACTAMENTE en el primer límite de bloque (índice 128 comparando
   bloque 64 contra 128; índice 256 comparando 512 contra 128).
2. **Smoothers de la reverb por bloque** — `Source/DSP/Effects/Reverb.h::processBlock`
   hace `sizeSmoother.getNextValue()` **una vez por llamada**, fuera del bucle de
   muestras, y con ese valor llama a `reverb.setParameters()`. Su rampa de 20 ms dura
   20 ms *por bloque*, no 20 ms de audio. Coherente con lo medido: en C nada diverge
   hasta ~448 muestras (44.1/48 kHz) o ~960 (96 kHz), o sea hasta que termina la
   primera rampa. La rampa de la reverb se comporta así desde antes de la migración a
   `DspEffects` (el comentario del fichero lo decía: "update parameters once per block")
   — el test de paridad de la migración no podía verlo porque comparaba una sola
   pareja de bloques.

El **núcleo** (osciladores, resonador, envolventes, filtros, voz aditiva y Neurotik)
es bit-exacto en los tres tamaños: eso es lo que hacía falta saber para la web, y sale
bien.

**Por qué importa para la web.** El `AudioWorklet` renderiza siempre en cuantos de
128 muestras y un host nativo suele ir a 256/512/1024. Para C y D, web y nativo no dan
la misma señal. Arreglarlo es pequeño (LFO por muestra manteniendo el valor del último
bloque no sirve: hay que avanzar y devolver por muestra, y el wrapper de la reverb
tiene que mover cada smoother dentro del bucle), pero **cambia la salida** de los
presets con FX y de los que usan la matriz de modulación — es una decisión de producto,
no un refactor. Queda como decisión abierta en `ROADMAP.md` (Fase 5) con esta misma
evidencia. Nota para el que lo coja: `dsp::LinearSmoothedValue::setTargetValue` **sí**
hace early-return con el mismo target (port fiel de JUCE), así que el problema NO es
rearmar la rampa, es llamar a `getNextValue()` fuera del bucle de muestras.

**Decisión sobre las dos rutas (2026-09-19): se arreglan las dos, y el motivo principal del
primero no es el bloque.**

- **Reverb → es un defecto.** Sus cuatro smoothers avanzan *una vez por bloque*, y la rampa
  está declarada como 20 ms (`reset(sampleRate, 0.02)` = 882 pasos). Con una llamada por
  bloque la rampa dura 882 **bloques**: ~2,5 s con bloque 128 y **~10 s con bloque 512** a
  44,1 kHz. O sea que al cargar un preset la reverb no llega a su valor en 20 ms, se arrastra
  segundos, y cuánto dura depende del buffer del host. Revisados los cuatro envoltorios:
  chorus, delay y saturación avanzan por muestra; la reverb es **la única** con este patrón.
  Arreglo: consumir `numSamples` pasos por bloque y rearmar `setParameters()` solo cuando el
  valor suavizado cambie (durante la rampa), no en cada bloque con el mismo valor.
- **Modulación → no es un defecto, es una tasa de control acoplada al host.** El LFO se lee
  una vez por bloque (`applyGlobalFX` → `lfo.processBlock(numSamples)` → `applyModulation()`),
  así que la matriz modula por bloque y su granularidad es la del host (128 en la web, 512 en
  un host de 512). Arreglo: tasa de control **fija** (bloques internos de 64/128 muestras con
  el resto encadenado), de modo que la modulación la defina el tiempo y no el troceado. Eso
  cambia la modulación en hosts de bloque grande (menos escalón) y obliga a **re-basar** la
  referencia de paridad.

Los dos van en pasos separados, con la matriz de 9 casos como verificación: al final,
`blockSizeDependentScenarios` debe quedar vacía o con una justificación escrita de lo que
quede.

**Reproducción** (no necesita emsdk, es el lado nativo):

```
cl /nologo /std:c++17 /O2 /W4 /EHsc /DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
   /I Source /I Source/DSP /I Source/Common /I ../ABDSharedCode /I C:/JUCE/modules \
   Tests/WasmParityTest.cpp <las 12 fuentes de DspSources.cmake> \
   C:/JUCE/modules/juce_core/juce_core.cpp C:/JUCE/modules/juce_core/juce_core_CompilationTime.cpp \
   shell32.lib ole32.lib oleaut32.lib shlwapi.lib user32.lib advapi32.lib
neuronik_parity.exe parity-native.json      # imprime la matriz y el veredicto por rate
```

(El target CMake `NEURONiK_WasmParityTest` sigue siendo el camino oficial; esto es
solo la vía corta para mirarlo sin compilar el plugin.) El generador **no falla** por
la dependencia de bloque: la publica en el JSON y avisa por consola, porque es una
propiedad del motor y no del puente WASM. El árbitro duro sigue siendo el 0 ulps por
caso.

**De paso (mismo día):**

- `Tests/ParameterBridgeTest.cpp`: los literales `0.6` / `0.75` / `1.0` pasan a `f`.
  El aviso `C4305` de MSVC solo saltaba con `0.6` (es el único de los tres que no es
  exactamente representable en float, y MSVC calla cuando lo es), pero los tres son el
  mismo caso y el fichero ya usaba `0.8f`/`0.5f` doscientas líneas más arriba.
- `WasmParityTest.cpp` decía "4 escenarios" en su cabecera y hay cinco desde
  `E_modelo_espectral`; corregido (y el comentario de CMake del target, igual).
- **`oscPitchCoarse` retirado del namespace de IDs** (no implementado). Estaba
  declarado desde el primer día pero nunca entró en el layout: no lo leía el motor, ni
  el panel, ni ningún preset. Era una promesa del draft viejo `DOC/3`
  (`oscPitchFine`, `oscPitchOctave`, `oscHarmonicCount`, ninguno adoptado tampoco).
  Misma regla que `harmMix` en 2026-09-16. Lo que sí existe es el pitch por voz (MPE:
  `EventType::PitchBend` → `IVoice::notePitchBend(semitonos)` → `pow(2, semis/12)`); un
  coarse tune global sería otra cosa y, si se quiere, es una feature con su tarea (se
  hace en el host transponiendo las notas, junto a `velocityCurve`/`midiChannel`, sin
  tocar motor ni ABI WASM). Consecuencia: `getUnroutedParameterIds()` queda vacío,
  `notInLayout` pasa a 0 en el contrato generado, y el test de regresión ahora exige
  que la lista esté vacía (si alguien declara un ID fuera del layout, salta). El
  mecanismo se queda: es lo que hace visible una divergencia nueva en vez de dejarla
  invisible. Detalle en `DSP_PARAMETERS.md`.

## Arreglo de la rampa de la reverb: duraba 960 BLOQUES, no 20 ms (2026-09-19)

Primer paso de la decisión "las dos rutas por bloque se arreglan". El envoltorio
`Source/DSP/Effects/Reverb.h` hacía `getNextValue()` **una vez por llamada**, fuera del
bucle de muestras. Su rampa está declarada como 20 ms (`reset(sampleRate, 0.02)`, 960
pasos a 48 kHz), así que en la práctica la subida no duraba 20 ms: duraba 960 **bloques**
— ~2,5 s con bloque 128 y ~10 s con 512 — y el tiempo lo imponía el buffer del host. Al
cargar un preset con reverb, el efecto se arrastraba durante segundos.

**Qué se cambió** (un fichero, `Effects/Reverb.h::processBlock`):

- El parámetro se lee y se aplica **por muestra**: bucle de una muestra contra
  `dsp::Reverb` (`processStereo(left+i, right+i, 1)`), porque `dsp::Reverb` solo expone API
  por bloque. Eso es lo que convierte la rampa en una función del tiempo y no del troceado.
- **Camino rápido** (el habitual): si ninguno de los cuatro smoothers está rampeando
  (`isSmoothing()` falso), se aplica el valor actual una vez y la reverb procesa el bloque
  entero de un golpe. Es el mismo recorrido que el bucle (el port ya itera por muestra por
  dentro), con una sola llamada: el modo por muestra se paga solo los ~20 ms siguientes a
  un cambio de parámetro.
- `applyParameters()` rearma `setParameters()` **solo si el valor cambió** (cuatro
  comparaciones): `setParameters` llama a `updateDamping()` y no queremos eso por muestra.
- Reverb apagada (target y valor actual ≤ 0,002): se sigue saltando el bloque —es
  equivalente, con wet 0 la señal no se toca— pero los cuatro smoothers **avanzan**
  (`skip(numSamples)`), así que encenderla más tarde arranca la rampa donde le toca por
  tiempo, no donde se quedó el bloque anterior. Con objetivo > 0 nunca se salta: la reverb
  procesa desde la primera muestra.

**Verificación** (target `NEURONiK_WasmParityTest` en `build-reference`, `/W4`, 0 avisos):

| Escenario | 64 vs 128 vs 512, a 44,1 / 48 / 96 kHz |
|---|---|
| A, B, E (núcleo) | bit-exacto (0/8192, 0/6144) — sin cambios |
| **C_fx_panico** | **bit-exacto en las 9 celdas (0/12288, maxAbs 0,0)** |
| D_modmatrix | sigue dependiendo (maxAbs 8,7e-3 a 44,1/48 kHz, 3,1e-3 a 96 kHz) |

`blockSizeDependentScenarios` pasa de `["C_fx_panico", "D_modmatrix"]` a
`["D_modmatrix"]`. Y contra el volcado del código anterior (mismo 48 kHz/128): A, B, D y E
**bit-idénticos** — el camino de reverb apagada no se tocó — mientras C difiere en todas
las muestras con maxAbs 8,9e-2 (peak 0,528966 → 0,519034). Diferir es el objetivo: al
acabar el render anterior su wet seguía al ~5% del objetivo (48 de los 960 pasos de la
rampa) y ahora llega a su valor en 20 ms, o sea que el preset con reverb suena como debe
sonar desde el primer bloque.

**De paso:** `WasmParityTest.cpp` usa `fopen`/`fprintf` (deliberado: volcados de MB con
formato `%.9g` que tiene que coincidir con el parser del `.mjs`) y MSVC avisaba `C4996`.
Se define `_CRT_SECURE_NO_WARNINGS` al principio del fichero con el motivo escrito: es un
generador host-only, no entra en el módulo WASM.

**Sigue abierto:** la modulación (`lfo.processBlock` una vez por bloque, escenario D). El
arreglo acordado es la tasa de control fija de 64/128 muestras, y ese sí obliga a
**re-basar** la referencia de paridad.

**Nota de contexto, no tocada:** el mapeo del envoltorio (`wetLevel = mix*0.5`,
`dryLevel = 1 - mix*0.2`) va sobre la escala interna del port, que es la de JUCE
(`dryScaleFactor = 2.0`): ese `dryLevel` da una ganancia seca de **1,6 a 2,0**, o sea un
boost de +4 a +6 dB de la señal seca cuando la reverb está activa (en esa escala, la
unidad está en `dryLevel = 0,5`). Es anterior a este arreglo (lo que cambia es cuándo se
aplica, no la fórmula) y decidir si se re-mapea es producto, no un bug del arreglo.

## Tasa de control fija del motor: 64 muestras (2026-09-19)

Segundo paso de la decisión sobre las dos rutas por bloque. El LFO se leía **una vez por
bloque del host** y la matriz de modulación se aplicaba con ese único valor, así que la
modulación era una escalera de paso = `blockSize`: 128 en el `AudioWorklet`, 512 en un
host de 512. Ahora la rejilla la fija el tiempo, no el buffer.

**Qué se cambió**

- `BaseEngine::kControlBlockSize = 64` + `controlCarry`: el motor renderiza en tramos de 64
  muestras DE AUDIO, con el sobrante **encadenado entre bloques del host** (un host de 96
  da 64+32, y el bloque siguiente empieza cerrando los 32 que faltaban). Así la rejilla no
  se desplaza según el troceado.
- `BaseEngine::renderVoicesWithControlRate(buffer)`: por tramo → avanza los LFOs, aplica la
  matriz (`applyModulation()`, ahora virtual pura en `BaseEngine`) y solo entonces renderiza
  las voces de ese tramo. Los dos motores (`NeuronikEngine`, `NeurotikEngine`) lo llaman en
  lugar de su bucle de voces, y `applyGlobalFX()` deja de leer los LFOs (sigue con FX y
  master por bloque, que ya eran por muestra por dentro).
- `LFO::processBlock` avanza la fase **por muestra** en vez de multiplicarla de golpe
  (`phase_ += increment * (numSamples - 1)`): con troceado, multiplicar redondea distinto y
  la fase del LFO dependería del troceado. De paso desaparece la segunda implementación del
  Sample & Hold que vivía en `processBlock`: avanzaba la interpolación **dos veces por
  muestra** (una en su propio bucle y otra dentro de `generateRandomSampleAndHold`), y el
  doble avance dependía del número de llamadas, o sea que el ruido del S&H también cambiaba
  con el buffer del host. Queda un solo camino, por muestra, y el S&H avanza una vez.
- Por qué 64 y no 128: el LFO llega a 20 Hz (`lfo1RateHz`/`lfo2RateHz`) y a 48 kHz una
  rejilla de 64 muestras son ~37 escalones por ciclo frente a ~19 con 128. Además 64 es
  múltiplo del `kSubBlockSize = 32` de las voces, así que el troceado no crea fronteras
  nuevas dentro de sus sub-bloques. Coste: la voz se llama 2 veces por cuanto en la web y 8
  en un host de 512 (cada llamada reconfigura su resonador desde el modelo espectral, que
  es el trabajo extra), y solo eso.

**Verificación** (target `NEURONiK_WasmParityTest`, `/W4`, 0 avisos)

| Escenario | 44,1 / 48 / 96 kHz × 64/128/512 |
|---|---|
| A, B, C, D, E | **bit-exacto en las 15 celdas** (0 diferencias) |

`blockSizeDependentScenarios` pasa de `["D_modmatrix"]` a `[]`. Y para aislar el cambio,
contra el volcado del paso anterior (reverb ya arreglada): A, B, C y E son **bit-idénticos**
(el camino sin modulación activa no se toca) y D cambia desde la muestra **64** exacta
(maxAbs 2,1e-3 a 48 kHz/128; peak 0,553536 → 0,555390), que es justo la primera frontera de
la rejilla nueva.

**Artefacto pendiente de regenerar:** el volcado `build-wasm/parity-native.json` lo regenera
`build_wasm.bat` y no está en git, pero el `.wasm`/`.js` de `WebPilot/public/worklet/` **sí
está trackeado** y es anterior a los dos arreglos (reverb y tasa de control). Hay que
ejecutar `build_wasm.bat` para regenerarlo, o el test Node comparará un módulo viejo contra
una referencia nativa nueva. No se ha hecho aquí porque necesita emsdk.

## Matematica determinista en el sustrato: sin/atan sin libm (2026-09-19)

Al regenerar el `.wasm` pendiente, la matriz de paridad destapó una diferencia de **10 ulp
en UNA muestra** del escenario C a 44,1 kHz (1/6144; `maxDiff` 2,98e-7, por debajo del guard
absoluto de 1e-6), igual en los tres tamaños de bloque. 48 y 96 kHz seguían bit-exactos, y
nativo y WASM eran cada uno invariante al tamaño de bloque: la diferencia era de toolchain,
no del motor.

**Causa, medida (no supuesta).** Mismo binario compilado con MSVC y con em++ sobre un barrido
de 400.000 argumentos: `sinf` difiere en 611/400.000 valores (1 ulp) y `atanf` en
24.694/400.000. En el lazo del chorus/delay ese 1 ulp se amplifica por encima del presupuesto
de 0 ulps. Se descartó la contracción FMA: `-ffp-contract=off` a solas no cambiaba el
resultado.

**Decisión (canónica, la misma que se tomó con `JUCE_UNDENORMALISE`):** uniformar la
aritmética en vez de relajar el gate. Nuevo `ABDSharedCode/DspCore/DspMath.h` (módulo
DspCore): `abd::dsp::sin/cos/atan` calculados SOLO con operaciones IEEE básicas (+ - * /),
idénticos en los dos toolchains. `DspChorus` y `DspSaturation` los usan. El target WASM
compila con `-ffp-contract=off` (clang podría fusionar `a*b+c` en `fmaf`; MSVC `/O2` no
contrae): es parte del contrato de determinismo del módulo, no un extra.

- Precisión: ~6 ulp de `sin` y ~3 de `atan` frente a la libm (5,2e-7 / 1,8e-7 absolutos),
  inaudible para un LFO y una saturación.
- Determinismo verificado en aislamiento: el mismo barrido compilado con MSVC y con em++ da
  salidas **idénticas bit a bit** (800.001 valores).
- **Aviso:** los valores NO son los de la libm, así que es un cambio de sonido deliberado (el
  motor es pre-1.0). `DspEffectsParityTest` actualiza sus `Reference*` a la misma matemática;
  ese test sigue probando el envoltorio de producto y el determinismo, no el cambio de libm
  (sería circular).
- Tests: `DspCoreTests` gana un bloque de DspMath (valores conocidos + precisión vs libm).

Verificación de esta pasada: `build_wasm.bat` (paridad 15/15 esperada) y `build.bat` (suite).

**Integración en el build general (2026-09-19).** El WASM vivía fuera de `build.bat`, así que el
worklet de `WebPilot/public/worklet/` podía quedarse en una pasada anterior (drift invisible:
los `.js`/`.wasm` se versionan y solo se nota como audio viejo). Ahora `build.bat` lo compila
como **paso [4/9]**, ANTES de exportar la WebUI (que copia `public/` -> `out/`) y de compilar el
host (que embebe `out/`): `call build_wasm.bat --internal-log nopause` (sin anidar su tee ni
quedarse en su pausa). Un fallo de compilación/paridad WASM **aborta el build**; se omite a
propósito con `build.bat nowasm` (y en el modo rápido `build.bat tests`).

`build_wasm.bat` gana además lo que ya tenía `build.bat`: **log espejo `wasm-last-run.log`** y
**pausa final** (saltable con `build_wasm.bat nopause`), para poder leer el resultado sin
prisa y para que el log quede en disco.

También `build.bat` comprueba por tamaño que `WebPilot\out\worklet\neuronik_dsp.wasm` coincide
con `build-wasm\neuronik_dsp.wasm` antes de compilar el host: si Vite no copió `public/` a
`out/`, el worklet embebido sería un DSP viejo y sonaría a la pasada anterior (síntoma mudo),
así que el build aborta. (La copia `build-wasm` -> `public/worklet` la hace `sync-wasm.mjs`,
paso 6/6 de `build_wasm.bat`; es la misma pieza que en ABDMS2000 copia el `.wasm` a las
carpetas de la versión web.)

---

## Andamiaje vainilla de la WebUI (Fase 8, 2026-09-19)

Se ejecuta la decisión de stack que quedó escrita en `ROADMAP.md` (Fase 8): la interfaz de
NEURONiK es **web sobre WebView2 con JS vainilla**, no React. Nace `ABDNeural/WebUI/`.

**Carpeta propia.** `ABDMS2000/WebUI` se usa solo como referencia de arquitectura (un módulo
de puente, uno de contrato, uno de UI); no se comparte código con ese proyecto.

**Portado del piloto, sin cambios de comportamiento** (es JS sin framework; lo que muere con
el piloto es su armazón React `app/page.jsx` + `lib/controls.jsx`):

| WebUI | Origen |
|---|---|
| `src/bridge/bridgeCore.js` | `WebPilot/lib/bridge.js` — transporte del bridge WebView2 |
| `src/contracts/parameters.js` | `WebPilot/lib/parameters.js` — adaptador del contrato generado |
| `src/contracts/paramValue.js` | `WebPilot/lib/paramValue.js` — normalizado ↔ unidades reales |
| `src/wasm/audioParams.js` | `WebPilot/lib/audioParams.js` — contrato → `GlobalParams` del worklet |
| `src/contracts/paramStore.js` | `WebPilot/lib/useParameterControls.js` — el pegamento del hook, ahora store vainilla |

`paramStore.js` es el único port con traducción: el hook guardaba el estado en `useState` y
devolvía handlers memoizados; el store expone un objeto de estado inmutable, `getState()`,
`subscribe()` (llama al oyente de inmediato y en cada cambio, y devuelve un `unsubscribe`) y los
mismos handlers (`pushParameter`, `handleChange`, `handleGesture`, presets, MIDI, modelos).
Mantiene a propósito los handles que el host ya usa: `window.__pilotReady` y
`window.__pilotSendMidi`.

**El contrato no se copia.** `src/contracts/parameters.js` importa
`WebPilot/generated/parameters.generated.js`, que sigue siendo la única copia (la escribe
`NEURONiK_ParameterExport`, paso 2/9 de `build.bat`). Cuando el piloto se retire (8.4) ese
directorio se muda a `WebUI/` y el import es de una línea. El `vite.config.js` abre su
`server.fs.allow` para poder leerlo (igual que el piloto hace con `ABDSharedAssets/tests`).

**Suite propia: 61 tests en 6 ficheros** (`cd WebUI && pnpm test`): los cuatro del piloto
portados (`bridgeCore`, `paramValue`, `parametersState`, `audioParams`), los de integración del
ex-hook (`paramStore`, incluidos gestos, snapshot, presets, MIDI y `subscribe`) y
`appContract.test.js`, que vigila contra el código fuente que el control base siga siendo un
`<input type="range">` de `masterLevel` (lo que conduce el `--selftest` del host) y que no haya
React en la entrada. El piloto sigue en **56/56**.

**Bundle, para el objetivo de 8.5:** `WebUI/dist` sale en **32,4 KB de JS (6,3 KB gzip)** frente
a los 306 KB (89 KB gzip) del piloto React, con las mismas dependencias compartidas. Es la
medición que sostiene "un bundle, un motor de UI".

**Workspace:** `WebUI` es miembro del workspace pnpm anidado de `WebPilot`
(`ABDNeural/WebPilot/pnpm-workspace.yaml`), como ya lo era `WebPilotVite` — así
`@abdsynths/shared` se resuelve zero-copy y un solo `pnpm install` cubre las tres piezas.

**Lo que NO se toca (a propósito):** `build.bat` sigue exportando `WebPilotVite` a
`WebPilot/out`, que es lo que embebe el host del piloto y lo que sirve `start.bat`. `WebUI`
compila a `WebUI/dist` y queda **fuera de ese circuito**: cambiar el motor de UI es un paso
deliberado de 8.2 (paridad de control), no un efecto colateral de crear la carpeta. Tampoco se
migran todavía las pestañas nativas, el LCD/D-pad, el navegador de presets con tags, el MIDI
Learn ni los visualizadores: eso es 8.2 y 8.3, y la lista está en el inventario 8.0 del ROADMAP.

**Dentro del plugin el audio es nativo** (8.1): la página habla por el bridge (APVTS) y el
motor WASM del worklet (`src/wasm/`, ya cubierto por `neuronik_wasm_parity.mjs`) es para la
página **fuera** del plugin. Dos motores sonando no es un caso soportado.

---

## Shell vainilla de la UI (base de 8.2, 2026-09-19) — hecha antes de 8.1 a propósito

Sobre el andamiaje anterior se montó la primera UI de verdad, **sin cablear a nada**: el
host, `build.bat`, `start.bat` y CMake siguen sirviendo el piloto React (`WebPilot/out`).
Esta shell compila a `WebUI/dist`, que hoy no consume nadie.

**Por qué en este orden (y qué me faltó a mí).** Empecé cableando la bancada del piloto
(`NEURONiK Web Pilot.exe`) a la UI nueva y el usuario lo paró con razón: **8.1 es "el
*editor del plugin* hospeda la página"**, y su DoD son Standalone y VST3 con el selftest de
4 direcciones — nada de eso se toca moviendo el *host del piloto* de carpeta. Y la pantalla
que estaba montando es 8.2 (paridad de control), que estaba ajustando a los selectores del
selftest en vez de al panel nativo. Se cerró la shell primero porque es **agnóstica de quién
la hospeda** (hospedar la página es ResourceProvider + adaptadores, los mismos con cualquier
página), así que no se tira: **8.1 es el siguiente paso**.

**Qué hay ahora en `WebUI/src/`:**

| Fichero | Qué es |
|---|---|
| `contracts/screens.js` | Los ids de cada pantalla, como datos (BRIDGE, GENERAL, KEYS), todos del contrato generado. |
| `ui/panel.js` | Shell: pestañas, el **control base** (`masterLevel`) y GENERAL con los 11 ids y su valor real. **Sin widgets** (los de la familia compartida son 8.2); la propia UI lo dice. |
| `ui/keyboard.js` | El teclado compartido (`@abdsynths/midi-keyb`) con la API de feedback del host (vía silenciosa: `setModWheel`/`setPitchBend` no re-disparan callbacks de usuario). |
| `app.js` | Arranque. El orden importa: el panel se monta **antes** de `store.start()` (el host mide "panel in DOM" y "page ready" por separado). |

**El contrato con el host, pinchado en tests** (esto es lo que evita que el E2E falle solo
dentro de WebView2, minutos después):

| Selector / handle | Lo lee | Test |
|---|---|---|
| el PRIMER `input[type=range]` = `#masterLevel` | NATIVE→JS y JS→NATIVE | `panel.test.js`, `keyboard.test.js` |
| `footer.panel-footer code` (JSON normalizado) | GENERAL (11 ids) | `panel.test.js` |
| `[data-tab="keys"]`, `#mod-wheel-container .kbd-wheel-slider` | MIDI | `keyboard.test.js` |
| `window.__pilotReady`, `window.__pilotSendMidi` | métricas y MIDI | `paramStore.test.js` |

El frágil es el primero: **el teclado también monta inputs `type=range`** (las ruedas), así
que el orden de las pantallas (BRIDGE antes que KEYS) es lo que mantiene el slider del
control base en cabeza. Reordenar las pestañas rompe el selftest, y hay test.

**Suite: 81 tests en 9 ficheros** (96 hoy, con la política de audio de 8.1) (`cd WebUI && pnpm test`), incluidos los que montan el
teclado compartido en jsdom de verdad (no una maqueta) y comprueban que `setMidiState`
mueve la rueda a 64 sin devolver el eco como input de usuario. Bundle: **62,4 KB de JS
(15,6 KB gzip)**.

**Lo que NO se hizo, para que no se confunda con hecho:** la paridad de 8.2 (widgets de la
familia compartida, reparto por las seis pestañas del panel nativo, envolvente dibujada,
matriz de modulación, `dspStatus` visible) y por supuesto 8.1. La lista completa sigue en el
inventario 8.0.

---

## Politica de audio fijada en codigo + motor del worklet portado (8.1, 2026-09-19)

Primer bullet de 8.1 cerrado; los otros dos (el editor hospeda la pagina, tamano/zoom) siguen
abiertos, asi que **8.1 no esta terminada**.

**La regla.** NEURONiK embarca el mismo DSP dos veces: el motor nativo del plugin y el modulo
WASM que corre en un AudioWorklet. Los dos sonando a la vez no es un caso soportado (voces
dobles, FX con fase rara) y nada en el protocolo lo impedia. Ahora:

- **Una sola senal**: `window.__JUCE__`, la MISMA que usa el puente. `bridgeCore.js` exporta
  `nativeBackend()` y `WebUI/src/audio/policy.js` lo consume; dos detecciones distintas habrian
  sido dos formas de equivocarse. La politica no puede discrepar del estado del bridge.
- **Una sola puerta**: `startAudioEngine()` (el motor portado) consulta la guarda ANTES de
  nada. Dentro de un host devuelve estado `blocked` con el motivo y **no llega a construir un
  `AudioContext`**; hay test que lo vigila espiando el constructor, que es la unica forma de
  estar seguro de que no hay segundo motor.
- **La pagina que el host sirve HOY tambien la cumple.** El piloto React es el que esta al otro
  lado del WebView2 mientras 8.1 no este, asi que su `audioControl()` recibe `bridgeAvailable`:
  dentro del host pinta `AUDIO: NATIVO` y no ofrece SOUND ON. Esa guarda desaparece con el
  piloto (8.4); el espejo definitivo es `WebUI/src/audio/policy.js`.

**El motor portado** (`WebUI/src/audio/audioWorkletEngine.js`, port de
`WebPilot/lib/audioWorkletEngine.js`) es el mismo ciclo de vida — AudioContext, worklet node,
handshake `neuronik:ready`, mensajes `params`/`engine`/`models`/`midi`/`panic`, teardown —
con dos cambios: la guarda de politica y un estado `blocked` propio (no es un error: es una
respuesta). El `.wasm` cruza por `processorOptions` (el unico canal que funciona desde un
export estatico) y los artefactos ya viajan solos: `publicDir` de Vite apunta a
`WebPilot/public`, asi que `dist/worklet/` sale con el procesador y el `.wasm`.

En la shell, la linea de audio es **letrero o control, nunca las dos**: dentro del host no hay
boton que pulsar, y en el navegador `SOUND ON` arranca el motor y el estado se empuja al
worklet por UN camino (`syncEngine`), el mismo para ediciones de la pagina y snapshots nativos.
Los mensajes MIDI del teclado van por las dos vias (bridge y worklet) porque solo una puede
estar viva segun la politica.

**Tests:** WebUI **96 en 11 ficheros** (nuevos: `policy.test.js`, `audioEngine.test.js`, y el
control de audio en `panel.test.js`), piloto **57 en 6** (con el nuevo guardian de fuente en
`pageContract.test.js`). Bundle: 68,0 KB de JS (17,7 KB gzip).

**Lo que queda abierto de este bullet:** la comprobacion **E2E**. El `--selftest` corre contra
el *host del piloto*, no contra el plugin, asi que la politica esta probada en unitario y en
la pagina, pero no dentro del WebView2 del plugin. Se cierra en 8.1 cuando el editor hospede
la pagina (la via barata entonces: que el selftest del editor lea un handle de la pagina, como
ya hace con `__pilotReady` y `__pilotSendMidi`).

---

## 8.1, paso 1: los adaptadores del bridge dejan de ser del banco de pruebas (2026-09-19)

Los tres adaptadores (`PresetManagerAdapter`, `MidiInjectionAdapter`, `EngineModelsAdapter`)
vivian dentro de `Source/WebPilotHost.cpp` — y **ese era el motivo real de que solo la bancada
del piloto pudiera hablar con la pagina**: no habia forma de reutilizarlos sin copiarlos.

Ahora estan en `Source/WebUI/BridgeAdapters.h`, en `namespace NEURONiK::WebUI`:

- **Header-only a proposito**: no llevan mas estado que una referencia al procesador (que
  sobrevive al bridge en cualquier host), asi que no necesitan `.cpp` ni fuente nueva en ningun
  target. Se incluyen y ya.
- El host del piloto pasa a `#include "WebUI/BridgeAdapters.h"` + tres `using`, y **ningun uso
  cambia** (los 9 sitios: `make_unique`, miembros y declaraciones se quedan igual).
- Comprobado a mano contra `WebUI/ParameterBridge.h`: las tres interfaces se implementan
  literalmente (`PresetController`: list/load/save/current; `MidiController`: noteOn/noteOff/
  pitchBend/modWheel/allNotesOff; `NativeModelController`: getNumModelSlots/getCurrentModel).
- El movimiento es **verbatim**: unico cambio, quitar la calificacion `NEURONiK::WebUI::` de
  los nombres base, que ahora son locales al namespace.

**Sin cambio de comportamiento esperado**: la bancada sigue sirviendo la pagina y el selftest
igual que antes; lo unico que cambia es donde vive el codigo. **Pendiente de compilar** (este
paso no lo he podido verificar yo).

Paso 2 de 8.1, que es el que falta: `ResourceProvider` compartido (disco en dev + snapshot
embebido), el `WebBrowserComponent` dentro de `NEURONiKEditor` (tamano/zoom sin romper
`resized()`), y llevar el selftest de cuatro direcciones a **Standalone y VST3** — hoy ese
arnes solo existe en `WebPilotHost.cpp`.

---

## 8.1, paso 2c: el selftest de cuatro direcciones pasa al editor, y la bancada usa el MISMO (2026-09-19)

Ultimo paso de 8.1 antes de retirar el piloto. El arnes deja de ser de la bancada y pasa a
`Source/WebUI/BridgeSelftest.h`; quien lo corre **en el plugin** es el editor, porque es la
unica superficie que hospeda la pagina en los dos formatos.

**Que se movio**

| Pieza | De | A |
|---|---|---|
| La maquina de estados del selftest | `Source/WebPilotHost.cpp` (~300 lineas dentro de `PilotComponent`) | `Source/WebUI/BridgeSelftest.h` (`NEURONiK::WebUI::BridgeSelftest`) |
| Selectores de la pagina | repartidos por el C++ de la bancada | `SelftestPage` (un solo sitio, documentado como contrato) |
| Quien lo dispara | solo la bancada (`--selftest`) | el editor del plugin (argv en Standalone, `NEURONIK_SELFTEST=1` en cualquier formato) |

La bancada **ya no tiene copia**: la usa igual, enchufandole su navegador y su veredicto. El
port es de comportamiento: los cinco scripts JS, los tiempos (400/600 ms) y el texto de las
lineas del log son los mismos, asi que un log del plugin y uno de la bancada se leen igual.

**El disparo, y por que son dos vias**

```text
NEURONiK.exe --selftest                    -> Standalone (proceso): codigo de salida 0/1
NEURONIK_SELFTEST=1 NEURONiK.exe           -> cualquier formato, y el UNICO del VST3
NEURONIK_SELFTEST_LOG=<ruta>               -> donde queda el transcript (opcional)
```

El VST3 lo lanza el DAW y no recibe argv, asi que la variable de entorno no es un atajo: es su
unica via. Con ella puesta el arnes corre dentro de pluginval o del DAW que abra el editor. El
log por defecto va a datos de usuario del sistema (`.../NEURONiK/neuronik-selftest.log`)
porque el VST3 no puede escribir junto a su propio binario (esa carpeta es del host).

**Dos precauciones que la bancada no necesitaba**, porque alli moria el proceso entero:

- **timeout de 30 s**: un hop perdido termina en FAIL, nunca deja el editor de un DAW colgado;
- **guarda de vida en TODOS los callbacks** (evaluaciones del navegador y `callAfterDelay`): el
  editor se puede cerrar con hops en vuelo, y el arnes se declara despues de `webView` en el
  editor para morir **antes** que el navegador. Sin esto, cerrar el editor a mitad del selftest
  es un `use-after-free` con la firma de un crash aleatorio del plugin.

**El contrato con la pagina, fijado por los dos lados.** Los anclajes que el arnes consulta (el
primer `input[type=range]`, el `<code>` del pie, la pestana KEYS, la rueda de modulacion,
`__pilotSendMidi`) y los 11 ids de GENERAL se comprueban en
`Tests/webuiSelftestContractTest.mjs` (ctest, `NEURONiK_WebUiSelftestContract`, test 18): que el
C++ declare exactamente esos anclajes, que cada uno siga en el fichero de la pagina que lo posee
**y** pinchado en la suite de la propia pagina, y que las dos listas de ids de GENERAL coincidan.
Un anclaje que se mueve en un solo lado deja el selftest comprobando lo que no cree: eso es un
falso OK, y es lo que este test impide. Verificado que falla de verdad (mutacion temporal del
anclaje → exit 1).

**Verificacion (Release, todo en verde)**

```text
build completo (cmake --build build-reference --config Release)  0 errores, 0 avisos propios
ctest -C Release                                                 18/18 (17 + el nuevo)
NEURONiK.exe --selftest                                          las 4 direcciones OK, exit 0
NEURONiK.exe con NEURONIK_SELFTEST=1 (sin argv)                  OK, exit 0
NEURONiK Web Pilot.exe --selftest (arnes compartido)             OK, exit 0 (sin regresion)
```

**Dos regresiones del paso 2, arregladas aqui** (el `build.bat` se paro en 5/10 y el log solo
ensenaba la primera, porque la compilacion aborta):

- **103 errores dentro de `juce_StandaloneFilterWindow.h`** al compilar `NEURONiKEditor.cpp`. El
  paso 2 quito `#include <juce_audio_utils/...>` del editor, y ese header **no incluye sus
  dependencias** (`juce_audio_devices` para `AudioDeviceManager`/`AudioIODeviceCallback`/
  `MidiInput` y `AudioProcessorPlayer` de `juce_audio_utils`): confia en el `JuceHeader.h` del
  wrapper. Los tres includes van ahora explicitos en el guard de `JucePlugin_Build_Standalone`,
  con el motivo escrito al lado.
- **10 `LNK2019` de `ParameterBridge`** al enlazar el VST3: `ParameterBridge.cpp` se quedo fuera
  de los targets del plugin (solo lo anadian la bancada y los tests), asi que
  `NEURONiKEditor.obj` pedia el puente y nadie lo aportaba. Va en el `foreach` de los targets
  que montan pagina y **no** en `NEURONIK_SOURCES`: quien lo necesita es la pagina, no el motor.

**`build.bat` 10/10** ejecuta ahora PRIMERO el selftest del **plugin** (la superficie que se
envia; su Standalone hospeda la pagina de `WebUI/dist`) y despues el de la bancada, que sigue
contando hasta el commit de retirada. El transcript del plugin queda en
`build-reference\neuronik-selftest.log`, para leer el detalle sin depender del stdout.

**Lo que queda de 8.1 / 8:** el **commit de retirada del piloto** (mudar las 3 SSOT —
`WebPilot/generated`, `bridge-protocol.json`, `public/` — y repuntar sus 4 consumidores, mas
`bridgeProtocolContractTest.mjs` y `webviewBridgeDirectionTest.mjs`), y despues 8.2 (paridad de
control). El **VST3 real** con un host dentro (DAW o pluginval) es 8.5: su arnes ya esta puesto
y documentado, pero no se apunta como verificado.

## 8.2 — el lienzo unico: los 70 controles en una pantalla (2026-09-19)

**Peticion:** lienzo mas grande (referencia ABDMS2000) e intentar todos los controles en una
sola pantalla; si no cabe, el sistema de los hermanos (fichas por secciones con lo principal +
panel deslizante lateral, que es EXACTAMENTE lo que hace ABDMS2000 con `slideDrawer`; ABDEep
mide 1200x768 y el suyo tambien va por ahi).

**Lo primero fue medir, no opinar.** Hechos que cambian la decision:

- ABDMS2000 tiene editor de **1080x680** (mas pequeno que el que tenia NEURONiK, 1100x720) y
  reparte casi todo en cajones; ABDEep 1200x768. O sea: "tamano MS2000" NO es mas grande.
- NEURONiK tiene 70 parametros, no los ~40 de un MS2000. Para que quepan de una vez hacen
  falta ~1440x900 de superficie, mas ancho que cualquier hermano: eso es "mas grande" aqui.
- Reparto por el contrato (sin tablas a mano): 46 `float`, 5 `bool`, 19 `choice`.

**Decision: un solo lienzo de 1440x900, sin pestanas de parametros.** Siete fichas en tres
bandas de 12 carriles, todas a dos filas, siguiendo la agrupacion del panel nativo:
OSCILADOR (12) + RESONADOR (4) + GLOBAL & MASTER (9) / FILTRO & ENVOLVENTE (11) + EFECTOS (12)
/ LFO 1 & 2 (10) + MATRIZ DE MODULACION (12) = **70**. La franja de teclado es fija abajo
(como en el nativo) y plegable con el boton TECLADO.

**El encaje es un numero, no una impresion.** `src/contracts/sections.js` es la SSOT del
reparto y la geometria, y `tests/sections.test.js` comprueba con esos mismos numeros: alto
calculado <= 1440x900, fichas a dos filas, bandas que llenan el ancho, y que la CSS declara
las MISMAS medidas (`--abd-cell-h: 80px`, `--abd-knob-size: 48px`...).

**Y se midio en un motor de verdad, porque jsdom no calcula layout.** Arnar temporal en
iframe + Chrome headless a 1440x900 (borrado despues): `scrollHeight == clientHeight == 900`,
`scrollWidth == clientWidth == 1440` -> **cero desborde, cero barras**; 70 celdas (45 knob /
5 toggle / 19 select), 36 teclas y la rueda de modulacion montadas, y el primer
`input[type=range]` del documento sigue siendo `masterLevel`. Esta medicion pago sola: mi
primera cuenta dejaba fuera bordes, huecos de fila y relleno del armazon, y el lienzo
desbordaba **33 px** (scroll activo) sin que ningun test unitario lo notara.

- **Tipo por descriptor, no por tabla:** `float -> Knob`, `bool -> Toggle`, `choice ->
  desplegable`. `masterLevel` es el UNICO control fuera de la familia compartida: su `range`
  nativo es la mitad del contrato del selftest (8.1 paso 2c) y va en la ficha GLOBAL, por
  delante de las ruedas del teclado.
- **Hueco encontrado en `@abdsynths/shared`:** la familia no tiene control de LISTA
  (Knob/Slider/Toggle/Wheel/XYPad). Los 19 `choice` van a un `<select>` nativo estilizado; un
  control `Select` compartido es candidato claro, sobre todo para los 28 destinos de la matriz
  de modulacion.
- **Sin pestanas, pero con los anclajes:** `[data-tab="keys"]` sigue publicado (ahora es el
  boton que pliega la franja de teclado) porque el selftest del host lo pulsa; el anti-drift
  (`Tests/webuiSelftestContractTest.mjs`) lo exige en `contracts/screens.js` y en la suite.
- **Verificacion:** vitest **127/127** (13 ficheros; +3 ficheros nuevos), `pnpm build` OK
  (80,2 KB JS / 20,8 KB gzip), build Release del plugin 0 errores, **18/18 ctest** y el
  `--selftest` del Standalone contra la pagina nueva: 4 direcciones OK, exit 0.
- **Editor nativo:** `NEURONiKEditor` pasa a 1440x925 (`canvasWidth`/`canvasHeight` + la barra
  de menu), para que el WebView reciba exactamente el lienzo. El ancho del lienzo vive en tres
  sitios que tienen que decir lo mismo (constante C++, `CANVAS` de sections.js,
  `--abd-canvas-w` de la CSS); los dos del lado de la pagina se comprueban entre si.

**A/B contra el nativo con el mismo preset (hecho 2026-09-19).** `Tests/nativePanelParityReport.mjs`
extrae los dos inventarios de sus fuentes —los patrones de `Source/UI/**` y el contrato +
`sections.js`— y los cruza id a id con el preset cargado (el INIT del contrato, o un
`.neuronikpreset` real con `--preset`). El informe completo esta anotado en
**`DOCS/WEBUI_VS_NATIVE_PARITY.md`**; el script corre en ctest (`NEURONiK_NativePanelParity`, test
19) solo por sus dos invariantes (ningun id fuera del contrato, ninguna celda repetida) y esta
verificado que **falla de verdad** (mutacion temporal del lienzo -> exit 1).

Lo que sale con el INIT: la pagina ve **70/70**, el nativo **66/70** con algun control y **1/70**
montado en lo que se envia (los paneles estan compilados y sin instanciar: se retiran en 8.4),
**4** parametros sin control nativo en ningun sitio (`oscLevel`, `midiThru`, `velocityCurve`,
`unisonEnabled` — este ultimo `notRouted`, y la pagina es la unica que lo dice), **41/66**
etiquetas distintas (el nativo abrevia: `VOLUME` vs Master Level, `ROOM` vs Reverb Size) y
**4/66** tipos (`mod*Amount`: fader horizontal vs knob).

El unico hueco que es **funcion** y no presentacion: el nativo **filtra los destinos de la
matriz por motor** (desactivando indices 21-27 en un timer de 100 ms) y la pagina entrega
`engines` en el view-model sin que **nadie lo consuma**; ademas el gating nativo es fragil por
indice. Lo demas es cosmetica deliberada de un panel que se retira (incluido que el nativo lee los
valores con `juce::String(v, 2)`, sin unidades). La lectura literal del DoD ("se ve identico")
queda descartada con evidencia: la paridad que se exige es **mismo valor real** y **misma
cobertura**, y eso se cumple.

**RANDOMIZE y primera retirada del arbol nativo (hecho 2026-09-19, despues del A/B).**

- **RANDOMIZE al estado, no al panel.** El sorteo vivia en `ParameterPanel::randomizeParameters()`
  y se iba a perder con el panel. Ahora es **`State/ParameterRandomizer`** (tabla de intencion +
  congelados, probado sin procesador ni UI) y el puente gana la accion **`randomize`** (sin
  campos: la fuerza sale de `randomStrength`, los congelados del APVTS). La pagina tiene el boton
  en la cabecera de GLOBAL & MASTER y sin host queda deshabilitado (`store.randomize()` devuelve
  false en modo local).
- **Dos defectos corregidos, con test** (`NEURONiK_ParameterRandomizerTest`, nuevo, test 20):
  1. la mezcla promediaba el valor actual en **REAL** con el sorteo en **NORMALIZADO**
     (`jmap(strength, currentValue, random0to1)`): todo parametro cuyo rango real no fuera 0..1
     acababa clavado en su maximo (un cutoff saltaba a 20 kHz). Ahora la mezcla es lineal en
     unidades reales y se convierte una vez;
  2. la ventana de `resonatorRes` (0.3..0.95) **no cabia** en su parametro (0.5..1): el test exige
     que cada ventana quepa en su rango, que a fuerza 0 no cambie nada, que lo congelado no se
     mueva y que la mezcla sea el punto medio en Hz.
- **Retirada del arbol sin instanciar.** Fuera del arbol y de `CMakeLists.txt`: los **cuatro
  paneles de parametros**, `PresetPanel`, `PresetBrowser` (y `PresetListModels.h`), `LcdDisplay`,
  `LcdMenuManager`, `SpectralVisualizer` y `EnvelopeVisualizer`. Con `ModulationPanel` se fue su
  `timerCallback`, que **reescribia el APVTS cada 100 ms** (puesto el destino de modulacion en Off)
  y era el bug mas serio que quedaba en el arbol. Se quedan `ParameterPanel` y `XYPad` porque la
  bancada los monta, y el overload de `VerticalSliderControl` porque **no era muerto** (es el
  master vertical: la afirmacion contraria del informe se corrigio).
- **El A/B sigue al arbol**: `Tests/nativePanelParityReport.mjs` pasa de 66 a **15** ids nativos y
  ahora **falla** si una superficie declarada desaparece (antes un borrado lo habria reventado con
  un stacktrace). El informe esta en `DOCS/WEBUI_VS_NATIVE_PARITY.md`.

**Verificacion:** build Release 0 errores · **20/20 ctest** · vitest **132/132** ·
`--selftest` del Standalone contra la pagina nueva (con el boton RANDOM): 4 direcciones OK, exit 0.
No commiteado; `Source/ModelMaker/Version.h` revertido (lo autobumpea compilar ese target).

**Curva ADSR (hecho 2026-09-19).** `src/ui/envelopeCurve.js` dibuja la envolvente de amplitud a
partir de los cuatro `env*` del lienzo, en la **celda libre** de la ficha FILTRO & ENVOLVENTE (11
controles en una rejilla de 6x2), asi que la geometria no cambia ni un pixel: la celda mide los 80
px fijos de `.cell` y el SVG se estira dentro. Decisiones que no son obvias: los tiempos se
comprimen con raiz cuadrada (el rango va de 1 ms a 5 s y el `skew` de esos parametros ya es
logaritmico; en lineal un ataque de 1 ms seria medio pixel) y el tramo de sostenido tiene ancho
propio (no es un tiempo: sin el, un sostenido sin rampas parecia una meseta de ancho cero).
No es una celda de parametro: lleva `.card__visual`, no `.cell`, para que "70 celdas" siga
significando lo mismo en los tests y en el informe de paridad. Vistas y acciones de ficha se
declaran como DATO (`SECTION_VISUALS`/`SECTION_ACTIONS` en `sections.js`) y las resuelve `app.js`;
el panel solo las pinta.

**`Select` compartido, gating por contrato y matriz al cajon (hecho 2026-09-19).** Los tres salen
del mismo hilo: el lienzo tenia 70 controles a la vez y el unico hueco que era FUNCION (no
presentacion) en el A/B era el filtrado de destinos por motor.

- **`Select` en la familia compartida** (`ABDSharedAssets/components/select.js` + CSS + skin
  `vector`/`ms2000` + 14 tests + demo): las 19 listas del lienzo ya no construyen ningun `<select>`
  a mano. Modelo de valor por **INDICE** (el hermano discreto del boolean de `Toggle`): normalizar
  a 0..1 queda al llamador porque en un `choice` ese mapeo lleva el skew del parametro, y meterlo
  en la capa compartida arrastraria matematica del APVTS. Dos cosas que un control nuevo necesita y
  que ahora estan resueltas: `applySkin` **falla con error explicito** cuando no hay renderer para
  un `CONTROL_KIND` (antes salia como "fn is not a function", que parece un error del llamador), y
  el nombre `.abd-select` choca con el de `controls.css` (la libreria CSS previa, que estiliza un
  `<select>` crudo) → el layout del bloque es **opt-in** (`.abd-select--labelled`) para que un
  `<select>` suelto siga viendose igual, con test. Es el unico de la familia sin `drag-core`
  (arrastrar por 28 opciones elige por accidente): el desplegable y las flechas cubren la edicion.
- **Gating de destinos por motor, en el CONTRATO.** `optionEngines` (un motor por opcion) +
  `engineParameter` (el id del selector) salen del generador; en C++ los deriva
  `applyEngineGating()` de la **tabla unica** de destinos (`State/ParameterDefinitions.h`:
  etiqueta + parametro que mueve, EN ORDEN porque el indice es estado de preset) cruzada con
  `engineCoverageFor()`, mas la cobertura de cada opcion del propio `engineType`. El reparto sale
  identico al del panel retirado (neuronik `2 3 10-16 20 21 22`, neurotik `23 24 25 26`, 12 de los
  dos) y el test de contrato lo pincha, junto con las etiquetas POR INDICE y que
  `getModDestinations()` liste la tabla (antes la lista estaba duplicada a mano dentro del layout).
  En la pagina, el `Select` deshabilita lo que el motor activo no consume, pone el motivo en la
  opcion (`title`) y **nunca reescribe el valor**: lo marca (`[data-divergent]`). Si el snapshot no
  trae el motor, no se aplica gating (no se inventa el activo).
- **Matriz de modulacion al cajon lateral.** Una ficha puede declarar `drawer` en el reparto: sus
  celdas se montan en `src/ui/drawer.js` (patron ABDMS2000/ABDEep/ABDCZ101) agrupadas por ruta, y en
  el lienzo quedan el resumen de las 4 rutas (`src/ui/modSummary.js`) y el boton. **Diferencias
  deliberadas con el `slideDrawer` de los hermanos**: el contenido NO se reconstruye al abrir (las
  70 celdas estan siempre en el documento: el selftest y la suite cuentan celdas, y reconstruir
  perderia el gesto en curso) y abrir/cerrar es una clase CSS. Los ids siguen en el reparto, asi que
  store, recuento y cobertura no cambian. Medido en Chrome a 1440x900: **0 px de desborde** con el
  cajon fuera de pantalla, 12 celdas en el cajon y 0 en la rejilla, 4 rutas en el resumen, teclado
  montado y `masterLevel` como primer `range`.
- **Flecos que la pasada dejo de paso**: la fabrica de vistas (`src/ui/visuals.js`) sale de `app.js`
  porque el harness del test la duplicaba (montaba una curva ADSR para CUALQUIER vista declarada, y
  al anadir el resumen contaba dos); el resumen **no** lleva `data-parameter-id` (ese atributo marca
  celdas de control en esta pagina, y marcarlo duplicaba el recuento de los 70); y un cajon
  destruido ya no muta estado.

**Fleco abierto, con el dato localizado: el ANILLO del valor modulado.** El procesador **si**
publica la modulacion viva (`getModulationValueForUI()` sobre `modulationValues[]`, que llenaba el
`ModulatedSlider` nativo), pero **no viaja en el cable**: el protocolo del puente
(`Source/WebUI/ParameterBridge.h`, versionado en `WebPilot/contracts/bridge-protocol.json`) no tiene
canal de modulacion. Hacerlo bien es un cambio de frontera: controlador en el puente + mensaje
aditivo (tipo `midiNoteState`) + poll del host + estado en el store + el anillo como opcion del
`Knob` compartido. Se deja para la proxima pasada **a proposito**: dibujar el anillo con la CANTIDAD
del slot no seria el valor modulado, seria otra cosa con el mismo nombre.

**Lo que queda de 8.2:** el anillo (arriba) y, si una seccion crece, mas cajon (el reparto es
dato: cambiarlo es una linea y el test de encaje avisa). Los slots de modelo A-D se cierran en la
pasada siguiente (abajo).

**Verificacion de esta pasada:** vitest **166/166** en la WebUI (+14 en `ABDSharedAssets`, 53/53) ·
`pnpm build` OK · build Release 0 errores · **20/20 ctest** · `--selftest` del Standalone: **4
direcciones OK, exit 0** · contrato regenerado (`parameters.generated.*`, con `optionEngines`) y
`ModelMaker/Version.h` revertido. No commiteado.

## 8.2 (b) — los slots de modelo A–D: la carga la hace el host y contesta TARDE (2026-09-19)

**Que era en el nativo.** El bloque MODEL de `OscillatorPanel` eran cuatro botones
`loadA..loadD` sobre el XYPad (`buttonClicked`): abrian un `juce::FileChooser` de
`*.neuronikmodel`, cargaban con `processor.loadModel(file, slot)` (slot 0 = A) y **rotulaban la
ranura con el nombre del fichero elegido**. Ese `loadModel` no devolvia nada y salia en silencio
si el fichero no era un modelo valido, asi que un fichero inservible dejaba el nombre puesto
hasta que el timer de 10 Hz lo corregia leyendo `getModelNames()`. Un tick diciendo una cosa y el
motor teniendo otra.

**Que hay ahora.** Ficha **MODELOS A–D** en el lienzo (`span: 2`, en la banda del LFO: la matriz
baja de 8 a 6 carriles porque en el lienzo solo alberga el resumen — 6 le sobran) con cuatro
filas (letra, nombre cargado, **CARGAR**) y una linea de estado (`n/4 cargados`, o el motivo del
ultimo fallo). Las ranuras son del MOTOR, no del APVTS (un preset lleva `modelPath<slot>`, no una
copia de los parciales), asi que la ficha **no tiene celdas**: el encaje de los 70 no se mueve y
un test lo fija, junto al reparto de esa banda.

**Decision de sitio, consultada.** Cualquier fila nueva son 84 px que el lienzo no tiene (18 px de
holgura), asi que el cajon lateral era la salida natural; se pregunto y se eligio **ficha propia
siempre visible** (los cuatro botones del nativo tambien lo estaban, sobre el pad) antes que un
cajon para cuatro botones.

**La carga, en el contrato (aditivo a v1).**

- `loadModel { slot: 0..3 }` (JS→nativo) es la **unica accion cuya respuesta llega DESPUES**: la
  pagina no tiene sistema de ficheros ni puede nombrar rutas (el cable no es de fiar), asi que el
  HOST abre el dialogo (`EngineModelsAdapter`, con el `juce::FileChooser` que tenia el panel) y
  contesta mas tarde. `NativeModelController` gana `loadModel(slot)` y `getModelName(slot)`.
- `modelsState` viaja con **`name`** por ranura ("EMPTY" cuando no hay nada): el nombre solo lo
  sabe el motor y las dos superficies tienen que ensenar la MISMA lista.
- `modelError { slot, detail }`: cancelar, elegir algo que no es un modelo, ranura fuera de rango
  o **fraccionaria** (se rechaza, no se trunca) o no haber backend. Una carga nunca falla en
  silencio, que era justo el defecto de arriba. `stats.modelLoads` / `stats.modelErrors` lo hacen
  observable.
- `NEURONiKProcessor::loadModel` ahora devuelve `bool` (antes no habia forma de saber si la carga
  ocurrio) y solo renombra la ranura cuando el modelo es valido.
- El contrato versionado documenta las tres formas y la regla (`behaviour.modelMessages`), y los
  dos tests de contrato pinchan los literales nuevos.

**Vida de un dialogo abierto.** El adaptador es dueño del `FileChooser` y `juce::FileChooser`
suelta su callback pendiente al destruirse (`~FileChooser` limpia `asyncCallback` y el pimpl se
apaga con `safeThis.lock()` fallando), asi que un host cerrado a media eleccion de fichero no
contesta a un puente muerto; en el editor, ademas, los adaptadores se declaran DESPUES del puente
(mueren antes) y el destructor corta el transporte primero. Se comprobo en las fuentes de JUCE
(`juce_FileChooser_windows.cpp`) antes de apoyarse en ello.

**En la pagina.** `loadModel(slot)` en el store (devuelve false sin host: no hay a quien pedirselo,
igual que `randomize`), `modelError` en el estado (un intento nuevo limpia el mensaje viejo, que es
de otra carga), y la vista `src/ui/modelSlots.js` como DATO de ficha (`SECTION_VISUALS`,
`parameterIds: []`): el panel le da sitio y **estado entero** (`paint(parameters, state)`, porque
`models`/`modelError` viven fuera del APVTS) y el handler de carga le llega por `options.onLoad`
desde `app.js`, para que el panel siga sin saber que dibuja cada vista. Un nombre con
`isValid: false` se **marca** en ambar (mismo criterio que el gating del `Select`), no se oculta.

**Verificacion de esta pasada:** build Release **0 errores** (VST3 + Standalone + bancada) ·
**20/20 ctest** · vitest WebUI **185/185** (+19: 12 en `tests/modelSlots.test.js` y el resto en
`sections`/`panel`/`bridgeCore`/`paramStore`/`appContract`) · `pnpm build` OK (95.2 KB JS) ·
medicion en Chrome a 1440x900 con la ficha nueva: **0 px de desborde**, 8 fichas, **70 celdas**,
19 select, 4 filas de ranura (228x206 px de ficha, 164 px de cuerpo para 86 px de vista) y
`masterLevel` como primer `range` · `--selftest` del Standalone: **4 direcciones OK, exit 0**. No
commiteado.

## 8.2 (c) — «cargar un modelo en A–D se ve y suena», comprobado, y destapa un fallo viejo (2026-09-19)

**El encargo, partido en dos mitades porque una sola superficie no puede hacer las dos.** Un proceso
con ventana no puede medir su propia salida de audio sin pelearse con el hilo que la está tirando
(con un dispositivo de audio abierto lo hace el `AudioProcessorPlayer`), así que la prueba se reparte:

- **SE VE** — la **quinta dirección** del selftest (`Source/WebUI/BridgeSelftest.h`), corriendo en el
  **plugin real**: escribe cuatro `.neuronikmodel` en el directorio temporal, los carga por
  `NEURONiKProcessor::loadModel` en A–D y lee los cuatro nombres (`.model-slots__name`) de la ficha de
  la página de verdad, en su WebView2. Se escriben en el **JSON del ModelMaker** a propósito: si el
  plugin dejase de entender ESE formato, las ranuras se quedan mudas y la dirección lo dice en voz
  alta en vez de dar OK con cuatro ranuras vacías.
- **SUENA** — `Tests/ModelSlotTest.cpp`, con el procesador real (**26 comprobaciones**).

**Lo que destapó la mitad «suena», que es la razón de que exista.**

1. **El formato del ModelMaker nunca se leía.** `PresetManager::loadModelFromFile` solo entendía el
   dialecto XML (`<NEURONIK_MODEL amplitudes=".." offsets=".."/>`) y la herramienta escribe **JSON**
   (`{amplitudes[64], frequencyOffsets[64], name, description}`), así que **los ficheros de la propia
   herramienta no cargaban**: la ranura se quedaba con nombre y sin sonido — exactamente el síntoma
   del panel nativo, pero por otra causa. Ahora se leen los dos dialectos y lo que no es un modelo se
   rechaza sin tocar la ranura.
2. **Mi primer arreglo tenía un `use-after-free`.** `juce::JSON::parse(texto).getDynamicObject()` sobre
   un `var` **temporal** devuelve un puntero que muere al final de la sentencia: el objeto parseaba
   «bien» y salía **vacío**. Lo cazó el propio test (un literal parseaba y el fichero no), y el patrón
   correcto es el que ya usaba `BridgeSelftest.h`: guardar el `var` y leerlo después. Anotado porque es
   un fallo con la firma de un crash aleatorio en producción, no de un error de compilación.
3. **Un crash de división por cero que NO era del test.** El test reventaba con
   `STATUS_INTEGER_DIVIDE_BY_ZERO` (0xC0000094) en el primer bloque **con nota**. La causa: cambiar de
   motor construye un motor nuevo y lo prepara con `getSampleRate()`/`getBlockSize()`, que valen 0
   hasta que el host fija la tasa. Dos caminos lo disparaban:
   - el **constructor del procesador** llamaba `LOAD_PARAM(engineType)`, así que construía un SEGUNDO
     motor (32 voces, delay, reverb) para prepararlo con 0 y tirar el primero — en Debug eso es un
     `jassert` en el smoothed value de la saturación. El motor de arriba ya se creó con ESE mismo
     valor; el que queda lo deja a punto `prepareToPlay`. `engineType` sale de esa lista.
   - `parameterChanged` preparaba el motor nuevo con la tasa del host **aunque el host no la hubiera
     dado**: ahora, sin tasa, el motor se prepara en `prepareToPlay` (un host siempre llama a prepare
     antes de que haya audio). Es la misma clase de bug que un preset restaurado antes de que el
     reproductor arranque, así que se cierra con guarda **y con test** (sección 6).
   El test ahora se prepara como lo hace un host (`setRateAndBufferSizeDetails`) en vez de confiar en
   `prepareToPlay`, que es una llamada virtual: **fue el arnés el que mentía**, no el motor.

**Una tercera cosa, ya en el arnés: ni un OK engañoso ni un FAIL ajeno.** La bancada del piloto sirve
todavía su exportación retirada (`WebPilot/out`), anterior a la ficha MODELOS A–D, así que su quinta
dirección fallaba por una página que ya no se mantiene — y un FAIL ahí no habla del puente. El arnés
no adivina: **el dueño declara** `BridgeSelftest::PageCapabilities`, cuyo defecto es la capacidad
**COMPLETA** (callar no libra de ninguna dirección), y la bancada declara `retiredPilotPage()`. Su
transcript dice `MODELOS: OMITIDO (no aplicable): la pagina de esta superficie no publica la ficha…` y
el veredicto lo repite **en su misma línea** (`RESULT: OK  (MODELOS no aplicable en esta pagina;
obligatoria en el plugin)`), para que un OK de cuatro no pueda parecer un OK de cinco. El editor del
plugin no declara nada y `Tests/webuiSelftestContractTest.mjs` lo fija por los dos lados: capacidad
por defecto completa, parámetro opcional, y **el único que se acoge al omitido es la bancada**.

**Verificación de esta pasada:** build Release **0 errores** (VST3 + Standalone + bancada) ·
**21/21 ctest** (incluido el nuevo contrato del arnés) · vitest WebUI **185/185** · `--selftest` del
**Standalone**: `MODELOS`, `NATIVO -> JS`, `JS -> NATIVO`, `GENERAL` y `MIDI` en verde, `RESULT: OK`,
**exit 0** · `--selftest` de la **bancada**: las cuatro que su página soporta en verde, `MODELOS
OMITIDO` declarado, **exit 0**. No commiteado.

## La bancada sirve la página del PLUGIN (y `--pilot-page` para la retirada) (2026-09-19)

**El problema, en una frase.** La bancada era, desde el principio, la única superficie con página; el
plugin la hospeda desde el paso 2c, y entonces la bancada se quedó comprobando **otra** página (la
exportación retirada, sin la ficha MODELOS A–D) que ya nadie mantiene: un veredicto verde ahí no decía
nada de lo que se envía, y su quinta dirección tenía que declararse no aplicable.

**Qué cambia.** `Source/WebPilotHost.cpp` gana `PageSource` (`pluginWebUI` por defecto, `pilotExport`
con `--pilot-page`):

- **Por defecto sirve `WebUI/dist`** (la página que embebe el plugin) desde disco, con la ruta
  grabada en el binario (`NEURONiK_WEBUI_DIR`) y el mismo override de desarrollo que el plugin
  (`NEURONIK_WEBUI_DEV_DIR`, apuntar ahí y recargar). Su `--selftest` corre las **cinco**
  direcciones, igual que el Standalone, y por eso `build.bat` ya no puede dar un verde de cuatro.
- **El fallback embebido no cambia de página.** El snapshot del exe es la exportación del piloto
  (`NEURONiK_WebPilotAssets`), así que solo se usa cuando la página servida ES esa (`--pilot-page`).
  Con la del plugin, un fallo de disco da la página de diagnóstico visible — que dice qué página se
  estaba sirviendo, dónde la buscó y cómo arreglarlo — en vez de responder a `WebUI/dist` con la
  página del piloto: un fallback que cambia de página no es un fallback.
- **La capacidad del arnés viaja con la bandera**, no con el ejecutable: `--pilot-page` declara
  `PageCapabilities::retiredPilotPage()` (y su transcript lo dice en voz alta); el defecto no declara
  nada, o sea las cinco direcciones obligatorias. Es el camino débil **pedido a propósito**.
- **El reporte dice qué página se sirvió** (`page`, `page root`) además de las métricas de arriba,
  así que un log viejo no se puede confundir con el de otra página.
- **`build.bat` simplificado de paso:** la guarda del selftest de la bancada ya no es
  `WebPilot\out\index.html` ni el resultado de la exportación del piloto (`WEBUI_BUILD_FAILED`
  desaparece), sino `WebUI\dist\index.html`: lo que se comprueba es la página que se envía.

**Verificación (las dos rutas, sobre el mismo binario):**

```
"NEURONiK Web Pilot.exe" --selftest              -> sirviendo WebUI/dist (la pagina del plugin)
   MODELOS OK · NATIVO->JS OK · JS->NATIVO OK · GENERAL OK · MIDI OK · RESULT: OK  (exit 0)
"NEURONiK Web Pilot.exe" --selftest --pilot-page -> sirviendo WebPilot/out (la exportacion retirada)
   MODELOS: OMITIDO (no aplicable) · las otras cuatro OK · RESULT: OK (MODELOS no aplicable)  (exit 0)
```

**Fleco abierto:** el snapshot embebido de la bancada sigue siendo el del piloto (y su target
**exige** `WebPilot/out` para configurarse). Cuando llegue el commit de retirada, eso se va con él:
la bancada pasará a servir solo `WebUI/dist` y su `CMakeLists` perderá el `FATAL_ERROR` y
`NEURONiK_WebPilotAssets`.

## La dirección MATRIZ: el selftest corre con el cajón abierto y la matriz en uso (2026-09-19)

**El encargo.** Verificar que el cajón lateral de la matriz no rompe el selftest del plugin **con la
matriz en uso** — no que el cajón pinte bien (eso lo cubre `WebUI/tests/drawer.test.js`), sino que el
resto del arnés siga midiendo lo mismo cuando la UI está en ese estado.

**Cómo se comprueba, y por qué así.** El arnés gana una dirección **0 (MATRIZ)** que corre la PRIMERA y
deja el cajón **abierto a propósito**, para que las otras cinco pasen con la matriz en uso y el lienzo
tapado (un anclaje que deja de ser el primero del documento, un cajón que roba el foco, un poll que
deja de empujar: es ahí donde se rompe una UI):

1. pone la matriz **en uso** por el APVTS, que es por donde la pondría un preset: ruta 1 = LFO 1 →
   Filter Cutoff, cantidad +0.5. Los índices que se esperan en la página NO están escritos en el
   arnés: el destino sale de `getModDestinationTable()` (append-only, y su orden ES estado de preset)
   y la fuente de `getModSources()`, así que mover un destino no invalida la dirección;
2. **pulsa el disparador** `[data-drawer-trigger]` — el mismo clic que daría un usuario, no una clase
   puesta a mano — y lee, **dentro del cajón abierto**: sus 4 rutas, sus 12 celdas (3 por ruta), la
   fuente y el destino como `selectedIndex` del `<select>` nativo y la cantidad en el `aria-valuenow`
   del dial del `Knob` compartido. Que el cajón abierto y su velo compartan id se exige aparte: si no
   coinciden, lo que se abrió no es el diálogo que ese disparador gobierna.

Leer los controles **dentro** del cajón no es un detalle de estilo: si la celda estuviera en el
lienzo, la aserción pasaría igual y el cajón podría estar vacío.

**El arnés se cazó a sí mismo (y por eso la dirección vale).** La primera versión comparaba
`drawer.dataset.drawer` con `trigger.dataset.drawerTrigger` para saber que el cajón abierto era el del
disparador: son **ids distintos** (`drawer-modMatrix` — el id del DOM — frente a `modMatrix` — el de la
sección), así que la dirección dijo **FAIL** en su primera corrida con todo lo demás ya en verde. El
fallo no era del cajón: era una suposición mía sobre el DOM. Se sustituyó por la pareja cajón/velo
(comprobación más fuerte y verdadera) y por leer los controles dentro del cajón.

**Dónde SÍ y dónde NO.** La capacidad `matrixDrawer` (`PageCapabilities`) nace **completa**: la página
de la bancada del piloto retirado no lleva cajón, y con `--pilot-page` la dirección se declara NO
APLICABLE en voz alta, igual que MODELOS. El `RESULT` lista **las dos** en su misma línea
(`RESULT: OK  (MATRIZ, MODELOS no aplicable(s) en esta pagina; obligatorias en el plugin)`), y el
contract-test fija que el único que se acoge al omitido sea la bancada.

**Verificación de esta pasada:** build Release **0 errores** · **21/21 ctest** (contrato del arnés
incluido, con los 5 anclajes nuevos pinchados por los dos lados) · vitest WebUI **185/185** (la página
no cambia: esta dirección no pide una línea de JS nueva) · `--selftest` del **Standalone**: MATRIZ +
MODELOS + NATIVO→JS + JS→NATIVO + GENERAL + MIDI en verde, `RESULT: OK`, **exit 0** · `--selftest` de
la **bancada** sobre la página del plugin: **las seis**, exit 0 · `--selftest --pilot-page`: cuatro
verdes y **dos omitidos declarados**, exit 0. No commiteado.

**Lo que esta dirección NO comprueba (a propósito).** Que la modulación llegue al AUDIO: eso vive en
el motor y el arnés no puede exigirlo sin garantizar que el host esté corriendo `processBlock` (en el
VST3 con un DAW dentro, el editor puede estar abierto con el audio parado). Lo que sí queda medido es
que la matriz está configurada y que **la página la enseña donde tiene que enseñarla**.

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida esta dirección
> (MATRIZ, la 2, y su subseccion «MATRIZ (canon): la ruta se pone por el APVTS y el cajon se queda
> ABIERTO»). Esta entrada cuenta como se llego a ella. OJO al leerla hoy: el texto cita la maquinaria
> de "NO APLICABLE" y la página del piloto RETIRADO, que ya no existen — la matriz se mide hoy en las
> DOS superficies (plugin y bancada) sin omitidos, y el cajon se queda abierto a propósito porque las
> direcciones siguientes corren con el lienzo tapado.

## Retirada del piloto: las tres SSOT se mudan y la bancada pierde su snapshot (2026-09-19)

**El encargo.** Retirar el piloto: mudar sus tres SSOT (el contrato de parámetros, el contrato
versionado del protocolo y `public/`) y quitar el snapshot que la bancada llevaba embebido, con
los consumidores repuntados en el mismo commit.

**Cuatro cosas que se decidieron mirando el árbol, no el plan:**

1. **La bancada SE QUEDA** — y no es un matiz: el plan decía "se borra tras la mudanza de las
   SSOT", pero al ir a borrarla aparece que `Source/WebPilotHost.cpp` es la **única** superficie que
   monta `ParameterPanel` + `XYPad` (el comentario del procesador ya lo decía: *"its only consumer
   since the EnvelopeVisualizer was retired"*) y la única que mide el arranque (`--auto-quit` →
   `pilot-startup.log`). Borrarla se habría llevado por delante el pad XY y las métricas. Lo que se
   va es su **camino del piloto**: `--pilot-page`, la elección de página (`PageSource`/`pilotRoot`),
   el respaldo embebido (`loadEmbeddedResource` + `NEURONiK_WebPilotAssets`, y con él el
   `FATAL_ERROR` de CMake que exigía `WebPilot/out` para poder configurar) y la maquinaria de
   omisión del arnés. Ahora sirve **solo** `WebUI/dist`, desde disco, y si el disco falla da la
   página de diagnóstico (que dice dónde la buscó) en vez de contestar con otra página.
2. **Los nombres "Pilot" se quedan.** El target (`NEURONiK_WebPilotHost`), su `.exe`
   (`NEURONiK Web Pilot.exe`), `Source/WebPilotHost.cpp`, `pilot-startup.log` y el helper
   `__pilotSendMidi` conservan el nombre. Renombrarlos toca CMake, `build.bat`, `start.bat`,
   `bridge-protocol.json` y tres tests (uno de ellos tiene el helper **pinchado por los dos lados**:
   el C++ y la suite de la página), y no era parte de este encargo. Queda anotado como deuda
   cosmética: mientras el nombre sea el único resto, el arnés no corre contra nada que se llame
   "piloto".
3. **El workspace pnpm de la WebUI era el del piloto.** Hallazgo que no estaba en ningún plan:
   `WebPilot/pnpm-workspace.yaml` listaba `'../WebUI'`, así que los enlaces de
   `WebUI/node_modules/@abdsynths/{shared,midi-keyb}` salían de ahí. Borrarlo sin más habría dejado
   a la WebUI sin poder resolver sus dos dependencias de workspace (el `pnpm install` desde WebUI
   camina hacia arriba, encuentra el workspace raíz de la suite —que **no** lista `ABDNeural/WebUI`—
   y deja `node_modules` sin enlaces). La WebUI estrena el suyo: `WebUI/pnpm-workspace.yaml`
   (ella + los dos paquetes compartidos) y `WebUI/pnpm-lock.yaml`.
4. **El contrato del protocolo nombraba ficheros muertos.** Su lista `implementations` decía
   `WebPilot/lib/bridge.js` y `WebPilot/app/page.jsx`, y `BridgeProtocolContractTest` **exige que
   existan** ("every implementation file the contract names exists"): ahora son
   `WebUI/src/bridge/bridgeCore.js`, `WebUI/src/app.js` y `Source/WebUI/NeuronikWebView.h`. De paso,
   `webviewBridgeDirectionTest.mjs` pasa a exigir **dos** emisores (`WebPilotHost.cpp` **y**
   `WebUI/NeuronikWebView.h`): el editor emite hacia la página desde 8.1 y el guard solo vigilaba
   uno.

**Las mudanzas (con `git mv`, para que se lean como mudanza y no como copia):**

| De | A | Consumidores repuntados |
|---|---|---|
| `WebPilot/generated/` | `WebUI/generated/` | `CMakeLists.txt` (`NEURONIK_PARAMETER_ARTIFACTS_DIR`), `Tests/ParameterExportTool.cpp` (destino por defecto), `build.bat` (paso 2/9) |
| `WebPilot/contracts/bridge-protocol.json` | `WebUI/contracts/bridge-protocol.json` | `CMakeLists.txt` (`NEURONIK_BRIDGE_PROTOCOL_JSON`), los dos tests del protocolo |
| `WebPilot/public/` (assets + worklet) | `WebUI/public/` | `WebUI/vite.config.js` (`publicDir`), `build_wasm.bat`, `sync_wasm.bat`, `start.bat` (guard de staleness) |
| `WebPilotVite/scripts/sync-wasm.mjs` | `WebUI/scripts/sync-wasm.mjs` | `build_wasm.bat` (paso 6/6), `sync_wasm.bat`, y un `pnpm sync:wasm` nuevo en la WebUI |
| `WebPilot/BRIDGE_PROTOCOL.md` | `DOCS/BRIDGE_PROTOCOL.md` | `Source/WebUI/ParameterBridge.h`, `HANDOFF` |
| `WEB_PILOT.md` | `DOCS/PILOT_RETIRED.md` (con aviso de que el piloto no existe) | la referencia del bridge en `HANDOFF` |
| — | `WebUI/pnpm-workspace.yaml` + `WebUI/pnpm-lock.yaml` | `pnpm install` de la WebUI |

**Lo que sale del árbol:** `WebPilot/` completo (app Next, `lib/`, `tests/`, `out/`, `package.json`,
`next.config.mjs`, `vitest.config.js`, su `.gitignore` y su workspace anidado) y `WebPilotVite/`
(el contra-piloto React); la bandera `build.bat nextui`; el camino `--pilot-page` de la bancada; el
snapshot `NEURONiK_WebPilotAssets` y su `FATAL_ERROR`; y `BridgeSelftest::PageCapabilities` con
`retiredPilotPage()` — su único usuario era la página retirada.

**Un cambio que se ve en el veredicto del arnés.** El `RESULT` deja de poder llevar el sufijo
`(MODELOS no aplicable en esta pagina; obligatorias en el plugin)`: no hay páginas a las que rebajar
el listón. `webuiSelftestContractTest.mjs` da la vuelta al check — antes exigía que la capacidad
fuera completa y que el omitido tuviera un único usuario; ahora **prohíbe** las dos cosas
(`PageCapabilities`, `retiredPilotPage`, `capabilitiesToUse`, `matrixSkipped`, `modelsSkipped`): no
basta con no usarla, no puede volver.

**Verificación de esta pasada:** `cmake -S . -B build-reference` **sin** `WebPilot/out` (el
`FATAL_ERROR` ya no existe y no lo echa de menos) · build Release **0 errores** (Standalone + VST3 +
bancada + 20 targets de test) · **21/21 ctest** · WebUI **185/185** con `pnpm test` y `pnpm build`
(`dist/worklet/` sale del `public/` mudado: la prueba de que el `publicDir` es el nuevo) · el
exportador regenera el contrato en `WebUI/generated/` con los mismos 70 parámetros · `--selftest` del
**Standalone**: MATRIZ + MODELOS + NATIVO→JS + JS→NATIVO + GENERAL + MIDI, `RESULT: OK`, exit 0 ·
`--selftest` de la **bancada**: las seis, exit 0, y `pilot-startup.log` con `page: WebUI/dist (la
pagina del plugin)`, `page root: .../WebUI/dist`, 4 recursos y sin línea de respaldo embebido ·
`node WebUI/scripts/sync-wasm.mjs` escribe en `WebUI/public/worklet/`.

**Lo que NO se ha tocado:** el VST3 con un host dentro sigue siendo 8.5; el panel nativo y el
`XYPad` siguen vivos *por* la bancada (su retirada es 8.4); y el anillo del valor modulado sigue
siendo el fleco abierto de 8.2.


## Documentación al día (2026-09-20)

Pasada cosmética de documentación, sin tocar código ni build:

- **`README.md` reescrito en lo operativo**: el build documentado es `build.bat` con sus modos
  (`tests`, `noselftest`, `nowasm`, `modelmaker`, directorio alternativo) y los artefactos en
  `build-reference/NEURONiK_artefacts/`; prerequisitos reales (JUCE por `JUCE_PATH` o `C:\JUCE`,
  Node+pnpm, emsdk en `C:\emsdk` para el WASM). Sección nueva de la versión web (`start.bat` 1-3,
  LOCAL MODE en el 8399, selftest de seis direcciones) y de la WebUI (contrato SSOT generado del
  APVTS, protocolo versionado, worklet WASM con paridad bit-exacta). `Scripts/manage.ps1` queda
  marcado como legado.
- **Secciones históricas anotadas** en este HANDOFF («Próximo trabajo recomendado», «Decisiones
  pendientes», «No hacer todavía») y nota de registro corrido bajo el título, para que nadie
  tome por vigente una decisión del arranque.
- **`DOCS/PLANS/ROADMAP.MD` pasa a stub**: era la copia congelada del 09-16 y había divergido de
  la raíz (que siguió creciendo con las Fases 7 y 8). El roadmap vivo es y sigue siendo
  `ROADMAP.md` en la raíz —lo citan este documento, `DOCS/PILOT_RETIRED.md`, `WebUI/README.md` y
  el test de paridad—; el README ahora enlaza a la raíz.

Sin verificación de build: esta pasada solo toca los tres documentos.

## 8.3, paso 1: el pad XY dibujado (2026-09-20)

**El encargo.** Cablear el XYPad de `@abdsynths/shared` en la WebUI como primera pieza del
8.3: pad para morphX/morphY con los nombres de los modelos A–D, con su test.

**Dónde vive cada cosa** (decisión de capa):

- **El COMPONENTE gana la capability en el compartido**: `ABDSharedAssets/components/xypad.js`
  (ya era del paquete — nació en la era del piloto) añade `corners` (opción) y `setCorners()`
  (en caliente): una etiqueta por esquina `[arriba-izq, arriba-der, abajo-izq, abajo-der]`,
  `''` oculta la suya. Con esquinas visibles el readout de porcentaje se esconde
  (`abd-xypad--corners`): las esquinas dicen QUÉ hay donde, la cruz dice DÓNDE, y el valor
  sigue en `aria-valuetext`. Estilos en `widgets.css`, 4 tests nuevos (16 en su fichero;
  suite 57/57) y `COMPONENTS.md` actualizado. Cero impacto en quien no la use: sin
  `corners`, el DOM del pad no cambia.
- **El wiring es de NEURONiK**: `WebUI/src/ui/xyPad.js` monta el compartido (200x150),
  coordina DOS gestos (uno por eje, fase completa `begin/change/end`; el `begin` anuncia el
  valor actual, igual que `handleGesture`) vía `pushParameter` — `handleChange` solo sabe
  cerrar UN id — y pinta las esquinas desde `state.models` con el mismo criterio de ranura
  vacía que las ranuras (`displayableName`, ahora exportada). Un paso de teclado viaja como
  `end` y SOLO en el eje que cambió. `paint` no pega con el dedo: durante un drag, el
  snapshot del host no mueve el pulgar.
- **La ficha MODELOS A–D es compuesta** (fábrica `visuals.js`): pad encima, ranuras debajo —
  el bloque MODEL del panel nativo, que era exactamente eso. `app.js` inyecta
  `onEdit → store.pushParameter`; el contrato de fuente de `appContract.test.js` vigilaba la
  línea del despachador y se actualizó con la composición.

**El encaje, por delante de la vista**: el pad necesita cuerpo real, así que
`SECTION_VISUALS['model-slots']` declara `minBodyHeight: 256` y `cardHeight()` lo respeta
(`max(pilas de celdas, cuerpo de vista)` — con filas de celdas no cambia nada). La banda 3
la cerraba el LFO (206); ahora la cierra MODELOS (298) → `CANVAS.height` 900 → **990** y
`--abd-canvas-h` a la par (el test de geometría exige el MISMO número en las dos SSOT). Los
knobs morphX/morphY SIGUEN en OSCILADOR: el pad es aditivo, `SECTION_PARAMETER_IDS` sigue en
70 y la paridad del informe no se mueve.

**Verificación de la pasada**: ABDSharedAssets **57/57** · WebUI **191/191** (`pnpm test`, 6
tests nuevos: montaje, teclado→`end` por eje cambiado, drag→dos gestos coordinados, paint
sin eco y sin pelea con el dedo, esquinas desde `modelsState`, composición) · `pnpm build`
en verde · informe de paridad `Tests/nativePanelParityReport.mjs`: *Estructura OK · 70
celdas web · contrato 70*. Sin tocar C++: el contrato de parámetros no cambia, no hay paso
2/9 que regenerar.

**Lo que NO es este paso**: el anillo del valor modulado que dibuja el pad nativo (ghost
ring con la posición base) sigue siendo el fleco abierto de 8.2 — necesita la telemetría del
puente. Y el espectral y el scope del mismo ítem 8.3 siguen esperando el canal de lectura a
30-60 Hz.

## 2026-09-20 (b): Load/Save Preset suben del menú Edit al File

Pedida por el usuario mientras probaba: la barra File/Edit/Help es del editor NATIVO
(`NEURONiKEditor::getMenuBarNames`), no de la página. `Load Preset...` y `Save Preset...`
viven ahora en **File** (entre New Session y Exit, como manda la convención); Edit queda
con Copy/Paste Patch, MIDI Channel, Voices, Zoom y Options. Los IDs de menú NO cambian
(1 y 2), así que `menuItemSelected` queda intacto.

**Hallazgo de la recompilación**: el editor lleva la WebUI EMBEBIDA
(`NEURONiK_WebUIAssets`, el fallback del 8.1) — el exe del 8.2 que probaba el usuario
servía la página vieja incrustada aunque `WebUI/dist` ya tuviera el pad. Recompilado el
Standalone (`--target NEURONiK_Standalone`): exe y embed llevan la página nueva. El VST3
se queda con el menú viejo hasta el próximo `build.bat`.

## 2026-09-20 (c): el teclado "no se distinguía" porque la página se SALÍA del viewport

**Diagnóstico (con captura del usuario):** el keybed compartido está BIEN (marfil sobre
`--kbd-bg` con sus fallbacks — nada que arreglar en `midi-keyb`). Lo que pasaba: la página
es un lienzo de diseño FIJO (1440x990) SIN ningún ajuste al viewport, y el editor es
redimensionable — en la ventana del usuario (~1424x780 CSS útiles tras la barra nativa)
sobraban ~210px por abajo: el pie y CASI TODA la franja del teclado quedaban fuera de
vista. El pad del 8.3 (lienzo 900→990) agravó un corte que ya existía en 8.2.

**Arreglo, en su capa (la página):** `WebUI/src/ui/fitStage.js` — `computeFit` (escala
acotada 0.25x–3x como el zoom nativo, centrado en el eje que sobra) y `mountFitStage`
(transform + margins + resize) sobre `#app` desde `app.js`; `body` con `overflow: hidden`
porque el transform no cambia el box de layout. Es el "escala del contenedor" que el
ROADMAP pone como sustituto del zoom. El componente compartido no se toca.

**Verificación:** WebUI **198/198** (`pnpm test`, 7 tests nuevos de fitStage) · `pnpm
build` verde · contrato de fuente de `app.js` actualizado (import con `CANVAS` + línea del
ajuste). El jsdom no mide layout: lo que se prueba es el CÁLCULO (escala/offsets/acotas)
y el ciclo de vida del listener de resize.

## 2026-09-20 (d): NEURONiK, primer consumidor del fondo tintable de la suite

**Que:** la WebUI adopta el mecanismo `.abd-theme-bg` de `@abdsynths/shared` (hoja nueva
`styles/components/backgrounds.css` del paquete): clase en el `body` de `index.html` e
import del css en `app.js`. Cero configuracion local: el tinte por defecto del mecanismo
es `var(--color-bg-base)` (#0a0e14) — el tema de tokens manda, como se diseno. La textura
entra por `soft-light` y se ve en los huecos entre tarjetas y en el letterbox del fit
(`#app` no pinta fondo propio); las tarjetas siguen opacas. El asset servido es
`bg_neutral.webp` SIN perdida (1.9 MB, pixel-exacto al PNG master de 2.8 MB): la lossy se
descarto con metricas — el degradado suave del grano se cuantiza en mesetas (banding) a
cualquier calidad, incluida q100. Vite emite el webp al dist (`bg_neutral-*.webp`).

**Verificacion:** WebUI **199/199** (1 `it` nuevo en appContract: clase en body + import
del css) · `pnpm build` verde con el webp en dist · Standalone recompilado (`--target
NEURONiK_Standalone`): exe y embed llevan la pagina con fondo. El VST3, hasta el proximo
`build.bat`. Ajuste fino de tinte por tema (`--abd-bg-tint`) pendiente de decidir temas.


## 2026-09-20 (e): la jornada se cierra en cinco commits temáticos

**Qué:** todo el trabajo del día quedó commiteado en ABDNeural. Los ficheros compartidos
entre temas (`app.js`, `appContract.test.js`, `main.css`, `HANDOFF.md`) se trocearon hunk
a hunk por tema con staging quirúrgico (`git hash-object` + `update-index` sobre versiones
intermedias verificadas en `.git/tmp-stage/`, sin tocar el árbol de trabajo):

- `791ed02` docs: README operativo, handoff registro corrido, roadmap viejo a stub, legado del piloto
- `f98ff5d` feat(webui): pad XY dibujado para morphX/morphY (8.3 paso 1)
- `b4d4e68` feat(editor): Load/Save Preset suben del menú Edit al File
- `3d95e88` fix(webui): ajuste del lienzo de diseño al viewport (fitStage)
- `5d0e412` feat(webui): primer consumidor del fondo tintable (.abd-theme-bg)

`git status` limpio y `git diff HEAD` vacío tras el cierre. El ROADMAP se puso al día en
la misma pasada: fila del zoom (el "escala del contenedor" prometido ya es fitStage),
nota de la reorganización del menú nativo y casilla de diálogos/menú anotada.

**Pendiente abierto:** `ABDSharedAssets` SIN commitear (~89 entradas: XYPad con `corners`,
`backgrounds.css`, `COMPONENTS.md`, contracts… mezcladas con trabajo previo sin trackear);
el VST3 sigue con el binario anterior hasta el próximo `build.bat`; del 8.3 quedan browser
de presets, LCD + menú MIDI, espectral/scope (necesitan canal de telemetría), MIDI learn y
menú/diálogos web; flecos: anillo de valor modulado (8.2) y tinte del fondo por decidir.


## 2026-09-20 (f): apuntado para más adelante — modo claro desde un menú "View"

**Idea del usuario, sin diseñar todavía:** un modo claro que se conmute desde un punto de
menú **"View"** en la nav-bar, como en otros synths nuestros. Referencia de temas: el
`themes.css` de ABDMS2000 (`data-theme` + tokens). Encaja con lo que ya existe: el fondo es
tintable por tema (`backgrounds.css`), así que el grueso del trabajo es un juego de tokens
claros + el ítem de menú, no CSS nuevo. Pendiente de decidir: si "View" es menú web nuevo
o entra en la casilla de diálogos/menú del 8.3 (ahí quedó apuntado en el ROADMAP).


## 2026-09-20 (g): ModelMaker sin C4996 — export a AudioFormatWriterOptions (y una mina en build.bat)

**Migración JUCE 8:** el export de audio del ModelMaker usaba la sobrecarga deprecada
`AudioFormat::createWriterFor(OutputStream*, ...)` (el C4996 del log del 18/09). Ahora:
`AudioFormatWriterOptions` (`withSampleRate/withNumChannels/withBitsPerSample`) y el stream
viaja en `unique_ptr` — la propiedad pasa al writer si abre; si falla, lo libera el scope
(la API vieja lo borraba por dentro). Solo `Source/ModelMaker/MainComponent.cpp` (~257): el
import no toca API deprecada — `AudioFormatManager::createReaderFor(const File&)` está
limpio en JUCE 8. Verificado dos veces: target suelto (cero C4996) y `build.bat modelmaker`
completo → RESULTADO: OK, 21/21 ctest, selftest de las seis direcciones en plugin Y bancada.
El bump de Version.h que deja la verificación (28→31 en tres builds) se descarta, como
aconseja el propio script.

**Mina preexistente en build.bat (fix incluido):** el bloque de aviso "WebUI dist no
existe" tenía un `)` sin escapar dentro del `if` (`echo ... interfaz). Selftest...`): al
PARSEAR el bloque, cmd cerraba el if ahí y el `.` siguiente lo mataba ("No se esperaba . en
este momento.", EXIT 255) ANTES del selftest de la bancada — sin banner de RESULTADO. Pasaba
en TODA pasada completa desde el endurecimiento del 09-19 (el grep de AVISO demostró que el
bloque nunca llegó a ejecutarse: moría al parsearlo). Escapado `^(...^)`; un scan del resto
de echoes con paréntesis no encontró más casos dentro de bloques.


## 2026-09-20 (h): modo claro de la SUITE diseñado en tokens.css (sin menú)

**Qué:** paleta clara completa en `[data-theme="light"]` del paquete compartido — el MISMO
juego de tokens de color/sombra del oscuro, con contraste WCAG medido (text-main 15.7-16.5,
text-muted 6.9-7.3, accent 5.4-5.7 como texto y 5.7 el blanco sobre accent). Decisiones de
diseño: el LCD NO cambia (autoiluminado como el hardware), LED/estados oscurecidos para
superficies claras, sombras suavizadas y profundidad invertida (elevado = más claro). Solo
color/sombra: tamaños/espaciados/fuentes se heredan — el test de contrato
(`tests/tokens.test.js`, 5 tests) vigila ambas direcciones: cobertura completa y sin
invenciones. De paso entraron al :root dos tokens que `widgets.css` consumía y NO existían
(`--color-bg-elev`, `--color-border`; valores dark bit-exacto a sus fallbacks). El fondo
tintable sigue al tema solo: el tinte por defecto es `var(--color-bg-base)` y
`backgrounds.css` re-resuelve el tinte en el elemento tematizado — funciona con
`data-theme` en `<html>` (MS2000) o en `<body>` (el demo, que ahora tiene botón "Light").
El menú "View" que lo conmute sigue pendiente: apuntado en el ROADMAP, fuera del DoD 8.3.

**Verificación:** ABDSharedAssets **62/62** (5 tests nuevos) · NEURONiK WebUI **199/199**
(sin cambios: consume tokens.css por import) · contraste medido con script (WCAG 2.1).


## 2026-09-20 (i): selector Dark/Light en la cabecera - el interruptor es compartido, el tema es de la suite

**Que:** para poder PROBAR el modo claro, la cabecera de la WebUI lleva ahora un selector de
temas montado con el `ThemeSwitcher` NUEVO del paquete compartido (`components/
themeSwitcher.js`, exportado por el barrel): dos temas, `dark` (el `:root`, se aplica SIN
atributo) y `light` (el bloque de tokens disenado hoy). Va en la esquina derecha, junto al
grupo de audio; SIN persistencia a proposito: cada carga arranca oscuro, asi selftest y
paridad nunca heredan el estado de una prueba manual. El menu "View" de navegacion sigue
siendo otra pieza (apuntado en el ROADMAP); este es el interruptor de prueba.

**Arquitectura (principio fijado por el usuario):** el interruptor es UNIVERSAL (paquete),
los temas son SOLO tokens de color, y todo lo demas (widgets, skins de forma, fondo
tintable, LCD, ajuste al viewport) es de la suite: un synth nuevo define sus bloques
`[data-theme=...]` y elige tipos de elemento, sin copiar CSS de widgets. ABDMS2000 aun
reparte su tema entre su `themes.css` local y el paquete: deuda conocida, sin tocar hoy.
`COMPONENTS.md` fija el principio.

**Verificacion:** ABDSharedAssets **68/68** (6 tests nuevos del switcher: data-theme en el
root elegido, dark=sin atributo, aria/estado activo, persistencia opcional, ids duplicados,
destroy con removeEventListener real) - NEURONiK WebUI **200/200** (1 it nuevo de contrato) -
`pnpm build` verde - paridad **70 celdas · contrato 70** (el selector no es celda de
parametro) - Standalone recompilado con el embed nuevo.


## 2026-09-20 (j): indice de commits del dia — donde esta TODO

**ABDNeural (10 commits):** `791ed02` docs (README/handoff/roadmaps/legado) · `f98ff5d` pad XY
8.3 · `b4d4e68` menu File · `3d95e88` fit al viewport · `5d0e412` fondo tintable · `fb419b4`
ModelMaker JUCE 8 · `850dd96` fix de build.bat · `9fbc417` selector Dark/Light · `3090370`
docs de la segunda mitad. Arbol limpio.

**ABDSharedAssets (5 commits, primera vez que se commitea la familia):** `62b36d1` ignore
node_modules · `fb32e9b` contracts esquema 2.0 + nuevos · `238c7b0` familia de controles +
tests (68) + ThemeSwitcher · `d1869f0` tokens claros + fondo tintable + audiolab/teclado ·
`4ebeb14` assets (fondos, renders, iconos) + COMPONENTS.md + demo. Fuera a proposito:
`abdbank/` (app completa dentro del paquete, probablemente extraviada: ABDBankManager ya es
repo propio — decidir su destino).

**Estado de la suite:** modo claro disenado y con contrato, selector universal, fondo
tintable, pad con corners — todo heredable por los synths definiendo solo tokens de color.
Pendientes del roadmap: 8.3 (browser, LCD, espectral+telemetria, MIDI learn, menu web),
"View" para el tema, fitStage por extraer al paquete, deuda de temas de ABDMS2000.


## 2026-09-20 (k): canal de telemetria nativa->web — el mensaje que desbloquea espectral, scope y anillo

**El mensaje:** `telemetryFrame` (aditivo a v1) — `{ seq, spectral: [64], envelopes: [2], lfos: [2], modulation: [targets], morph: [x, y] }`, todo 0..1. Sondeo y CON DIFF de valor (epsilon 1/255): un synth quieto no emite nada, un valor que se mueve emite exactamente un frame. Los hosts emiten cada 2 ticks del timer de 30 ms (~15 Hz). `stats.telemetrySent` cuenta los frames.

**Lado nativo:** interfaz `TelemetryController` en `ParameterBridge.h` (lo que el bridge necesita de un backend visual, nada mas; el procesador ya implementaba lo equivalente para la UI nativa via `IVisualizationSource`) + `VisualizationSourceAdapter` en `BridgeAdapters.h` (pass-through). `sendTelemetry()` en el bridge; instalacion y sondeo decimado en los DOS hosts (NeuronikWebView y WebPilotHost, el mismo timer de modelsState/midiNoteState).

**Lado JS:** `src/bridge/telemetry.js` — consumidor que guarda el ULTIMO frame y reparte una llamada por frame a los suscriptores (futuro espectral, scope, anillo). Los frames NO son estado de la app: viven FUERA del ciclo `setState` a proposito (llegan 15 veces por segundo). `bridgeCore` anade el dispatch `onTelemetry` (octavo listener nativo->JS) y `paramStore` cables el push.

**Contrato:** `telemetryFrame` en `bridge-protocol.json` (con `behaviour.telemetryPoll`: la politica de sondeo con diff) y checks en los DOS tests de contrato (C++ compara el literal contra `BridgeActions::telemetry`; el mjs entrega un frame y lo ve llegar como `onTelemetry`).

**Tests:** `ParameterBridgeTest` seccion 11 (17 checks: primer frame siempre emite, campos uno a uno, quieto=no mensaje, movimiento=un frame, sub-epsilon no emite, seq monotona, sin backend=no-op). Web: `tests/telemetry.test.js` (6) + `tests/bridgeTelemetry.test.js` (3, arnes de `window.__JUCE__` falso) — y los dos tests que fijaban el numero de listeners pasan de 7 a 8 (el cambio legitimo del contrato).

**Verificacion:** ctest **21/21** - WebUI **209/209** - selftest del plugin y de la bancada **OK** (seis direcciones sobre el canal real, la pagina nueva) - Standalone y bancada recompilados.

**Lo que desbloquea (consumidores pendientes):** visualizador espectral (fila 8.3 del roadmap), scope, y el anillo del valor modulado (la cantidad por destino viaja en `modulation[]` — queda la pintura en el Knob compartido).
## 2026-09-20 (l): fitStage extraido a @abdsynths/shared — CZ101 ya tenia su copia artesanal

**La deduplicacion que justifica la extraccion:** CZ101 lleva su propio `scaleUI()` en su
app.js (~20 lineas: `min(scaleW, scaleH)` sobre su 1409x768, listener de resize, SIN acotar
y con el centrado delegado en `transform-origin: center`). Dos implementaciones del mismo
mecanismo divergiendo: el patron que la suite viene eliminando.

**En el paquete:** `components/fitStage.js` (`computeFit` puro + `mountFitStage` con
viewport inyectable), exportado por el barrel. Generalizacion minima: `minScale`/`maxScale`
son opcion (defaults 0.25x-3x, el rango del zoom nativo de NEURONiK) — adoptar el compartido
o mantener el comportamiento sin tope de CZ101 es ahora decision de una linea. El tamano de
diseno sigue entrando por parametro (el paquete no conoce lienzos ajenos). Documentado en
COMPONENTS.md como INFRAESTRUCTURA de pagina, no control: sin contrato de control, y con el
requisito de uso `body { overflow: hidden }` en el CSS de cada pagina.

**En NEURONiK:** `app.js` importa `mountFitStage` del barrel; borradas `WebUI/src/ui/fitStage.js`
y su test (los 7 tests viven ahora en el paquete, portados tal cual, + 2 nuevos de cotas
custom). La asercion del `appContract` fija el consumo del paquete (no copia local). La
llamada `mountFitStage(root, { width: CANVAS.width, height: CANVAS.height })` no cambia.

**Verificacion:** ABDSharedAssets **77/77** (9 de fitStage) - NEURONiK WebUI **202/202** -
`pnpm build` verde - Standalone recompilado con el embed nuevo y selftest **OK** (seis
direcciones). Pendiente para CZ101 (su repo): sustituir `scaleUI` por
`mountFitStage(container, { width: 1409, height: 768 })` — gana acotacion y centrado
explicito. MS2000 no adopta hoy (layout fluido, sin lienzo que escalar).
## 2026-09-20 (m): el bump del ModelMaker pasa a ser RELEASE-GATED

**El defecto:** `Scripts/update_version.ps1` corria en CADA compilacion del target (un custom
command con OUTPUT ficticio, siempre "caducado") y quemaba un numero de
`Source/ModelMaker/Version.h` — un fichero VERSIONADO en git — por cada build de
verificacion. `build.bat` incluso aconsejaba "descarta el incremento" a mano.

**El cambio:** el ps1 solo incrementa si la build viene marcada
(`NEURONIK_MM_RELEASE=1`); `build.bat modelmaker release` pone la marca justo antes de
compilar el target y la retira despues. El custom command es ahora PRE_BUILD del target
(sin dependencia muerta `UpdateVersion`, sin `-E env` que congelaria el valor en
generacion: el ps1 HEREDA el entorno de la invocacion). La version oficial del plugin no
depende de esto; la define CMake.

**Verificado:** build SIN marca -> `skipped`, Version.h intacto; CON marca -> `Incremented
version to 0.1.29`; SIN marca de nuevo -> no lo toca (se queda 29). El incremento de la
prueba se descarto (`git checkout -- Version.h`, sigue 0.1.28).

**Docs:** README (seccion ModelMaker: dos invocaciones), ROADMAP Fase 9 (sin el "arrastra
UpdateVersion") y build.bat (token `release`, banner y mensajes honestos).
## 2026-09-20 (n): el flujo del ModelMaker reproducido de punta a punta — por fin, con analisis real

**Lo que nadie habia corrido hasta hoy:** audio -> `detectPitch`/`analyze` del ModelMaker ->
export `.neuronikmodel` -> `loadModel` en las ranuras A-D. `ModelSlotTest` cubria el lector y
las ranuras con JSON sintetico; el ANALISIS real del ModelMaker no tenia prueba y no habia
invariant de su flujo. El test `NEURONiK_ModelMakerRoundTripTest` (Tests/
ModelMakerRoundTripTest.cpp, 22 checks) reproduce el flujo completo con el MISMO codigo que
la GUI conduce: sintetiza el "sample" (A4, 1 s, 64 armonicos 1/n — el espectro de un
instrumento real), corre el HPS del analizador (detecta 439.45 Hz), analiza, escribe el JSON
EXACTO de exportModel(), carga en A-D y verifica tres niveles: el ESTADO de las ranuras
(contenido <= 1e-3 contra el analisis, nombres), lo que SUENA (RMS 0.23; la tabla del motor,
que el resonador RE-ESCALA por SUMA — forma contractual, no amplitudes crudas: argmax=
fundamental, suma=1.000, ratio de forma 3.080 vs 3.080) y lo que la PAGINA veria
(modelsState via EngineModelsAdapter, con el nombre del fichero).

**Hallazgo del round-trip (no bug, semantica documentada):** la tabla de
`spectralDataForUI` son CUOTAS (suma 1.0), no el modelo en bruto: quien lea
`spectralDataForUI` como amplitudes del modelo esta leyendo otra cosa. Los consumer
(espectral del 8.3, anillo) tienen que saberlo.

**Limitacion documentada en el propio test:** la deteccion HPS del analizador puede irse a
la octava con notas graves (la ventana FFT de 8192 muestras = 170 ms contiene <2 periodos de
un A2): el GUI del ModelMaker lo corrige a mano con su combo nota/octava; el flujo
automatizado usa A4 y no lo pisa. Con un periodo y medio la deteccion HPS era inestable;
con A4 es estable. Es limitacion de SENAL de la herramienta, no del plugin.

**En el build:** ctest 22/22 (el test nuevo entra en la lista de build.bat). El flujo GUI
con raton (LOAD AUDIO -> ANALYZE -> EXPORT) sigue siendo manual a proposito: la logica es
exactamente la que el test invoca.

## 2026-09-20 (o): ABDMS2000 migrado al selector universal — la mecanica sube, los efectos se quedan

Los 3 botones `.mode-tab` de la nav-bar dejaron paso a un <select> unico
(`ThemeSwitcher` con `variant: 'select'`): la interfaz respira y la eleccion de tema
tiene UNA sola ruta. Los temas son datos declarativos (`WebUI/src/contracts/themes.js`:
id/label/bodyClass/payload); el switcher compartido aplica `data-theme` y la clase de
skin con politica de dueno unico, y entrega el payload (el indice de `synthMode`) como
segundo argumento del onChange. Los efectos del synth se quedan en casa:
`applyThemeEffects()` manda synthMode al store/bridge y sincroniza el bank manager; las
acciones de menu `skin-*` pasan por el switcher. El LCD universal queda apuntado para
diseno posterior (la bancada CZ101 lleva ~880 lineas de LCD propia).

## 2026-09-20 (p): LCD universal disenado en el paquete — la maquina del nativo vuelve como dato+hooks

El LCD del 8.3 ya no hay que escribirlo: la familia vive en @abdsynths/shared
(lcdMachine: maquina PURA Idle/Nav/Edit con arbol inyectado y hooks; lcdScreen:
ping-pong + preview + cola con prioridad; lcdPanel: D-pad con hold-repeat) y su
gemelo C++ en ABDSharedCode/LcdDisplay (ABDShared::LcdDisplay, INTERFACE, gate
WASM, sonda compilada contra JUCE). Herencia: la maquina es el LcdMenuManager
nativo retirado (c811b75, recuperado de git) con el arbol como DATO y los
efectos como callbacks; el scroller es el de CZ101; la cola, la de ABDEep.
Verificado: 109/109 en el paquete (26 tests LCD), QA visual en la demo
(menu GLOBAL/EFFECTS/PANIC navegado en vivo), C++ compilado. Guia y plan de
adopcion (CZ101 ~880 lineas, MS2000, ABDEep): docs/LCD_GUIDE.md.

## 2026-09-20 (q): GLOBAL & MASTER al final del lienzo + Segmented universal (dos repos)

Mudanza 8.3: la ficha GLOBAL & MASTER vive al FINAL del lienzo, abajo a la
izquierda (primera de la banda models+modMatrix+globalFull). Patron de la
matriz: en el lienzo solo queda el fader MASTER (el control base del host,
intacto para el selftest) + RANDOM + EDITAR, que abre el cajon lateral con
tempo/velocity/MIDI/congelados/randomStrength apilados en columna (cajon sin
`groups`: buildSlotColumn con distintivo n1..nN, CSS .drawer-slot--column).
La geometria la paga el LFO (se muda a los carriles libres de la banda
superior, mismas 2 filas): el lienzo sigue midiendo 990 y el encaje da
holgura >= 8px. tests: sections 18/18, panel incluido.

Componente nuevo UNIVERSAL en los dos repos compartidos: Segmented, el
hermano compacto del Select (listas de DOS opciones siempre visibles: motor
y sync de LFO). JS: ABDSharedAssets/components/segmented.js (radiogroup con
roving tabindex, flechas, vetados con nota, divergente conservado) + CSS en
widgets.css + renderer en skins vector/ms2000; 7 tests propios.
C++: ABDSharedCode/Segmented/Segmented.h (ABDShared::Segmented, INTERFACE,
gate WASM, sonda ejecutable ABDShared_SegmentedProbe compilada y en verde,
exit 0). Cableado en NEURONiK: SEGMENTED_CHOICES en controls.js decide la
presentacion (decision de pagina, no de contrato) — engineType + los dos
sync de LFO en segmentado, waveforms (6) y listas largas/gateadas siguen en
Select. Recuento del lienzo: 45 knobs + 1 base + 5 toggles + 16 selects +
3 segmented = 70. Sonda C++ y ctest de contrato en verde; verificado
segmented 7/7, paquete 10 ficheros, WebUI 21 ficheros, build Vite.

Inventario ABDEep (WebUI) para futuras extracciones al paquete:
- Step-editor de 16 pasos (panel LFO3): patrones ritmicos; NO reusable en
  NEURONiK hoy (su LFO no tiene secuenciador), candidato LFO avanzado.
- Filas de botones LED (shape-led / led-row de los paneles OSC): la version
  universal ya existe (Toggle con LED del paquete); para waveforms de 6 hay
  que decidir entre Select y una fila LED - decidir con la bancada real.
- Numberbox +/- con edit directo (paneles EDIT): util para BPM/canales; no
  extraido. Candidato si el cajon de GLOBAL & MASTER pide edicion fina.
- Tab bar deslizante de paneles: la navegacion por fichas del lienzo de
  NEURONiK lo cubre; extraer solo si otro synth pide pestanas.
- Scope/canvas visual: en curso via telemetria (espectral del 8.3).
Los ya extraidos esta jornada: LCD universal (p) y Segmented (q).

## 2026-09-21 (r): fitStage en TODA la suite — cuatro synths, un mecanismo

Lo que empezo como arreglo de NEURONiK ("no se distinguen las teclas")
termino siendo infraestructura de la suite: los CUATRO synths heredan el
ajuste al viewport del fitStage compartido.

- Paquete (ABDSharedAssets): `onlyShrink` (paginas fluidas: nunca ampliar,
  identidad sin margenes) y caja NATURAL del stage (`stageWidth/Height`) para
  el centrado honesto con floors; identidad LIMPIA el transform (un scale(1)
  residual crea containing block y re-anclaria overlays fixed). 21/21 tests.
- NEURONiK: sin cambios (lienzo fijo, barrel).
- MS2000: FLUIDO con floor de diseno (min-width/height 1080x680 en #app) +
  onlyShrink: por encima crece responsive, por debajo escala entero; las
  media queries del dashboard siguen mandando el reflow. Overlays en body,
  fuera del stage. 134/134 + build Vite.
- CZ101: copia gestionada resincronizada; su test de identidad actualizado al
  contrato nuevo (sin transform). Suite en su baseline conocida (fallos WASM
  preexistentes, conjunto identico con y sin el cambio; fitStage 4/4).
- ABDEep: copia gestionada (scripts/sync_shared.mjs, CJS) + glue ESM
  js/fit-stage.js (unico modulo de su app, montado al final del body sobre el
  chasis 1200x768). Suite 4750/4750 con el baseline guard actualizado
  (docs/baseline_fase0_v32.md: 107 files / 4750 tests).

## 2026-09-21 (s): auditoria de los dos motores — normalizacion, guardia y catch-up

Peticion: explicar los dos motores (hecho en conversacion: Resonator = reesintesis
aditiva por 64/128 osciladores seno; ResonatorBank = sintesis modal por 64/128
biquads BP excitados por ruido coloreado + impulso) y ARREGLAR lo debil, mas una
investigacion de mejoras en cuatro ejes. Verificacion: `build.bat tests` 22/22.

**Arreglados hoy (Source/DSP):**

- **Resonator.cpp — normalizacion inflada por parciales mudos.** Un parcial sobre
  la guardia ponia `phaseIncrements=0` pero su amplitud seguia en `totalAmplitude`
  -> el nivel total caia con modelos brillantes/agudos. Ahora la rama muda anula
  TAMBIEN `tempAmps[i]` (paridad con el banco, que ya lo hacia).
- **ResonatorBank.cpp — mismo defecto, orden inverso.** El banco acumulaba
  `totalAmplitude` ANTES de `updateFilterCoefficients` (que anula los parciales
  fuera de guardia): los mudos inflaban el denominador igual. El total ahora se
  suma DESPUES, desde `partialAmplitudes_v` (cubre main+unison de un vistazo).
- **DspSafety.h — `kNyquistMargin = 0.45f` compartida.** Las guardias discrepaban
  (0.45 additive vs 0.48 banco). El 0.48 es PELIGROSO para un biquad peak: con w
  cerca de pi, alpha=sin(w)/(2Q) y el redondeo puede empujar el polo fuera del
  circulo unitario. Los tres sitios usan la constante; rationale en su cabecera.
- **AdditiveVoice/NeurotikVoice — catch-up de smoothers por `skip()`.** Ambas
  voces avanzaban smoothers con bucles manuales de `getNextValue()` (hasta 31x9
  llamadas por sub-bloque de 32) dentro del loop de render. El sustrato compartido
  (`abd::dsp::LinearSmoothedValue`) ya trae `skip(n)` O(1) para exactamente esto.
  Matiz de semantica: las llamadas del bloque anterior ya avanzaron el smoother,
  asi que por sub-bloque toca avanzar `thisBlockSamples-1`, no `thisBlockSamples`
  (el codigo viejo sobre-avanzaba una muestra de control por bloque; inaudible,
  pero el skip queda exacto).

**Hallazgos apuntados al ROADMAP (Fase 8.3/Fase 9), sin tocar:**

- `unisonSpread` es un parametro MUERTO: expuesto en APVTS y WebUI
  (contracts/sections.js), guardado en Resonator (`setUnison`), usado NUNCA en el
  render. La capa unison suena igual en cualquier posicion del knob. O se
  implementa (spread -> semitonos: parcial i a `f*(1+detune*(1+spread*i/64))`) o
  se retira del contrato.
- Ambos motores son MONO por voz: la imagen estereo sale de la capa unison
  (0.707) y del chorus global. El `unisonSpread` real (parcial i desviado
  +/-spread*i/64 por canal L/R) seria el stereo-widening barato que falta.
- `ModelMaker/SpectralAnalyzer`: analiza SOLO los primeros 8192 muestras
  (windowed sobre un buffer probablemente no envasado), muestrea la magnitud en
  la frecuencia armonica EXACTA (sin busqueda del pico en ±2 bins) y deja
  `frequencyOffsets` a cero con un TODO. La mitad del modelo que morphean los
  motores nunca se genera. Mejoras apuntadas: analisis multiframe con
  seleccion por magnitud media, peak-picking local, offsets reales, y curvature/
  parabolic-interpolation para precision sub-bin.
- Telemetria: el frame lleva spectral/envelopes/lfos/modulation/morph; no lleva
  forma de onda. El scope apuntado en 8.3 exige un campo nuevo (`wave[]`)
  — cambarlo con la SSOT del exportador del canal.

## 2026-09-21 (t): los fixes del motor pasan la paridad bit-exacta (build completo)

Confirmacion de la entrada (s): tras `build.bat` COMPLETO (10:16), el paso WASM
recompilo los cuatro ficheros tocados (Resonator, ResonatorBank y las dos voces)
y la paridad WASM<->nativo salio **bit-exacta 9/9** (44.1/48/96 kHz x bloques
64/128/512, maxUlp=0, 184.320 muestras).

La matriz que pinta el cambio fino es la de independencia de tamano de bloque
(128 vs 64/512, bit-exacta en los 5 escenarios x 3 SR): es la que NO perdonaria
una desalineacion de la rejilla de control de 32 en el catch-up por `skip()` —
si `skip(thisBlockSamples-1)` desalineara, ahi saltaria. No salto. Smoke OK y
artefactos sincronizados con `WebUI/public/worklet`, byte-identicos al commit
`8299a95` (rebuild determinista; arbol limpio tras el build).

Detalle coherente con el fix, no una regresion: a 96 kHz los peaks de
D_modmatrix (0.37) y E_modelo_espectral (0.29) bajan respecto a 44.1/48 —
menos parciales por debajo de 0.45*SR con la normalizacion ya sin los mudos.
Paridad mantenida en los tres SR.

## 2026-09-21 (u): auditoria applyGain sobre datos externos en las rutas WASM

Revision cruzada ABDNeural/ABDEep tras la leccion CZ101 1.2.1 (applyGainRamp de
JUCE real, vectorizado bajo -O3 -msimd128, escribio fuera de la region emmalloc
cuando el AudioBuffer envolvia datos externos del heap de JS).

- ABDNeural: el patron EXISTE (DspEngineFacade::process envuelve outL/outR de
  JS con setDataToReferTo y por ahi llega masterLevelSmoother.applyGain en
  applyGlobalFX) pero esta BLINDADO por tres hechos: (1) el AudioBuffer y el
  SmoothedValue son el port abd::dsp (DspCore.h), escalar por diseno — su
  FloatVectorOperations lo documenta (intrinsics solo con
  JUCE_USE_SSE_INTRINSICS/NEON/VDSP, que nadie define); (2) el build WASM es
  -O2 sin -msimd128 (wasm/CMakeLists.txt); (3) no hay ni un applyGainRamp en
  Source. juce_dsp SI enlaza en el build, pero ningun modulo del motor llama a
  sus rutinas de buffer sobre el buffer del facade. Guardia documental anadida
  en el facade: prohibido aplicar rutinas de buffer de JUCE real sobre ese
  buffer; si el build gana -msimd128, auditar antes de enlazar.
- ABDEep: el patron NO existe. Su WasmBridge usa un AudioBuffer PROPIO y
  estatico (heap del WASM), el resultado sale por memcpy a los punteros de JS,
  compila JUCE real pero SIN -msimd128 (-O3), y su unico applyGain
  (SynthEngine.cpp:236, master gain) opera sobre el buffer propio. Sin accion.
- Criterio de suite (vigilado por la paridad bit-exacta): los tres hechos que
  blindan ABDNeural son convenciones, no contratos pinados.

## 2026-09-22 (v): ENV 1 y ENV 2 como fuentes de la matriz — la envolvente del filtro sale del armario

La antigua ficha FILTRO & ENVOLVENTE se separa (FILTRO en la banda del motor,
ENVOLVENTES junto a MODELOS A-D) y las dos envolventes entran en la matriz:
**ENV 1 (amplitud) = fuente 6, ENV 2 (filtro) = fuente 7**, anadidas AL FINAL
de `getModSources()` porque los choice se guardan por indice (insertar en
medio re-mapearia presets guardados). Tres piezas coordinadas:

- **Preset nuevo:** defaults `mod1Source`=6 -> destino 1 (Osc Level) y
  `mod2Source`=7 -> destino 10 (Filter Cutoff), amounts 1.0. Toda ruta nueva
  nace con las envolventes cableadas por la matriz, visibles y editables.
- **Preset existente:** `insertEnvModRoutes` (PresetMigration, desde
  PresetManager al cargar) inserta las dos rutas en la primera ranura libre
  (mod1, luego mod2; solo si source y destino son 0/Off; sin sitio, nada).
  El sonido es IDENTICO: en el DSP "sin ruta" = factor de routing 1.0
  (sentinela en las voces) — el cableado hard-wired de siempre, ahora
  VISIBLE y editable. `filterEnvAmount` sigue como profundidad; su default
  paso 0 -> 1.0 porque con 0 la ruta insertada nacia MUDA (0 x amount = 0),
  defecto que la prueba de escala del destino destapo (sonda C++ nativa con
  analisis de centroide: la ruta escala proporcional al amount y es coherente
  con la del LFO en el mismo destino, ~±18 kHz maximos; NO se reajusto el
  DSP). La migracion NO toca el valor guardado del knob.
- **DSP:** reemplazo dentro del sumatorio, bit-exacto con amount 1.0 (ENV2:
  `fEnv x (fEnvAmount x amount) x 18000 Hz`; ENV1 sobre el nivel del VCA).
  Nuevos destinos 12/13 (Flt Attack/Decay, solo responden a ENV 2). Nota
  estructural: ENV2->Cutoff solo actua en el motor ADITIVO (NeurotikVoice no
  tiene filtro ni envolvente de filtro — mejorable, apuntado en 9).

**Reparto del lienzo (estado tras las mudanzas 8.3):** tres bandas de 12
carriles — OSCILADOR 6 + RESONADOR 2 + FILTRO 4 / ENVOLVENTES 5 + MODELOS 2 +
EFECTOS 5 (MODELOS al centro, solo el pad XY en el lienzo; espectral y 4
ranuras en el cajon) / LFO 2 + MATRIZ 6 + GLOBAL & MASTER 4. Los 70 ids
siguen en SU seccion: store, recuento y cobertura intactos.

**Prueba:** PresetRoundTripTest "ENV route migration" (inserta 2, indices
6->1 y 7->10, amounts 1.0, ningun otro parametro tocado, ranuras ocupadas ->
0, knob sobrevive); paridad WASM<->nativo **bit-exacta 9/9** con los defaults
nuevos; ctest nativo 22/22; suite WebUI 221/221; selftest E2E de seis
direcciones OK contra la pagina nueva. Ademas del dia: formas de onda LFO
decididas (fila LED con glifos + nombres cortos canonicos de waveforms.js —
comparativa montada en el cajon real; con nombres largos, RANDOM S&H truncaba)
y NumberBox universal (ABDEep -> @abdsynths/shared) para masterBPM y
midiChannel en el cajon de GLOBAL & MASTER.
## 2026-09-22 (w): el fondo del binario de plugin en dieta — tile en vez de master

El binario del plugin embebia `bg_neutral.webp` (1.95 MB) via BinaryData; los fuentes
embebidos (`BinaryData*.cpp` de build-reference/JuceLibraryCode) sumaban **8.1 MB**.
El tile seamless `bg_tile512.webp` (341 KB, lossless, costura 0.000, cero banding —
seccion 4b de STYLES_GUIDE en @abdsynths/shared) sustituye al master como fondo del
dist: los fuentes embebidos bajan a **2.69 MB (-5.4 MB, -67%)**, verificado recompilando
el plugin completo. La variante `--full` (master) queda como asset externo, no embebido.

**Decision de pipeline:** el dist de la WebUI ahora solo lleva `bg_tile512-*.webp` (341 KB)
y el paso BinaryData del CMake recoge solo lo que el dist usa. Cualquier synth de la
suite que embeba su WebUI hereda la misma reduccion importando `@abdsynths/shared`
(asset + backgrounds.css).

## 2026-09-23 (m): FASE 10 completa — el ModelMaker ya produce modelos temporales (Neuron-parity)

La Fase 10 del plan temporal (`docs/ARCHITECTURE/TEMPORAL_MODELS_PLAN.MD`) queda implementada
de punta a punta: la cadena ModelMaker → SpectralModel → morfeo XY → renderizadores es ahora
temporal, el salto al paradigma Neuron que apuntaba la investigación.

- **10.1 struct + formato v2**: `SpectralModel` guarda N frames (1..16; frame 0 canónico en
  `amplitudes`/`frequencyOffsets`, extras en `extraAmps`/`extraOffsets`, `frameSpanHz` del
  análisis). Escritor v2 SIEMPRE (`format: 2` + `frames` + `frameSpanHz`, offsets acotados a
  media banda); lector recupera frames y span; un plugin viejo lee v2 como v1.
- **10.2 morphZ en el motor**: parámetro 0..1 (default 0 = bit-compat), smoother por voz,
  muestreo de frames por sub-bloque con caché, **destino 28 de la matriz de modulación**
  (LFO2 = animación cíclica, ENV2 = gesto por nota).
- **10.3 WebUI**: contrato regenerado (72 parámetros, 0 sin enrutar), knob MORPH-Z en el cajón
  EDIT de MODELOS; encaje del camino B (morphX/Y solo en el pad, bow en RESONADOR).
- **10.4 analizador temporal**: `SpectralAnalyzer::analyzeTemporal()` — N ventanas repartidas
  por el fichero, cada frame mide la rejilla n·f0 sobre SU espectro (emparejamiento por índice;
  la ventana de media banda f0 garantiza que el parcial k nunca invade al vecino),
  normalización GLOBAL que conserva la evolución de nivel (el decaimiento es el gesto).
  Combo FRAMES (1/2/3/4/6/8) en la GUI; 1 frame = camino estático de siempre (equivalencia
  bit a bit verificada).
- **10.5 docs**: esta entrada y la marca IMPLEMENTADA en ROADMAP. Pendiente cosmético: anillo z
  en el pad y modelo de ejemplo para el selftest.

Verificación: `NEURONiK_TemporalAnalysisTest` 14/14 (equivalencia, emparejador con bloques
escalonados ±9 Hz a ±0.02 Hz, decaimiento 1.000→0.125 conservado, roundtrip v2 completo con
semántica z honesta: z=0.5 con 3 frames ES el frame 1); ctest 26/26; suite WebUI 233/233;
sonda CLI (`NEURONiK_ModelMakerRealWavProbe`, registrada en CMake sin add_test) con WAVs reales
de CZ101: 3 patches → f0 correcta (123.8 Hz, la sub-octava de 62 Hz descartada con DFT de
red armónica), inharmonicidad real (49-64 offsets >1cent, hasta 53.8 Hz), y 4 frames v2 →
recarga con evolución real entre frames. Paridad WASM-nativo 9/9 bit-exacta vigente (los
escenarios no usan frames).

## 2026-09-23 (n): sonda CLI del ModelMaker — el análisis verificado en producción

`NEURONiK_ModelMakerRealWavProbe` (`Tests/ModelMakerRealWavProbe.cpp`, bloque propio en
`CMakeLists.txt` clonando el set `NEURONIK_SOURCES` del roundtrip, sin `add_test`: es sonda de
verificación, no test de ctest). El ModelMaker es app GUI sin CLI, así que la sonda enlaza los
mismos ficheros de producción que la GUI y conduce su flujo exacto, headless:

```text
WAV → detectPitch → analyze / analyzeTemporal
    → serialización v2 (dialecto idéntico a exportModel, clamps incluidos)
    → PresetManager::loadModelFromFile → sampleFrame
    → engine real (loadModel por FIFO + getCurrentModel)
```

Uso:

```bash
cmake --build build-reference --config Release --target NEURONiK_ModelMakerRealWavProbe
./build-reference/Release/NEURONiK_ModelMakerRealWavProbe.exe ../ABDCZ101/DOCS/patches/CZ-BASS1.wav ...
```

Métricas de la última verificación (2026-09-23, WAVs reales de CZ101, mono 16-bit/44.1k):

| WAV      | f0       | centroide   | parciales | offset máx | v2 estático | temporal (4 frames) | pico min |
|----------|----------|-------------|-----------|------------|-------------|---------------------|----------|
| CZ-BASS1 | 123.8 Hz | parcial 6.8  | 49/64     | 53.8 Hz    | 6146 B      | 15770 B             | 0.171    |
| CZ-HAMOG | 123.8 Hz | parcial 10.4 | 64/64     | 32.3 Hz    | 6488 B      | 16824 B             | 0.735    |
| CZ-PAD1  | 123.8 Hz | parcial 2.6  | 17/64     | 24.8 Hz    | 5517 B      | 14408 B             | 0.374    |

`RESULT: OK` en los tres: el ciclo completo WAV→modelo→recarga→sampleFrame→engine cierra y el
frame 0 queda bit-igual al bloque canónico (compatibilidad v1 intacta). Hallazgos que la sonda
destapó: el HPS acierta la octava (la sub-octava de 62 Hz de BASS1/HAMOG se descartó con DFT
directo —sin red armónica propia, era coloración—), la inharmonicidad real viaja en los offsets
(el antiguo TODO de la fase 9 está muerto) y `frameSpanHz` llega con la dimensión correcta
(= f0 del análisis, 123.82 Hz en BASS1). El decaimiento entre frames se conserva
(pico 1.000 → 0.171 en BASS1): la normalización global respeta el gesto temporal.

Los modelos generados quedan en `build-reference/probe-models/` (artefactos de verificación,
no commiteados).

## 2026-09-23 (o): el anillo de morphZ en el pad XY — el tercer eje gana su superficie

- **Aro SVG** sobre el pad compartido (que no se toca: su contrato lo pinean los tests del
  paquete), en la capa de wiring de NEURONiK (`WebUI/src/ui/xyPad.js`): pista tenue, arco
  relleno por `stroke-dasharray` con `pathLength=100` (1 unidad = 1% de z) y hit-area que
  captura solo el trazo — el arrastre XY interior queda intacto (pin dedicado).
- **Gesto coordinado**: drag por ángulo (0 arriba → 360° = z=1) con el mismo protocolo
  begin/change/end que morphX/Y; el knob MORPH-Z del cajón EDIT y el aro comparten el
  parámetro — un control, un nodo.
- **Teclado + a11y**: role=slider "Morph Z", flechas ±0.01, PageUp/Down ±0.1, Home/End 0/1.
El aro es la superficie visible del mismo eje que alimenta el **destino 28** de la
  matriz de modulación (LFO2 = ciclo, ENV2 = gesto por nota) sobre el modelo temporal
  de N frames (entrada (m)): aro, knob y matriz son vistas del único parámetro `morphZ`.

- **Encaje del camino B cerrado**: morphX/morphY salen de las celdas del oscilador (el pad XY
  es su único control del lienzo), RESONADOR agrupa el cuarteto modal (resonancia, impulso,
  bow) y OSCILADOR vuelve a 12 celdas en 6×2.

Verificación en verde: suite WebUI **238/238** (5 pins nuevos del aro en `tests/xyPad.test.js`:
montaje accesible, gesto completo con las tres fases, aislamiento del pad, teclado y paint sin
eco). En vivo sobre el dist servido: drag de 180° deja el store en `morphZ:0.5` con el aro al
50% ("Morph Z 50%" en el slider accesible) y una flecha arriba lo mueve a `morphZ:0.01` con el
arco repintado — la ruta teclado→tienda→footer JSON confirmada en producción.

## 2026-09-23 (p): HPS refinado y guardia de maximo local — el estimador no cae a sub-armonicos ni fabrica parciales

Defecto destapado por la sonda con material real: con una nota anclada en ~124 Hz y un
sub-oscilador fuerte en 62 (firma de `CZ-SWEP1`: 62@0.39, 124@1.0, 186@0.56, 248 vacio),
el producto HPS en 62 multiplica los tres parciales reales que comparten y el estimador
cayia al sub-armonico. Ademas, `measurePartial` asignaba amplitud a huecos de la rejilla
que caian en la falda de un parcial ajeno: parciales fantasma (uno a -1381 cents) que
sostenian medianas "bonitas" de modelos des-afinados.

Refinado en `SpectralAnalyzer` (nucleo unificado `detectPitchImpl`, compartido por
`detectPitch` y `detectPitchFromSpectrum` — fin de la duplicacion de la 10.6):

1. **Ancla por esbozo**: maximo crudo del espectro en rango musical con maximo local
   +-1 bin, ANTES del producto.
2. **Candidatura del producto restringida**: el HPS solo puede degradar el ancla UNA
   octava abajo (candidato a +-60 cents de ancla/2, con soporte espectral >= -40 dB),
   nunca hacia arriba (el modelo en Python destapo que con ventana amplia el producto
   secuestra anclas correctas con crestas de fuga).
3. **Escalera de octava musical**: impares vacios frente a 2f0 => x1/2 (sub-octava); y
   regla nueva de fundamental debil (2f0 domina todo su entorno y la serie muere tras
   el 3er parcial) => x2. Calibrada contra las escaleras medidas de los cinco WAVs.
4. **Guardia de maximo local en `measurePartial`**: un hueco de la rejilla sobre la
   falda de un parcial ajeno queda a cero (los vecinos inmediatos del espectro global
   no pueden superarlo) — sin fantasma.

Metricas de la sonda (`NEURONiK_ModelMakerRealWavProbe`, antes/despues del refinado):

| WAV | antes | despues |
|---|---|---|
| CZ-SWEP1 | f0 **64.5 Hz** (sub-octava del real ~124), mediana -4.5 cents sostenida por fantasmas | f0 **124.8 Hz**, parcial 1 = **-3.5 cents**, mediana -11.0 cents sobre parciales REALES, hueco 248 = 0.000, frames temporales 123.4-124.2 Hz |
| CZ-BASS1 / HAMOG / PAD1 | 123.8 Hz | sin cambio — modelos regenerados **byte-identicos** (el refinado es no-op sobre series completas) |
| CZ-RRISE | f0 erronea 81.4 Hz | exencion de barrido con detecciones honestas **291.5 -> 624.2 Hz** (el barrido real de 14 semitonos) |

Verificacion cruzada: pin permanente nuevo en `SpectralAnalyzerTest.cpp` (bloque
"Fundamental debil con sub-octava fuerte": fuente sintetica con la firma exacta de SWEP1
=> detecta 123.97 Hz y la tabla del modelo es la honesta: 124=1.000, 248=0.000 — la
guardia en accion —, 372=0.060); ctest **26/26**; sonda `RESULT: OK` en los cinco con el
check de sub-octava (`PROBE_HALVE_F0=1`) como guardia para material futuro; el analizador
de produccion regenera los modelos tonales sin mover un byte (verificacion aparte del
mismo dia). El matiz honesto: la mediana de SWEP1 empeora de cifra (-4.5 -> -11 cents)
porque ahora se mide sobre la raiz verdadera con parciales reales — antes la cifra
sostenia un modelo una octava abajo lleno de fantasmas.

## 2026-09-23 (q): 10.6 cerrada — frames con f0 por frame, y RRISE resulto ser material en capas

La Fase 10.6 hace que el modelo temporal "cante" el pitch del WAV: `SpectralModel`
guarda la raiz de cada frame (`extraF0[15]`, frame 0 = canonico), el `FrameSampler`
interpola la raiz en dominio log y entrega el ratio; el motor lo multiplica por parcial
(`partialFreq *= gridRatio` en Resonator y ResonatorBank — 1.0 en estaticos, bit-compat).
Formato v2 con `frameF0` opcional; analizador con HPS POR VENTANA
(`detectPitchFromSpectrum`, frame 0 conserva la raiz global). Verificado: ctest 26/26
(armonicos estables en el emparejador, roundtrip de extraF0, sampler que interpola la
raiz en z=0.5), suite WebUI 233/233, aceptacion con barrido monofonico sintetico
(293.7 -> 440 Hz): trayectoria capturada 344.5 -> 387.9 -> 436.0 Hz, reproducida por el
motor con morphZ.

El hallazgo: **CZ-RRISE no es polifonia — es material EN CAPAS sobre UNA rejilla** (E1,
41.62 Hz; residuo medio 2.8 cents en los 121 picos de 9 ventanas; drone = n1..3
persistente, "voz lider" = envolvente espectral que trepa por n7..15). Con una sola
rejilla no es representable y la sonda lo decia con "ESTATICO no aplicable: barrido".
Cierre del agujero: el analizador estatico ahora lleva **guardia de desviacion de pitch**
(`measurePitchDeviation`: HPS por ventana, desviacion plegada en cents sobre (-600,+600]
CONTRA LA RAIZ — los saltos de octava del estimador colapsan a 0, leccion de PAD1 —,
umbral 150 cents), `analyzeTemporal` queda exenta (es la representacion legitima de los
barridos). Superficie: sonda imprime "guardia: desviacion X cents"; GUI del ModelMaker
avisa en naranja y BLOQUEA la export del estatico des-afinado; test permanente (vibrato
+-6 cents => guardia 0; chirp de una octava => dispara a 599 plegados). ctest 26/26 y las
metricas canonicas de la sonda intactas (BASS1 49/64, HAMOG 64/64, PAD1 17/64, SWEP1
61/64). El diseno de la Fase 11 (capas: clustering coseno, formato v2.1, morphZ2/3) y su
investigacion NMF viven en `DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD`.

## 2026-09-23 (r): sonda — validación acústica y check de sub-octava (el modelo se verifica contra la fuente)

Dos capas de verificación en `NEURONiK_ModelMakerRealWavProbe` (paso 4.5 del flujo, tras
el sampler y antes del engine) que convierten la sonda en un verificador acústico de
punta a punta: WAV → modelo → ¿suena a la fuente?

**Validación acústica (`acousticCheck`)**: DFT INDEPENDIENTE del analizador (Goertzel por
banda sobre el segmento de RMS máximo, ventana Hann de 8192, avance 1024 — no comparte
FFT con el análisis). Para cada uno de los **top-8 parciales más fuertes** del modelo,
busca el pico REAL del espectro en su banda (k·f0 ± f0/2, nunca invade al vecino) con
interpolación parabólica sub-bin y compara la frecuencia del modelo contra la medida, en
cents. Frecuencia **contractual** (mediana de los 8 > ±35 cents ⇒ des-afinado); amplitud
informativa (el analizador promedia ventanas, la sonda mide una). Los top-8 y no todos:
los parciales débiles caen en picos vecinos de fuentes con notas dobles (rejillas
entrelazadas a ~62 Hz en el material CZ101) — naturaleza de la fuente, no del modelo; la
mediana es la estadística robusta (un des-afinado sistemático la dispara, los vecinos
sueltos no).

**Check de sub-octava (escalera de energía)**: la mediana no distingue una raíz a la
MITAD de la fundamental real (el modelo desplazado una octava abajo también reparte
armónicos contra picos reales). La firma inequívoca: los IMPARES de la rejilla (f0, 3f0)
caen entre parciales reales y quedan vacíos mientras 2f0 canta. Contrato:
`m2 > 1e-6 && mOdd < 0.15·m2 && m3 < 0.15·m2` ⇒ FAIL "raiz sub-octava: f0 y 3f0 vacios
frente a 2f0". La escalera completa (f0/2, f0, 2f0, 3f0 con pico real medido) se imprime
siempre para diagnóstico. Verificado por conmutador `PROBE_HALVE_F0=1` (divide SOLO la f0
de la escalera): SWEP1 con raíz halved → f0 43.1 Hz mag 2.0 / 2f0 61.9 mag 117.9 / 3f0
107.7 mag 4.8 → **FAIL como debe**; producción sin el conmutador es no-op bit a bit.

Métricas (2026-09-23, los cinco WAVs de CZ101, mono 16-bit/44.1k):

| WAV | f0 | mediana | máx | escalera (f0/2 / f0 / 2f0 / 3f0) | veredicto |
|---|---|---|---|---|---|
| CZ-BASS1 | 124.2 Hz | **−3.4** | 229.0 (vecinos sueltos) | 212 / **525** / 98 / 59 — f0 dominante | OK |
| CZ-HAMOG | 123.8 Hz | **−1.6** | 4.4 | 153 / **401** / 327 / 169 — serie llena | OK |
| CZ-PAD1 | 124.6 Hz | **−0.1** | 3.1 | 0.5 / **1185** / 404 / 219 — sub vacía | OK |
| CZ-SWEP1 | 124.8 Hz | **−11.0** | 212.9 | 118 / **302** / 2.9 / 92 — 2f0 real vacío (n4 es hueco de la fuente) | OK |
| CZ-RRISE | 291.5 Hz | (−316.6 informativa) | — | no corre: exención de barrido | exento |

La exención de barrido (10.6) antecede al check: RRISE imprime su trayectoria
(291.5→624.2 Hz) y la mediana informativa sin fallar — el modelo temporal con f0 por
frame es su representación legítima. El matiz de SWEP1: su "error de octava" histórico no
era sub-octava del estimador (refinado en la entrada (p)) sino una fundamental débil con
el armónico 2 cantando; el check queda como guardia permanente para material futuro.

## 2026-09-23 (s): hallazgo de los barridos CZ — la trayectoria de pitch medida y lo que revela

Metodología (DFT independiente del analizador): ventana Hann de 8192, picos = máximos
locales con interpolación parabólica sub-bin; ajuste de rejilla armónica por mínimos
cuadrados (barrido de f0 27.5–600 Hz). Es la vara de medir con la que se leyeron los dos
barridos del banco CZ101.

**CZ-RRISE (10 s)** — cresta dominante por ventana (t = 0 → 9.85 s):

| t (s) | 0.00 | 1.23 | 2.46 | 3.69 | 4.92 | 6.15 | 7.38 | 8.61 | 9.85 |
|---|---|---|---|---|---|---|---|---|---|
| cresta (Hz) | 291 | 333 | 375 | 458 | 500 | 541 | 583 | 624 | 624 |

Lee como un barrido de pitch de ~13.6 semitonos (291→624 Hz)… **y no lo es**: los 121
picos de las 9 ventanas caben en UNA rejilla f0 = **41.62 Hz (E1)** con residuo medio
**2.8 cents** (máx 4.3 Hz), índices n = 1..18 contiguos. La cresta migrante son los
múltiplos n = 7→15 de la MISMA serie; el "drone" (42/83/125 Hz) es n = 1..3 persistente
(mag de n1 por ventana: 307,133,177,312,145,165,308,156,154 — constante). Es decir:
**envolvente espectral trepando por la escalera armónica de un drone de pitch fijo** —
barrido de formante, no polifonía ni glissando. El estimador por ventana (suelo 50 Hz)
persigue la cresta; por eso la sonda reporta "ESTATICO no aplicable: barrido" y la
guardia del analizador mide 436 cents de desviación.

**CZ-SWEP1 (4.6 s)** — pitch fijo: la cresta dominante vive en **124.1 Hz** (mag 302)
todo el fichero (frames del analizador: 123.4–124.2 Hz). Su "sweep" es ESPECTRAL
(filtro/formante sobre nota sostenida). La familia tonal (<800 Hz) ajusta a UNA rejilla
f0 = **62.18 Hz** (residuo medio 4.4 cents, máx 7.7) con índices n = {1,2,3,5,6,7,9,10}:
impares (familia del sub-oscilador a 62) y pares (familia de la nota a 124) —
"dos rejillas entrelazadas" que son UNA serie con huecos. Por encima, el racimo del
transitorio (1497–1938 Hz) son los armónicos n24–n31 de la misma rejilla (12/12 a
<28 cents).

Consecuencias ya materializadas: (1) el refinado del HPS (entrada (p)) ancla SWEP1 en su
octava correcta (124.8 Hz) con la regla de fundamental débil; (2) la guardia de
desviación de pitch (entrada (q)) convierte el des-afinado silencioso de RRISE en fallo
claro con los cents impresos; (3) los modelos temporales con f0 por frame (10.6) son la
representación legítima de los barridos — la aceptación con barrido monofónico sintético
(293.7→440 Hz) capturó la trayectoria 344.5→387.9→436.0 Hz y el motor la reproduce con
morphZ; (4) el diseño de la Fase 11 (capas) y la puerta de plegado de octava viven en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD`. Regla práctica que deja el hallazgo: en
material CZ101, **primero ajustar la rejilla, después leer la trayectoria** — una cresta
migrante sobre una serie fija es timbre que evoluciona, no nota que se mueve.

## 2026-09-24 (t): Fase 11.1 — las capas entran en el struct y el v2.1 en el fichero (y el procesador que se pasó de la pila)

El diseño de `DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` arranca por el formato, que es
lo que no se puede cambiar después sin romper ficheros: **el struct de capas y el
`.neuronikmodel` v2.1**. Esta entrada es el contrato de lo que ya está implementado y las
tres decisiones que cualquier lector futuro debe conocer.

**1. La capa 0 ES la raíz, y no hay copia.** `SpectralModel` gana `layerCount` (1..3),
`extraLayers[2]` (solo los frames de las capas 1 y 2), `layerWeights`, `layerFrameWeights`
y `layerNames`. La capa 0 no tiene almacén propio: sus frames son `amplitudes` /
`extraAmps` / `extraOffsets` / `extraF0` y su rejilla `frameSpanHz`. Eso da tres cosas
gratis: ningún camino v1/v2 se entera de que existen capas, no hay dos copias del mismo
frame que puedan divergir, y el puente WASM (`memcpy` de 128 floats sobre
`amplitudes`+`frequencyOffsets`) sigue siendo válido sin tocar el layout. La API de capas
(`numFramesOf`, `ampAt(l,f,i)`, `ampsOf(l,f)`, `f0At(l,f)`, `frameWeightAt`, `layerWeightAt`,
`layerNameAt`, `setLayerCount`, …) resuelve el índice 0 contra la raíz: la asimetría del
almacenaje no se filtra. Índices **0-based** (la capa "1" del diseño humano es la 0 del
código). Con `layerCount == 1` todo queda inerte.

**2. El dialecto tiene UN escritor, y el lector es tolerante.** `Source/Common/
SpectralModelWriter.h` (`Common::modelToJson` / `writeModelToFile`) es el único sitio que
escribe `.neuronikmodel`; `exportModel()` de la GUI se queda con la política (nombre,
guardia de afinación, diálogo), el roundtrip y la sonda RealWav llaman al mismo código. El
v2 puro **no cambia ni un byte** (sin `layers`, `format` sigue siendo el entero 2). El v2.1
añade `layers { layerCount, layers[ {name, weight, frames, frameWeights} ] }` y sube a
`format: 2.1`; el lector lo acepta si está y lo ignora si no está. Detalles que costaron un
test: el **nombre y el peso de la capa 0 se leen del bloque** (la raíz del v2 lleva el
nombre del MODELO), y sus frames **no** se releen (manda la raíz). Un fichero con más de 3
capas se trunca, una capa sin frames válidos corta la lectura sin invalidar el modelo.

**3. El presupuesto real no era el WASM, era la pila.** El modelo pasa de ~8 KB a ~25 KB
(2 capas extra × 16 frames × 129 floats) y `NEURONiKProcessor` se fue a ~1 MB: su cola de
comandos llevaba `std::array<Command, 32>` con un `SpectralModel` **inline** por comando
(800 KB). Los tests y la bancada crean el procesador en la PILA, así que
`StatePersistenceTest` y `ModelSlotTest` empezaron a morir al arrancar, en silencio (ctest
reportaba SEGFAULT y 13 s de "ejecución", con stdout perdido por el buffer). Arreglo: la
cola se reserva en el constructor (`std::make_unique<Command[]>(32)`) — el hilo de audio
solo indexa, como antes, sin asignar — y el procesador lleva
`static_assert (sizeof (NEURONiKProcessor) < 128 * 1024)` para que la próxima regresión de
ese tipo la pare el compilador y no un crash mudo. El clamp del v1 WASM
(`layerCount = 1` en `neuronikLoadModel`) es la misma decisión por el otro lado: el worklet
cruza 128 floats planos (su buffer es fijo, no depende de `sizeof`), así que suena la capa 0
y el bloque por slot se queda en ~33 KB.

**Verificación de la 11.1 (todo verde, 2026-09-24):**

```text
cmake --build build-reference --config Release --parallel   -> OK
ctest --test-dir build-reference -C Release                 -> 28/28 (7,7 s)
NEURONiK_WasmParityTest <scratch>.json + cmp con la referencia -> idéntico BYTE a BYTE
node Tests/neuronik_wasm_parity.mjs build-wasm/neuronik_dsp.js -> 5 esc. x 9 casos, 0 ulps (184 320 muestras)
NEURONiK Web Pilot.exe --selftest                           -> EXIT=0, "RESULT: OK"
```

El roundtrip v2.1 vive en `Tests/ModelMakerRoundTripTest.cpp` (§9 "Capas (v2.1)" y §10 "el
v2 puro no cambia"): 2 capas con nombres, mezclas, frames bit a bit (con el clamp de banda
del escritor), `frameF0` por frame, pesos temporales, truncado de 5 capas a 3, la ranura
del engine exponiendo la capa 0 y un modelo por defecto de 1 capa (el invariante del puente
WASM). El binario WASM versionado (`WebUI/public/worklet`) no se regeneró: el DSP no cambió
y el clamp lo hace innecesario; cuando se reconstruya, la paridad sigue siendo el árbitro.

## 2026-09-24 (u): el refinado del HPS, verificado con la sonda (SWEP1 en su octava) y el esbozo del producto que no hace nada

Encargo de la jornada: «implementa el refinado del HPS (esbozo del pico antes del producto de
armónicos) y verifica con la sonda que SWEP1 sale en la octava correcta». **El refinado lleva en
producción desde la entrada (p)** (2026-09-23, commit `d1a83d2`, en `detectPitchImpl`), así que
lo que faltaba era la verificación de punta a punta y un matiz de documentación: el esbozo elige
el **ancla**, no se aplica a los factores del producto (que multiplica bins crudos,
`hps[i] = m[i]*m[2i]*m[3i]*m[4i]`). El contrato está pinneado en `SpectralAnalyzerTest` §4
(«Fundamental débil con sub-octava fuerte»: 62@0.39 / 124@1.0 / 186@0.56 / 248@0.05 / 372@0.06
⇒ detecta **123.97 Hz** y la tabla honesta 124=1.000, 248=0.000).

**Verificación con la sonda (2026-09-24, los cinco WAV del banco CZ101):**

```bash
cd ABDNeural
./build-reference/Release/NEURONiK_ModelMakerRealWavProbe.exe \
  ../ABDCZ101/DOCS/patches/CZ-{BASS1,HAMOG,PAD1,SWEP1,RRISE}.wav    # RESULT: OK
```

| WAV | f0 (sonda) | estático | lectura |
|---|---|---|---|
| CZ-SWEP1 | **124.6 Hz — la octava correcta** | parcial 1 **−3.5 cents**, mediana −4.5 sobre 8 parciales reales (máx 212.9 en el 4: artefacto de las dos familias de la fuente, no del estimador) | escalera: f0/2 61.9 Hz mag **117.9** / f0 124.1 mag **301.8** / 2f0 264.1 mag **2.94** (vacío ⇒ la candidatura de sub-octava muere ahí) / 3f0 372.4 mag 91.7; frames 123.3–124.3 Hz; rejilla 25.2 cents de residuo con 365 observaciones |
| CZ-BASS1 / CZ-HAMOG / CZ-PAD1 | 124.3 / 124.3 / 124.6 Hz | medianas −3.4 / **−1.6** / **−0.1** cents (máx 3.1 en PAD1) | sin cambio respecto al registro de (p) |
| CZ-RRISE | 273.2 Hz, desviación 548 cents | exento de check estático (barrido 256→601 Hz) | el temporal con f0 por frame sigue reproduciendo la trayectoria |

`ctest` **28/28**. La sonda no mueve un byte de los modelos CZ exportados.

**La medición negativa (para que nadie la repita):** se implementó la variante que faltaba
— esbozo (±1 bin) aplicado a **todo el espectro antes del producto** — y se midió antes/después:
los **cinco modelos CZ salen byte a byte idénticos**, el log de la sonda es idéntico línea a
línea y `ctest` sigue 28/28. Para no depender de un solo banco se portó `detectPitchImpl` a
Python (`_hps_proto.py`: mismo ancla, misma ventana del producto, misma guardia de soporte y
misma escalera de octava) y se barrieron 28 casos sintéticos (serie 1/k, firma SWEP1, fundamental
débil, series inarmónicas a +20/+40/+60 cents acumulados, huecos en los pares, armónicos altos
dominantes): **ninguna decisión de octava cambia**; la única diferencia son unos pocos Hz dentro
de un caso que ambas variantes fallan. Conclusión: la octava la deciden el ancla y la escalera
musical, no los factores del producto — la variante se revirtió y el doc del HPS lo dice ahora
explícitamente. El barrido deja además el límite conocido por escrito: el estimador solo baja
UNA octava desde el ancla, así que material con la fundamental muy débil y los armónicos 4+ dominantes
se queda en el armónico ancla (caso "armónicos altos" del barrido). El reproducer queda en el
repo para futuras series.

## 2026-09-24 (v): el banco CZ101 en UN preset (CZ101-BANK) — las cuatro ranuras y el pad XY tocando el banco

Encargo de la jornada: «empaqueta los modelos de los cinco patches CZ101 como assets de
ejemplo y anade un preset de fabrica que los use». **El empaquetado ya estaba** (entrada (m) y
commit `f6921d2`: los seis modelos y los seis presets en `Assets/` viajan embebidos en
`NEURONiK_FactoryModels` y `installFactoryPresets()` los deja en `Documents/NEURONiK` solo si
faltan), asi que lo genuinamente nuevo es el preset de BANCO, que es justo lo que ningun preset
de un solo modelo podia hacer: llenar las CUATRO ranuras.

**Que se hizo.** (1) `Tests/FactoryPresetGenerator.cpp`: la tabla pasa de `{presetName,
modelFile}` a `{presetName, models[4]}` (ranuras A-D, `nullptr` = ranura no declarada), se
`removeProperty` de las cuatro `modelPath<slot>` antes de cada preset y solo se escriben las
declaradas; la guardia de round-trip comprueba POR RANURA que declarado == presente y con
marcador. La septima fila es `CZ101-BANK` con BASS1/HAMOG/PAD1/SWEP1. (2) Regenerados los
ficheros con el generador: **7 presets OK** y `md5sum -c` confirma que los seis presets previos
salen **byte a byte identicos** (el cambio de tabla no toca lo ya versionado). (3)
`Assets/Presets/CZ101-BANK.neuronikpreset` (3655 bytes) al `juce_add_binary_data` →
`NeuronikFactoryModels::CZ101BANK_neuronikpreset`. (4) `installFactoryPresets()` gana su
entrada. (5) `FactoryPresetAudioTest` gana la §5 del banco.

**El orden de las ranuras no es decorativo.** El morfeo bilineal del resonador
(`Resonator::updateHarmonicsFromModels`) es `ampTop = lerp(A,B,morphX)`,
`ampBottom = lerp(C,D,morphX)` y `lerp(top,bottom,morphY)`, asi que la esquina (0,0) es la
ranura A y la diagonal (1,1) la D. El banco se coloca para que el pad se lea como un tablero:
X = bajo↔HAMOG, Y = pad↔sweep, esquina A = CZ-BASS1. Cualquier reordenacion futura de la tabla
del generador cambia el mapa del pad, no solo el preset.

**§5 del test de fabrica (lo que caza).** Carga real del preset de banco desde el stage,
`slotsOk` (los cuatro `modelPath` en el APVTS + `getModelNames()` = nombres de fichero sin
extension), y luego la tabla de parciales de 64 bins que el motor publica para la UI
(`spectralDataForUI`) en la esquina A (0,0) y en el centro (0.5,0.5) tras un `processBlock` con
la nota 47 (B2, la f0 de los cuatro patches): si las ranuras B-D no estuvieran cargadas o el pad
no llegara al motor, la tabla del centro seria la de A y la distancia acumulada seria 0 — este
es el check que lo caza (medido: **0.169**, umbral 0.05). Mas RMS finito y > 1e-4 en ambas
posiciones (0.13879 y 0.20737), sin contrato de cents: el render del banco suma cuatro modelos.
Trampa encontrada al escribirlo: `stage.deleteRecursively()` vivia ANTES de la nueva seccion y
borraba los modelos del stage que el preset de banco necesita para cargar — el borrado se movio
al final del test.

**Verificacion (2026-09-24):** `NEURONiK_FactoryPresetAudioTest` **EXIT=0**, «All checks
passed» (los seis presets de un modelo intactos + banco: instalado sin marcador y con las cuatro
rutas reales, A-D cargadas, pad moviendo la tabla, banco sonando en las dos posiciones);
`ctest` **28/28** (7.13 s); selftest del piloto WebView2 **EXIT=0 / RESULT: OK** con las cuatro
direcciones ACCIONES en verde.

**Pendiente apuntado:** el banco toca el pad XY como selector de tablero, pero la ranura D
(SWEP1) vive en la diagonal, que es la esquina mas dificil de alcanzar a mano: si el banco se
quiere como demo de escaparate, un preset hermano con el SWEP1 en una esquina ortogonal (o
morphZ animando la diagonal) es mas agradable de tocar y reutiliza todo lo de esta entrada.

## 2026-09-25 (w): Fase 11.2 — el clustering por forma de envolvente (y las tres medidas que cambiaron el diseño del plan)

Encargo: «implementa la fase 11.2: clustering por forma de envolvente en SpectralAnalyzer con test
de trazas sinteticas». Hecho y verificado: el modulo puro (`Source/ModelMaker/Analysis/LayerClustering.h`),
su cableado en `SpectralAnalyzer::analyzeTemporal` (segunda lectura del mismo espectro contra la
rejilla comun + `buildLayeredModel`), la puerta de rejilla, y `Tests/LayerClusteringTest.cpp`
(target `NEURONiK_LayerClusteringTest`, 10 casos / **29 checks en verde**). `ctest` **29/29** (los 28
de siempre + el nuevo). Sin cambios en DSP, en el formato o en los assets: la paridad nativa/WASM y
el v2 puro no se tocan.

**Tres cosas que el test obligó a cambiar del diseño del plan** (todas medidas, no opinadas):

1. **El coseno de §3.3 no separa** — y se midió: drone del RRISE (n1) contra n7 da **0.69 ≥ 0.55**,
   los fusionaría, porque el coseno es ciego al soporte y una traza plana correlaciona con todas
   (§10.4 ya lo anticipaba). Decide la afinidad `1 − distancia` en el plano (SOPORTE, ENTROPIA), con
   el soporte pesado **x2** (con los dos ejes a peso 1 la afinidad media del grupo de voz {n7, n15}
   contra el drone sale 0.56 —justo por encima del corte— y el drone se absorbía; con x2, 0.42). El
   coseno queda expuesto como `envelopeCosine()` para que el test lo MIDA.
2. **La guardia de §3.7 (energía sola) invertía la raíz**: con la voz 20× más fuerte que el drone,
   el drone baja del 10 % de la energía y se reabsorbía *en la voz* — la capa 0 pasaba a ser el
   barrido y un lector v2 viejo (que oye la raíz) oiría justo lo que no es el cuerpo del sonido.
   Ahora "floja" exige además ser **episódica** (soporte medio < `kPersistentSupport` = 0.75: la capa
   reabsorbible del test suena en 4 de 8 frames, el drone que debe sobrevivir en 8 de 8).
3. **El orden de las capas no estaba en el plan**: la capa 0 (la RAIZ) es la más **persistente**
   (soporte medio; a igual soporte, energía). Con el drone del RRISE de un solo parcial, la voz suma
   MÁS energía (2051 vs 1857) — ordenar por energía pondría el barrido en la raíz.

Además, el §3 del test necesita un drone de **dos** parciales (n1 y n3): con uno solo el reparto
honesto es 1 capa, porque una capa de un único índice es degenerada por diseño (§3.7, y el caso 5
del test lo pinnea). Es la misma regla que impide que un armónico suelto se convierta en "capa".

**La puerta de rejilla** (nueva, `kLayerGridCents` = 100 cents sobre la f0 por ventana plegada en
octava, como `measurePitchDeviation`): material cuyo pitch barre **no se parte**; su representación
honesta es el camino de una capa con f0 por frame (10.6). El caso §9 del test lo pinnea (el mismo
drone + voz, con un barrido 200 → 600 Hz encima, sale en 1 capa con `lastLayerGridRejected()`).

**Medida sobre el banco real CZ101 (sonda RealWav, 5 WAV, RESULT: OK)**: **SWEP1 es el único WAV que
toma el camino de capas** (f0 estable, timbre por capas) y sale con **2 capas** —cuerpo n1/n5 + banda
alta n19..64 contra la banda n2..n23 que barre (pico 0.46; frameWeights 1.00/0.68/0.71/1.00)—;
BASS1, HAMOG, PAD1 y **RRISE siguen en 1 capa** (el RRISE barre 102 → 601 Hz por ventana: la puerta
lo rechaza). Los assets versionados **no** se regeneran en esta fase: el ejemplo de capas en assets es
la 11.5 y el motor que SUMA las capas es la 11.3 (hoy el engine suena la capa 0, que es el contrato
del v2.1 y lo que mantiene el bloque de ~33 KB del v1 WASM).

## 2026-09-25 (x): el suelo de E1 — decidido con medidas: la vía es la f0 MANUAL (el suelo no se toca)

Encargo: «decide y aplica la vía para el suelo de detección de E1: f0 manual en el ModelMaker o
bajar el rango del estimador». **Decidido y aplicado: la f0 manual**, y con las tres alternativas
medidas antes de elegir — no por criterio, por número.

Qué dice la medida (material sintético de la firma de bajo: E1 = 41.62 Hz con fundamental 0.35 y
el 2º armónico dominante, más los cinco WAV del banco y un control A1 = 55 Hz; la sonda estrena
`PROBE_F0=<hz>` para fijar la rejilla a mano sin GUI):

| vía | resultado medido | veredicto |
|---|---|---|
| bajar el ancla a 35 Hz | **16/16 modelos byte a byte idénticos** (banco + E1 + control) | **NO-OP**: el ancla no es lo que ata |
| abrir la puerta de sub-octava (±60 cents + un bin) | E1 débil: 83,2 → **41,6 Hz** (mediana −1,8 cents)… y **CZ-SWEP1: 124,6 → 62,6 Hz** (una octava abajo, 97,9 cents de error) | **revertido**: rompe el caso para el que se afinó el HPS |
| **f0 manual** (la elegida) | modelo correcto (mediana −1,8 cents frente a **501** del automático) | aplicada, con lo que le faltaba |

Por qué el suelo no era la restricción: en la firma de bajo el ancla del HPS es **su 2º armónico**
(el pico más fuerte de la ventana), así que bajarlo no cambia la lectura; y la evidencia que
resolvería el caso —el producto de armónicos en el bin del sub-octava, hps 2,27e9 frente a 8,07e3
del ancla, 12 dB— la tira la **puerta de "una octava abajo"**, que exige ±60 cents del ancla/2
mientras un bin a 40 Hz mide ~130 cents: es irrealizable por debajo de ~150 Hz (por eso ese camino
está *muerto* en todo el banco CZ, y abrirlo mueve SWEP1).

Lo que **faltaba** para que la vía manual fuera real, y es lo que se aplicó:

1. `SpectralAnalyzer::anchorFloorHz(sampleRate)` — fuente única del suelo (50 Hz cuantizados al
   bin: 48,45 Hz a 44,1 kHz), que usan el ancla del HPS y la política nueva.
2. `analyzeTemporal` **ya no re-estima la f0 por ventana por debajo del suelo**: la rejilla del
   llamador manda en TODOS los frames. Medido antes: un modelo temporal de E1 salía con la rejilla
   del usuario en el frame 0 y **una octava arriba en los demás** (41,6 / 83,2 / 83,2 / 83,2);
   ahora 41,6 en los cuatro. Por encima del suelo el camino es el de siempre (los cinco modelos del
   banco, byte a byte idénticos).
3. `SpectralAnalyzer::refineGrid(audio, sr, seed)` — el mismo ajuste por mínimos cuadrados que
   `detectPitch` pero con la semilla del **usuario**: el ModelMaker pule el f0 escrito a mano y
   publica SU residuo (medido: 41,62 → 41,62, residuo 0,4 cents y 30 picos).
4. ModelMaker: el chequeo de discrepancia **pisaba** el f0 a mano (41,62 → 83,24) cuando la nota y
   la octava estaban seleccionadas — por debajo del suelo el detector no es evidencia, así que ya
   no lo pisa; y el indicador de rejilla declara "fijada a mano (el estimador no llega hasta
   aquí)" junto al residuo de ESA rejilla.
5. Sonda: `PROBE_F0=<hz>` (la vía manual medida sin GUI) + `SpectralAnalyzerTest` §6 con las seis
   comprobaciones (valor del suelo, la medición negativa del estimador —83,24 Hz, pinneada—, el
   modelo con la rejilla a mano 41,62 = 0,369 / 83,24 = 1,000 / 124,86 = 0,588, los frames 41,62 en
   los cuatro, la frontera —a 62 Hz vuelve la estimación por ventana: 83,17— y `refineGrid`).

Verificación: `ctest` **29/29** y los cinco modelos del banco byte a byte idénticos a la referencia
(`_probe_before_e1.md5`, 16/16) — el único modelo que cambia en todo el banco/probe es
`_e1_full-temporal` (el arreglo pretendido) y ninguno del banco CZ.

Para material **por debajo de E1** (B0 = 30,87 Hz) o multi-raíz: misma vía, f0 a mano.

## 2026-09-25 (y): CZ-SWEP1 regenerado con el analizador refinado — las métricas (y el asset que se quedó atrás)

Encargo: «registra en HANDOFF.md las métricas del modelo CZ-SWEP1 regenerado con el analizador
refinado». Regenerado con la sonda (el único camino que analiza el WAV real hoy; escribe en
`build-reference/probe-models/`, que está gitignorado — los assets versionados NO se tocan):

```bash
cd ABDNeural
./build-reference/Release/NEURONiK_ModelMakerRealWavProbe.exe ../ABDCZ101/DOCS/patches/CZ-SWEP1.wav
# -> RESULT: OK, ciclo completo WAV -> modelo -> recarga -> sampleFrame -> engine
```

### El análisis (estático)

| magnitud | valor medido |
|---|---|
| f0 del ajuste por mínimos cuadrados | **124,598 Hz** — el campo `frameSpanHz` del modelo (lo que puso una referencia anterior: 124,798 Hz) |
| indicador de rejilla de la UI | **residuo 25,2 cents** con **365 picos** (verde ≤15, amarillo ≤40: el SWEP1 es una rejilla pero con la familia de banda ancha estirada) |
| escalera de octava | f0/2 61,9 Hz mag **117,89** \| **f0 124,1 Hz mag 301,83** \| 2f0 264,1 Hz mag **2,94** (hueco) \| 3f0 372,4 Hz mag 91,66 — la fundamental gana y la candidatura de sub-octava muere en el hueco de 2f0 |
| validación acústica | 8 parciales (top-8): **mediana −4,5 cents**, max **212,9** (parcial 4: modelo 493,7 Hz vs medida 436,5 Hz — el artefacto de las DOS familias de la fuente, no del estimador); parcial 1 **−3,5 cents** |
| modelo | `format: 2` (v2 puro, 1 capa), 1 frame, **62/64** amplitudes ≥ 1e-3, dominante 1,000, suma 3,099, **6463 bytes** |
| engine | ranura A válida, parcial 1 (=dominante) a 1,000; `sampleFrame` a z=0/0,5/1 conserva el dominante; `frameSpanHz` sobrevive el ciclo del lector de producción |

### El análisis temporal (4 frames) — y las CAPAS

| magnitud | valor medido |
|---|---|
| modelo | `format: 2.1`, **layerCount 2**, 4 frames, **43 198 bytes**, pico mín 0,826, dominante máx 1,000 |
| trayectoria f0 | **124,6 / 124,6 / 124,6 / 124,6 Hz** (rejilla común: la puerta de la 11.2 la aprueba) |
| **capa 0** `"capa 1"` (raíz) | **48 parciales** — n1, n5, n8, n13, n19 y la banda n21…n64 — pico **1,000**, energía 7,2 (**55,0 %**), `frameWeights [0,835 0,971 1,000 0,963]` |
| **capa 1** `"capa 2"` | **16 parciales** — n2, n3, n4, n6, n7, n9…n18, n20, n23 — pico **0,460**, energía 5,9 (**45,0 %**), `frameWeights [1,000 0,682 0,714 0,999]` |

Las dos capas suman 64/64 índices: el clustering no deja parciales fuera y la raíz (la que oye un
lector v2 viejo) es la del cuerpo —n1 con la banda ancha—, con la banda que barre (n2…n23) en la
capa 1. **SWEP1 es el único WAV del banco que toma el camino de capas** (f0 estable con timbre por
capas); BASS1, HAMOG, PAD1 y RRISE salen en 1 capa (el RRISE barre 102 → 601 Hz por ventana: la
puerta de rejilla lo rechaza). Medido con el test de la 11.2, no a ojo: `LayerClusteringTest`.

### El asset versionado se quedó atrás (por diseño, hasta la 11.5)

`Assets/Models/` guarda **6 modelos**: cuatro estáticos (BASS1, HAMOG, PAD1 y SWEP1) y dos
temporales (BASS1-temporal y RRISE-temporal) — **no hay `CZ-SWEP1-temporal`**, así que el modelo
temporal de este registro vive solo en la salida de la sonda. El `CZ-SWEP1.neuronikmodel` versionado
está en `format: 2`, `frameSpanHz` **124,798 Hz**, 61 amplitudes activas y suma 2,974 (6402 bytes).
Contra el regenerado difiere en:

- la rejilla: **124,798 → 124,598 Hz = −2,8 cents** (el refinado del HPS + el ajuste por mínimos
  cuadrados del 2026-09-23/24);
- **los 62 offsets sub-bin** (todos: el modelo viejo los midió sobre la rejilla desviada) y el
  **parcial 16, que estaba a 0 y ahora mide 0,125** (la rejilla afinada lo trae de vuelta);
- 6402 → **6463 bytes**.

No se regeneran los assets en esta fase a propósito: el ejemplo de capas en assets es la **11.5** y
el motor que SUMA las capas es la **11.3** (hoy el engine suena la capa 0, que es el contrato del
v2.1 y lo que mantiene el bloque de ~33 KB del v1 WASM). Este registro deja las métricas del
regenerado para que la 11.5 pueda comparar antes/después sin volver a analizar a ciegas.

## 2026-09-25 (z): el modo REJILLA FIJA — declarado en la UI, en el analizador y en el fichero

Encargo: «convierte la rejilla fijada a mano en un modo declarado del ModelMaker: un "rejilla fija"
que desactive el seguimiento de pitch por ventana, lo anuncie en el indicador y quede escrito en el
modelo». Hecho, y con la promesa medida: **una rejilla declarada no es una rejilla adivinada**, así
que el modo es explícito en los tres sitios (decisión, análisis y fichero) en vez de una heurística
que adivina intenciones.

**Las tres promesas del modo y dónde vive cada una:**

| promesa | dónde | medido |
|---|---|---|
| el seguimiento por ventana se apaga | `analyzeTemporal(..., frameCount, fixedGrid = true)`: los `frameF0` son la rejilla declarada en TODOS los frames | CZ-BASS1 (cuyas ventanas saltan de octava **de verdad**: 124,7 / **62,4** / 123,6 Hz en la lectura normal) sale con **124,3 / 124,3 / 124,3** en el modo |
| el indicador lo anuncia | `gridLabel`: "REJILLA FIJA (sin seguimiento de pitch por ventana)"; y el detector **no pisa** la rejilla declarada (el chequeo de discrepancia se salta) | toggle en la UI + `updateGridIndicator()` |
| queda escrito en el modelo | `SpectralModel::gridFixed` → `"gridFixed": true` en el `.neuronikmodel`, leído de vuelta por `PresetManager::loadModelFromFile` | `grep '"gridFixed": true'` en el modelo de la sonda; el roundtrip lo lee |

**Formato:** la clave es **opcional y ortogonal a las capas** — no sube `"format"` (sigue siendo el
entero 2 sin capas y 2.1 con ellas) y **solo se emite cuando es true**, así que los ficheros de
siempre no cambian ni un byte: medido, los **16 modelos del banco + probe siguen byte a byte
idénticos** (el único que cambia en todo el arnés sigue siendo `_e1_full-temporal`, de la decisión
del suelo de E1) y el roundtrip comprueba que sin el modo la clave **no aparece ni en el texto** del
fichero.

**Verificación:** `SpectralAnalyzerTest` §7 (con material que barre: sin el modo la ventana sigue el
barrido 220 → 412,5 Hz y el modelo no declara nada; con el modo, la rejilla declarada en los 4 frames
y `gridFixed` a true; y el estático declara la procedencia solo cuando se pide) + `ModelMakerRoundTripTest`
§11 (la clave, que no sube de versión, el ida y vuelta completo, y que sin el modo no aparece en el
texto) + `ctest` **29/29**. La sonda gana `PROBE_FIXED=1` (combinable con `PROBE_F0`) para medir el
modo sin GUI:

```bash
PROBE_F0=124.3 PROBE_FIXED=1 ./build-reference/Release/NEURONiK_ModelMakerRealWavProbe.exe \
  ../ABDCZ101/DOCS/patches/CZ-BASS1.wav     # -> trayectoria f0=124.3..124.3 + "gridFixed": true
```

**Por qué importa más allá de E1:** hasta ahora el analizador decidía por su cuenta cuándo seguir el
pitch (y esa decisión se apoyaba en evidencia del material, no en la intención del usuario). Con el
modo, el usuario que conoce la fuente —un bajo cuyo f0 sabe, un patch con rejilla dudosa, material
donde el estimador salta de octava como en BASS1— puede declarar la rejilla y obtener un modelo
coherente **y auditable**: quien lea el fichero sabe que la rejilla no la confirmó el estimador.
Es la pieza que faltaba para que "f0 manual" fuera un modo de trabajo y no un campo de texto.

## 2026-09-25 (aa): el pad XY y el anillo morph-Z verificados EN VIVO con CZ-SWEP1 — y los cuatro fallos que la verificación destapó

Pedido: «carga el modelo `CZ-SWEP1.neuronikmodel` en NEURONiK y verifica en vivo el pad XY y el anillo
morph-Z con un modelo real». La verificación es la dirección **MORPH**, la octava del selftest del
puente (`Source/WebUI/BridgeSelftest.h`), que corre en el WebView2 del piloto —motor de verdad, modelo
de verdad, gestos de puntero de verdad—, así que no es una lectura de DOM: es la página viva.

```bash
cd build-reference/NEURONiK_WebPilotHost_artefacts/Release
./"NEURONiK Web Pilot.exe" --selftest        # -> RESULT: OK, EXIT=0 (ocho direcciones)
```

**Lo que la dirección mide, y lo que salió**

| paso | medición |
|---|---|
| carga | `CZ-SWEP1.neuronikmodel` -> ranura D (3), el MISMO fichero de `Documents/NEURONiK/Models` al que apunta el `modelPath3` del preset CZ101-BANK |
| las dos vistas, de acuerdo | esquinas del pad `selftest-model-a\|selftest-model-b\|selftest-model-c\|CZ-SWEP1` y ficha MODELOS A-D lo mismo (esperado, idéntico) |
| motor -> página | APVTS 0,2 / 0,8 / 0,6 -> pad X 0,200 / Y 0,800 (pulgar 0,200), aro 0,600 con arco 60,0 |
| página -> motor | gesto real (pad al 75 % de su ancho y 75 % de su alto = valor 0,25; aro 1/4 de vuelta) -> APVTS 0,750 / 0,250 / 0,250, y la página pintada igual |
| geometría | hit-testing en la página viva: centro del pad **en el pad**, trazo del aro **en el aro** (`padHit:1`, `ringHit:1`) |

**Los fallos que destapó (los cuatro arreglados aquí).** Ninguno era visible en un test de manejadores:
los tres primeros necesitan la página de verdad, y el cuarto la pila entera.

1. **El aro se comía TODOS los gestos del pad.** `.xy-pad__zring` es un overlay con `inset` negativo, así
   que su caja cubre el pad entero; sin `pointer-events: none` el hit-testing del navegador devuelve el
   contenedor y **nada** de lo que hay debajo recibe un dedo — el pad quedó sin gestos en todo su interior.
   Medido: `document.elementFromPoint(centro del pad)` devolvía el aro. Regla añadida a `main.css` (la caja
   transparente al puntero; el trazo conserva `pointer-events: stroke`, 11 unidades de ancho) y un test que
   lee la REGLA de la hoja de estilos: jsdom no tiene layout ni `elementFromPoint`, así que el hit-testing
   de verdad se mide en la página viva, no en un test de manejadores (que pasaba igual con el pad tapado).
2. **`morphX`/`morphY` no tenían dueño en el store.** Son dos parámetros del contrato que viven en una
   VISTA (el pad de la ficha MODELOS, `SECTION_VISUALS`), no en una celda del lienzo, y `SCREEN_PARAMETER_IDS`
   era «los 70 del lienzo». El store ignora los mensajes nativos de un id que no tiene, así que el motor
   los movía (un preset, el RANDOM, el XYPad nativo, la automatización) y el pad seguía dibujando su
   esquina: divergencia vista en vivo con el banco CZ101 cargado. Ahora salen del catálogo
   (`VISUAL_PARAMETER_IDS`) y `screens.test.js` compara contra el contrato GENERADO en vez de contar 70 a
   mano — si el APVTS gana un parámetro, el test falla hasta que la página lo posea.
3. **Un cambio NATIVO de modelo no llegaba nunca a la página.** Los modelos no son del APVTS, así que el
   sondeo de parámetros no los miraba, y la única otra publicación era la que responde a una carga pedida
   por la PROPIA página: cargar un preset en el motor (o desde el panel nativo) dejaba la ficha MODELOS A-D
   —y las esquinas del pad— con los nombres viejos indefinidamente. `publishPendingChanges` ahora compara
   nombres y publica un `modelsState`; publica el CAMBIO, no el estado (no lo repite en el sondeo siguiente).
   Test en `Tests/ParameterBridgeTest.cpp`.
4. **El presupuesto de arranque de la bancada (20 s) se comía el selftest.** Cuenta desde el lanzamiento e
   incluye la carga de la página, y con `dist` recién construido la carga se midió en **14 s**: el proceso
   terminaba con `reason=timeout` **en mitad de la última dirección y SIN veredicto** — y sin veredicto
   `finish` declara FAIL. Ahora ese presupuesto vigila la PÁGINA (`metrics.reactReadyMs < 0`) y, una vez
   lista, quien decide es el del propio arnés; y el arnés empieza su presupuesto de 30 s cuando la página
   puede contestar (una carga lenta no es un hop perdido, y la cota no se pierde: sigue habiendo 30 s).
5. **`parseMorph` leía de un `var` temporal ya destruido.** `juce::JSON::parse (raw).getDynamicObject()`
   engancha el puntero a un objeto que vive dentro del temporal: al final de la EXPRESIÓN muere, y en
   Release la lectura no revienta, contesta «no existe» a TODAS las propiedades. Medido: con el JSON crudo
   correcto delante (`"padHit":1,"ringHit":1`), el pad y el aro salían a −1 y las esquinas vacías, y la
   dirección fallaba en su PRIMERA lectura. Los otros lectores del arnés ya guardaban el `var` en un local;
   este era el único que encadenaba las dos llamadas.

**Comprobado además en un navegador real** (`vite preview` sobre `WebUI/dist`, modo local: el puente es el
canal del WebView2 —`window.__JUCE__`— y sin él la página no puede tener modelos, así que esto mide
geometría y pintado, no motor): `pointer-events: none` en la caja del aro y `stroke` + `11px` en el trazo,
`elementFromPoint(centro del pad)` -> `abd-xypad__pad`, `elementFromPoint(punto del trazo)` -> `zring-hit`.
(La captura de pantalla del panel no se pudo tomar: el panel de navegador del cliente no estaba disponible
en ese momento.)

**Verificación:** `ctest` **29/29** (incluye el contrato del selftest con los anclajes nuevos del pad y el
aro: `padVisual`, `padSurface`, `zRing`, `zRingFill`, y las ocho direcciones obligatorias), `npx vitest run`
**242/242**, `node Tests/webuiSelftestContractTest.mjs` -> OK, y el piloto **EXIT=0 / `RESULT: OK`**.

**Control negativo del aro (medido, no supuesto):** con la regla apagada
(`python _toggle_ring_css.py off`) y `dist` reconstruido, la MISMA dirección mide `centro del pad
TAPADO, trazo del aro en el aro -> FAIL` (`RESULT: FAIL`); repuesta la regla, `centro del pad en el
pad, trazo del aro en el aro -> OK`. O sea: la dirección distingue el estado bueno del malo, y el
trazo del aro sigue siendo agarrable aunque la caja se coma el pad — que es justo el reparto que
declara la hoja de estilos.

**Limitación honesta:** lo que se ha verificado es el pad y el aro contra el motor real (parámetros,
gestos, hit-testing) con un modelo CZ-SWEP1 ESTÁTICO en la ranura D. El dibujo del pad en el modelo
temporal de dos capas de la 11.2 no cambia porque la página solo pinta nombres, no amplitudes; el motor que
suma las capas sigue siendo la 11.3.

Todo sigue **sin commitear**, junto con la 11.1/11.2, la decisión de E1 y el modo rejilla fija.

## 2026-09-25 (bb): Fase 11.3 — el motor SUMA las capas (y CZ-SWEP1 suena entero)

Dos peticiones en el mismo turno:

- **11.2** (clustering por forma de envolvente): **ya estaba implementada** en el turno anterior
  (entrada `(w)`): modulo puro `LayerClustering.h`, cableado en `SpectralAnalyzer::analyzeTemporal`,
  `NEURONiK_LayerClusteringTest` (10 casos / 29 checks) y `ctest` verde. La variante **coseno** es
  justo la que se midio **contraproducente**: coseno(drone del RRISE, n7) = **0.69 ≥ 0.55**, los
  fusionaria; el test lo expone (`envelopeCosine()`) para que quede medido, no opinado. No se toco
  nada de 11.2 en este turno.
- **11.3** (el motor suma las capas): implementada aqui, con los dos motores y el contrato.

**Lo implementado**

| pieza | donde |
|---|---|
| sampler por capa + suma de capas | `Source/DSP/FrameSampler.h`: `sampleLayerFrame` (z propio + pesos: estatico x temporal del frame), `sampleLayeredFrame` (frame EFECTIVO del slot = SUMA), `LayerMorphZ`, `restLayerMorphZ()` |
| z por capa en los motores | `Resonator` (aditivo) y `ResonatorBank` (modal): `setLayerMorphZ(layer, z)`; el cache de frames se invalida con cualquiera de los tres z |
| voces | `Params::morphZ2/morphZ3` con el glide de 20 ms de siempre + `modMorphZ2/3` antes de actualizar coeficientes |
| contrato | `morphZ2`/`morphZ3` (0..1, default 0.0) y destinos de matriz **29/30** ("Morph Z 2"/"Morph Z 3") al final de la tabla |
| WebUI | la pagina POSEER los dos ids y su control vive en el cajon de MODELOS, junto a MORPH-Z (tres perillas: una por capa) |

**Las dos decisiones que hacen que esto sea barato y exacto**

1. **Suma espectral, no un banco por capa.** El reparto de 11.2 da a cada indice UNA capa, asi que
   sumar amplitudes por indice es EXACTO y los 128 biquads siguen siendo 128 (tres bancos habrian
   sido 384). El offset y la f0 de cada parcial los aporta la capa que MAS suena en ese indice: con
   el reparto del analizador es su dueno exacto; con capas solapadas a mano es determinista, porque
   un parcial del motor tiene UNA frecuencia, no varias.
2. **Con una capa, bit-exacto.** `sampleLayeredFrame` delega en el sampler de una capa cuando
   `layerCount == 1` y los pesos por defecto son 1.0 (`x * 1.0f` es exacto en IEEE-754, tambien para
   −0.0 y NaN): la paridad A–E y todo el legado no se mueven un bit.

**Medido con material REAL — CZ-SWEP1 (la peticion era que el modelo suene entero)**

```bash
rm -rf build-reference/probe-models
./build-reference/Release/NEURONiK_ModelMakerRealWavProbe.exe ../ABDCZ101/DOCS/patches/CZ-SWEP1.wav
```

```
[probe]   capas en el motor: 2 capas (capa 0: 48 indices, capa 1: 16)
[probe]     motor z2=0: capa 0 = 0.513, capa 1 = 0.487
[probe]     motor z2=1: capa 1 = 0.515 (su propio eje mueve su peso)
```

Las dos capas **suenan** (antes la de 16 indices era muda: el motor leia solo la raiz), y la capa 1
responde a SU eje. La sonda gana ese bloque: con una capa dice «UNA capa (el clustering no separo
nada)», y si alguna capa con indices no suena, **falla**.

**El fallo que la fase destapo (y que habria mordido en cuanto alguien usara el destino 28)**

`IVoice::resetModulations` no limpiaba `modMorphZ`. La matriz SUMA (`case 28: modMorphZ += rawMod`)
en cada tramo de control, asi que con una ruta a Morph Z el z de la voz **acumulaba sin freno**:
corria hasta 1.0, ahi se quedaba y no volvia ni al quitar la ruta. Era el unico destino fuera de la
lista de reset. Arreglado (los tres z se limpian) y pinneado en el test nuevo.

**Hallazgo pinneado, NO arreglado (medido):** el remapeo de rejilla por frame de la 10.6 no se
activa. Los dos motores calculan `gridRatio = frameF0 / frame.f0At(0)` sobre el frame MUESTREADO,
pero el snapshot del sampler no copia `frameSpanHz`: `f0At(0)` sale 0 y el ratio queda en **1.0
siempre** aunque el frame traiga `frameF0` (medido en el test: 565,7 Hz de `frameF0` con ratio 1).
Activarlo mueve el sonido de material real ya existente, asi que se deja como decision explicita
(el test lo pinnea como MEDIDA, no como fallo).

**Verificacion**

- `NEURONiK_LayerEngineTest` (**nuevo**, 16 checks) con un RRISE sintetico de dos capas (capa 0 =
  drone de dos frames; capa 1 = parcial que barre): bit-exactitud con una capa, la suma, los pesos
  (estatico x temporal = 0,5 x 0,25 → 0,125), el barrido por capas **medido con Goertzel sobre el
  audio renderizado** (morphZ1 mueve el drone y NO toca al barrido; morphZ2 al reves), el banco
  modal y el arreglo de `resetModulations`.
- `ctest` **30/30**; `npx vitest run` **242/242**; `node Tests/webuiSelftestContractTest.mjs` OK; el
  piloto `--selftest` **EXIT=0 / `RESULT: OK`**.
- Contrato regenerado (74 parametros, 31 destinos, 69 implementados). Dos denominadores escritos a
  mano en los tests de la WebUI (`10 / 27` y `23 / 28` destinos) pasan a derivarse del contrato
  generado: fueron exactamente lo que rompio al crecer la tabla, y volverian a romperlo.

**Limitaciones conocidas**

- El puente WASM sigue clavando `layerCount = 1` (11.1): en modo local del navegador (worklet) suena
  la capa RAIZ. El motor nativo es el que suma.
- El eje por capa se controla con tres parametros (`morphZ`, `morphZ2`, `morphZ3`); la fila de capas
  «bonita» del cajon (nombre, peso, envolvente) y el destino condicional de la matriz siguen siendo
  11.4, y el ejemplo de capas en assets es 11.5.

Todo sigue **sin commitear**, junto con la 11.1/11.2, la decision de E1, el modo rejilla fija, el
banco CZ101 y el ultimo tramo de la verificacion en vivo del pad/aro.


## 2026-09-25 (cc): la puerta de plegado de octava — mono-rejilla vs bi-rejilla, medido

**Qué es.** La puerta del plan §10.3, que hasta ahora no existía con nombre propio: la dispersión de
las f0 por ventana **después de plegar la octava** es el discriminador mono-rejilla / bi-rejilla del
material. Plegar es la equivalencia de octava aplicada a la medida (la desviación en cents reducida
modulo 1200 a (-600, +600], la misma convención —y el mismo código— que `measurePitchDeviation`), y
su razón de ser está medida: un salto de octava del **estimador** (una ventana lee f0 y otra 2f0) es
un artefacto de la lectura, no una rejilla distinta, y colapsa a 0 al plegar. Lo que no colapsa es
una raíz genuinamente distinta: una quinta (701,96 cents) sobrevive como -498,04, y una cuarta da el
**mismo** -498,04 (la puerta ve «raíz distinta», no «hacia dónde»).

**Qué queda.** `SpectralAnalyzer::foldOctaveCents()` (el plegado) y `measureOctaveFold()` (la
medida: `foldCents` post-plegado, `rawCents` cruda, `observations`, `octaveFlips` —ventanas que
saltaron de octava— y el veredicto `biGrid` contra `kOctaveFoldCents` = 100). Es un módulo puro: el
test lo prueba con vectores de f0 sintéticos, sin audio. **Sin datos** (sin ventanas, sin f0
positiva o sin referencia) `foldCents` es -1 y no hay veredicto: no se exenta nada por falta de
evidencia. La medida se toma en **toda** llamada a `analyzeTemporal` —también cuando el clustering no
llega a dar capas y también si el material sale vacío— y se publica en `lastOctaveFold()`; la sonda
la imprime por WAV.

**El corte se llama ahora `kOctaveFoldCents`** (antes `kLayerGridCents`, que era el mismo 100 con
otro nombre): `gridIsCommon` —el predicado que ya decidía si las capas se rechazaban— **es**
exactamente el veredicto de la puerta contra la rejilla del llamador, así que las decisiones sobre el
banco **no cambian**; lo que gana el analizador es que la comparación ahora tiene nombre, números y
test en vez de estar enterrada en un bucle.

**Medido con la sonda en los cinco WAV del banco CZ101** (post-plegado | crudo | saltos de octava |
veredicto | capas):

- **CZ-BASS1: 9,4 | 1194,4 | 1 | mono-rejilla | 1.** El caso que justifica el plegado: el estimador
  salta de octava en 1 de 4 ventanas (trayectoria 62,4 / 124,7 Hz) y la dispersión **cruda** son
  1194,4 cents —una octava— mientras la plegada son 9,4. Una sola rejilla.
- **CZ-HAMOG: 0,0 | 0,0 | 0 | mono-rejilla | 1.**
- **CZ-PAD1: 4,2 | 4,2 | 0 | mono-rejilla | 1.** El «PAD1 1200,5 cents» de §10.3 venía del barrido
  de mínimos cuadrados de la investigación (otra medida): el estimador del analizador lee PAD1
  estable. La inestabilidad de octava del estimador —el caso que la puerta debe perdonar— es la que
  reproduce el test con un vector sintético de 62/124 Hz.
- **CZ-SWEP1: 17,5 | 17,5 | 0 | mono-rejilla | 2 capas (48 + 16 índices).** La puerta no le toca
  nada: conserva sus dos capas y su reparto (0,513 / 0,487 en el motor).
- **CZ-RRISE: 502,5 | 1702,5 | 3 | BI-REJILLA | 1.** Sigue sin partirse, ahora con el motivo escrito:
  sus ventanas (102,2 → 601,2 Hz) no comparten la rejilla ni después de plegar.

**Test.** `Tests/LayerClusteringTest.cpp` gana la mitad C (**20 checks nuevos**, 58 en el fichero):
la medida pura (ventanas alrededor de la rejilla; el salto de octava del estimador, 1200 crudos que
plegados dan 0; quinta y cuarta con la MISMA dispersión plegada; la frontera de 100 cents; sin datos
no hay veredicto; la referencia solo importa por su octava; la rejilla declarada da 0) y dos casos de
punta a punta: el barrido del caso 9 (571,5 post-plegado ⇒ bi-rejilla y modelo de una capa) y un
**sub una octava por debajo del drone** (crudo 1200,01 con **7 de 8 ventanas** saltadas ⇒ plegado
0,01 ⇒ mono-rejilla y las capas se quedan: el artefacto del estimador no exenta al material).

**Verificación.** `ctest` **30/30**; los cinco WAV del banco siguen dando `RESULT: OK` en la sonda.
Todo sigue **sin commitear**, junto con la 11.1/11.2/11.3, la decisión de E1, el modo rejilla fija, el
banco CZ101 y la verificación en vivo del pad/aro.


## 2026-09-25 (dd): el formato ralo de parciales — DISEÑADO (fase 11.6), con las medidas delante

**Qué es.** El diseño de la extensión rala de v2.1: la lista de índices activos se declara UNA vez
por capa y cada frame lleva amplitudes + **ratios de inharmonicidad** `r = f/(n·f0)` en lugar de 64
offsets en Hz. Todo aditivo y opcional (sin las claves, el v2.1 de hoy byte a byte) y la decisión de
arquitectura que lo abarata: **el formato es ralo, el modelo en memoria sigue denso** (64+64 por
frame), así que `SpectralModel`, el motor, `FrameSampler` y el puente WASM no cambian y la paridad
bit-exacta no corre riesgo. Nada implementado: la sección §11 de
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` es diseño medido.

**La separación de conceptos que el diseño deja clara** (son dos cosas y solo una es tamaño):
el **ratio** no ahorra bytes, es la forma invariante a la transposición y lo que hace activable el
remapeo de 10.6 sin romper el timbre — medido en CZ-BASS1 (trayectoria 124,31 / 124,66 / 62,35 /
123,63 Hz): el ratio del índice 5 se mueve 1,8 cents entre frames (n=1: 1,9; n=8: 1,1) mientras su
offset en Hz pasa de +0,29 a +0,62 y el del índice 25 recorre 8,5 Hz; y el mismo frame leído contra
su f0 (62,35) o contra el `frameSpanHz` (124,31) da ratios que difieren ×2, por eso la referencia se
declara en el registro. El **ralo** sí es tamaño, pero solo en bandas finas.

**Lo medido (y las dos sorpresas).** Con los modelos temporales reales de los cinco WAV (4 frames;
arrays densos = 2240 B/frame): activos por frame con el suelo del analizador (1e-3) → HAMOG 63-64
(el ralo **pierde** 2×), BASS1 57/34/55/31 (mixto), PAD1 16-17 (**gana 2,1×**), RRISE 5/13/2/3
(**gana 7-17×**), SWEP1 capa 0 = 48 (pierde 1,4×) y capa 1 = 12/10/9/14 (**gana 2,5-3,8×**). El punto
de cruce con el coste real de entrada (≈53,4 B) está en **~41 activos de 64 (65 %)**, así que la
regla es **por frame** y no por formato. Totales de los arrays del banco: denso 53 760 B → **33 199
(62 %)** con la regla y **30 797 (57 %)** con la lista hoisteada. Las sorpresas: en un fichero de
capas `frames[]` y `layers[0].frames[]` son **el mismo contenido byte a byte** (8960 B de los 43 198
de CZ-SWEP1, el 20,7 %, que el lector ya ignora porque la capa 0 es la raíz: se propone
`"framesFrom": "root"`) y **el 23-29 % de todos los ficheros son cifras de 17 dígitos** que el
escritor imprime por defecto — más margen que la densidad, y decisión aparte (no toca el formato,
cambia TODOS los ficheros escritos).

**Compatibilidad y qué hace declarativo el cambio.** `"format"` sigue siendo 2.1 (es una ortografía
del mismo dato, no un dialecto): la **raíz nunca se escribe rala**, así que el plugin v1 y el v2
siguen sonando lo mismo; el lector viejo de v2.1 (`readLayer` exige `amplitudes` de 64 y hace
`break`) perdería la capa rala y sonaría la raíz — esa degradación queda **pinneada en el test**, no
como accidente. Y el ralo solo sale de la herramienta si un humano lo pide: toggle **«Ralo»** en el
ModelMaker (por defecto NO, como el modo rejilla fija) y `PROBE_SPARSE=1` en la sonda, de modo que
las baselines byte a byte de los 16 modelos del banco siguen siendo red de seguridad.

**Verificación prevista (la del test de la fase)**: round-trip denso→ralo→leído con cota de
precisión (la vía rala **no** es bit-exacta: divide y multiplica; amplitudes bit-exactas y offsets
`|Δ| <= 1e-4 Hz`, con `sampleFrame` < 1e-6 por parcial), la regla de densidad en sus dos ramas
(`kSparseMaxActive` = 41), la lista hoisteada con ceros por frame, `framesFrom: "root"` sin perder el
modelo, la degradación negativa y la sonda `PROBE_SPARSE=0/1` sobre los cinco WAV.

**Estado.** DISEÑO, sin código: `ctest` sigue **30/30** (no se tocó nada de motor ni de analizador).
Todo sigue **sin commitear**, como el resto de la jornada.

### 2026-09-25 — Offsets transpuestos (Δn/n): la inharmonicidad sigue al teclado

**Qué es.** El modo declarado que faltaba para que el transporte sea musical. El motor sumaba el
offset en Hz (`f_n = n·base + δ`), así que la desviación en cents respecto del armónico
(`1200·log2(1 + δ/(n·base))`) **cambiaba con la nota**: el modelo perdía su carácter al subir o bajar
por el teclado. Con `offsetsTranspose` el offset se declara medido contra la rejilla de análisis y el
motor lo escala por `base/f0`, con lo que los cents quedan invariantes
(`1200·log2(1 + δ/(n·f0))`) — es el ratio `r_n = 1 + δ/(n·f0)` de §11.4 del plan de capas, ahora en
vivo y **sin tocar la densidad** del fichero (el ralo de 11.6 es otra cosa: tamaño).

**Cómo.** `SpectralModel::offsetsTranspose` + `offsetRootHz`; `offsetScaleAt(base)` devuelve el factor
(**1.0 exacto** sin modo, así el legado es bit a bit). Los DOS motores —`Resonator` (aditivo) y
`ResonatorBank` (modal)— multiplican el offset interpolado por ese factor antes de sumarlo: la misma
regla en los dos puntos donde antes había la misma asimetría. La rejilla de referencia viaja al
snapshot en `offsetRootHz` (el sampler la copia de `frameSpanHz`) porque el snapshot **no lleva
`frameSpanHz`** — el hueco pinneado que mantiene inactivo el remapeo de 10.6: se usa el mismo dato en
un campo aparte para no encender aquello. `offsetScaleAt` cae a `frameSpanHz` sobre el modelo fuente.
En el fichero es `"offsetsTranspose": true`, opcional y ortogonal (`"format"` sigue siendo 2/2.1), así
que los modelos actuales no cambian ni un byte; el ModelMaker lo declara con un toggle («Offsets
transp.», junto a «Rejilla fija») que además lo aplica al preview sin re-analizar.

**Lo medido.** `ctest` **31/31** (el test nuevo, `Tests/TransposableOffsetsTest.cpp`, fija: bit-exactitud
en la nota de la rejilla, invariancia en cents una octava arriba medida con Goertzel sobre el motor
real —64,4 → 65,2 cents con el modo, 33,0 sin él— y que el modo y su rejilla sobreviven al sampler por
los dos caminos). `ModelMakerRoundTripTest` §12 cubre el lector de producción. La sonda, sobre los
cinco WAV (`PROBE_TRANSPOSE=1`), pone las dos leyes al lado sobre el parcial más desviado con
amplitud ≥ 0,05: **CZ-RRISE +113,1 cents en su rejilla ⇒ +57,5 una octava arriba sin el modo** (el
modo los conserva), HAMOG −11,1 → −5,5, SWEP1 +9,5 → +4,8, BASS1 −1,0 → −0,5, PAD1 +0,9 → +0,5. Y el
modo y su rejilla sobreviven el ciclo WAV→v2→lector de producción (la sonda falla si no).

**Alcance.** La referencia es la rejilla canónica del fichero: con `frameF0` por frame (10.6) el ratio
se define contra el f0 de ESE frame, y eso solo se puede hacer bien cuando 10.6 esté vivo. Se declara
y se acota. La ley de cuerda rígida (`r_n = sqrt(1 + B·n²)`, un solo B por capa) sigue apuntada como
la extensión natural de esto.

**Estado.** IMPLEMENTADO y verificado; sin commitear, como el resto de la jornada.

### 2026-09-25 — Los seis modelos del banco CZ101, assets del selftest E2E

**Qué es.** El selftest del puente cubría el FORMATO de un modelo, no el material: la dirección MODELOS
cargaba UN modelo real embebido (el temporal de CZ-BASS1) y llenaba las otras tres ranuras con JSON
sintéticos de un parcial (`selftest-model-b/c/d`). Ahora viajan **embebidos los SEIS**
`.neuronikmodel` del banco CZ101 —`Assets/Models`, los que genera la sonda del ModelMaker desde los WAV
de `../ABDCZ101/DOCS/patches`— en `NEURONiK_SelftestAssets` (antes, uno), y el E2E es autocontenido.

**Cómo.** `loadSelfModels()` de `BridgeSelftest.h` hace dos pasos 1) escribe los seis al directorio
temporal del arnés y **relee cada uno con el lector de producción**, exigiendo `isValid` y los frames
que el asset declara (1 en los cuatro estáticos, 4 en los dos temporales), con lo que los dos dialectos
del v2 —denso y con f0 por frame— pasan por el lector de verdad; y 2) carga los **cuatro del banco**
(CZ-BASS1, CZ-HAMOG, CZ-PAD1, CZ-SWEP1 —los mismos que el preset CZ101-BANK pone en las esquinas del
pad) en las ranuras A-D. El nombre que la página enseña sale del **nombre de fichero**
(`getFileNameWithoutExtension`, lo que publica el processor), no de la clave `name` del JSON: por eso
los assets se escriben con su nombre real y `modelName()` los espera tal cual.

**El agujero que cierra.** La validación del asset se registraba en el log como FAIL y **no contaba en
el veredicto** (ni se guardaba en un miembro): un asset corrupto habría dado un selftest OK mudo. Ahora
la suma de los seis entra en `finish()` como `modelsAssetsOk`, junto a la comprobación (que ya existía)
de que la ficha MODELOS de la página enseña los cuatro nombres cargados en este proceso.

**Medido.** El selftest del piloto (`--selftest`) en **EXIT=0 / RESULT: OK**, con las esquinas del pad
y la ficha A-D enseñando `CZ-BASS1|CZ-HAMOG|CZ-PAD1|CZ-SWEP1` (nombres reales, donde antes había
`selftest-model-a|b|c|d`) y los seis assets releídos uno por uno por el lector de producción. La
dirección MORPH sigue cargando el `CZ-SWEP1.neuronikmodel` **instalado** en Documents: eso mide otra
cosa (que `installFactoryModels()` corrió y que el fichero al que apunta `modelPath3` del preset de
banco existe), así que no se toca. `ctest` **31/31**.

**Estado.** IMPLEMENTADO y verificado; sin commitear, como el resto de la jornada.

### 2026-09-25 — La f0 manual medida: RRISE sí (E1), SWEP1 no es gratis

**Qué se hizo.** Generar los `.neuronikmodel` de CZ-RRISE y CZ-SWEP1 con **detección de f0 manual**
(el campo de pitch del ModelMaker + «Rejilla fija», que la sonda reproduce con
`PROBE_F0=<hz> PROBE_FIXED=1`) y **evaluar la mejora musical** con dos medidas independientes: el
chequeo acústico de la sonda y un análisis propio en numpy de los picos del WAV (Hann 8192 en la
ventana de RMS máxima y en 9 ventanas, picos con interpolación parabólica, encaje a la rejilla del
candidato). Los ficheros generados están en `build-reference/probe-manual/` (incluida la corrida
automática para comparar con el MISMO binario). No se tocaron los assets del banco.

**RRISE (41,62 Hz = E1): mejora clara, y confirma §2 del plan de capas.** El material es una serie
armónica sobre 41,62 Hz (41,9 / 83,1 / 124,6 / 166,6 / 208,3 / 249,4 / 291,1 / 333,0 / 374,5 Hz ≈ n =
1,007 / 1,997 / 2,994 / 4,002 / 5,005 / 5,992 / 6,994 / 8,001 / 8,999) y lo que se mueve es la
importancia relativa de los armónicos: el pico más fuerte sube de 291 Hz a 666 Hz (n=7 → n=16). No hay
barrido de pitch. Con la f0 manual: f0 por frame 41,6 en las tres (antes 102,2 / 526,2 / 601,2),
plegado **0,0 cents y 0 saltos** (antes 502,5 y 3 ⇒ "bi-rejilla"), encaje de los 100 picos medidos
**100 % ≤ 25 cents** con mediana 0,9 (antes 6 % y 344,0), acústica de la sonda **−0,7 / máx 11,3**
(antes −337,5 / 386,0) y **31 / 35 / 43 / 49 parciales activos** en los 4 frames (antes 5 / 13 / 2 / 3,
con el dominante del frame 1 en 1 100 Hz). El detalle musical: la energía **migra por la escalera**
(dominante n=7 → n=10 → n=14 → n=15, drone n=1 firme en 42 Hz, 0,53 → 0,26) — el eje morph vuelve a
mover TIMBRE. Generados: `CZ-RRISE-temporal` (4 frames, `frameF0` = 41,62, `gridFixed`, 15 974 B) y el
estático `CZ-RRISE` (55 activos, 6 487 B).

**SWEP1 (62,5 Hz): el encaje mejora igual de claro, pero el número tiene dos significados.** El
material es una serie sobre 62,5 Hz (pico más fuerte en 124-125 Hz, n=2; un n=1 a 0,4-0,7). Con
62,5: mediana 8,9 cents y **95 %** de los 209 picos dentro de un cuarto de tono (antes 25,4 y 45 %),
máx 1 202,1 → 52,3, y la acústica de la sonda baja su máximo de 212,9 a 97,9. Pero `frameSpanHz` es
también la **referencia de reproducción** (el motor hace `n·base` con `base` = nota tocada), así que
ese modelo suena una **octava arriba** al tocar la nota del parche: es el "abrir la puerta de
sub-octava … ROMPE CZ-SWEP1" de `SpectralAnalyzer.h`, que no habla del encaje espectral (lo mejora)
sino de la correspondencia tecla↔altura. Y con 124,6 el modelo **no puede** contener la serie impar
(62,5 < n=1; 188/311/374 en n+½): pierde el drone de sub-octava, que la sonda enseña como diagnóstico
(61,9 Hz mag 117,9). **Para SWEP1 la f0 manual es un intercambio, no una mejora**; lo que falta es
separar la rejilla de ANÁLISIS de la referencia de REPRODUCCIÓN (hoy el mismo `frameSpanHz`).

**Verificación.** `ctest` **31/31** y el selftest del piloto **EXIT=0** siguen verdes (no se tocó
código de producción: el experimento es sonda + análisis externo). Documentado en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` §9.2, que además deja dicho que este resultado
**refuerza** el cierre de §9.1/§10: el "bi-rejilla" de RRISE era seguimiento, no dos raíces, así que
multi-rejilla sigue fuera de alcance y con menos motivo todavía.

**Estado.** MEDIDO y documentado; los modelos quedan como candidatos sin promover. Sin commitear,
como el resto de la jornada.
## 2026-09-25 (ab): el caso gemelo de la escalera — la bajada x1/2 pinneada, y la cuantizacion asimetrica que esconde

Pedido: «anade el caso gemelo al test: fundamental debil con sub-octava que SI debe caer a 62 (la
regla x1/2 de la escalera)». El test del analizador pinchaba un solo sentido de la escalera —la firma
REAL del CZ-SWEP1 (fundamental debil, sub-octava fuerte, serie que MUERE tras el 3er parcial) NO baja
al sub-armonico— y faltaba el simetrico.

**Que se hizo.** Se anadio el caso **4b** a `Tests/SpectralAnalyzerTest.cpp`: fundamental debil con el
2f0 dominante pero serie CONTINUA hacia arriba, donde el periodo real es el del sub-armonico y la
regla x1/2 SI tiene que degradar el ancla una octava. Trae cuatro checks: `detectPitch` cae al
sub-armonico, NO se queda en el 2f0, el modelo reparte como la fuente (n1 debil / n2 dominante, al
reves que en el SWEP1) y la via por ventana (`analyzeTemporal`) cae igual — la bajada no puede ser un
artefacto de la ventana unica del HPS. El discriminador entre los dos casos queda escrito: en SWEP1 la
serie muere (la puerta x2 no deja subir al 2f0), en el gemelo sigue.

**Por que 64,5 Hz y no 62 (el hallazgo).** La peticion decia «caiga a 62», y **62 no es alcanzable**.
A 44,1 kHz / 8192 el bin mide 5,383 Hz; la regla x1/2 solo puede bajar UNA octava y solo dentro de
±60 cents de `ancla/2`, y a ~62 Hz ese margen es ANCHO DE UN BIN — asi que el ancla tiene que caer en
un bin PAR. Con el ancla en el bin 23 (las 62,3 Hz de la sub-octava real del SWEP1) las candidatas
(bin 11 = 59,2 y bin 12 = 64,6) quedan a ±75 cents y la bajada es inalcanzable; con el ancla en el bin
24 (129 Hz) el bin 12 (64,5 Hz) cae dentro y SI ocurre. La cuantizacion es **asimetrica**: la regla de
SUBIR (bandas de ±25 % sobre f0/2f0/3f0/4f0) no la sufre porque su ventana es relativa; la de BAJAR
(una ventana de ±60 cents sobre `ancla/2`) si. Se probaron nueve candidatas A-I antes de fijar la
firma: solo la que tiene el ancla en bin par baja (C, 64,5 Hz); todas las de raiz 62 se quedan en 124.

**Medido.** `detectPitch` = 64,50 Hz (objetivo 64,5 ±3, y >30 Hz lejos de 129); reparto del modelo
n1 = 0,100 / n2 = 1,000; `analyzeTemporal` 64,50 / 64,50 Hz en los frames 0 y 3. `ctest` **31/31** y el
selftest del piloto **EXIT=0 / RESULT: OK** (las seis modelos CZ101 siguen OK). Antes de anadir el
caso se quito el bloque de exploracion temporal que el turno anterior habia dejado en el test y se
restauro la llave de cierre desplazada del caso 4.

**Estado.** Pinneado y verificado. Sin commitear, como el resto de la jornada. La raiz de 62 sigue sin
ser alcanzable por el estimador automatico: la via honesta sigue siendo la f0 manual / separar rejilla
de analisis y referencia de reproduccion (ver la entrada de la f0 manual en RRISE/SWEP1).

## 2026-09-25 (ac): la puerta de plegado como PRE-FILTRO del analisis temporal (plan §10.4)

Pedido: «implementa la puerta de plegado de octava como pre-filtro del analisis temporal con su test».
El plan §10.4 ya lo pedia («la puerta 10.3 entra ANTES de 11.2: material bi-rejilla no paga
clustering»); lo que faltaba era el codigo.

**Que se hizo.** `SpectralAnalyzer::analyzeTemporal` pasa a dos pasadas. (1a) PRE-PASADA: una ventana
por frame, se guarda el espectro y se estima la f0 por ventana —la evidencia de la puerta—. La PUERTA
(`measureOctaveFold` + `gridIsCommon`). (1b) MEDIDA: `perFrame` SIEMPRE (el camino de una capa, bit a
bit); la rejilla COMUN (`perFrameCommon`) y el `clustering` SOLO si la puerta la declara COMUN. Antes
la puerta se medIa despues de todo y el clustering se corria igual para rechazarlo acto seguido; ahora
en bi-rejilla no se mide la rejilla comun ni se corre el clustering.

**Por que dos pasadas.** Para decidir ANTES hace falta la f0 de TODAS las ventanas, y la f0 sale del
espectro de cada frame. La pre-pasada lo guarda (~130 KB a 8 frames) para no repetir la FFT: el
pre-filtro ahorra el trabajo del clustering, no lo duplica.

**Salida BIT A BIT igual, medido.** Sonda sobre los cinco WAV: tabla de plegado identica (BASS1 9,4 /
HAMOG 0,0 / PAD1 4,2 / SWEP1 17,5 mono; RRISE 502,5 bi) y SWEP1 conserva sus 2 capas (48 + 16 indices)
y su 0,513 / 0,487 en el motor. Lo unico observable que cambia: en bi-rejilla `lastLayerGridRejected()`
pasa a true tambien cuando el clustering habria dado una sola capa (ya no se calcula), y nace
`lastClusteringSkipped()` — exencion ANTES vs rechazo DESPUES, dos accesos distintos.

**Test.** Mitad D de `Tests/LayerClusteringTest.cpp` (8 checks): bi-rejilla eximida con el analisis
intacto (f0 por frame sigue el barrido, f0 final 575,1 Hz); mono-rejilla no eximida (2 capas); y el
mismo material con REJILLA FIJA → plegado 0 y no se exime (la puerta la decide la politica de f0, no el
audio). `note()` de los casos 9 y 12 actualizados.

**Verificacion.** `ctest` **31/31**, selftest del piloto **EXIT=0 / RESULT: OK**, sonda **RESULT: OK (5
wav)** con la tabla de capas identica. Documentado en el plan (§10.4 + «Implementado») y ROADMAP.

**Estado.** Implementado y verificado. Sin commitear, como el resto de la jornada.

## 2026-09-25 (ad): la pestana CAPAS del ModelMaker (Fase 11.5)

Pedido: «empieza la Fase 11.1 del plan de capas: struct layers en SpectralModel y formato v2.1 con
su roundtrip test». Al ir a empezarla, **11.1 ya estaba implementada y verificada** (struct aditivo,
formato v2.1, un solo escritor `SpectralModelWriter.h`, lector en `PresetManager`, clamp del v1 WASM
y el roundtrip de 2 capas en `ModelMakerRoundTripTest` — ctest #15, verde). Se acordo seguir por la
**11.5**, que era lo que faltaba de la GUI.

**Que se hizo.** El ModelMaker gana el tercer panel «CAPAS (POR QUE SE PARTIO)»: una columna por
parcial (n=1..64) con la ALTURA = su pico normalizado al GLOBAL y el COLOR = su CAPA (la 0 es la
raiz: cian, 1 magenta, 2 ambar), la TRAZA de su envolvente por frame encima (normalizada a SU pico:
la forma) y una leyenda con nombre, numero de indices y peso por capa. Con una sola capa el panel lo
declara («mono-rejilla: el analisis no partio el material»). Es el feedback visual de la 11.2.

**Los datos de vista son un modulo PURO** (`Source/ModelMaker/Analysis/LayerView.h`): `buildLayerView`
devuelve `layerOfPartial[64]` (-1 inactivo), `peakOfPartial[64]` (global-normalizado), `envelope[64]
[frames]` (forma), la leyenda y los contadores. La GUI solo pinta; el test lo prueba con modelos
sinteticos, sin abrir ventana — el invariante que explota es que cada indice pertenece a UNA capa.

**Footgun destapado de paso.** `SpectralModel::amplitudes` / `frequencyOffsets` eran los UNICOS
arrays del struct sin inicializador: un `SpectralModel` por defecto llevaba **basura de pila** en los
64 parciales. Lo cazo el test del modelo vacio (el mismo slot de pila reciclado devolvia los datos
del caso anterior). Ahora van a `{}` como el resto; sin cambio de audio (la paridad A–E sigue
bit-exacta), pero un modelo por defecto queda VACIO y cualquier vista puede leerlo entero.

**Lo que ya estaba (no se toco).** `Assets/Models/CZ-RRISE-temporal.neuronikmodel` es asset del
selftest y de los factory models desde el 10.6; la sonda `NEURONiK_ModelMakerRealWavProbe` ya reporta
«capas en el motor» por WAV (adelanto de la 11.3). Lo que faltaba de la 11.5 era la GUI.

**Verificacion.** `ctest` **32/32** (31 + el target nuevo `NEURONiK_LayerViewTest`), selftest del
piloto **EXIT=0 / RESULT: OK**, y el ModelMaker **compila** (rebuild completo tras tocar el header
core de `SpectralModel`). El ASPECTO del panel es lo unico no automatizable: es una app WIN32 y queda
para el ojo.

**Estado.** Implementado y verificado. Sin commitear, como el resto de la jornada. Quedan de la fase
11.4 (WebUI) y 11.6 (formato ralo).

## 2026-09-25 (ae): donde vive la semantica del estimador de pitch (referencia)

La semantica del estimador de f0 —la de la puerta de plegado, el gemelo del SWEP1 y la paridad del
bin— esta PINNEADA en `DSP_PARAMETERS.md`, seccion «Analizador de pitch (ModelMaker) — semantica
pinneada»: ancla y suelo (48,45 Hz; E1 lee su 2o armonico), los TRES movimientos de la escalera de
octava con su condicion exacta, la cuantizacion ASIMETRICA de la bajada (por que 62 Hz es
inalcanzable y 64,5 si baja), las dos familias de «fundamental debil» (lo que discrimina es si la
serie SIGUE o MUERE), la puerta como PRE-FILTRO, la tabla medida del banco CZ101 y las guardias
(`pitchGuardCents`, `kPartialFloor`, `kMinLsObservations`).

`LAYER_SEPARATION_PLAN.MD` apunta ahi desde §10.3; esta entrada es el puntero para quien llegue por el
log. Nace de un footgun repetido: el hallazgo del gemelo (entrada (ab)) y el de la paridad del bin
(entrada (ac)) se re-derivaron en dos turnos distintos, asi que ahora tienen una casa documental en
vez de vivir repartidos entre HANDOFFs. `STYLES_GUIDE` (ABDSharedAssets) NO era el sitio: es el
sistema de diseno (tokens/CSS).


## 2026-09-25 (af): el anillo morphZ del pad, verificado en el navegador (destino 28)

Pedido: «verifica en el navegador el anillo morphZ del pad girando con LFO2 -> destino 28 tras los
fixes del engine standalone, y documenta el resultado».

**Antes de poder verificar: la pagina NO arrancaba.** `WebUI/src/app.js` moria al inicializarse con
`ReferenceError: Cannot access 'drawerVisualSpec' before initialization`: el bloque nuevo de
ENVOLVENTES calcula `routeControls` con `drawerVisualSpec?.id === 'envelope-blocks'`, y esa `const` se
declaraba MAS ABAJO (TDZ). Pantalla en blanco, cero UI, cero anillo. Arreglo minimo: subir SOLO la
declaracion de `drawerVisualSpec` por encima de `routeControls` (el spec antes de las rutas, y
`drawerVisual` —que necesita las rutas ya resueltas— donde estaba). Dos lineas movidas, feature
intacta.

**Protocolo y resultado (motor local WASM, standalone).** Con SOUND ON el worklet carga
`neuronik_dsp.wasm` (92.932 B), reporta `_neuronikInit OK` y ejecuta `process()`; el meter lleva
`morphZMod = _neuronikGetMod(28)`. Ruta puesta por la UI real: `mod1Source = LFO 2` (indice 2) y
`mod1Destination = Morph Z` (indice **28**, confirmado en la tabla de choices en vivo), amount por
defecto 1.00. Observado `.zring-mod` con un MutationObserver:

- refresco cada **89 ms** — son los 32 bloques x 128 muestras a 48 kHz del meter (85,3 ms);
- el arco barre **0..100** de punta a punta: 6, 56, 91, 100, 81, 39, 0 0 0 0 0 0, 21, 68, 96, 97, 71,
  24, 0 0 0 0 0 0, 36, 79, 99, 92, 59, 9 — la semionda positiva del seno y el silencio de la negativa
  (la base de morphZ es 0, asi que una contribucion negativa clava el arco en 0: ~47 % de las muestras);
- periodo medido **~1,07 s** con LFO 2 a **1,0 Hz** (teorico 1,000 s).

**Acoplamiento con el rate de LFO 2 (la prueba de que es ESA fuente).** Bajando el rate con las
flechas del knob (una pulsacion = -0,01 normalizado) a **0,6 Hz**, el periodo medido pasa a
**1,69-1,78 s** (teorico 1,667 s). El bias de +2..7 % es la cuantizacion del meter (89 ms) contra el
periodo medido a mano. Control negativo A/B/A: con `mod1Source = Off` el arco queda clavado en 0
durante las 30 actualizaciones siguientes (un solo valor distinto) y al volver a `LFO 2` se reanuda
(0, 1, 7, 16, 29, 38, 44, 52...). La ruta se dejo puesta y el rate restaurado a 1,0 Hz.

**Geometria (para quien lo busque).** `.xy-pad__zring` es `absolute; inset:-7px` sobre el pad y su
aro es un circulo SVG REAL de **195 px** (viewBox 0 0 100 100 con `preserveAspectRatio` por defecto).
El arco de modulacion es `.zring-mod` (#00c3ff, 4,5 px, opacidad 0,45, `pathLength=100`); `.zring-fill`
(2,5 px, opacidad 1) es la base de morphZ y `.zring-track` va con `stroke: none`. O sea: en reposo lo
unico visible del anillo es el arco de modulacion, y por eso el pad «gira» cuando la matriz mueve el 28.

**Verificacion.** Motor standalone OK en el navegador (Chromium del panel, 1680x1050), `ctest` no
tocado en este turno, WebUI **255/256** y evidencia visual del arco a media vuelta sobre el pad.

**Lo que quedo rojo y NO es de esto (WIP de ENVOLVENTES).** `WebUI/tests/appContract.test.js:109`
espera el literal `return createEnvelopeBlocks({` en `WebUI/src/ui/visuals.js`, y ese fichero tiene
ahora `const blocks = createEnvelopeBlocks({` + `blocks.setRouteOpener(...)` + `return blocks;` (la
siguiente asercion del MISMO test ya espera `blocks.setRouteOpener`). Es una asercion obsoleta de un
cambio a medias; el arreglo es `return` -> `const blocks =`. No se toco: son ficheros de otro frente.

**Hazard de medicion (apuntado).** El panel puede recargar la pagina por su cuenta al perder la
conexion con vite (`ERR_NETWORK_IO_SUSPENDED` -> «[vite] server connection lost»), y el store vuelve a
sus defaults: la ruta se pierde y el anillo se para. Si una medida sale plana, PRIMERO comprobar que
la ruta sigue puesta y que el motor sigue ON — una muestra «ramp» rara de este turno era exactamente
eso, y se descarto.


## 2026-09-25 (ag): ENVOLVENTES — tamanos, destinos junto a cada grafica y el plan de N slots

Pedido: «los knobs de las adsr estan seguidos en vez de en bloques y solo hay un grafico. crea el
grafico que falta y lleva todos los controles del adsr a la pestaña deslizante, deja solo los
graficos en el frontal a modo informativo, al lado de cada grafico pon los destinos de cada uno
(deberian estar definidos en la matriz de modulacion) y su % de amont, si hay algun destino
precableado/hardcodeado quitalo y añadelo como una conexion por defecto en la matriz de modulacion».

**Lo primero: la mitad del pedido YA estaba hecha** (sin commitear, de la sesion en curso): el cajon
tenia dos bloques —ENV 1 y ENV 2— con su curva y sus cuatro knobs cada uno, el frontal solo las dos
curvas, y los destinos leidos de la MATRIZ (`envRoutesFromSnapshot`, fuentes 6/7). El usuario lo
estaba mirando en `localhost:8399` (otra copia) y ya no. Lo que faltaba de verdad era el TAMAÑO y los
destinos **en el cajon**.

**Hecho (medido en el navegador, Chromium del panel).**

- **Cajon**: la curva no tenia altura propia — mandaba la proporcion intrinseca del SVG (viewBox
  100x48) sobre los ~570 px de ancho del cajon, o sea **~275 px por curva** y las dos se comian la
  pestaña. Ahora `.env-block .card__visual` tiene **altura fija 120 px**; cada bloque queda en 264 px
  y los dos caben (528 px en total).
- **Frontal**: las curvas bajan de ~167 a **157 px** y las columnas pasan de 10 a **18 px de gap**
  (**21 px reales** entre los dos dibujos: se leen como dos graficos). El recorte sale del relleno de
  la columna, asi que la banda del lienzo no cambia de alto (la fija `--abd-env-body`, espejo del
  `minBodyHeight` del contrato: cambiarla habria movido el encaje de toda la fila).
- **Destinos junto a cada grafico**: los bloques del cajon ganan `.env-block__routes` y pintan la
  MISMA fila del lienzo (destino -> **+100 %**), resuelta contra el snapshot en cada giro; sin ruta,
  «sin ruta en la matriz». El boton IR A LA RUTA sigue (es el salto a la MATRIZ; las filas dicen a
  DONDE va).
- **Fuera el destino precableado en el TEXTO**: «ENV 1 · AMP» / «ENV 2 · FILTER» y el subtitulo
  («ENV 1 amplitud · ENV 2 filtro») pasan a identidad — **ENV 1 / ENV 2** y «ENV 1 y ENV 2 · destinos
  desde la matriz». El papel lo declara la ruta: reencaminar la fuente ya no deja un rotulo mintiendo.
  No habia ningun destino hardcodeado en el PINTADO (ya salia de la matriz); lo hardcodeado era el
  texto.

**Verificacion.** WebUI **256/256** (21 ficheros; el fallo ajeno de `appContract.test.js` que
reporte en la entrada (af) ya lo cerro su autor) y las cuatro medidas de arriba tomadas en vivo:
curvas de 120 px en el cajon, 157 px y 21 px de aire en el frontal, rutas «Osc Level -> +100 %» /
«Filter Cutoff -> +100 %» bajo cada grafica y los dos bloques con sus 4 knobs.

**Plan de la matriz de N slots (NO empezado: cruza el cable y el audio).** El usuario decidio:
`filterEnvAmount` debe dejar de ser parametro propio y vivir como **ruta por defecto** (desactivar =
amount 0 %), la matriz debe tener **n slots** (o 32 si hay tope) y lo mismo para la primera
envolvente. Coste medido antes de proponer nada: la matriz asume 4 rutas en **20 sitios del C++**
(`ParameterDefinitions.h`, `NEURONiKProcessor.cpp`, `BridgeSelftest.h`), en la **tabla de offsets del
puente WASM** (campos 22..33) y en el worklet (indices + `INT_FIELDS`), ademas del WebUI
(`sections.js`, `envelopeViews.js`, `wasm/audioParams.js` y 4 suites); el precableado del motor es de
**una linea** (`AdditiveVoice.cpp:231`, `env2Depth = fEnvAmount * modEnvCutoff`). Orden propuesto:
(1) matriz a 32 slots **añadiendo** los nuevos campos al FINAL del layout (34..117) para no desplazar
los indices viejos — los presets de 4 rutas siguen cargando; (2) `filterEnvAmount` -> amount por
defecto de la ruta ENV 2 -> Cutoff, con migracion de presets, cc79 y el knob de la ficha FILTRO como
consecuencias a cerrar; (3) ENV 1 -> Amp como ruta (la mas delicada: la amplitud hoy la gatea el
`ampEnvelope` de la voz y no pasa por la matriz).


## 2026-09-25 (ah): el indicador de rejilla del ModelMaker, clicable

Pedido: «Haz cliclable el indicador de rejilla para cargar el f0 detectado en el editor de pitch con
un clic».

**Que se hizo.** `gridLabel` (la linea «Rejilla: f0 X Hz | residuo N cents | M picos») pasa a ser
pulsable: `useDetectedFrequency()` escribe la f0 detectada en `pitchEditor` y sincroniza nota/octava.
El cursor de mano y el tooltip lo anuncian — un texto que se puede pulsar y no lo parece es una
trampa — y sin material (`detectedFrequency <= 0`) el clic no hace nada (el indicador va vacio).

**La trampa que habia que mirar antes.** La pareja de carga es la del camino de grabar: texto a 2
decimales + `updateRootNoteFromFreq`. Ese helper es seguro **porque no notifica**: solo rellena los
combos que estan sin elegir y con `dontSendNotification`, asi que no hay ida y vuelta por la nota mas
cercana. Si hubiera notificado, la rejilla se habria cuantizado a C2 (64,50 -> 65,41 Hz) — justo el
error que el proyecto lleva toda la jornada midiendo y evitando (`DSP_PARAMETERS.md` §analizador de
pitch: la bajada de octava es sensible a menos de un semitono).

**Por que `MouseListener` y no `onClick`.** **JUCE 8.0.12 no tiene `Label::onClick`** (el header solo
trae `onTextChange`/`onEditorShow`/`onEditorHide`): el clic se escucha con
`gridLabel.addMouseListener (this, false)` y un `mouseUp` en el componente, con el mismo guardia que
usa el Label para editarse (`! e.mouseWasDraggedSinceMouseDown()`), para que un arrastre que ACABA
encima no cuente como clic.

**No re-analiza (a proposito).** Cargar el dato y analizar son dos decisiones distintas: el clic deja
la f0 en el editor y el boton Analizar sigue siendo el unico que analiza. Si se quiere «un clic y
listo», es un cambio de una linea (llamar tambien a `analyzeAudio()`), pero deja el modelo cambiado
por un clic accidental.

**Verificacion.** Compila: `NEURONiK_ModelMaker.vcxproj -> build-reference/Release/NEURONiK_ModelMaker.exe`
(solo los dos `C4996` preexistentes de `juce::Font`, en las lineas 55/61, ajenas al cambio). El clic
en si no es automatizable — es una app WIN32 y no hay arnes de GUI en el repo: lo que si queda fijado
por construccion es que la carga usa el MISMO camino ya probado del flujo de grabar.


## 2026-09-25 (ai) — El aviso de pitch inestable enseña tambien el residuo de la rejilla

**Que.** Tras ANALYZE, si la guardia dispara, el ModelMaker dice «Pitch inestable (N cents): el
modelo estatico quedaria des-afinado; usa mas frames  |  residuo M cents (K picos)».

**Por que las dos cifras juntas.** Son dos preguntas distintas y se leen mejor de par: la guardia
dice CUANTO se mueve el pitch (> 150 cents con la octava plegada, entre ventanas) y el residuo dice
DE QUE CLASE es el material (RMS en cents de los picos contra k*f0). Residuo bajo = el material SI
es una rejilla y el problema es el movimiento (la respuesta son frames). Residuo alto = el material
no es una rejilla (offsets, ruido, barrido), y mas frames no arreglan eso.

**De donde sale el numero (y por que NO se recalcula).** Se cita el ULTIMO residuo publicado —
`detectPitch` / `refineGrid` sobre ESTE mismo buffer—, el que la fila del indicador de rejilla ya
esta enseñando. Recalcularlo habria dado una SEGUNDA medida que puede discrepar de la primera
(`fitGridLeastSquares` publica el residuo de SU f0 refinada, no de la semilla que se le pasa): una
sola medida, dos sitios que la citan, y ninguna forma de que el aviso y el indicador se
contradigan. Si el dato no existe (`-1`) el aviso lo dice: «residuo n/d (material insuficiente)».

**Un defecto que salio al paso.** El aviso vivia en el header, con los MISMOS bounds que el nombre
del fichero (~220 px de ancho a 800): el texto ya se recortaba a media frase, y con el residuo
detras no habria cabido nunca. Ahora tiene fila propia de ancho completo —`area.removeFromTop (20)`
justo debajo del indicador de rejilla— y esa fila **solo se descuenta mientras hay aviso** (sin
aviso, `setBounds ({})`: el area de abajo no pierde un pixel). `performAnalysis` llama a `resized()`
cuando el aviso se enciende o se apaga —no en cada analisis—, y la fuente escala con el zoom igual
que la del indicador, que es el otro texto largo de esa banda.

**Verificacion.** `SpectralAnalyzerTest` §5(c) fija la invariante que sostiene el cambio, sobre un
barrido de una octava (220 -> 440 Hz, 2 parciales, 44,1 kHz):
`raiz 220.62 Hz | guardia 595.792 cents | residuo 284.7 cents (10 picos) | frames 1`, con tres
checks: (1) el analisis estatico da 1 frame y la guardia dispara; (2) el residuo **sigue publicado
tras `analyze()`** (>= 0 y con observaciones) — que es justo lo que el aviso necesita para poder
citarlo; (3) y cae en la banda ALTA (> 40 cents): el barrido no es UNA rejilla. Nota de API
encontrada al escribir el test: el analisis ESTATICO **no** toca `model.isValid` (solo lo declaran
`analyzeTemporal` y `buildLayeredModel`), asi que la validez del estatico se comprueba por lo que SI
declara — un unico frame. Compila `NEURONiK_ModelMaker.exe` (solo los dos `C4996` preexistentes de
`juce::Font`); `ctest` **32/32**. El texto en pantalla no es automatizable (app WIN32, sin arnes de
GUI): lo que queda fijado es el par de numeros y su procedencia.

## 2026-09-25 (aj): los rangos del residuo del banco CZ101, fijados en el ctest

El residuo que el indicador de rejilla del ModelMaker enseña —RMS en cents de los picos de todas las
ventanas contra la rejilla k*f0— nunca habia estado clavado en ningun test contra MATERIAL REAL: los
tests de rejilla (`LeastSquaresGridTest`, `SpectralAnalyzerTest`) usan senales sinteticas, y la sonda
RealWav imprime el numero para el ojo pero no lo juzga. Ahora si: `Tests/Cz101ResidualRangesTest.cpp`
recorre los CINCO WAV del banco hermano (`../ABDCZ101/DOCS/patches`) y cada uno declara su rango
medido, su banda y su guardia. Es la diferencia entre "el numero se puede mirar" y "el numero no puede
cambiar sin que alguien lo note".

**Lo que el banco dice** (medido el 2026-09-25, 1 canal @ 44,1 kHz, 1 frame):

| material | residuo | picos | banda | f0 | guardia |
|----------|---------|-------|-------|----|---------|
| CZ-BASS1 | 6,2 | 275 | verde | 124,31 | de pie |
| CZ-HAMOG | 4,5 | 379 | verde | 124,31 | de pie |
| CZ-PAD1 | 7,1 | 110 | verde | 124,68 | de pie |
| CZ-SWEP1 | 25,2 | 365 | amarillo | 124,60 | de pie |
| CZ-RRISE | 243,9 | 36 | naranja | 273,15 | 548 cents |

La lectura musical es la que justifica el test: los tres tonales (BASS1, HAMOG, PAD1) son UNA rejilla y
el residuo se queda por debajo de 8 cents; SWEP1 —fundamental debil con sub-octava— sigue siendo una
rejilla pero con estructura de sobra por debajo (el sub-armonico que la escalera no baja), y eso son 25
cents, amarillo; y RRISE es un barrido: no hay UNA f0 que sostenga los picos, el residuo se va a casi
244 cents y la guardia dispara (548), que es exactamente el caso en el que la UI enseña el aviso de
pitch con el residuo al lado.

**Una sola definicion de las bandas.** El test no re-declara los umbrales: `SpectralAnalyzer` gana
`static constexpr float residualGreenCents = 15.0f` y `residualAmberCents = 40.0f`, y
`MainComponent::updateGridIndicator` pasa a pintar con ELLAS en vez de con los literales 15,0f/40,0f. El
pincel del indicador y el test leen ahora el mismo numero; antes eran dos copias que podian separarse.
Consecuencia buscada: cruzar una frontera de banda (si algun dia SWEP1 baja al verde) es una DECISION
que hay que venir a declarar aqui, no un cambio que se cuela — que es justo lo que un test de banda
debe vigilar.

**Que comprueba cada material:** rango del residuo, banda, un minimo de observaciones (100/100/50/100/10
—PAD1 es el mas pobre en picos y RRISE el peor de todos, con 36—), f0 en rango audible, y el estado de
la guardia: de pie (0 cents) en los cuatro cuasi-monotonicos, disparando (> `pitchGuardCents`, 150) solo
en el barrido. El cruce de las dos cifras —residuo alto Y guardia disparada— es la invariante del aviso
de pitch: con material cuasi-monotonico la guardia esta de pie aunque el residuo suba.

**Disponibilidad del banco.** El material vive en el repo hermano, asi que CMake inyecta
`NEURONiK_CZ101_WAV_DIR` solo si `../ABDCZ101/DOCS/patches/CZ-BASS1.wav` existe; sin inyeccion la rama
`#else` imprime `[skip]` y sale 0 (ABDNeural se compila solo). Pero con la ruta inyectada los cinco
ficheros son OBLIGATORIOS: un WAV que falte es FAIL, no skip — la inyeccion ya significa "el banco
esta", y saltar material ausente esconderia justo lo que el test mide.

**Verificacion.** El binario imprime 26 checks `[OK]` con las cinco lineas de material y sale 0; la rama
de salteo se comprobo forzando el `#else` (compila, imprime el skip, sale 0) y se revirtio despues.
`ctest` **33/33** (el nuevo es el #19, 0,13 s).

## 2026-09-25 (ak): el distintivo del cajón de la MATRIZ, vivo (`setHeader`)

El cajón EDIT de la MATRIZ nace con `badge: '4 RUTAS'` —el inventario de la ficha— y con los defaults
del contrato (`mod1Source` = ENV 1, `mod2Source` = ENV 2, las otras dos en `Off`) ese literal ya mentía.
Ahora el badge es un DATO VIVO: la ficha declara `drawer.liveBadge` (`ids` = las cuatro FUENTES,
`label` = `RUTAS`) y el panel, en cada `paint`, cuenta las rutas ASIGNADAS —fuente distinta de la
primera opción de su lista, `Off`— y lo escribe con `setHeader` del `createDrawer` compartido. `setHeader`
es exactamente la pieza para esto: reescribe el dato (título/distintivo) **sin reconstruir el cuerpo**,
que es el contrato del cajón compartido. Los cajones inventario (2 ADSR, 4 LFO, 4 RANURAS, 8 GLOBAL) no
declaran `liveBadge` y conservan su literal: no cambian con el uso.

Detalle que costó una pasada: el índice `id -> view-model` para leer el `choice` tiene que salir de
`section.controls` —los DESCRIPTORES, con `options`—, no del array `controls` del panel: ese es la celda
ya montada y no guarda las opciones. Con el índice equivocado caían los 51 tests del panel con
`Cannot read properties of undefined (reading 'length')` en el primer paint.

Misma sesión, en el paquete compartido (`ABDSharedAssets`): el cajón `createDrawer` asume la gestión de
foco —al abrir el foco entra en el primer control del cuerpo (o el cierre), Tab/Shift+Tab ciclan dentro y
al cerrar vuelve al disparador, el `document.activeElement` de antes— y **cerrado va `inert`**: el
contenido sigue en el DOM (el contrato del cajon no se toca) pero sale del orden de tabulacion, porque
`aria-hidden` solo esconde de quien lo entiende y los controles seguian siendo alcanzables con Tab. Es lo
que hace que el EDIT de la MATRIZ sea usable con teclado sin que la página tenga que hacer nada.

**Verificacion.** `npx vitest run` en `WebUI`: **258/258** (21 ficheros; `tests/panel.test.js` 51, con el
caso nuevo que fija 2/4 -> 1/4 -> 2/4 moviendo las fuentes). `node Tests/webuiSelftestContractTest.mjs`:
OK (anclajes y ids de GENERAL). Ficheros: `WebUI/src/contracts/sections.js` (la declaración),
`WebUI/src/ui/panel.js` (`drawerSpecs`, `controlsById`, `liveDrawerBadge` y la escritura en `paint`) y
`WebUI/tests/panel.test.js` (el caso). Del lado compartido: `vitest` de `ABDSharedAssets` **589/589** (el
cajón suma su caso de `inert`), smoke ARIA en Chromium real **19/19** y, en la sonda de navegador,
cerrado dos Tab desde fuera pasan de largo del cajón y `focus()` sobre su contenido no lo mueve; al
abrir el foco entra en el primer control del cuerpo, Tab desde el último cierra el ciclo en el botón de
cierre y Shift+Tab lo deshace, y ESC lo devuelve al disparador con el `inert` de vuelta.

## 2026-09-26 (al): el residuo de rejilla, con UNA frase en las tres superficies del aviso de pitch

El aviso de pitch ya no habla distinto segun donde se lea. La frase del residuo esta ahora en el
analizador (`SpectralAnalyzer::gridResidualNotice()`), junto a la medida que cita, y la escriben las tres
superficies: la fila del ModelMaker (que ya lo citaba, con el texto montado a mano), el dialogo `Modelo
des-afinado` que BLOQUEA la exportacion del estatico y el reporte de la sonda RealWav. Antes: la fila
decia "residuo 243,9 cents (36 picos)", el dialogo no citaba el residuo (solo la desviacion) y la sonda lo
pintaba en su propia linea con otro formato (`residuo=243.9 cents obs=36`), que ademas enseñaba
"residuo=-1.0" cuando el material no daba para medir. Ahora las tres dicen lo mismo, incluido el caso sin
datos: "residuo n/d (material insuficiente)".

Medido con WAV reales del banco CZ101 (sonda): CZ-RRISE "guardia: desviacion de pitch 548 cents (material
no cuasi-monotonico) | residuo 243.9 cents (36 picos)" y su linea de rejilla con la MISMA frase; CZ-PAD1,
sin guardia, "residuo 7.1 cents (110 picos)" — las mismas cifras canonicas del ticket (aj) (243,9 / 36 y
7,1 / 110).

**Verificacion.** `NEURONiK_SpectralAnalyzerTest` imprime `RESULT: OK (0 fallos)` con dos checks nuevos
que fijan la frase: la del barrido ("residuo 284.7 cents (10 picos)") y la de un analizador sin material
("n/d"). La GUI (`NEURONiK_ModelMaker`, con el dialogo nuevo) y la sonda compilan sin errores nuevos, la
sonda corre sobre los dos WAV de arriba y `ctest` sigue **33/33**. Ficheros: `SpectralAnalyzer.h`/`.cpp`
(la frase), `MainComponent.cpp` (la fila del aviso y el dialogo) y `Tests/ModelMakerRealWavProbe.cpp` (las
dos lineas del reporte) y `Tests/SpectralAnalyzerTest.cpp` (los dos checks).

## 2026-09-26 (am): la huella del modelo de 25 KB, medida y con presupuesto

FASE 11.1 llevo `SpectralModel` de ~8 KB a ~25 KB, y con el se midio por primera vez lo que de verdad
cuesta. El resultado incomoda un poco: **el modelo no se guarda una vez por motor, se guarda ocho veces
por voz**. `Resonator` y `ResonatorBank` llevan cada uno `std::array<SpectralModel,4> models` (los slots
A-D) y `std::array<SpectralModel,4> frameCache` (el frame muestreado): 8 x 25 032 B = 195,6 KB dentro de
una voz que mide 207 392 B (`AdditiveVoice`) / 208 784 B (`NeurotikVoice`). El modelo es el 94% de la
voz.

Y la polifonia NO manda en el heap: los dos motores pre-asignan **32** voces en su constructor y
`setPolyphony()` solo mueve `activeVoiceLimit` (jlimit 1..32), sin liberar ninguna. Con polifonia 16,
los 32 objetos siguen ahi:

| pieza | talla | notas |
|---|---|---|
| `SpectralModel` (1 slot) | 25 032 B (24,4 KB) | 3 capas x 16 frames |
| `Resonator` / `ResonatorBank` | 206 592 / 208 272 B | 8 modelos cada uno |
| `AdditiveVoice` / `NeurotikVoice` | 207 392 / 208 784 B | 8 modelos + osciladores + estado |
| motor preparado (32 voces) | 6,46 / 6,37 MB | 48 kHz; incluye FX y jitter por voz |
| cola de comandos (32 modelos) | 801 024 B | en el HEAP (Fase 11.1) |
| `NEURONiKProcessor` (pila) | 34 096 B | la cola ya no vive aqui |
| `NEURONiKProcessor` con 16 voces | 7,20 MB heap | un motor + cola + managers |

Medido con un contador propio de bytes VIVOS sobre `operator new`/`delete` (cuenta `new`/`delete`, que
es por donde pasan `std::vector` y los `make_unique` de las voces; `juce::HeapBlock` no lleva modelos y
queda fuera). La prueba mide con el objeto AUN VIVO: la primera version medía al volver de una lambda y
daba cero, porque el motor ya se habia destruido.

**Presupuesto (y su guardia).** `Tests/MemoryBudgetTest.cpp` —nuevo, `NEURONiK_MemoryBudgetTest`, Test
#15 en `ctest`— fija el techo por slot (26 KB), por voz (224 KB), por motor (7,5 MB) y por procesador
(8,5 MB), e imprime la medida exacta al lado. La talla de compilacion la guardan `static_assert`
(modelo, voces, `sizeof(NEURONiKProcessor) < 128 KB`), y hay dos `static_assert` de coherencia del propio
presupuesto (8 modelos deben caber en una voz; motor + cola deben caber en el procesador). Verificado:
test en verde y `ctest` **34/34**.

**Lo que deja apuntado, sin tocar.** Dos reducciones evidentes, ninguna hecha: (1) reservar voces
perezosamente hasta `activeVoiceLimit` en vez de 32 fijas —recortaria ~3,2 MB por motor con polifonia
16—; y (2) que el `frameCache` de una voz no sea un modelo entero sino un frame muestreado (amps +
offsets + procedencia), que es lo que de verdad guarda: ~200 KB por voz caerian a ~3 KB. Cualquiera de
las dos mueve el presupuesto, y por eso la prueba esta antes: para que el cambio se vea en un numero.

## 2026-09-26 (an): las voces del motor, perezosas (16/8 en vez de 32 fijas)

Los motores traian 32 voces creadas en el constructor para CUALQUIER polifonia: `setPolyphony()` solo
movia `activeVoiceLimit` y no liberaba nada, asi que pedir 16 pagaba 32. Con el modelo en 25 KB eso eran
~6,4 MB por motor que no se usaban.

Ahora la reserva es PEREZOSA. `BaseEngine` gana `ensureVoices(count)` (crece la lista hasta `count`,
preparando la voz nueva si el motor ya estaba preparado) y `createVoice(index)` virtual en cada motor (la
neurotik usa el indice como semilla del ruido); el constructor reserva la capacidad FIJA
(`voices.reserve(kMaxVoices)`), asi que crecer hace `push_back` sin reasignar el buffer que el hilo de
audio ya indexa. El constructor de cada motor llama `ensureVoices(activeVoiceLimit.load())` —16 la
aditiva, 8 la neurotik—; `setPolyphony(n)` crece ANTES de publicar el limite (la invariante
`size() >= limit` tiene que estar puesta cuando el audio lea el limite) y `prepare()` reserva lo que falte.
`getNumAllocatedVoices()` publica el numero para el test.

Cuidado con el hilo de audio: la reserva la hace el hilo de mensajes —constructor, prepare y
`setPolyphony`—, nunca el de audio; el procesador envuelve `engine->setPolyphony` en el mismo
`ScopedLock(getCallbackLock())` que el cambio de motor. BAJAR devuelve SOLO las ociosas
(`!isActive`): las que aun sueltan cola se compactan al frente (ver
`BaseEngine::reclaimIdleVoices`) y la capacidad en caliente (`voices.reserve(32)`) queda
intacta, solo baja `size()`; volver a subir solo re-crea (prepare si ya estaba preparado).

Medido (MemoryBudgetTest, seccion 3):

| pieza | antes (32 fijas) | ahora (perezosa + bajar ociosas) |
|---|---|---|
| motor aditivo, limite 16 | 6,46 MB | **3,23 MB** |
| motor neurotik, limite 8 | 6,37 MB | **1,60 MB** |
| motor al techo 32 | 6,46 / 6,37 MB | 6,46 / 6,37 MB (peor caso igual) |
| `NEURONiKProcessor`, polifonia 16 | 7,20 MB | **4,04 MB** |
| `NEURONiKProcessor`, polifonia 32 | 7,20 MB | 7,20 MB |

Bajar con voces sonando respeta la cola: bajar a 8 con 3 activas deja 8 (ociosas devueltas,
activas intactas); bajar a 2 con 3 activas deja 3 (`max(limit, activas)`); con 0 activas
bajar a 1 deja 1 y la reserva en caliente 4—32 queda en 32 (capacity 32 intacta, volver a subir
solo pushea). La pendiente por voz es ~206,5 KB (el modelo x8, ver la entrada anterior).
Verificado: `NEURONiK_MemoryBudgetTest` en verde con la seccion 3 (16/8 al nacer, 32 al subir,
4 tras bajar a 4 sin notas, bajar con activas, reserva caliente y pendiente por voz) y `ctest`
**35/35** (el 34 anterior era sin el test de la bajada ociosa). Ficheros:
`Source/DSP/BaseEngine.h/.cpp` (tri-rama `setPolyphony` + `reclaimIdleVoices`), `Source/DSP/CoreModules/NeuronikEngine.h/.cpp`,
`Source/DSP/CoreModules/NeurotikEngine.h/.cpp`, `Source/Main/NEURONiKProcessor.cpp` (cerrojo) y `Tests/MemoryBudgetTest.cpp`.

## 2026-09-26 (ao): modulo WASM reconstruido y worklet sincronizado

La entrada 2026-09-24 (t) dejo apuntado que el binario WASM versionado en `WebUI/public/worklet` no se
habia regenerado tras FASE 11.1. Ya lo esta: `build_wasm.bat` recompilo el DSP real (emscripten + Ninja)
y corrio su cadena entera —referencia nativa, paridad Node, smoke y `sync-wasm.mjs`— en una pasada.

**La evidencia de que el struct de capas no movio el comportamiento** no es que la paridad de verde (el
test regenera la referencia y compara contra ella, asi que eso solo prueba nativo == WASM el mismo dia):
es que la referencia nativa REGENERADA es **byte a byte la misma** que la de ayer.

| artefacto | antes | ahora |
|---|---|---|
| `build-wasm/parity-native.json` | md5 `bd80f767…` | md5 `bd80f767…` (identico) |
| `build-wasm/neuronik_dsp.js` | md5 `d529f87b…` | md5 `d529f87b…` (identico) |
| `build-wasm/neuronik_dsp.wasm` | 100 729 B, md5 `593caeb3…` | 101 416 B, md5 `7baa5b40…` (+687 B) |

El `.wasm` cambio de TALLA (el struct de capas es mas grande, y eso mueve el layout de BSS/codigo), pero no
de comportamiento: 9/9 casos de la matriz A-E a **0 ulps** (184 320 muestras, 3 tasas x 3 tamanos de
bloque), la referencia nativa intacta y el smoke con audio (`peak=0.53199 finite=true`). Es exactamente lo
que anticipaba 11.1: el worklet usa bloques fijos de 128 floats y no depende de `sizeof`.

`WebUI/public/worklet` queda **sincronizado**: `neuronik_dsp.wasm` (101 416 B) y `neuronik_dsp.js` tienen
el mismo md5 en `build-wasm/` y en el worklet. `neuronik-worklet.js` es el pegamento de la pagina y no lo
toca el sync. Los tres estan versionados en git y salen como modificados. `ctest` **34/34**.

## 2026-09-26 — la metrica del clustering, seleccionable (y el coseno, medido)

La desviacion de 11.2 estaba documentada pero no era ELEGIBLE: el coseno de la seccion 3.3 del plan
vivia como `envelopeCosine()` solo para que el test lo midiera. Ahora `LayerClustering.h` declara
`enum class LayerMetric { Descriptors, EnvelopeCosine }` y la afinidad entra como parametro: en
`clusterTraces(traces, numTraces, numFrames, floor, metric)` y en
`analyzeTemporal(audio, sr, rootFrequency, frameCount, fixedGrid, layerMetric)`. La ultima llamada la
publica `SpectralAnalyzer::lastLayerMetric()` (tambien cuando la puerta de plegado exime el material y
el clustering no corre: el test lo pinnea). El punto de eleccion es UNO —la lambda de afinidad del
aglomerativo—, asi que media, medoide, clamp a `kMaxLayers` y guardia de degeneracion son los mismos
con las dos metricas.

Medido en la seccion E nueva del test de trazas, el coseno pierde en los dos juegos de datos: en las
SINTETICAS del criterio de aceptacion (voz en 2 de 8 frames) su afinidad con el drone es **0.49 <
0.55**, no fusiona nada, y el clamp a 3 + la guardia colapsan en **1 capa** donde los descriptores
dan las 2 correctas; en las REALES de 9 ventanas del RRISE fusiona de mas (drone y n7, **0.69 >= 0.55**)
y tambien da 1 capa; con el audio de punta a punta, 1 capa frente a 2. El coseno no falla por su
forma —separa n7 de n15, de soportes disjuntos, con coseno 0.09— sino por su ceguera al SOPORTE: una
traza plana correlaciona con todo lo que dure parte del fichero, y lo que no correlaciona tampoco
llega al corte. El defecto sigue siendo `Descriptors`; lo nuevo es que ahora esta DEMOSTRADO, no
argumentado. Verificado: `NEURONiK_LayerClusteringTest` OK (**74 checks**, 0 fallos) y `ctest` **34/34**.

## 2026-09-26 — rejillas entrelazadas en la sonda: el parcial 4 de SWEP1 no es un error de 212 cents

La validacion acustica del probe (`ModelMakerRealWavProbe`, paso 4.5) comparaba cada parcial fuerte
del modelo contra el pico real de su banda. En material construido sobre una sub-oscilacion (el CZ
vive en 62/124) el pico real de esa banda puede ser un IMPAR de f0/2: la otra familia de la fuente,
que ninguna rejilla k*f0 representa — el parcial 4 de SWEP1 (493,7) mide 436,5 porque el pico real
es 436,1 = 7*62,3. Eso no mide la fidelidad del modelo; mide la riqueza de la fuente.

**Diagnostico**: firma de entrelazado (pico a menos de un cuarto de f0 de un impar de f0/2 y a mas
de un cuarto de su armonico nominal), la linea del parcial lo explica con el impar exacto y deja de
alimentar el maximo. **Decision sobre el check acustico: no se exime el material — se corrige la
contabilidad.** La mediana contractual se toma sobre la familia que la rejilla representa (menos de
3 propios ⇒ sobre todos, y el aviso lo dice), los entrelazados siguen contando en el minimo de 3
validados y las guardias quedan intactas: barrido exento (RRISE) y `HALVE_F0` dispara el fail de
sub-octava en mono-familiar (PAD1). Medido en los cinco WAV del banco: SWEP1 max 212,9 -> **16,6**,
BASS1 (tambien bi-familiar: impares 7/11/15/19 de 62,2 Hz) 229,0 -> **8,0**; medianas sin mover
(BASS1 -3,4 / HAMOG -1,6 / PAD1 -0,1 / SWEP1 -4,5); `RESULT: OK` en los cinco.

## 2026-09-26 — el pad XY, el anillo morphZ y las ranuras, sin host (modo local del navegador)

Dos caminos faltaban para que la pagina sonara sola de verdad. (1) EL MORPH NO LLEGABA AL DSP POR
NINGUN CAMINO: morphX/morphY/morphZ son VoiceParams (no GlobalParams) y el puente WASM no tenia
export que los tocara — en el plugin los escribe synchronizeEngineParameters desde el APVTS; el
worklet no tiene APVTS. Ahora: `setMorph(x,y)` / `setMorphZ(z)` en los dos motores (read-modify-write
de pendingVoiceParams, el mismo canal RT-safe de siempre) y `neuronikSetVoiceMorph(x,y,z)` en el
puente, con clampeo a [0,1] en la frontera. (2) LAS RANURAS SOLO SE LLENABAN VIA HOST: `loadModel`
pide el dialogo nativo y sin host nadie contesta modelsState. Ahora el store acepta la via local —
`loadModel(slot, { requestLocalFile })` pide el fichero al input oculto de app.js,
`loadLocalModel()` lo parsea al MISMO shape de modelsState (src/audio/localModels.js; un v2.1 de
capas avisa «(capa 0)» en el nombre: el v1 del puente suena la raiz) y el estado `models` viaja al
worklet por `neuronik:models`, el canal que ya existia. La ficha RANURAS habilita CARGAR sin host
con `localModelReady`; sin host y sin camino local sigue deshabilitada (no se finge nada).

El botón SOUND ON sincroniza modelos y morph al arrancar y al cambiar de motor: el motor nuevo
despierta en los defaults del struct (0.5/0.5/0) y la pagina re-aplica su posicion
(pushMorphToWorklet/getWorkletMorph en audioWorkletEngine.js; case `neuronik:morph` en el worklet).

De regalo, el meter del worklet destapo un TDZ real del arranque: `modRings` vivia dentro del bloque
de montaje y el primer frame (~48 Hz) llegaba antes que la declaracion (`modRings is not defined`).
Sube a nivel de modulo en app.js.

Verificado en Chromium real (vite :5199, sin __JUCE__): SOUND ON -> badge «AUDIO: motor local del
navegador (WASM) · ON · 48.0 kHz» con el binario nuevo (101 751 B); CARGAR en A con un
.neuronikmodel generado al vuelo -> «SMOKE-BASS, 1/4 cargados»; nota 48 -> «1 voz activa» y 0 al
soltar (el modelo suena); pad -> readout X 85% / Y 85% y morphX/morphY nuevos en el store que
syncEngine cruza al motor. Paridad WASM reconstruida: 0 ulps en 15 escenarios, referencia nativa
byte a byte la misma (bd80f767...), worklet sincronizado (mismo md5), ctest 34/34 y WebUI 286/295
(los 9 rojos: lcdTop/sections, edicion en vuelo de otro agente).

## 2026-09-26 — fase 11.3 en el camino WASM: el motor del navegador suma las capas

La suma de capas existia en nativo (sampleLayeredFrame, 11.3, ctest verde) pero el navegador
no la tenia: neuronikLoadModel (v1) solo cruza 128 floats planos y clava layerCount = 1, asi
que en modo local un modelo de 2 capas sonaba su raiz y callaba la capa 1. Como loadModel
REEMPLAZA el struct del slot, cargar la capa extra en una segunda llamada habria BORRADO la
raiz: la frontera nueva es UNA llamada. Export `neuronikLoadModelLayers(slot, engineType,
data, isValid, extraData, layerFrames, layerWeight, frameWeights)`: raiz por memcpy del layout
v1 + capa 1 desde extraData con layout {amps[64], offsets[64], frameF0} x layerFrames (193
floats/frame) + pesos temporales al final, peso estatico como escalar; clampeo a kMaxFrames
(16); sin capa extra queda layerCount = 1 (camino v1 intacto, paridad 0 ulps).

Cadena completa: el parser local (localModels.js, extractExtraLayer) valida el v2.1 amplitud a
amplitud (no finito -> 0; capa rota -> no se anuncia; capas 2+ truncadas documentado; el
nombre ya NO lleva el aviso «(capa 0)»), el shape viaja por neuronik:models y el worklet
serializa la capa 1 en su scratch nuevo (3*64*16+17 floats) para llamar al export; ante
cualquier duda cae al v1. Test nativo del export en NEURONiK_LayerEngineTest seccion 6 (layout
reconstruido == sampleLayeredFrame directo bit a bit; morphZ2 mueve el offset al frame
elegido); ctest 34/34.

E2E real (node contra build-wasm/neuronik_dsp.wasm, modelo CZ-SWEP1-temporal, nota 48, z2=0):
el parcial n14 (1831.3 Hz, SOLO capa 1, amp 0.433 en frame 0) pasa de energia 2.90e-4 (v1) a
4.36e-3 con capas (ratio 15.1x, umbral 3x) y el RMS global sube de 0.80067 a 1.13986 (+42%).
Primera pasada en rojo por probe mal elegido: se escogio n4 por amplitud maxima GLOBAL pero n4
es 0 en el frame 0 (solo vive en frames 1-3) y lo que se media era leakage (ratio 1.8).
Leccion: el probe del E2E de capas se elige por amplitud EN EL FRAME 0 que suena (z2=0), no
por el maximo del modelo entero. WASM reconstruido: 102 857 B, paridad 0 ulps (96000 Hz,
bloques 64/128/512), referencia nativa byte a byte la misma (bd80f767...), worklet
sincronizado (mismo md5). Vitest de la rodaja local: localModels + workletMorph + modelSlots
= 31/31; WebUI 286/295 (los 9 rojos: lcdTop/sections, edicion en vuelo de otro agente).

## 2026-09-26 — fase 11.4: cada capa, su propio volumen (el aro mezcla capas)

La 11.3 dejo cada capa con su propio eje z, pero el VOLUMEN lo fijaban solo los pesos
estaticos del fichero: la pagina no podia mezclar capas, solo recorrerlas. Nueva ganancia por
capa: LayerGains (0..1, default 1.0 = bit-exacto) escala cada capa DESPUES de muestrearla en
sampleLayeredFrame (sobrecarga de 4 args; la de 3 delega con ganancias en reposo), y el dueno
del offset/f0 por indice se decide sobre amplitudes ya escaladas (apagar una capa cede el
mando del offset a la vecina: es lo que se oye). Camino completo hasta la pagina:
setLayerGain en Resonator/ResonatorBank (layerGains entra en los latches lastLayerZ/
lastConsumedZ del cache), layerGain2/3 como VoiceParams con smoothers de 20 ms clonando el
camino morphZ2/3, setVoiceLayerMorph(g2,g3) virtual en BaseEngine con read-modify-write de
pendingVoiceParams en los dos motores, export neuronikSetVoiceLayerMorph (clampeo a [0,1] en
la frontera) y el mensaje neuronik:morph del worklet llevando z2/z3 (undefined = sin cambio;
el motor conserva). En local, syncEngine empuja los volumenes del store (morphZ2/morphZ3 del
contrato generado, ya existian) junto al morph, y el re-apply al cambiar de motor los
recuerda. El ARO del pad emite los TRES z en cada fase (begin/change/end, puntero y teclado):
cada capa sigue su propia linea de frames y la suma ES la mezcla.

Semantica medida y documentada: el resonador NORMALIZA la suma de parciales (invNorm), asi
que la ganancia de capa REPARTE el espectro (drawbars), no es un master de la capa: al callar
la capa 1 (g2=0), n14 (1831 Hz, solo capa 1) cae 4.92e-3 -> 1.44e-3 (3.4x) y la raiz n1 sube
0.364 -> 0.539. El E2E (_t58_e2e_gains.mjs) aprendio dos lecciones de higiene: AllNotesOff
ANTES de cada render (las voces se acumulan sin noteOff) y purga de 200 bloques despues (la
cola de release ~500 ms de la medida anterior contamina la ventana siguiente). Verificado:
LayerEngineTest seccion 7 (el camino de 3 args es el de ganancias en reposo bit-exacto;
ganancia 0 apaga SU parcial sin tocar la raiz; 0.5 conserva el offset del z2), ctest 34/34,
WASM reconstruido (105 192 B, paridad + smoke, sync al worklet) y vitest 299/299 con el
contrato del aro pineado en los tres z.

## 2026-09-26 — el panel CAPAS ensena el veredicto de la puerta de plegado (mono/bi + cents)

La pestana CAPAS del ModelMaker (11.5) ensenaba COMO se repartio el material pero no POR QUE la
puerta de plegado dejo pasar (o exento) el clustering. Ahora el veredicto viaja DENTRO de la
vista: LayerView::FoldVerdict (measured, biGrid, foldCents, rawCents, observations,
octaveFlips), rellenado por la sobrecarga buildLayerView(model, analyzer.lastOctaveFold()) —
la MISMA measureOctaveFold (10.3) que consume la sonda y decide el clustering, cero politicas
nuevas en la vista. paintLayerView gana una fila sobre la leyenda: bi-rejilla (ambar: cents
post-plegado y crudos, "exento de clustering") o mono-rejilla (verde suave: cents, saltos de
octava del estimador plegados a acuerdo, ventanas). Honestidad ante todo: sin ventanas
utiles, o en analisis estatico (la puerta solo vive en analyzeTemporal), la fila dice "sin
medida" — la vista de un argumento (el legado) tampoco declara veredicto.

Pineado en NEURONiK_LayerViewTest seccion 4 (5 checks nuevos): mono medido (4 ventanas bajo el
corte), bi con la quinta (701.96 -> -498.04, sobrevive al plegado), sin ventanas utiles y el
camino de 1 argumento SIN veredicto. El target gana SpectralAnalyzer.cpp + juce_dsp/
juce_audio_basics (el test ya llama a measureOctaveFold). ctest 34/34; el ModelMaker compila
con la fila nueva.

## 2026-09-26 — el giro del anillo morphZ con LFO2, comprobacion manual -> E2E pineado

La comprobacion manual en Chromium (LFO 2 -> destino 28, arco de .zring-mod, periodo ~1,07 s
a 1,0 Hz) era la unica guardia de esa ruta. _t59_e2e_zring.mjs la convierte en E2E contra el
mismo binario build-wasm/neuronik_dsp.wasm: escena por el MISMO canal que la pagina (mirror
de GlobalParams = layouts base 22 campos + matriz 12 concatenados, neuronikSetGlobalParams),
ruta Ruta 1 = LFO 2 (source 2) -> Morph Z (dest 28) con amount 1.0, y lectura de
neuronikGetMod(28) por bloque de 128 — la magnitud que la pagina pinta en el arco. Medido:
periodo 1.000 s a 1.0 Hz y 1.666 s a 0.6 Hz (teorico 1.6667), barrido ±1.000 = amount (el
seno entero, no solo la semionda positiva: neuronikGetMod entrega la contribucion CON signo;
el recorte a 0 lo hace la pintura con la base a 0), control A/B/A con fuente Off (arco
0.000000 exacto) y reanudacion al volver a LFO 2.

Dos lecciones de cable, ahora documentadas en el test: (1) sin neuronikSetEngine no hay
motor y TODO mide 0 — las demas sondas ya lo llamaban, esta lo destapo al medir telemetria;
(2) el mirror a ceros deja lfo2.rateHz=0 y lfo2.depth=0 y el LFO no corre: la escena debe
escribir rate/depth/waveform como hace la pagina. Bug de la propia sonda en el camino: las
vistas del mirror se construian con gpPtr>>2 (byte offset como word) y escribian en el sitio
equivocado — corregido a byteOffset en BYTES. Sin codigo de produccion tocado; ctest 34/34
y vitest 299/299 siguen siendo la red (esta prueba es de la pagina real, no del DSP).

## 2026-09-26 — ZRING: el anillo morphZ por el camino nativo (telemetria frame.modulation[28])

El motor local del navegador ya tenia su E2E (_t59_e2e_zring.mjs); faltaba la otra mitad: el
camino NATIVO del plugin. La direccion ZRING del selftest del puente (BridgeSelftest.h, la
octava, corre tras MORPH y es quien cierra el veredicto) monta la ruta por el APVTS —LFO 2
(fuente 2) -> Morph Z (destino 28), amount 1.0, rate 1.0 Hz, depth 1.0, sync Free explicito—,
el motor REAL del plugin la aplica en su render de audio y la telemetria nativa
(frame.modulation[28] -> setZMod) pinta el arco .zring-mod. La colecta: evaluate siacrono de
UNA instantanea del span del arco, PACED con afterDelay(30) y con marca de tiempo de reloj de
pared; el periodo sale de cristas locales >= 70% del maximo, DEDUPLICADAS por meseta (el arco
se cuantiza a guiones enteros: 66 ms de telemetria frente a ~1 us de subida -> el maximo se
repite) y separadas >= 350 ms. Medido en la bancada WebView2: periodo 1002.5 ms (esperado
~1000), max 100 guiones, fuente Off -> 0.0 exacto, A de nuevo -> 99.

Tres lecciones de la bancada, en el propio fichero: (1) el evaluate es SINCRONO y responde en
~1 ms — sin pacing la colecta cubria 200 ms de arco y la ventana de reanudacion moria antes
de que la primera telemetria llegara a la pagina; (2) el RANDOM de la direccion ACCIONES
sortea lfo2SyncMode: sin fijar Free, el rate de 1.0 Hz se ignora (manda bpm/division) y el
LFO corre lentisimo; (3) el timeout del arnes paso de 30 a 90 s: una colecta paced de 240 +
24 + 140 tomas mas las esperas son ~13 s de ZRING sola. Compila y corre en la bancada;
ctest 34/34 intacto. Nota del turno: AGUJA (otra direccion nueva, en vuelo de otro agente)
salia FAIL por SU cuenta ("SIN NIVEL") y dejaba el RESULT global en FAIL; ZRING media OK.

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida esta dirección
> (ZRING, la 14 y ULTIMA del arnés, con su subseccion propia). Esta entrada cuenta como se llego a
> ella. Sus numeros estan SUPERADOS por la canon y no conviene reusarlos tal cual: la ruta se pone
> con los indices DERIVADOS de las tablas de contrato (`getModSources().indexOf("LFO 2")` y
> `destinationIndexFor(IDs::morphZ)`), no con los literales "fuente 2"/"destino 28" que se leen aqui,
> y el gate del periodo es 300..800 ms entre CRESTAS (no ~1000): el arco CON SIGNO culmina dos veces
> por periodo, asi que a 1 Hz lo correcto son ~500 ms. El margen y su por qué estan en el
> constructor del arnés, junto a las constantes.

## 2026-09-27 — el arco del anillo morpheZ con SIGNO: la semionda negativa se ve

renderZ (WebUI/src/ui/xyPad.js) pintaba el arco de modulacion de la base al EFECTIVO clampeado
0..1 (la misma matematica de la voz), asi que con morphZ a 0 la semionda negativa de un LFO
era invisible: el clamp la dejaba a 0. Contrato nuevo: el arco NACE en la base y corre en el
sentido del signo de la contribucion — horario si suma (start = base), ANTIHORARIO si resta
(start = base - span, mod 100: TERMINA en la base) — y SIN recorte: span = |contribucion|
(cap 100) y si pasa de la vuelta envuelve por las 12 (el dash de un circulo cerrado ya
envuelve). Lo que ensena el arco es LA CONTRIBUCION; el efectivo lo sigue clampeando la voz.
Vitest 300/300 con los seis tests del arco re-pineados y uno nuevo: morphZ=0 + setZMod(-0.95)
-> offset -5, span 95 (el caso pedido); 0.9+0.5 -> 50 guiones envolviendo; negativa con base
0.25 -> termina en la base.

La direccion ZRING de la bancada se actualizo al contrato nuevo: el colector lee TAMBIEN el
start (=-dashoffset), el periodo se mide ahora sobre |sin| — el arco culmina 2 veces por
periodo: **497.5 ms** a 1.0 Hz (umbral adaptativo de cristas 35% del maximo cuando el arco es
grande, mesetas separadas >= 250 ms) — y la validez exige la semionda negativa VISTA:
instantaneas con start en (5, 95) = lado antihorario. Medido en la bancada: 497.5 ms, max 100,
**182/480 instantaneas antihorarias**, Off -> 0.0, A de nuevo -> 100. Ventanas de muestreo al
doble (480/280 tomas) para seguir viendo >= 2 periodos. Leccion de bancada: el selftest sirve
WebUI/dist (el bundle), no src — sin `npm run build` la direccion media la pagina vieja y el
arco salia sin signo (spans 0/100 alternados, starts a 0). RESULT global del run sigue en
FAIL por AGUJA ("SIN NIVEL"), direccion en vuelo de otro agente; ZRING y el resto miden OK.








## 2026-09-27 — el indicador de rejilla del ModelMaker es un MODELO PURO con su test

El texto de la fila "Rejilla: f0 ... | residuo ... | N picos", su color por bandas del
residuo y lo que el clic carga en el editor de pitch salieron de
`MainComponent::updateGridIndicator` a `Source/ModelMaker/Analysis/GridIndicator.h`, un
modulo PURO sin JUCE: `GridIndicatorModel` (entradas: f0, residuo, picos, FIJA,
TRANSPONIBLES, bajo el suelo del estimador; salidas: `band()`, `argb()`, `text()`,
`clickLoadsHz()`, `clickEditorText()`). Las constantes de las bandas (verde 15.0, ambar
40.0 cents) viven ahi y `SpectralAnalyzer` las aliasa — el test de rangos del banco
CZ101 y la UI siguen leyendo el mismo numero de la misma fuente. La GUI solo RECOGE la
entrada y PINTA: el bloque que anadia los avisos a mano quedo fuera (viajan dentro del
texto del modelo, en el orden de siempre). El clic queda descrito por el modelo: f0 SIN
cuantizar (64.50 no es 65.41 de C2) y texto del editor a 2 decimales. Test propio sin
JUCE (`NEURONiK_GridIndicatorTest`, 28 checks: bandas y fronteras inclusivas, RGBA
exactos, texto exacto aviso a aviso, clic con y sin material). ctest **35/35** (el
34 anterior era sin el test nuevo).

## 2026-09-27 — el indicador de rejilla se alcanza y acciona con el TECLADO

Tab enfoca la fila "Rejilla: f0 ..." y Space/Enter actuan el mismo camino que el clic
(keyPressed -> useDetectedFrequency; el guard de material vive ahi, asi que sin f0 la
tecla no hace nada — igual que el clic). El foco SE VE: paintOverChildren dibuja un
anillo blanco redondeado alrededor de la fila solo mientras la tiene, un
globalFocusChangeListener (GridFocusRingCallback, nuevo en MainComponent) repinta al
entrar y al salir, y se da de baja en el destructor antes de destruir miembros. Sin
material cargado el indicador sale del ciclo de Tab (setWantsKeyboardFocus false: un
activable sin accion no roba un paso de teclado) y vuelve a entrar en cuanto hay f0.
Tooltip actualizado: "Clic o Espacio/Enter: cargar la f0 detectada en el editor de
pitch". Build ModelMaker OK (solo los C4996 de siempre), ctest **35/35** y arranque
de bancada OK.

## 2026-09-27 — commit 157fa16: AGUJA (la octava direccion), build.bat nopause y las causas raices

La jornada que cierra `157fa16` anade al arnes del bridge la direccion AGUJA (Stage::needle,
"1c" del plan): el motor SUENA (nota 60 por el teclado de la pagina) y los niveles REALES de
las dos envolventes (envelopes[amp,filter] del frame de telemetria) pintan como AGUJA sobre
las curvas ADSR, en las DOS vistas (lienzo y cajon). Sondeo acotado (patron ZRING, tomas
cada 30 ms): silencio (ocultas) -> nota (visibles y coherentes con el motor) -> release
(ocultas, o cola en descenso coherente con el motor). El criterio de SOSTENIDO es la
COHERENCIA pagina<->motor (minimo desfase |pagina-motor| sumado de las dos agujas, leido EN
la misma toma; margen 0.08), no umbrales absolutos: la ADSR de la SESION manda (su sustain
real vivia a 0.12) y en la bancada la telemetria de la pagina llega 1-2 periodos tarde.
Con ella, finish() exige las ocho direcciones. AGUJA OK en plugin y bancada.

Causas raices que salieron en el E2E (todas muertas, con evidencia en el log de la corrida):

1. La regex que lee el 'd' de la aguja (`M0,y Ly,y`) iba con backslashes DOBLES en el
   literal C++ (cuadruplicados por dos capas de escape al editar): nunca casaba y el
   "nivel" salia del clamp disfrazado (y:-1 -> 1). Los diagnosticos crudos (el outerHTML
   del nodo viajando en el JSON de la lectura) la cazaron.
2. Los umbrales absolutos (>0.4 / >0.2) eran el criterio equivocado: pagaban la ADSR de
   fabrica, no la de la sesion del usuario.
3. El "SE QUEDARON" del release era la COLA LARGA de la ADSR de sesion descendiendo
   honestamente (0.94 -> 0.11 en 3 s): el frame final de nivel 0 SI viaja (la aguja del
   filtro se ocultaba sola). Descartadas por evidencia la hipotesis del epsilon del diff
   del puente y la del meter WASM empujando [0,0].
4. Dos builds fallaron EN SILENCIO (C2001 "nueva linea en constante" por un escape) porque
   `cmake --build ... | tail` enmascaraba el exit code: regla nueva, comprobar
   `EXIT=${PIPESTATUS[0]}` y la fecha del exe tras cada build.

`build.bat` queda automatizable: flag `nopause` (misma convencion que build_wasm.bat) y
corregida la comilla colgada del parseo `release` (`set "MM_RELEASE=1""` corrompia la linea
siguiente). La pasada completa corre sola y deja build-last-run.log: 35/35 ctest, paridad
WASM bit-exacta 9/9 y selftest del plugin veredicto OK.

El commit trae ademas el trabajo de las sesiones concurrentes del mismo arbol (capas fase
11.x con clustering/LayerView/GridIndicator, banco CZ101-BANK embebido, ZRING del anillo
morphZ, SpectralModelWriter, regenerados WebUI/generated + worklet). Quedo FUERA y sigue
pendiente de commit: el boton VOLVER A LA RUTA (routeBack.js + panel.js + CSS + 5 tests;
305/305 WebUI, selftest OK) y los scripts de trabajo de agentes, purgados con git clean e
ignorados desde .gitignore (`/_*.py`, `/_t*.mjs`, `/_e1_*`).

Nota para la siguiente sesion: ZRING en la bancada se queda sin CPU (NO_RING a porrillo,
dt medio 87 ms frente al periodo esperado ~500 ms) y falla o agota su timeout aunque en el
plugin pasa 2 de 2; le falta margen de periodo y presupuesto de tomas para hosts justos.

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida AGUJA (la 6, con sus
> CUATRO fases) y ZRING (la 14); esta entrada cuenta como se llego a ellas. La nota del cierre ya
> esta RESUELTA en la canon: el margen del periodo paso a 300..800 ms y las tomas escalan con
> `NEURONIK_SELFTEST_BUDGET`, asi que el sintoma que se describe aqui (NO_RING por falta de CPU) es
> el que motivo el margen, no uno abierto.

## 2026-09-27 — los badges de los cajones EDIT de la WebUI son vivos (MODELOS y GLOBAL)

El `liveBadge` de la MATRIZ paso a familia con tres modos en `ui/panel.js::liveDrawerBadge`
(ahora recibe el SNAPSHOT entero, no solo parameters): `assigned` (rutas con fuente != Off,
como siempre), `loaded` (MODELOS: ranuras del motor con modelo, la misma verdad de la vista
model-slots via displayableName; total derivado de MODEL_SLOT_LABELS) y `touched` (GLOBAL:
celdas del cajon apartadas del default del contrato — descriptor/generated/'0 en real', nunca
undefined; margen 1/8192; masterLevel NO cuenta, no es celda del cajon). Los ids que cuenta
touched los deriva drawerFor (spec = drawer + ids de la ficha), asi el contrato no replica la
regla del reparto. Los literales 4 RANURAS / 8 GLOBAL quedan como inventario pre-paint.
Tests en panel.test.js: MODELOS con puente (3/4 con una divergente que SI cuenta) y GLOBAL
(0/8 -> 2/8 -> 1/8 al volver al default: verdad del snapshot, no historial). Vitest **310/310**.

## 2026-09-27 — nopause, el parseo release y la retencion por coherencia de AGUJA

Tres piezas de la jornada de AGUJA que conviene tener a mano (la entrada del commit 157fa16
las nombra; esta las documenta a fondo):

1. **`build.bat nopause`.** El script acababa SIEMPRE en `pause`, y una pausa no hay quien la
   conteste desde una automatizacion: la pasada completa no podia correr sola. El flag sigue
   la convencion de build_wasm.bat (que ya lo usaba para anidar su llamada sin pausa): se
   parsea como los demas argumentos (`NOPAUSE=1`) y en `:finish` hace `exit /b %EXIT_CODE%
   antes del pause — el codigo de salida llega intacto a quien llame. Quedo documentado en
   la cabecera del script. Es lo que permite la pasada completa desatendida con su
   build-last-run.log (35/35 ctest, paridad WASM 9/9, selftest del plugin).

2. **La comilla colgada del parseo `release`.** El parseo de argumentos tenia
   `set "MM_RELEASE=1""` — una comilla de MAS despues de la cerrada. En batch la cadena no
   muere ahi: la comilla suelta abria un literal que se tragaba el principio de la LINEA
   SIGUIENTE (`) else if /I "%%A"=="noselftest"`), de modo que el flag `release` (build.bat
   modelmaker release, el que incrementa la Version.h de ModelMaker) corrompia el parseo de
   TODOS los argumentos despues de el, sin error visible: el sintoma era un flag que no
   hacia nada. Corregido a `set "MM_RELEASE=1"`. Moraleja: en batch, una comilla de mas no
   es un detalle estetico, es un parseo roto en silencio.

3. **La retencion por coherencia de AGUJA.** El sondeo de la aguja RETIENE una toma del
   sostenido para cruzarla con el motor, y el criterio de retencion era "la de nivel de
   pagina mas alto". En el plugin pasaba; en la bancada fallaba con un desfase que no era de
   coherencia sino de INSTANTE: la telemetria de la pagina llega 1-2 periodos tarde, la toma
   de nivel maximo cazaba el attack de la pagina contra el sustain del motor, y la
   comparacion medía dos momentos distintos. El criterio nuevo retiene la toma mas
   COHERENTE: el minimo |pagina-motor| sumado de las dos agujas, leido EN el mismo instante
   de la toma (needleBestSkew). Con el, AGUJA es OK en plugin y bancada por igual — la
   medición compara el mismo numero por dos caminos, no dos instantes. El mismo principio
   vivo despues en las gemelidades de las cuatro curvas (lienzo vs cajon, mismo y) y en las
   barras ENV del cajon de la MATRIZ (mismo frame que las agujas).

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida este criterio en
> AGUJA (la 6) y en las barras ENV (transversal, medidas DENTRO de AGUJA: no hay dirección propia).
> La subseccion «Barras ENV (canon)» fija las TRES invariantes, y «Medidor-PANIC (canon)» la fase 4
> que pulsa el medidor. Esta entrada cuenta como se llego a ellas.

## 2026-09-27 — el anillo del pad BAILA sin host (ruta local de la MATRIZ a Morph Z)

El pad ya tenia su vista `model-xy` y su anillo, y el anillo ya sabia pintar la
contribucion del destino 28 (con signo y envolviendo por las 12), pero en MODO LOCAL
no habia NADA que lo moviera: el motor WASM nace con la matriz del contrato —las dos
rutas de las envolventes y los slots 3/4 en Off—, asi que `_neuronikGetMod(morphZ)`
valia 0 y el arco se quedaba en su base. El gesto Neuron (un LFO modulando el eje
temporal del pad) solo existia si el usuario giraba la ruta a mano en la MATRIZ.

La pagina lo SIEMBRA en local: `paramStore::seedLocalMorphZRoute()` deja **LFO 2 ->
Morph Z al 100%** en el primer slot LIBRE (el 3; los slots 1 y 2 llevan las rutas de
las envolventes ENV 1 -> Osc Level y ENV 2 -> Filter Cutoff, y pisarlas cambiaria el
sonido del legado). Solo escribe si el slot sigue VIRGEN (fuente y destino en Off) y
el store POSEE los tres ids; con host devuelve false y no toca nada (manda el APVTS).
La fuente y el destino son LABELS que se resuelven contra la tabla del contrato
(`choices.indexOf`), nunca un indice escrito a mano: crecer la tabla no re-apunta la
ruta.

Dos piezas mas hacian falta para que esa ruta LLEGUE al motor, y las dos son de
procedimiento:

1. **El re-sync al arrancar el motor.** `syncEngine()` se corta en seco mientras no
   haya motor (`!isAudioEngineReady()`) y SOUND ON arranca DESPUES del primer paint,
   asi que el estado de la pagina (matriz incluida) solo llegaba al worklet con el
   primer gesto del usuario. Ahora `onAudioEngineChange` re-aplica el snapshot en el
   salto a `ready`: el motor acaba de nacer con el mirror por defecto del worklet y
   este es el unico momento en que la matriz, los modelos y el pad llegan a un motor
   recien arrancado. Los guards de `lastEngineIndex`/`lastMorph` evitan repeticiones.
2. **El bundle.** El selftest del host sirve `WebUI/dist`, no `src`: sin `npm run
   build` mide la pagina vieja (la leccion ya estaba escrita en la entrada de ZRING).

Medido con el binario REAL del worklet (`build-wasm/neuronik_dsp.wasm`, el mismo que
sirve la WebUI) y por el MISMO camino que la pagina —el store de verdad y el mapeo
`gpFieldsFromState` del worklet— en `Tests/localMorphZRouteTest.mjs`:

| dato | medida |
|---|---|
| fields enviados (fila 3 de la matriz) | `[28, 2]`, `[29, 28]`, `[30, 1]` (LFO 2, Morph Z, amount 1.0) |
| `_neuronikGetMod(28)` | **-1.0000 .. 1.0000** (los dos signos: el arco da la vuelta) |
| periodo medido entre crestas | **998.7 ms** (LFO 2 a 1 Hz, el default del contrato) |
| control negativo (slot 3 en Off) | **0** exacto |

Pruebas: `NEURONiK_LocalMorphZRoute` (`Tests/localMorphZRouteTest.mjs`, ctest **38/38**;
con el mismo skip que el guard del worklet: sin `build-wasm` avisa y pasa) y vitest
**338/338**: `paramStore.test.js` fija la siembra y sus dos NO (no pisa una ruta ya
asignada; no escribe a medias en un store que no posee los ids de la matriz),
`workletMorph.test.js` fija que la ruta viaja como los campos 28/29/30, y
`appContract.test.js` cierra el cableado (la siembra vive DENTRO de la rama local y el
apunte clave: el re-sync a `ready`). `WebUI/dist` reconstruido. Ficheros:
`WebUI/src/contracts/paramStore.js`, `WebUI/src/app.js`, `Tests/localMorphZRouteTest.mjs`,
`CMakeLists.txt` y los tres tests de la WebUI.

## 2026-09-27 — la pagina RECUERDA sus ranuras de modelo sin host (memoria local)

El plugin sabe volver a sus cuatro ranuras porque el PRESET lleva la ruta del fichero
(`modelPath<slot>`): al reabrir el proyecto el procesador recarga y publica `modelsState`.
La pagina del navegador no tiene ni preset ni sistema de ficheros — lo unico que tiene
es el `<input type="file">` que el usuario elige a mano —, asi que sin memoria cada
recarga empezaba con las cuatro ranuras EMPTY y habia que volver a buscar los mismos
ficheros.

Ahora la pagina RECUERDA: `loadLocalModel` guarda en `localStorage` el TEXTO CRUDO del
.neuronikmodel (no el objeto parseado) y `restoreLocalModels` repuebla las ranuras al
arrancar, DENTRO de la rama local de app.js (primero el shape vacio de `seedLocalModels`,
la memoria encima y el input de fichero al final). Se guarda el TEXTO porque el lector del
modelo es UNO: recuperar vuelve a cruzar `parseModelText`, el MISMO parser que un fichero
recien elegido; guardar el objeto seria congelar un dialecto derivado que el dia que el
escritor publique v2.2 habria que mantener. `readModelFile` se parte en `readModelText` +
parser (mismo lector, dos pasos) para que el texto sea accesible sin duplicar la lectura.

Tres decisiones que no son obvias:

1. **Con host NO se toca nada.** `restoreLocalModels` sale en seco si hay bridge: en el
   plugin manda `modelsState` (el preset sabe sus rutas) y una ranura recordada en una
   sesion de navegador no puede colarse en un proyecto.
2. **Es por ranura y gana la ULTIMA.** Cargar dos ficheros en la misma ranura deja el
   segundo (el payload es un mapa ranura -> texto) y las otras tres no se tocan.
3. **Best-effort, pero sin silencios.** Sin `localStorage` (un WebView que lo niega, modo
   privado, Node) o sin cupo, la CARGA sigue igual — la ranura suena en la sesion — y un
   `console.warn` dice que no se recordara. Al recuperar, una entrada que ya no parsea se
   DESCARTA en vez de romper el arranque, y el motivo se pinta con el `modelError` de
   siempre en la ficha RANURAS.

Medido con el binario REAL del worklet y por el MISMO camino que la pagina (el store de
verdad, con un `localStorage` de mentira porque Node no lo tiene) en
`Tests/localModelCacheTest.mjs`:

| dato | medida |
|---|---|
| texto guardado | el fichero TAL CUAL (identidad de string contra CZ-BASS1.neuronikmodel) |
| ranura recuperada | nombre, validez y 64 amplitudes **bit a bit** contra el fichero (mismo parser) |
| control negativo | sin memoria: **0** recuperadas y las cuatro EMPTY |
| entrada corrupta | se descarta (EMPTY + aviso) y la buena del MISMO almacen sobrevive |
| el MOTOR | misma nota (69, 1 s): RMS **0.49542** ranura vacia vs **0.36251** con la recordada, maxDiff **0.69636** |

Pruebas: `NEURONiK_LocalModelCache` (`Tests/localModelCacheTest.mjs`, ctest **39/39**; mismo
skip sin `build-wasm` que el guard del worklet) y vitest **359/359**: `localModelCache.test.js`
nuevo (round trip por ranura, version del payload, almacen que lanza al tocarlo, cupo lleno,
basura en el payload), `localModels.test.js` (el texto crudo se lee aparte del parser),
`paramStore.test.js` (la memoria repuebla la ranura, con host no la lee, la rota se descarta)
y `appContract.test.js` (el orden del arranque local). `WebUI/dist` reconstruido. Ficheros:
`WebUI/src/audio/localModelCache.js` (nuevo), `WebUI/src/audio/localModels.js`,
`WebUI/src/contracts/paramStore.js`, `WebUI/src/app.js`, `Tests/localModelCacheTest.mjs`
(nuevo) y los cuatro tests de la WebUI.

## 2026-09-27 — el smoke del MODO LOCAL, automatizado (Playwright + Chromium) y el bug que caza

El smoke del modo local -SOUND ON, ranura de modelo, nota y pad- se comprobaba A MANO y el
resultado se dejaba escrito en HANDOFF. Ahora es un test: `WebUI/e2e/localMode.spec.js` con
`WebUI/playwright.config.js`, cinco casos sobre Chromium de verdad, `pnpm test:e2e` y ctest
`NEURONiK_WebUiLocalModeE2e`.

QUE MIRA (lo que ningun otro test puede): el vitest corre en jsdom, donde no hay AudioContext, y
los tests de node leen el WASM a mano; este arranca el AudioWorklet por el camino del usuario y
ESPIA la frontera: envuelve `MessagePort.prototype.postMessage` y `AudioWorkletNode` para ver lo
que la pagina manda y lo que el motor reporta en el meter. Los cinco casos:

1. SOUND ON -> badge `AUDIO: motor local del navegador (WASM) · ON · 48.0 kHz`, y la fila 3 de la
   MATRIZ (`[[28,2],[29,28],[30,1]]`, LFO 2 -> Morph Z al 100%) cruzando por `neuronik:params`;
2. el anillo del pad BAILA: el arco de modulacion recorre los DOS signos (nace en la base y
   TERMINA en ella) y `morphZMod` barre -0.99 .. +1.00;
3. una nota (tecla 48, raton sostenido) -> «PANIC: parar 1 voz activa», y al soltar -> 0;
4. la ranura A carga el `CZ-BASS1` REAL por el dialogo del input oculto (filechooser de
   Playwright), llega al motor (`neuronik:models` con la ranura 0 valida) y SOBREVIVE AL F5
   (la memoria local de la entrada anterior, ahora verificada en un navegador de verdad);
5. arrastrar el pad mueve morphX/morphY (arriba-derecha los dos grandes, abajo-izquierda los dos
   pequenos: no basta con que algo cambie).

EL BUG QUE CAZA (y que se arregla aqui): **la pagina no mandaba NADA al worklet**. `app.js`
guardaba `engineSnapshot = engine`, y ese `engine` es el MISMO objeto vivo del modulo
(`audioEngineState`, mutado en el sitio): cuando llegaba el aviso de `ready` el `status` de ese
objeto YA era 'ready', asi que `wasReady` salia SIEMPRE true y el re-sync no disparaba nunca.
Sintoma exacto, medido: el badge decia ON y el worklet procesaba (el meter latia), pero matriz,
LFO, modelos y pad se quedaban en los defaults del struct -el anillo quieto y una nota sonando
con el motor sin configurar-. Se arregla con una COPIA (`engineSnapshot = { ...engine }`), que es
lo que el nombre prometia. El test se comprobo ROJO con el bug puesto y VERDE con la copia.

Es justo el agujero que la entrada anterior creia tapado: su guard solo se podia pinchar en el
TEXTO de `app.js` (appContract), porque ningun test miraba la frontera real. Ahora hay test en
los dos niveles: el anclaje de la copia en vitest y la frontera en el navegador.

DOS TRAMPAS DEL ENTORNO, medidas (no supuestas) y documentadas en el spec:

1. **`--mute-audio`**: sin el, Chromium crea un `AudioContext` 'running' cuyo reloj NO AVANZA
   (ni headless ni con ventana: probado) y el worklet no procesa nunca, aunque el badge diga ON.
   Con el flag el grafo se tira (medido: 72 bloques en 0,19 s de reloj).
2. **El servicio de audio tarda ~4 s en arrancar.** El test ESPERA a que el reloj avance
   (`ensureAudioClock`, contextos de usar y tirar) antes de pedir audio; si no arranca, los casos
   que necesitan procesar se SALTAN con el motivo -no se finge un verde-.

El E2E sirve `dist` (el artefacto que embebe el plugin), asi que el comando del servidor
CONSTRUYE antes de servir: sin `npm run build` el test mediria un bundle viejo. Cinco casos en
~36 s de reloj; dentro de `ctest` el test tarda ~65 s (build + servidor + navegador).
Verificado: `ctest` **40/40** y vitest **359/359**. Ficheros: `WebUI/playwright.config.js`,
`WebUI/e2e/localMode.spec.js`, `WebUI/package.json` (@playwright/test 1.63 + `test:e2e`),
`WebUI/.gitignore`, `WebUI/src/app.js` (el fix), `WebUI/tests/appContract.test.js`,
`CMakeLists.txt` y `WebUI/README.md`.

## 2026-09-27 — el distintivo VIVO también vive en el lienzo, y pulsarlo abre su cajón

Los distintivos vivos que se inventaron para los cajones (0/4 RANURAS, 0/8 GLOBAL) solo se veían
ABRIENDO el cajón: la cabecera de un cajón cerrado sale desplazada fuera de pantalla, así que el dato
—que es justo el dato que resume la ficha— no se leía sin abrir nada. La petición: que el badge sea
CLICABLE y abra su cajón, como ya hace la franja de GLOBAL & MASTER (TEMPO / MIDI / RANDOM).

Decisión de sitio (la del usuario, no la del panel): el chip cuelga de la CABECERA de la ficha, al lado
del botón EDIT — no en el cuerpo, que era la otra opción. Y solo la piden las fichas que lo declaran
(`liveBadge.onCard`: MODELOS y GLOBAL); la MATRIZ conserva su resumen de filas y las fichas de literal
fijo (LFO, ENVOLVENTES) no cuelgan nada, porque un chip con un dato que no cambia tiene apariencia de
dato vivo y no lo es.

LO QUE NO CABÍA, MEDIDO (y por eso el chip lleva solo la fracción): la cabecera de una ficha es una fila
FIJA de 20 px y la de MODELOS mide 211 px de diseño, con el título (85) y el EDIT (54) ya puestos. Con el
rótulo entero del distintivo (`0/4 RANURAS`, 70 px) el chip empujaba el EDIT 22 px FUERA de la ficha
—que con `overflow: hidden` lo recorta, dejando la ficha sin abridor: exactamente lo contrario de lo que
pedía el gesto-. Compactando tipografía seguía desbordando 12 px; con la sola fracción (`0/4`, 29 px) el
encaje queda en cero desbordamiento y el EDIT dentro. Así que el chip muestra `0/4` y el rótulo entero se
queda en el `title` y en la etiqueta accesible (`0/4 RANURAS: abrir el cajón de MODELOS A–D`), que
además contiene el texto visible (WCAG 2.5.3). La cabecera del cajón, que sí tiene sitio, sigue
mostrando el rótulo entero.

CÓDIGO: el chip es un `<button class="card__badge" data-live-badge="<id>">` que se registra en
`liveBadgeChips` (panel.js) y se repinta en el MISMO bucle que el `setHeader` del cajón, con el MISMO
`liveDrawerBadge()`: un solo cálculo, dos destinos, imposible que discrepen. `shortLiveBadge()` recorta el
rótulo a la fracción; `destroy()` suelta el registro. El gesto es el de la franja (`onDrawerOpenedByUser`
+ `drawer.open()`), así que un salto de ruta pendiente se cancela igual. La hoja nueva
(`.card__badge` en styles/main.css) replica las métricas del `.card__action` de al lado, en tono atenuado:
es información, no la acción de editar.

VERIFICADO: vitest **361/361** (dos casos nuevos en `panel.test.js`: el chip de MODELOS con su recuento
vivo y el de GLOBAL, el chip solo donde se pide, el EDIT dentro, y el gesto que abre SU cajón) y
Playwright **6/6** (caso nuevo en `e2e/localMode.spec.js`: los dos chips, el encaje de la cabecera medido
en el navegador, cada cajón abriendo y cerrando con ESC, y el chip subiendo a `1/4` al cargar el
CZ-BASS1 de verdad). Comprobado también a mano en el navegador: los dos chips abren su cajón y el
distintivo del cajón y el del lienzo dicen lo mismo.

## 2026-09-27 — los distintivos vivos también saben de MOTORES: el modo 'active'

Los distintivos vivos ya sabían contar rutas, ranuras y celdas tocadas. Faltaba la pregunta que
pregunta el motor: con NEUROTIK en marcha, cuántas celdas de esta ficha están realmente sonando.
Nuevo modo `liveBadge.mode: 'active'`, pensado para las fichas CON MOTOR.

LA VERDAD NO ES NUEVA, es la del gating: se lee `engines` (la cobertura por PARAMETRO que el host
deriva con `engineCoverageFor`) y `optionEngines` (la cobertura por OPCIÓN de las celdas gateadas,
los destinos de la MATRIZ). El panel no cuenta nada a mano:

  - celda GATEADA (choice con `engineParameter`): activa si la opción que tiene SELECCIONADA es
    alcanzable con el motor activo - la misma regla con la que `setEngine` deshabilita opciones. Un
    destino de NEURONiK seleccionado con NEUROTIK en marcha es una celda apagada, y el distintivo lo
    dice en vez de contar un destello que no suena;
  - celda normal: `engines === 'both'` o el motor activo la consumen; `host` la consume el
    procesador, así que cambiar de motor no la apaga y cuenta siempre; `none` no la consume nadie y
    nunca cuenta.

El motor activo lo dice el MISMO choice que la UI usa para gatear. El detalle que salió al
implementarlo: ese selector (`engineType`) es celda de la ficha OSCILADOR, así que una ficha de sonda
que no lo tenga en su lienzo se quedaba sin distintivo. Se resuelve por CONTRATO cuando no es celda
(`controlsById.get(gateId) ?? describeControl(gateId)`), que es la SSOT igual que en el modo `touched`.
Y un VALOR ausente cuenta como su default (como en `assigned` y `touched`): lo que no se inventa es el
SELECTOR — sin él en el contrato no hay distintivo, porque un "activo" sin motor que lo sostenga sería un
número inventado.

DóNDE CUELGA, Y DóNDE NO (medido con la cobertura real del contrato generado):

  - LFO 1 & 2: 10/10 con los dos motores. Los dos LFO son DSP COMPARTIDO (`engines: 'both'`), así que
    la caja está entera siempre: el chip dice la verdad, y esa verdad no se mueve. Es la única ficha
    con cajón donde el número cambia, porque las que lo tienen no tienen cajón todavía;
  - FILTRO: 2/2 con NEURONiK, 0/2 con NEUROTIK. RESONADOR: 0/3 y 3/3. ENVOLVENTES: 8/8 y 4/8.
    OSCILADOR: 9/12 y 6/12. Son las fichas coarsely interesantes y NINGUNA tiene cajón, así que hoy
    no hay dónde colgar su distintivo: el modo está escrito y probado, y la decisión de dónde viven
    estas cifras es de quien manda en el lienzo.

Lo que se entrega: el modo en `liveDrawerBadge` (`activeCellsBadge` + `drawerCellIds`, este ultimo
compartido con `touched`) y la ficha LFO declarándolo, con su chip en la cabecera (10/10, encaje
medido: cero desbordamiento y el EDIT dentro).

VERIFICADO: vitest **364/364** (tres casos nuevos: la cobertura por motor de una ficha de sonda con las
tres clases -NEURONiK, NEUROTIK, compartida-, el espejo del gating en una celda gateada con destinos
de un solo motor, y el LFO entero con los dos motores) y Playwright **7/7** (el caso nuevo mide el
gating REAL en el navegador: con NEURONiK la lista de destinos apaga Excite Noise y Res Bank Res, y
tras cambiar a Neurotik apaga Inharmonicity y Filter Cutoff, mientras el chip del LFO sigue en 10/10).
Con eso queda demostrado que el distintivo y la celda leen la misma verdad.
## 2026-09-27 — VOLVER: el retorno del salto ENV -> MATRIZ se mide (la direccion 1b-bis)

> Canon: **«SECCION CANONICA — VOLVER (direccion 1b-bis)»**, al final de este fichero. Esta
> entrada cuenta como se llego; el guion vigente, el ciclo de vida del boton, la regla de
> cancelacion y la fontaneria de cierres estan alla.

El arnes ya media el salto de ruta (ENV-RUTAS) y su segunda via (RESUMEN-RUTAS), pero daba
por hecho que el salto era de ida: nadie comprobaba que el usuario puede VOLVER. La direccion
VOLVER (Stage::back, "1b-bis" del plan) cierra esa historia: el boton "VOLVER A LA RUTA n"
del cajon de origen (routeBack del panel, ui/routeBack.js) reabre la MATRIZ en el MISMO slot
que trajo al usuario a ENVOLVENTES.

El guion, en cinco medidas (backDirection(slot)):

1. RE-SALTO: pulsa OTRA VEZ la misma fila `.env-route` del slot medido por ENV-RUTAS. El
   EDIT -> "IR A LA RUTA" del bloque "RUTA n" es el unico gesto que SIEMBRA el retorno (las
   filas del lienzo no llevan retorno: el VOLVER no nace con ellas). Exige matriz abierta.
2. CIERRE POR USUARIO: el ✕ de la matriz, no un ESC programatico — es el gesto real.
3. BOTON PRESENTE: cerrar la matriz reabre el cajon de ENVOLVENTES, y al pie tiene que
   estar `.env-block__back` con SU numero ("VOLVER A LA RUTA n"). Sin boton (oculto o
   descolgado) el gesto no existe -> FAIL.
4. CLIC: la matriz reabierta tiene que quedar con SU velo y el MISMO slot resaltado
   (data-slot-highlight).
5. El SLOT VIAJA del guion de ENV-RUTAS al de VOLVER, sin hardcodear. (CORREGIDO el 28 de
   septiembre: aqui se decia que se derivaba del APVTS, "el primer slot con fuente ENV", y ya
   no vale —el Standalone restaura el `filterState` de la sesion anterior y ese puede traer la
   matriz del usuario—: el slot sale de lo que la pagina PINTA. Ver el canon de VOLVER.)

Ciclo de vida del boton (la parte no obvia): nace con el salto CON retorno, se REFRESCA en
cada ida y vuelta completa (la vuelta reancla) y MUERE si el usuario cierra el cajon de
origen por si mismo o abre otro cajon antes de cerrar la matriz. Sin salto con retorno
pendiente el boton NO esta en el DOM: el EDIT no lo nace. La SSOT del texto y la clase vive
en routeBack.js (ROUTE_BACK_TEXT / ROUTE_BACK_CLASS); el panel solo juega con `hidden` y
`textContent`.

Robustez: si el re-salto falla (pagina sin respuesta, cajon equivocado), la direccion no
muere sola — deriva a RESUMEN-RUTAS con el mismo slot, el patron de la jornada. El veredicto
(backOk) entra en el AND de finish() como el de todas las direcciones. Verificado en la
ultima corrida del Standalone (transcript de 27 Sep 20:04): VOLVER -> OK entre ENV-RUTAS y
RESUMEN-RUTAS, corrida completa RESULT: OK.

## 2026-09-27 — el VST3 recompila con el dist del dia

La WebUI viaja EMBEBIDA en el binario: sin recompilar, el VST3 seguia sirviendo el bundle de
su ultima pasada. Recompilado (`cmake --build build-reference --config Release --target
NEURONiK_VST3`, EXIT=0) con el dist actual — regenerado a las 20:25 y con CERO fuentes mas
nuevas que el bundle, verificado con `find WebUI/src -newer <bundle>` —. Comprobado dentro
del binario: embebe el chunk JS `index-DWGnYOxA.js`, el CSS `index-B4o-rgAy.css` y el worklet
`neuronik_dsp.wasm`. Artefacto: build-reference/NEURONiK_artefacts/Release/VST3/NEURONiK.vst3
(6.174.208 bytes, 27 Sep 20:28).

## 2026-09-27 — la MATRIZ tiene conmutador: la ruta local del pad, apagable y con LFO a elegir

La siembra local (LFO 2 → Morph Z en la fila 3) se plantaba sola al arrancar y no habia manera de
quitarla sin abrir el cajon y dejar la fila 3 en Off a mano — y de elegir otro LFO, ni eso. Ahora la
cabecera de la ficha MATRIZ lleva el conmutador: un boton que enciende y apaga la ruta y un desplegable
con los LFO. El VALOR INICIAL es la siembra misma (encendida, LFO 2), leida de `LOCAL_MORPH_Z_ROUTE`:
arrancar sin la ruta es una decision del usuario, arrancarla es la de fabrica. Un F5, con el conmutador
apagado, vuelve a apagado.

EL DATO VIVO, COMO SIEMPRE. `state.localMorphRoute = { enabled, source, sources }` lo publica el store
y lo pinta el panel con el MISMO snapshot que las celdas. `sources` no es una constante escrita a mano:
sale de la tabla de fuentes del contrato (`/^LFO \d/` sobre los choices de `mod3Source`), asi que un LFO
nuevo aparece solo en el desplegable. Un LFO que la tabla no lista se RECHAZA entero: ni el estado ni la
fila se mueven, porque un rechazo no es medio gesto.

LO QUE ESCRIBE, Y CUNDO. Con ON, la fila 3 queda como la siembra (esa fuente → Morph Z al 100%); con OFF
vuelve a VIRGEN, que son los defaults del PROPIO contrato de sus tres ids — exactamente como estaba antes
de la siembra, no un numero puesto a mano. Y hay una diferencia importante con la siembra: el conmutador es un GESTO
explicito, asi que escribe la fila aunque el usuario la hubiera tocado antes; la siembra sigue solo
pisando slots virgenes. Con el conmutador apagado, `seedLocalMorphZRoute()` no siembra: apagar es no
sembrar. Con HOST no hace nada (la matriz es del APVTS) y la vista lo pinta deshabilitado, con el
estado sigue visible y el title diciendo por que.

DONDLE VIVE Y POR QUE EN LA CABECERA. El cuerpo de la MATRIZ tiene altura FIJA (164 px) para las cuatro
filas del resumen, y añadir una línea mas lo descuadra (por eso el conmutador no es una franja como la de
GLOBAL). La cabecera es la fila ancha de la ficha, y alli caben los dos controles con las metricas del
`.card__action` de al lado. El boton nace DESHABILITADO (el patron de la accion RANDOM): antes del primer
paint no hay estado que invertir. El desplegable se deshabilita con el boton apagado pero CONSERVA el LFO
elegido: apagar no lo olvida.

UN SOLO AVISO POR GESTO. `setLocalMorphRoute()` calcula parametros y estado y llama a `setState()` UNA vez
(asi lo fija un test): nada de un frame con la fila ya cambiada y el conmutador con la verdad vieja, que
en el motor local se veria como un destello de una sola fila.

MEDIDO, con el WASM de verdad (ctest `NEURONiK_LocalMorphZRoute`): encendida, `GetMod(28)` barre
-1.0000..1.0000 con periodo 998.7 ms; apagada, mide **0 EXACTO** en los dos signos (no "casi cero"); con
LFO 1 la fila viaja con el indice 1 y el anillo vuelve a barrer los dos signos. En el navegador (E2E con
`--mute-audio`): el anillo se queda quieto con el arco en `span 0` y un solo valor distinto en 1.5 s al
apagar, y vuelve a bailar al encender; la fila 3 cruza al worklet como `[[28,0],[29,0],[30,0]]` apagada y
`[[28,1],[29,28],[30,1]]` con LFO 1. El distintivo vivo de la MATRIZ baja de 3/4 a 2/4 al apagar, sin
tocar nada mas: cuenta rutas asignadas y la fila 3 ya no lo está.

VERIFICADO: ctest **40/40**, vitest **373/373** (6 casos nuevos en `paramStore.test.js` y 3 en
`panel.test.js`), Playwright **8/8** (un caso nuevo). Un aviso de la instrumentación en el camino: los
gestos NO viajan por el `onAction` de las acciones de ficha sino por `handlers.onLocalRoute` (que es por
donde ya van `onChange`, `onPanic` y `onStartSound`), asi que el panel no lleva estado del conmutador: el
paint se lo guarda y el clic solo dice hacia donde va.
## 2026-09-27 — VOLVER extendida: la regla de CANCELACION, medida E2E

> Canon: **«SECCION CANONICA — VOLVER (direccion 1b-bis)»**, al final de este fichero. Esta
> entrada cuenta como se llego; el guion vigente, el ciclo de vida del boton, la regla de
> cancelacion y la fontaneria de cierres estan alla.

Abrir GLOBAL con el retorno pendiente MATA el retorno, y el boton no reviva. Era la regla
que quedaba sin cobertura: backDirection media el ciclo feliz (ida, boton, vuelta) y la
cabecera documentaba "abre otro cajon antes de cerrar la matriz" como muerte del boton, pero
nadie la habia pulsado. La direccion VOLVER ahora lo mide (BridgeSelftest.h,
scriptOpenGlobalDuringReturn + scriptCloseDrawerById + scriptReadPostCancelState).

EL GUION, tras el OK del boton: pulsa el EDIT de la ficha GLOBAL & MASTER
(`[data-drawer-trigger="globalFull"]` — el id de la seccion es globalFull, no global: el
error NO_GLOBAL_EDIT_TRIGGER del primer intento lo enseño) mientras la MATRIZ sigue abierta
con el retorno vivo. Tres aserciones nuevas:

1. EN EL INSTANTE: cajon de GLOBAL abierto, la MATRIZ SIGUE ABIERTA detras (el mueble no
   impone exclusion mutua: dos cajones a la vez) y el boton `.env-block__back` YA NO ESTA
   en el cajon de origen (onDrawerOpenedByUser -> cancelRouteReturn: routeReturn = null y
   routeBack.clear()). Un detalle de parseo del cierre asentado aqui: cerrar "el cajon
   abierto" con document.querySelector('.drawer--open .drawer__close') con DOS cajones
   abiertos cierra el EQUIVOCADO — scriptCloseDrawerById cierra cada cajon POR ID.
2. CERRAR GLOBAL: el boton NO REVIVE (el retorno ya no existe que reanclar).
3. CERRAR LA MATRIZ (el cierre que antes pagaba la vuelta): cerro, el cajon de ENVOLVENTES
   NO SE REABRE SOLO y el boton sigue ausente. La vuelta se quedo sin cobrar, como manda
   la regla.

Fichero tocado: Source/WebUI/BridgeSelftest.h (guion, dos scripts nuevos, el anidamiento de
backDirection crece tres pasos: abrir GLOBAL -> cerrar GLOBAL -> leer estado; doc de
cabecera 1b-bis). Compilado en los tres objetivos (Standalone, VST3, bancada). Verificado
en corrida real del Standalone: las cuatro medidas de VOLVER OK y RESULT: OK
(selftest-exit=0), y NEURONiK_WebUiSelftestContract verde con los selectores nuevos.
## 2026-09-27 — VOLVER en el arnes: encaje tras ENV-RUTAS, el settle de modales y el cierre por cajon abierto

> Canon: **«SECCION CANONICA — VOLVER (direccion 1b-bis)»**, al final de este fichero. Esta
> entrada cuenta como se llego; el guion vigente, el ciclo de vida del boton, la regla de
> cancelacion y la fontaneria de cierres estan alla.

Tres piezas de la fontaneria de VOLVER que las dos entradas anteriores dan por supuestas y esta deja
escrita, para que tocarlas no obligue a releer el arnes:

**1. El ENCAJE: corre detras de ENV-RUTAS y su slot VIAJA por datos.** El orden de las direcciones es
MATRIZ -> ENV-RUTAS -> VOLVER -> RESUMEN-RUTAS -> AGUJA, y la cadena es de DATOS, no un guion fijo:
ENV-RUTAS deriva el slot de lo que la pagina PINTA (CORREGIDO el 28 de septiembre: antes se
derivaba del APVTS y no vale) y ese mismo slot llega a backDirection(slot) como argumento. VOLVER no lo hardcodea: el re-salto pulsa la MISMA
fila .env-route que pulso ENV-RUTAS, el boton tiene que estar presente con SU numero ("VOLVER A LA
RUTA n") y la matriz reabierta con SU resalte — si la sesion del usuario trae otra matriz, la
direccion mide ESA. Hacia delante, cualquier salida de VOLVER cae en RESUMEN-RUTAS (pasandole el slot como
avoid: un segundo slot medido, no la repeticion del primero), que settlea el lienzo y encadena AGUJA;
el fallo tambien cae ahi o encadena AGUJA directo — nunca dos AGUJAS en paralelo (el doble encadenado
de una jornada anterior la corria dos veces: aguja "visible" en silencio y notas atascadas en MIDI).

**2. El SETTLE de modales (scriptSettleDrawers).** VOLVER deja la pagina con modales (la matriz queda
abierta detras de toda la direccion y RESUMEN-RUTAS la reabre al final), y AGUJA necesita empezar sin
ellos: sus lecturas viven en el lienzo y en el cajon de ENVOLVENTES. El settle cierra TODO cajon
abierto y ITERA (hasta cinco cierres): la vuelta del retorno puede REABRIR el cajon de origen cuando
la matriz se cierra — justo el comportamiento que VOLVER mide — asi que un cierre puede dejar otro
cajon abierto detras. Cierra por el ✕ y no por el velo: el ✕ es el cierre de usuario (el mismo gesto
que la direccion esta midiendo) y evita el caso anotado en el codigo de que un cierre por velo
consuma un retorno pendiente por un camino no cubierto aqui. Vive despues de RESUMEN-RUTAS, justo
antes de encadenar AGUJA — la cola settle -> AGUJA que un dia vivio dentro de VOLVER murio con el
encadenado: hoy el settle es de RESUMEN-RUTAS.

**3. El CIERRE por cajon abierto.** El mueble (createDrawer) deja el ✕ de cada cajon en el DOM aunque
este cerrado (inert, aria-hidden), asi que "cerrar el abierto" es
document.querySelector('.drawer--open .drawer__close'): el abierto es UNICO mientras solo haya UNO.
Asi cierran scriptCloseMatrixDrawer (el paso 2 del guion de VOLVER: el cierre que paga la vuelta tiene
que ser el ✕ de usuario, no un close programatico) y el settle entero. El limite aparecio al extender
VOLVER con la regla de cancelacion: con DOS cajones abiertos (GLOBAL y la matriz), ese selector cierra
el EQUIVOCADO (el primero del documento, no el que toca). De ahi scriptCloseDrawerById, que cierra por
ID ([data-drawer="drawer-<id>] .drawer__close): cada mueble se dirige por su nombre y el selector
"el abierto" solo se usa donde un solo cajon abierto es parte del contrato. La semantica que hace del
✕ la herramienta correcta en los tres sitios: el cierre de usuario es el que PAGA el retorno (reabre
el origen y reancla el boton), el que lo CANCELA si antes se abrio otro cajon, y el que deja el lienzo
limpio en el settle.
## 2026-09-27 — las barras ENV del cajón de la MATRIZ: el nivel de cada envolvente en SU fila

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida esta pieza (Barras ENV); esta entrada cuenta como se llego a ella.

Cada fila del cajón de la MATRIZ lleva una barra fina de nivel (`.drawer-slot__env-level`) que se
rellena de abajo a arriba con el nivel EN VIVO de la envolvente que la fuente de esa fila tiene
asignada: la fila con fuente ENV 1 baila con la envolvente de amplitud y la de ENV 2 con la del
filtro; el resto de filas no llevan nivel. La misma pieza vive en DOS sitios: las filas del cajón
(`panel.js::paintEnvLevels`) y las filas del RESUMEN del lienzo (`modSummary.js`) — el CSS es uno
(`.drawer-slot__env-level` en styles/main.css: relleno por `linear-gradient` con la variable
`--env-level`, oculta con `data-live='false'`).

La decisión corre en DOS fases, y esa separación es el diseño:

1. **QUÉ filas viven (por snapshot).** En cada `paint` (cuando cambia el estado), la fuente de la
   fila (`mod1Source..mod4Source` por `data-slot`) se traduce a índice de opción y se cruza con el
   par del contrato `envelopes=[amp, filter]` → ENV 1 = 0, ENV 2 = 1; cualquier otra fuente deja
   `data-envelope=-1` y `data-live=false`. Escribir la decisión por frame sería tirar CPU en
   re-leer snapshots que no cambian.
2. **El NIVEL (por frame).** El canal de telemetría (~15 Hz) repinta llamando a `paintEnvLevels()`
   SIN parámetros: reutiliza la última decisión y escribe `--env-level` con
   `frame.envelopes[envelope]` clampeado a 0..1 — el MISMO canal de frames que alimenta las agujas
   de las curvas ADSR (dos caras del mismo dato, nunca dos verdades). Detalle vivo del test: una
   fila que muere conserva el último nivel recibido en la variable, pero `data-live=false` la
   oculta — sin zombie visibles.

La verificación E2E vive DENTRO de AGUJA (no hay dirección propia): la misma lectura que mide las
agujas y las cuatro curvas recoge las barras (`#drawer-modMatrix .drawer-slot__env-level,
.mod-summary__row .drawer-slot__env-level` → live / level / envelope por barra) y son legibles aun
con el cajón CERRADO (el mueble mantiene el DOM con `inert`: el settle que deja el lienzo limpio
antes de AGUJA no las apaga). Las invariantes medidas, con el criterio de coherencia propio de la
dirección (margen 0.08):

- **En el sostenido:** TODA barra viva pinta el nivel de SU envolvente del MISMO frame que las
  agujas: `|barra − aguja| < 0.08` para ENV 1 contra el nivel de amplitud y para ENV 2 contra el
  de filtro (`barsOkInHold`, un `all_of` sobre las barras leídas). Una barra que pinte la
  envolvente hermana o un frame viejo ROMPE el OK.
- **Tras el release:** las vivas se APAGAN con su envolvente, con el mismo criterio de cola que
  las agujas (no se exige cero seco): `barra < nivel_sostenido * 0.5` Y
  `|barra − nivel_nativo_ahora| < 0.08` (`barsOkAfterRelease`). La barra no se queda clavada
  cuando la nota muere.
- Las dos invariantes entran en el AND del veredicto de AGUJA (`needleOk`) y en su línea de log:
  `... barras 8 (OK) ...` (4 filas del cajón + 4 del resumen). En la última corrida completa, OK en
  plugin y bancada.

Al nivel unitario lo fijan `panel.test.js` ("la BARRA de nivel del cajón de la MATRIZ vive SOLO en
filas con fuente ENV (frame al instante)": sin frames el nivel es 0, 0.25/0.9 por envolvente al
llegar el frame, y ENV 2 repintando con su último nivel recibido) y su gemela para el resumen
("las filas ENV del RESUMEN llevan la barra de nivel (frame al instante, mudanza incluida)").

Ficheros: `WebUI/src/ui/panel.js` (creación de la barra en la fila + `paintEnvLevels` + suscripción
al canal de frames), `WebUI/src/ui/modSummary.js` (la misma pieza en el lienzo),
`WebUI/src/styles/main.css` (`.drawer-slot__env-level`) y `Source/WebUI/BridgeSelftest.h`
(lectura, `NeedleReading::Bar` y las dos invariantes de AGUJA).


## 2026-09-27 — OLVIDAR: una ranura se vacia de verdad, con el aviso en la misma linea que el fallo

La ficha RANURAS solo sabia cargar. Un modelo equivocado en una ranura se quedaba ahi para siempre: sin
host no hay preset que lo sustituya, y la memoria local (2026-09-27) lo devolvia en cada F5. Ahora cada
fila con modelo lleva su **OLVIDAR**, que deja la ranura en EMPTY y saca su texto de la memoria del
navegador, de modo que un F5 ya no la devuelve.

**Que vacia es el ESTADO, no solo la memoria.** `forgetLocalModel(slot)` reescribe `models[slot]` con la
entrada vacia de FABRICA (`emptyLocalModelSlot()`), no borra la entrada. Asi la vacia viaja al motor por el
canal `neuronik:models` que ya usaba la carga, el worklet llama a `neuronikLoadModel(..., isValid: 0)` y
el timbre se descarga en el MISMO gesto, no en la siguiente recarga. Medido contra el WASM real
(`Tests/localModelCacheTest.mjs`): despues de olvidar, la misma nota suena BIT A BIT igual que con una
ranura que nunca se cargo (`maxDiff 0`), y distinta de cuando el modelo estaba (RMS 0.49542 vacia / 0.36251
con CZ-BASS1). Vaciar una ranura no es silenciar el motor: sigue sonando, con el timbre de fabrica.

**El aviso va por el canal del error, con su propio tono.** La ficha tiene UNA linea de estado, asi que el
gesto escribe en `state.modelNotice` (`{ slot, detail, tone }`), al lado de `modelError`: `tone: 'ok'` para
el acierto (`✓ olvidada: la ranura vuelve a EMPTY y no sobrevive al F5`) y `tone: 'warn'` para el acierto a
medias, cuando la sesion se vacio pero el navegador no solto la memoria (`⚠ ... al recargar volvera`). No
se confunden los papeles: `modelError` sigue siendo lo que el host o el parser no pudieron hacer, y si hay
fallo de carga EL FAILURE MANDA sobre el aviso (un mensaje, no dos en la misma linea). El aviso lo limpia
cualquier gesto posterior de ranuras: abrir el dialogo, cargar, o la respuesta del host.

**Por que `warn` y no `error` en el a medias.** Es el caso que la UI tiene que poder pintar: un almacen
que acepta la escritura y la pierde (cuota nearly lleno, modo privado que lanza al tocar) haria que un
"OLVIDAR" pareciera haber funcionado y el F5 devolviese el modelo. `forgetCachedModelText()` por eso
VERIFICA releiendo lo que queda en vez de fiarse del `setItem`, y devuelve `{ forgotten, remembered }`: la
segunda es la que produce el `warn`. Al reves, un almacen que no se puede LEER no marca `remembered`
(nada que leer = nada que restaurar en el F5 tampoco; el aviso se evita por el motivo correcto).

**Solo donde tiene sentido.** Con host el boton NO aparece: las ranuras son del preset (su
`modelPath<slot>`) y el motor las volveria a cargar al recargar el proyecto, asi que vaciarlas desde la
pagina seria un gesto que no se sostiene. Con `onForget` ausente se dibuja deshabilitado (no se finge).
Nace OCULTO en las ranuras vacias —no deshabilitado— porque no hay nada que olvidar y tres de cuatro
filas suelen estarlo; aparece al cargarse y desaparece al olvidarse. La fila pasa a cuatro columnas
(`12px minmax(0,1fr) auto auto`): medido en Chromium, con el boton visible la fila NO desborda (0 px) y lo
que se recorta es el nombre largo, que es `minmax(0, 1fr)` con elipsis. Olvidar es POR RANURA: las demas
textos de la memoria se quedan donde estaban, y si era la ultima se borra la clave entera en vez de dejar
un payload vacio.

Ficheros: `WebUI/src/audio/localModelCache.js` (`forgetCachedModelText`), `WebUI/src/audio/localModels.js`
(`emptyLocalModelSlot` y `displayableModelName`, la verdad unica de "esta ranura tiene nombre", que la
vista ya no duplica), `WebUI/src/contracts/paramStore.js` (`forgetLocalModel` + `state.modelNotice`),
`WebUI/src/ui/modelSlots.js` (boton y linea de estado), `WebUI/src/ui/visuals.js` y `src/app.js`
(`onForget`), `WebUI/src/styles/main.css` (`.model-slots__forget`, `.model-slots__status[data-state=warn]`),
`WebUI/tests/{localModelCache,paramStore,modelSlots,appContract}.test.js`,
`WebUI/e2e/localMode.spec.js` y `Tests/localModelCacheTest.mjs`.

Verificado: **ctest 40/40**, **vitest 395/395** (18 casos nuevos: la memoria verifica su escritura, el store
no se finge ni con host ni con una ranura vacia y emite UN solo frame con la fila y el aviso a la vez, la
vista solo ofrece el boton donde tiene sentido), **Playwright 9/9** (el caso nuevo carga el CZ-BASS1 de
verdad, lo olvida, comprueba la entrada EMPTY en el worklet con sus 64 amplitudes a cero, la memoria sin
esa ranura y que tras el F5 sigue EMPTY).


## 2026-09-27 — regresion VISUAL del lienzo: una referencia por ficha, y el umbral del hermano Resulto ciego

La suite afirmaba que el lienzo existe y que sus textos dicen lo que deben (vitest en jsdom, el E2E de
Chromium sobre el DOM y los mensajes al worklet), pero no que **se pinte bien**. Un `min-width: 0` que se
cae, un token de color que se invierte o un `padding` que empuja un boton fuera de su ficha no rompen
ninguna asercion de texto. Ahora `e2e/visual.spec.js` tiene una foto de referencia por ficha y las
compara (patron de ABDMS2000: `snapshotDir`, `snapshotPathTemplate` y `expect.toHaveScreenshot`).

**Las once referencias.** Nueve fichas (una por seccion de `SECTIONS`), el lienzo entero y el lienzo
entero con el tema claro. La lista de fichas SALE DEL CONTRATO, no de una lista escrita a mano: una ficha
nueva nace con su referencia que falta (y el test falla diciendo que la ha creado, que es el aviso de
"miralo antes de aceptarlo") y una ficha que desapareciera del DOM haria fallar su `toHaveCount(1)` — el
fallo de un selector no puede quedarse en verde, que es el fallo que el propio spec del hermano ya
documenta. El lienzo entero esta por una razon medida: las nueve fichas por separado cazarian un cambio
de pintura DENTRO de una ficha, pero no un cambio de REPARTO (que ficha cae en otra banda, un alto que
estira la fila de al lado), donde las fotos seguirian siendo identicas. El tema claro va entero y una
sola vez: son los mismos tokens con otros valores, y dieciocho archivos para cazar lo que una foto ya
dice.

**El viewport es el tamano de DISENO, y no se supone: se exige.** El lienzo es de diseno fijo y
`mountFitStage` lo escala con `transform` para caber entero en el editor, asi que un viewport mayor
daria una referencia REESCALADA (borrosa y dependiente del viewport del que regenero). El spec importa
`CANVAS` de `src/contracts/sections.js` — el SSOT, no un numero escrito — lo pone como viewport y ADEMAS
mide que `#app` no tenga transform antes de capturar. Si el diseno cambia y el ajuste deja de ser
identidad, fallan los once tests con un mensaje que lo dice, en vez de dejar nueve referencias
reescaladas. El escenario tambien es fijo y por eso NO se arranca el audio: con el motor encima, el
medidor, el anillo y el LCD se mueven con el reloj del `AudioContext` y la foto seria distinta cada vez.

**El umbral, medido (y el 100 del hermano era ciego aqui).** Tres numeros, no copiados:

1. el RUIDO entre dos corridas seguidas es de **0 pixeles** (la suite entera pasa con `maxDiffPixels: 0`);
2. la regresion mas PEQUENA que se ha podido construir —el distintivo vivo `0/4` de la ficha MODELOS
   pasando de color apagado a acento, mismo tamano, otro color— mueve **77 pixeles**;
3. un desplazamiento de 1 px en la separacion de la rejilla de la ficha mueve **206**.

Con el `maxDiffPixels: 100` de ABDMS2000 esa primera regresion **PASABA en verde**: alli el numero
absoluto cubria capturas de pagina completa (~900.000 px, 0.011%) y aqui cada ficha son ~145.000 px, donde 100
pixeles ya son elceptible. Queda **20** (`threshold: 0.2` sigue siendo lo que absorbe el antialiasing:
un pixel cuenta como distinto a partir del 20% de diferencia), un factor 3.8 por debajo de la regresion mas
pequena y con margen para un build de Chromium que se lleve un punado de pixeles. Con 0 tambien pasa
aqui: subirlo solo con un motivo escrito.

**Lo que NO cubre, y conviene saberlo antes de confiar en un verde.** Las referencias son de
Chromium/Windows y hay que compararlas alli: la pagina usa las fuentes del sistema (`system-ui`, sin
webfont) y su rasterizado cambia entre sistemas operativos (el workflow del hermano lo dice con el mismo
motivo, y por eso su job corre en `windows-latest`). Una referencia de Windows sobre un runner Linux
fallaria por la fuente, no por el codigo. Ademas el alcance es el LIENZO: los cajones, los modales y el
tema escrito en el `title` de un control no tienen referencia, asi que un fallo ahi solo lo pilla el resto
de la suite. Y las referencias son binarios versionados (~715 KB: las dos de pagina entera pesan 296 y
313 KB cada una, las nueve fichas 5-17 KB).

**Dos tests de ctest, y por qué no uno.** `NEURONiK_WebUiVisualRegression` va aparte de
`NEURONiK_WebUiLocalModeE2e` para que regenerar referencias con `--update-snapshots` no toque las del
smoke y para poder dejar la visual fuera mientras se investiga; el E2E de audio invoca su spec y no el
`test` a pelo. Las dos montan su servidor con `npm run build` sobre el MISMO `dist`, asi que medido: con
`ctest -j2` los dos `vite build` a la vez se pisan y el segundo falla al arrancar ("Build failed", 13 s).
Un `RESOURCE_LOCK "neuronik_webui_dist"` de ctest lo arregla sin serializar los otros 39 tests, y cada
suite usa su puerto (`NEURONIK_E2E_PORT`: 5236 el smoke, 5239 la visual) porque con `--strictPort`
compartirlo seria un EADDRINUSE.

Ficheros: `WebUI/e2e/visual.spec.js` (nuevo), `WebUI/e2e/snapshots/*.png` (11 referencias nuevas),
`WebUI/playwright.config.js` (umbral, `snapshotDir`, `snapshotPathTemplate` y puerto por variable de
entorno), `WebUI/package.json` (`test:visual`, `test:visual:update`), `CMakeLists.txt` (el test nuevo, el
`RESOURCE_LOCK` y el spec del E2E de audio) y `WebUI/README.md` (comandos y contrato).
## 2026-09-27 — AGUJA fase 4: el PANIC del medidor, medido (el clic apaga el medidor y silencia las agujas)

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida esta pieza (AGUJA); esta entrada cuenta como se llego a ella.

El medidor de voces de la cabecera es un boton (`.voice-meter`) cuyo clic es PANIC — el gesto doble:
notas apagadas por el bridge Y panico al worklet. Era el unico gesto de la cabecera sin cobertura E2E.
AGUJA lo mide como su CUARTA fase (BridgeSelftest.h: scriptPanicPress + needlePanicPhase en el sondeo).

EL DISENO QUE IMPORTA: tras el release de la fase 3 el motor ya esta MUDO, asi que medir el clic ahi
no demostraria nada (medidor apagado y agujas ocultas seria el estado natural, no el efecto del
gesto). La fase RE-ARMA una nota (scriptKeysAndNoteOn 60, el mismo helper de las fases 1-3), espera
~400 ms a que el medidor LLEGE encendido por telemetria, y SOLO ENTONCES mide: el script captura el
estado del medidor ANTES del clic (meterBefore) y pulsa; el sondeo acotado (patron ZRING, 100 tomas
a 30 ms para la bancada y su frame retrasado) exige que el frame siguiente traiga el medidor APAGADO
(0 leds) y las CUATRO agujas ocultas. La condicion de salida del sondeo exige las dos cosas: agujas
ocultas solas podrian ser la cola natural, el medidor apagado es lo que demuestra el CORTE.

La invariante completa del gesto: medidor ENCENDIDO antes (>= 1 led, medido, no supuesto) -> clic ->
medidor apagado + agujas ocultas. Entra en el AND del veredicto de AGUJA (needleOk = ok && panicOk)
y en su log: `AGUJA: PANIC por el clic en el medidor (1 led(es) antes) -> medidor apagado, agujas
env=off/flt=off (bloques off/off) (ocultas) -> OK`. Verificado en corrida real del Standalone:
las cuatro fases OK y RESULT: OK (selftest-exit=0). Compilado en los tres objetivos (Standalone,
VST3, bancada).

Nota de convivencia: mientras se editaba este fichero, la linea `struct NeedleReading` desaparecio
del arnes en una edicion concurrente (el cuerpo quedo huerfano y el compilador dijo C2065 'reading'
en las firmas de mas abajo, tres errores abajo del C2059 real). Repuesta con la insercion minima.
Si un fichero cambia de mtime entre tu edicion y tu build, relee la zona antes de culpar a tu diff.


## 2026-09-27 — PENDIENTES: todo lo que quedó abierto en esta sesion, con su dueño y su coste

Indice unico de lo que NO quedo cerrado en las diez direcciones de esta sesion (todo lo demas esta
resuelto y verificado; los bloques "**Hecho (2026-09-27)**" del ROADMAP y las entradas anteriores de este
archivo son el historial de lo entregado). Cada punto lleva donde vive, que falta exactamente y quien
tiene que decidir, porque la mayoria no es trabajo incompleto sino **decisiones que son tuyas** y que
se dejaron escritas en vez de inventadas.

Los puntos marcados (medido 2026-09-27) se han vuelto a comprobar hoy contra el arbol; los demas
provienen de la medicion del momento en que se planteo y conviene repetirlos si se toca el gating o los
defaults.

### A. Decisiones que esperan respuesta (no son trabajo a medias: son tuyas)

1. **Conmutador de la ruta local del pad: la elecion no sobrevive al F5.** El estado
   (`state.localMorphRoute`) es de sesion: nace de la siembra y un F5 con el conmutador apagado vuelve a
   apagado, porque el store se crea con `LOCAL_MORPH_Z_ROUTE` como valor inicial. Persistirlo exige
   `localStorage` como la memoria de las ranuras y cambia el arranque de la pagina (versionar la clave,
   que hacer si el LFO elegido ya no esta en la tabla de fuentes). Ofrecido el 2026-09-27, sin respuesta.
2. **OLVIDAR con host: no hay accion de puente que lo haga.** El boton no aparece con `bridgeAvailable`
   porque las ranuras son del preset (`modelPath<slot>`) y el motor las volveria a cargar al recargar el
   proyecto. Vaciarlas de verdad en el plugin es un `unloadModel` en `NEURONiKProcessor` mas su camino
   inverso al `modelsState` (y el `modelError` de reojo, para que el fallo se pinte en el mismo sitio).
3. **CZ101: dos bloques nacen con una celda fuera de fabrica** (VOICE ENGINE `1/6` por `LINE_SELECT`, la
   pagina arranca en Line 1 indice 0 y el contrato dice Line 1+1 indice 2; ARPEGGIATOR `1/9` por
   `ARP_GATE`, la pagina escribe 0.8 y el contrato 0.5). No se toco ninguno: con que valor arranca el
   motor es decision de la pagina. Si el distintivo debe salir en `0/N` al abrir, hay que alinear markup o
   contrato — y eso cambia lo que el motor arranca leyendo, no solo lo que se ve.
4. ~~**Scripts de un solo uso sin trackear**~~ — **RESUELTO (2026-09-28)**: borrado el scratch de
   las sesiones cerradas de los dos repos (los `_live_badges_*.py`, `_edit.py`, `_dbg2.py` de
   `WebUI/`, las sondas `Tests/_inter_probe*.cpp` y `_mod28_probe.mjs` — su target ya no estaba en
   el `CMakeLists.txt`—, mas `_*.md5`, `_*.part.txt`, `_t17_matrix.sh` y los `_apply_*.py` /
   `_fix_*.py` / `patch_entrelazada.py` de la raiz del monorepo) y añadida la REGLA que lo
   evita: `.gitignore` con `/WebUI/_*.py`, `/Tests/_*probe*`, `/_*.md5`, `/_*.part.txt`,
   `/_t*.sh`, `/.freebuff/`, `/.agents/` aqui y los equivalentes en la raiz de la suite. Ya no
   hay que revisar la lista de untracked commit a commit.

### B. Huecos tecnicos conocidos, con el sitio exacto

5. **El gating solo apaga OPCIONES de un choice, no knobs.** Por eso un distintivo `active` puede decir
   `0/2` sobre celdas que el lienzo sigue pintando como vivas. Hoy son 0 celdas, y esta comprobado por que
   (2026-09-27): la unica ficha con distintivo `active` es LFO, y sus diez celdas declaran `engines: 'both'`
   — cuatro floats y seis choices, ninguna gateada. El dia que una ficha con distintivo tenga una celda
   gateada, el gris por celda tiene que ir con el distintivo o los dos contaran historias distintas. Vive en
   `setEngine` / `Source/State/ParameterRegistry`.
6. **Cuatro fichas no pueden colgar distintivo porque no tienen cajon** (medido 2026-09-27 sobre
   `src/contracts/sections.js`): OSCILLATOR, RESONADOR, FILTRO y EFECTOS no declaran `drawer`, asi que no
   hay sitio donde colgar un `liveBadge` ni un boton que abra. Y son justo las que mas se moverian con el
   modo `active` (FILTRO `2/2` → `0/2`, RESONADOR `0/3` → `3/3`, OSCILADOR `9/12` → `6/12`; cifras
   medidas en el navegador el 2026-09-27, a repetir si se toca el gating). Envolvente en un cajon o
   renunciar al dato: es una decision de superficie, no un bug.
7. ~~**ENVOLVENTES tiene cajon pero no declara distintivo, y su numero SI se mueve**~~ —
   **RESUELTO (2026-09-28)**: la ficha declara `liveBadge: { mode: 'active', label: 'ACTIVAS',
   onCard: true }`, cuelga su chip pulsable en la cabecera (abre SU cajon) y las dos cifras estan
   MEDIDAS en el navegador: **8/8 con NEURONiK, 4/8 con NEUROTIK** (las dos ADSR enteras, o solo
   las del filtro). Es el unico caso donde el criterio "si el dato no se mueve, no cuelgues nada"
   no se sostenia, y la incoherencia no la cazaba ningun test: ahora la fija
   `appContract.test.js` ("toda ficha con cajon cuelga distintivo", con la lista de fichas con
   cajon a la vista) y la leen el panel y el E2E del modo local. Referencia visual de la ficha
   regenerada: el cambio son 132 pixeles en una caja de 31x13 en la esquina de su cabecera, y
   nada mas se movio.

### C. Verificaciones que no se hicieron (y por que)

10. **CZ101: el distintivo nunca se ha visto pasar por un bundle de PRODUCCION.** Se verifico en Chromium
    contra la pagina de desarrollo (`index.html` con los 11 parciales), no despues de `npm run bundle` ni de
    `npm run build:css:prod`, que es donde un `querySelector` renombrado o un `:empty` mal colocado se
    rompen en silencio. Offerido el 2026-09-27, sin respuesta. Comandos: `npm run bundle`,
    `npm run build:css:prod`, `npm run validate:css`.
11. **El camino de preset/banco no se ejercita en ningun navegador** (ABDNeural ni CZ101): necesita el
    motor de audio y el reloj del `AudioContext`, que en este navegador no avanza sin `--mute-audio` (y el
    E2E de Playwright si lo lleva, pero no hay caso de preset). Queda cubierto por tests de cableado, no por
    un clic. Con un caso de preset en `e2e/localMode.spec.js` se cerraria.
12. **Las referencias de regresion visual son de Chromium/Windows y hay que compararlas en Windows**: la
    pagina usa las fuentes del sistema (`system-ui`, sin webfont) y su rasterizado cambia entre sistemas.
    No hay workflow de CI para ellas (el hermano ABDMS2000 si lo tiene,
    `.github/workflows/webui-visual-qa.yml`, con `runs-on: windows-latest` y el bootstrap del workspace
    pnpm multi-repo). Copiar ese workflow es el paso que haria que la regresion corra sola.
13. **Tamano de las referencias**: ~715 KB versionados (las dos de pagina entera pesan 296 y 313 KB cada
    una; las nueve fichas, 5-17 KB). Si en este repo pesan mas que lo que aportan, la de tema claro es la
    primera que se recorta (esta cubierta por los tokens, no por la pintura de las fichas).

### D. Lo que NO esta pendiente (para no volver a mirarlo)

- La lista de DIRECTRIONES entregadas esta en los diez ultimos bloques "**Hecho (2026-09-27)**" del
  ROADMAP, cada uno con su medicion. En una linea: voces ociosas al bajar la polifonia, siembra de la ruta
  del pad, memoria local de las ranuras, E2E de navegador, distintivos vivos clicables, modo `active`,
  distintivos de bloque en CZ101, conmutador de la ruta, OLVIDAR de ranura y regresion visual del lienzo.
- `reclaimIdleVoices` (turno 1) existe y `MemoryBudgetTest` lo fija, pero **ninguna UI lo llama**: es
  presupuesto de motor, no un gesto. No es un pendiente, es a donde apunta si algun dia hace falta.
- El `clearLocalModelCache` (borrar las cuatro de golpe) sigue sin UI a proposito: el gesto del usuario es
  olvidar de una en una, que se puede deshacer recargando el fichero.
## 2026-09-27 — las dos copias de cada barra ENV son GEMELAS (resumen vs cajon, misma fila y frame)

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida esta pieza (Barras ENV (gemelidad)); esta entrada cuenta como se llego a ella.

Hasta ahora AGUJA comparaba cada barra contra las agujas (margen 0.08, la geometria del path contra
el frame crudo) pero no las copias ENTRE SI: la barra de la fila n del cajon de la MATRIZ y la de la
fila n del RESUMEN del lienzo podian discrepar sin que nadie lo supiera. Ahora la direccion etiqueta
cada barra con su ORIGEN (drawer vs resumen) y su FILA (data-slot, via bar.closest), y compara por
PAREJA: misma fila, misma envolvente, ambas vivas -> |nivel_a - nivel_b| < 0.01.

Por que el margen es el ESTRICTO de dibujo (0.01, como la gemelidad de las curvas) y no el 0.08 de la
coherencia pagina-motor: las dos copias se repintan del MISMO canal de telemetria (panel.js
paintEnvLevels por frame; modSummary.js con su propio listener sobre los mismos frames), asi que su
desacuerdo no puede ser de datos ni de instante — solo de PINTURA: una copia que dejo de recibir
frames, un setProperty perdido, un selector roto. Dos caminos, el mismo numero: la historia de la
retencion por coherencia de AGUJA, aplicada a las barras.

Dos guardias mas en la asercion: exige al menos UNA pareja viva por lado (si el array viniera partido
— un selector que deja de casar —, un vacio no puede dar OK gratis) y entra en el AND del veredicto
de AGUJA y en su log: `barras 8 (OK, gemelas OK)`. Verificado en corrida real del Standalone
(RESULT: OK, selftest-exit=0), compilado en los tres objetivos; la bancada lo mide en su proxima
pasada.
## 2026-09-27 — el medidor de voces es un BOTON PANIC: tooltip dinamico y el role de a11y arreglado

El medidor de voces de la cabecera (`.voice-meter`) llevaba tiempo siendo clic a la vista, pero su
marcado mentia: un `<div role="img">` — una IMAGEN para el arbol de accesibilidad. Consecuencias
reales de ese role: un lector de pantalla lo anunciaba como imagen (nada de boton, nada de accion),
no entra en el orden de tabulacion por si mismo, y el clic mas importante de la cabecera (parar TODO
el sonido) era invisible para quien no usa el raton.

EL ARREGLO, en panel.js: el elemento es un `<button type="button">` nativo — role implicito de
boton, foco con teclado (la CSS le da `:focus-visible` con el trazo del acento, porque los leds solos
no dan afordancia de foco), activable con Enter/Espacio y anunciado con el nombre de SU ACCION. El
look no cambia: `appearance: none`, sin borde ni fondo — son leds, el tooltip y el cursor dicen
"se puede pulsar", no un boton rectangular de browser.

EL TOOLTIP DINAMICO lo escribe `setVoiceMeter(count)` en cada conteo de voces, y describe la accion
CON EL NUMERO VIVO de lo que el clic va a parar:

    `PANIC: parar ${voices} ${voices === 1 ? 'voz activa' : 'voces activas'}`

El mismo texto va a los DOS sitios: el `aria-label` (el nombre accesible del boton) y el `title`
(el tooltip visual del raton) — la misma verdad para quien ve y para quien escucha. Con cero voces
el boton se OCULTA (`hidden = voices === 0`): un PANIC sin nada sonando no se ensena. Detalle de
polifonia: el motor puede cantar MAS voces que leds (MAX_VOICES_UI = 8); el numero real viaja en el
label sin inventar leds — 10 voces activas son 8 leds encendidos y un "parar 10 voces activas".

EL GESTO que dispara es el PANIC doble de la franja (`handlers.onPanic`): notas apagadas por el
bridge Y panico al worklet. Ese clic ya lo mide la direccion AGUJA en su fase 4 (re-arma una nota,
clic sobre el medidor ENCENDIDO, el frame siguiente trae el medidor apagado y las agujas ocultas) —
asi que la superficie a11y que arreglamos aqui es exactamente la que el E2E pulsa.

Cobertura: `panel.test.js` (medidor y leds), `e2e/localMode.spec.js` (el label con el conteo vivo:
"PANIC: parar 1 voz activa" con la nota sonando, 0 leds activos tras el corte) y la fase 4 de AGUJA
en el arnes. Ficheros: `WebUI/src/ui/panel.js` (button + setVoiceMeter) y
`WebUI/src/styles/main.css` (la afordancia: cursor, focus-visible).

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida la fase PANIC de AGUJA, y
> su subseccion «Medidor-PANIC (canon)» fija el marcado (el boton, los 8 leds, `hidden` y
> `data-active`), el label dinamico y el paso a paso de la fase 4 que lo pulsa; esta entrada cuenta
> como se llego a el.

## 2026-09-27 — la ayuda contextual de gestos del cajón de MODELOS y su encaje con los tooltips cortos

La ficha MODELOS tiene una particularidad de layout que dicta donde vive su ayuda: el pad XY vive en
el LIENZO (la vista model-xy) y el cajón de la ficha muestra el espectral y las cuatro ranuras — el
usuario edita el eje z del pad (knobs de morphZ/capas) DENTRO del cajón, lejos del widget que arrastra.
De ahí que la ayuda contextual de gestos viva EN EL CAJÓN: `details.model-help` en ui/visuals.js,
"AYUDA CONTEXTUAL: los gestos del pad (que vive en el LIENZO de esta misma ficha) documentados donde
el usuario edita su eje z". El comentario del código lo dice y esta entrada lo desarrolla.

LA PIEZA: un `<details>` nativo (plegado por defecto, accesible y funcional sin una línea de CSS)
con resumen "Gestos del pad XY" y cuatro items, en la pareja termino/descripción (`<strong>text</strong>`):
- "Pad: arrastrar = mover el punto (morphX/morphY); con Shift, movimiento relativo a 1/10";
- "Aro (morphZ): arrastrar = girar hasta el valor; con Shift, fino 1/10 por el camino corto";
- "Aro (teclado): flechas = ±1% · RePag/AvPag = ±10% · Inicio/Fin = 0/100% (con foco en el aro)";
- "Esquinas A–D: pulsar una ranura cargada abre el cajón de MODELOS en esa ranura (Enter/Space con foco)".

LA VERDAD VIGENTE DEL ITEM "Esquinas A-D" (lo que esta entrada fija): la esquina ya NO salta el pad —
abre ESTE cajón en SU ranura (ui/xyPad.js, onCornerClick; Enter/Space con foco, porque la esquina
lleva role=button y tabindex). Es la historia de la entrada "Esquinas A-D clicables" de este diario:
el item decía "salta el pad" cuando la verdad de hoy es la inversa, y el texto se corrigió al gesto
real. La corrección se comprobó en dos niveles: la aserción del test (xyPad.test.js: el cuarto item
contiene "abre el cajón de MODELOS") y los tooltips de la esquina (xyPad.js: con nombre ->
"Abrir MODELOS: ranura B "nombre""; con divergencia -> "“nombre”: el motor no ha podido cargar el
fichero (¿movido o borrado?) · Clic: abrir MODELOS").

EL ENCAJE CON LOS TOOLTIPS CORTOS — la regla de las dos capas, escrita en el propio código (visuals.js:
"los títulos de pad y aro son la versión corta de esto"): la superficie (pad y aro, xyPad.js) lleva
tooltips de UNA LINEA que se leen al pasar el ratón ("Arrastrar = mover · Shift = fino (1/10)", "Arrastrar
= girar · Shift = fino (1/10)"); el cajón lleva la referencia COMPLETA — misma información mas el
mapa de teclado del aro y el gesto nuevo de las esquinas, que no cabe en un title. Mismo convenio que
modelSlots.js: los títulos cortos informan de estado puntual (divergencia, disponibilidad de OLVIDAR),
la ayuda del cajón documenta el GESTO completo. Dos capas, una sola verdad: si un tooltip corto y la
ayuda discrepan, es un bug — la aserción del test ata el item de esquinas al gesto real y la ayuda muere
con la vista (destroy hace help.remove()).

La CSS (styles/main.css, .model-help) es deliberadamente tenue: tipografía más pequeña, el tono
apagado de las etiquetas de celda y el summary en mayúsculas con cursor pointer — información, no
acción. Cobertura: xyPad.test.js (plegado por defecto, el summary, los CUATRO items con sus términos,
la aserción de la verdad vigente de esquinas, que abrir es un gesto del usuario y que destroy la
desmonta). Fichero: WebUI/src/ui/visuals.js (la pieza entera vive aqui, en la rama model-slots).

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida el gesto de esquinas que
> esta ayuda documenta (MORPH, direccion 12); esta entrada cuenta como se llego a el.

## 2026-09-27 — la aguja del MODO NAVEGADOR tiene verificacion visual E2E (WebUI/needle-probe)

En el PLUGIN la aguja vive del canal bridge y la cubre la direccion AGUJA del arnes; en el navegador
el MISMO par (envelopes=[amp, filter]) viaja por el meter del worklet (onWorkletEnvelopeLevels) y no
tenia verificacion visual: el smoke mira el meter y el anillo, pero nadie habia mirado los paths de
las agujas leyendo el motor WASM de verdad. La pieza nueva: `WebUI/needle-probe/` (index.html +
needleProbe.js), una pagina de PRUEBA que monta las DOS vistas de produccion (envelope-curves del
lienzo y envelope-blocks del cajon: las CUATRO agujas) con los view-models del contrato
(describeControl sobre el generado) y el snapshot por defecto, arranca el motor WASM con SOUND ON y
alimenta las agujas por el MISMO cable que app.js — cero logicas gemelas.

EL TRUCO DE LA PAGINA: `<base href="/">`. El motor resuelve worklet/ y el .wasm contra
document.baseURI; la pagina vive bajo /needle-probe/ y sin el base el fetch del worklet iria a 404.
Con el base en la raiz, la resolucion es identica a la del plugin. `window.__needles()` devuelve las
cuatro agujas {visible, y, level} con la MISMA escala que el arnes (viewBox 100x48, PAD 2), y
`window.__probeReady` cierra el montaje.

El spec (`WebUI/e2e/needleProbe.spec.js`, dos casos sobre el DEV server — puerto 5237, segunda
entrada de playwright.config.js, NO entra en dist porque es una pagina de prueba y necesita los
fuentes sueltos): (1) sin motor, las cuatro agujas EXISTEN pero OCULTAS; (2) SOUND ON + nota por el
mismo mensaje neuronik:midi que manda el teclado -> las cuatro VISIBLES con nivel > 0.2 (el sustain
del contrato vive en 0.7: una aguja pintando la cola del attack no pasa), GEMELAS entre vistas
(|lienzo - cajon| <= 0.02, el re-parseo del path solo puede meter redondeo), y tras note-off las
cuatro OCULTAS (la cola baja del suelo del dibujo). El guard del reloj de audio es el mismo del
smoke (skip honesto si el entorno no procesa).

DOS lecciones de la puesta en verde, para no repetirlas: `expect.poll(...).toSatisfy` NO existe en
Playwright 1.63 — se envuelve el predicado en .then(...) y se aserte con toBe(true). Y el hallazgo
de la jornada: en modo local el worklet solo canaliza GlobalParams (matriz, LFOs, FX) y el morph —
NO hay canal de VoiceParams/ADSR —, asi que las dos envolventes viven en los defaults de C++
(sustain 0.7/0.7), los knobs de envolvente de la pagina no llegan al motor local y NO se puede
exigir amp != filtro sobre los defaults (el intento con filterSustain=0.3 por neuronik:params lo
dejo medido: el motor ni se entera). El feed de la aguja sigue siendo REAL (_neuronikGetEnvelopeLevels
del motor); lo que falta es la plomeria de ADSR al worklet — pendiente del dueno del DSP, no de esta
pagina.

> **CADUCADO (2026-09-28)**: la plomeria de ADSR al worklet YA EXISTE — es el canal `neuronik:voice`
> (POD de ocho floats en la frontera + `setVoiceEnvelope` en los dos motores) con su `pushVoiceToWorklet`
> en el motor de la pagina. La entrada se conserva tal como estaba el dia; la prueba que faltaba
> (exigir amp != filtro) se escribio de verdad esa misma tarde, en la entrada de abajo.

Verificado: needleProbe 2/2 y la suite E2E COMPLETA 22/22 (smoke + regresion visual del mismo arbol,
el fallo aislado del conmutador fue un flake de timing y pasa en la corrida completa). Ficheros:
`WebUI/needle-probe/index.html`, `WebUI/needle-probe/needleProbe.js`, `WebUI/e2e/needleProbe.spec.js`
y `WebUI/playwright.config.js` (el webServer paso a ARRAY con el dev de la pagina).

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida la AGUJA del plugin; esta
> entrada cubre su hermana del modo navegador, que comparte vistas y escala pero viaja por el meter.

## 2026-09-27 — ESQUINA: la calle de vuelta del pad entra en el arnes (la direccion 7b)

El gesto inverso que las esquinas clicables nacieron para dar ya tenia wiring (ui/xyPad.js
onCornerClick -> app.js setCornerOpener -> panel.openDrawerRoute('models', slot)) y test unitario,
pero ninguna direccion del arnes lo pulsaba en la pagina viva del plugin. La nueva ESQUINA lo mide:
un CLIC en la esquina A del pad (la ranura 0, cargada desde el arranque) tiene que abrir el cajon de
MODELOS con SU velo y la ranura 0 resaltada en data-slot-visual — la numeracion 0-based del motor,
espejo del data-slot 1-based de las filas de celdas — y con el nombre de la ranura en la fila
resaltada. Encaje: corre DETRAS de MORPH (que deja el pad pintado con el modelo REAL en D) y ANTES
de ZRING (que necesita la matriz abierta: ESQUINA la re-abre por el APVTS tras cerrar el cajon de
MODELOS por ID, el settle de siempre). El veredicto (cornerOk) entra en el AND de finish().

TRES trampas que la puesta en verde dejo medidas:

1. **La ranura de la esquina NO esta en el DOM.** El componente compartido no escribe data-slot en
   las esquinas: su indice se deriva de la POSICION (`['tl','tr','bl','br'].indexOf(corner.dataset
   .corner)`). El guion del arnes lo deriva IGUAL que produccion — first CORNER_NOT_CLICKABLE fue
   el arnes buscando un atributo que nadie pinta.
2. **El texto de la esquina es el NOMBRE del modelo, no la letra.** Comparar contra "A" era una
   asercion falsa (salio "CZ-BASS1" -> FAIL). Ahora se compara contra modelName(0), lo que anade
   gratis la comprobacion de que esquina y ranura del cajon siguen hablando del mismo motor.
3. **El panic nativo es un allNotesOff y la cola de la sesion es ALEATORIA.** ACCIONES RANDOM mueve
   envRelease antes de AGUJA, asi que "nota fuera" (fase 3) puede salir con la voz todavia sonando:
   el re-arm apilaba una segunda voz (2 leds) y el presupuesto corto de la fase PANIC se quedaba
   con el medidor ENCENDIDO. Arreglo de presupuesto y de forma: needleWaitQuiet (silenciar y
   esperar medidor APAGADO) y needleWaitArmed (clic sobre >= 1 led MEDIDO, no sobre un delay) — la
   invariante queda "armado medido -> clic -> NADA sigue sonando", con 12 s (400 tomas) para la
   cola aleatoria. La correccion del id viejo `drawer-global` -> `drawer-globalFull` en el cierre
   de la cancelacion salio de la misma releida.

Verificado en corrida real del Standalone: las 14 direcciones OK (selftest-exit=0), ESQUINA con su
linea de log (`clic en la esquina A ("CZ-BASS1", esperado "CZ-BASS1") -> cajon "drawer-models"...
ranura resaltada 0 ... -> OK`) y PANIC de vuelta en OK. Compilado en los tres objetivos. La bancada
mide las 14 en su proxima pasada. Nota de procedimiento: el C1075 de esta edicion se localizo con
un conteo de parentesis por linea (delta 1 = falta UN cierre) — con anidamientos de seis niveles,
el balance de pares antes de compilar ahorra un ciclo.

## 2026-09-27 — RESUMEN-RUTAS, la esquina clicable y la ayuda de gestos corregida: las tres caras del mismo viaje

Tres piezas nacidas el mismo dia y pensadas juntas: la direccion del arnes que mide la segunda via de
salto (RESUMEN-RUTAS), el gesto inverso de las esquinas del pad (clic -> cajon de MODELOS) y la
correccion de la ayuda contextual que le dice al usuario como se llama ese gesto hoy.

**1. RESUMEN-RUTAS (direccion 5 del canon, Stage::summaryRoutes).** El resumen de matriz (la banda
`mod-summary` del fondo del lienzo) pinta las cuatro rutas y cada fila es un BOTON que abre el cajon
en SU slot — el mismo salto que ENV-RUTAS por otra vista, y por eso se mide en otra direccion. El
guion (scriptSummaryRouteJump): elige la fila del slot `avoidSlot % 4 + 1` — el slot que ENV-RUTAS ya
medio NO repite cobertura —, pulsa SU boton y exige: cajon de la MATRIZ abierto con SU velo, SU fila
`drawer-slot` resaltada (data-slot-highlight), y el cruce de verdad de la direccion: los dos selects
del slot pulsado (`mod<n>Source/Destination`) ensenan el indice que el APVTS tiene en ESOS
parametros — lo pintado contra el motor, sin hardcodear. Encaje: cualquier salida de VOLVER cae en
ella (con el slot como avoid: un segundo slot medido), y al terminar hace el settle del lienzo
(scriptSettleDrawers: cierra todo cajon abierto por su X, iterando porque la vuelta del retorno puede
re-abrir el origen) y encadena AGUJA. Su FAIL deriva a AGUJA — nunca dos AGUJAS en paralelo (la
leccion del doble encadenado, ya escrita).

**2. La esquina clicable (el gesto que hoy mide la direccion ESQUINA).** El wiring en tres capas:
xyPad.js declara `onCornerClick` y SEPARA el gesto de abrir del gesto de morfeo (stopPropagation en
captura sobre el pointerdown de la esquina: el pulgar no salta al punto, no nace begin/change/end),
la esquina solo es abrible con modelo cargado (dataset `clickable`, role=button, tabindex, Enter/Space
con foco) y app.js la cablea con el slot 0-BASED (`setCornerOpener -> openDrawerRoute('models',
slot)`; A=0 como el motor; el panel lo traduce a fila del cajon). La cobertura a tres niveles: 5
aserciones de esquinas en xyPad.test.js (nombres A-D desde state.models, el viaje al opener con el
slot del motor, el clic NO morfea, Enter/Space y esquina vacia NO abrible), la direccion ESQUINA del
arnes en la pagina viva del plugin, y los tooltips de esquina (normal y divergente) como version
corta de la ayuda.

**3. La correccion de la ayuda de gestos (visuals.js, el <details> del cajon de MODELOS).** El item
"Esquinas A-D" decia que la esquina SALTA EL PAD; la verdad vigente es la INVERSA — abre el cajon de
MODELOS en su ranura (el gesto inverso a IR A LA RUTA) —, asi que el texto se reescribio a "pulsar
una ranura cargada abre el cajon de MODELOS en esa ranura (Enter/Space con foco)" y la asercion del
test se actualizo a CONTENER "abre el cajon de MODELOS". Moraleja que deja escrita: la ayuda que
documenta un gesto VIVO caduca cuando el gesto cambia — y la asercion del test es lo que la obliga a
enterrarse. Las tres piezas comparten la misma regla de oro: el slot de la esquina viaja 0-based por
datos, el resalte del cajon se pinta en la numeracion que exista (data-slot-visual 0-based en MODELOS,
data-slot 1-based en celdas) y ninguna capa hardcodea letras ni filas.

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida RESUMEN-RUTAS (5) y
> ESQUINA (13); esta entrada junta las tres piezas y cuenta como se llego a ellas.

## 2026-09-27 — el DUAL-PATH de las envolventes (bridge nativo vs meter WASM) y el unsubscribe de los onWorklet*

Las agujas de las curvas ADSR y las barras ENV comen del MISMO par `envelopes=[amp, filter]`, y ese
par llega por DOS caminos que nunca viven a la vez (la policy de audio lo garantiza: dentro de un
host el worklet no arranca — status blocked — y en el navegador no hay bridge):

- **PLUGIN (bridge nativo).** El procesador publica los niveles REALES en cada processBlock
  (getEnvelopeLevelsForUI) y el puente los manda en el frame de telemetria `envelopes: [amp, filter]`
  a ~15 Hz. Las vistas lo consumen por el canal que el store les inyecta al crearse
  (`onTelemetry: store.onTelemetry`): cada curva pinta SU aguja con setLevel, y las barras del cajon
  y del resumen repintan del MISMO frame. Es el camino que la direccion AGUJA mide (coherencia con
  el motor, gemelidad, PANIC).
- **NAVEGADOR (meter WASM).** El worklet lee los niveles del motor con
  `_neuronikGetEnvelopeLevels` y los manda en su `neuronik:meter` (~21 ms de cadencia, junto con
  voices, lfo1 y morphZMod: un mensaje, varios consumidores). `audioWorkletEngine` lo reparte en
  TRES listas de listeners (`onWorkletEnvelopeLevels`, `onWorkletVoices`, `onWorkletMorphZ`) y
  `app.js` cablea las cuatro agujas de las DOS vistas (`needleFor('env'|'filter').setLevel`) igual
  que el E2E de needle-probe verifica.

EL EMBUDO que hace honesto el dual-path: en local, el mismo handler que pinta las agujas ALIMENTA el
canal de telemetria del store (`pushTelemetryFrame({ spectral: SILENT_SPECTRAL_FRAME, envelopes:
[amp, filter] })`). Asi TODAS las vistas que leen `frame.envelopes` (las barras de la MATRIZ, el
espectral) funcionan identicas en los dos mundos: un solo contrato de frame, dos fuentes. Lo que la
pagina NO hace es fingir el resto del frame — el espectral local va silencio, no inventa parciales.

**El unsubscribe** (la pieza de higiene que faltaba): los tres `onWorklet*` devuelven su REMOVER
(`indexOf` + `splice` sobre su lista). Sin el, un suscriptor que muere (una vista destruida, un test
que termina, una pagina que se re-monta) se quedaba ligado al meter para siempre: cada frame volvia
a pintar nodos fuera del DOM y los tests acumulaban listeners falsos. Los tests lo fijan en
`workletMorph.test.js`: el meter SIGUE LLEGANDO tras el unsubscribe y el listener ya no ve nada
("el meter sigue llegando pero nadie lo escucha"), los frames malformados (sin envelopes o con el
array incompleto) NO envenenan a los suscriptores, y `onWorkletVoices` trae su propio remover con el
mismo contrato. app.js se suscribe para la vida del proceso (la pagina es la aplicacion), pero la
API existe para todo lo que puede morir: vistas, pruebas y futuros remontajes.

Ficheros: `WebUI/src/audio/audioWorkletEngine.js` (el fan-out del meter + los tres removers),
`WebUI/src/app.js` (el cable dual: needleFor x4 + pushTelemetryFrame en local) y
`WebUI/tests/workletMorph.test.js` (los tres tests de suscripcion). Cobertura cruzada del par: AGUJA
en el plugin (bridge), needle-probe 2/2 en el navegador (meter), y las invariantes del canon para
los dos.

> Canon: la "SECCION CANONICA — las 15 direcciones del selftest" consolida AGUJA y las barras ENV
> (los consumidores del par); esta entrada documenta el TRANSPORTE dual que los alimenta.

## SECCION CANONICA — las 15 direcciones del selftest: indice, AGUJA, barras ENV, medidor-PANIC y MIDI-CC

Referencia unica y vigente de lo que mide el arnes (Source/WebUI/BridgeSelftest.h). Las entradas
cronologicas de este fichero cuentan COMO se llego a cada pieza; donde una entrada vieja y esta
seccion discrepen, manda esta. Resumen por direccion al final de cada pasada: `Resumen del selftest
(por direccion)` en build-last-run.log (lo parsea Scripts/selftest_summary.ps1).

### Presupuesto (canon): como escalarlo sin recompilar

El watchdog y TODAS las tomas del arnes (ZRING 540/30/320, AGUJA y sus sondeos) cuelgan de un
presupuesto base de 150 s (`defaultTimeoutMs`). La env var `NEURONIK_SELFTEST_BUDGET` en
SEGUNDOS escala TODO por su factor (segundos / 150): el constructor de BridgeSelftest la lee
al arrancar y deja la linea de evidencia (`[selftest] presupuesto por
NEURONIK_SELFTEST_BUDGET: 300 s (factor 2.00: watchdog 300 s, ZRING 1080/60/640 tomas)`).
Valor ausente, no numerico o <= 0: los numeros de siempre. Es un FACTOR, no un tope: las
colectas salen temprano, asi que un presupuesto mayor nunca ralentiza una pasada sana — solo
amplia el techo del peor caso. NO se escalan los gates fisicos de ZRING (300..800 ms, ligados
al LFO de 1 Hz) ni el pacing de 30 ms: son propiedades del motor, no del host. Uso: fijar
`$env:NEURONIK_SELFTEST_BUDGET` junto a `NEURONIK_SELFTEST_LOG` antes de lanzar el Standalone
con `--selftest`.

### Indice (orden de corrida)

| # | Direccion | Que mide, en una linea |
|---|---|---|
| 1 | MODELOS | los 6 assets del banco se releen con el lector de produccion y los 4 entran en las ranuras A-D; la pagina ensena los nombres |
| 2 | MATRIZ | el cajon abre con 4 rutas y 12 celdas, la ruta 1 en uso; queda ABIERTO a proposito para las siguientes |
| 3 | ENV-RUTAS | la fila ENV de la ficha ENVOLVENTES salta a la MATRIZ en SU slot (derivado de lo que PINTA la pagina, no del APVTS) |
| 4 | VOLVER | el retorno es reversible: el boton reabre la matriz en el MISMO slot; y la CANCELACION: abrir GLOBAL mata el retorno y el boton no reviva (detalle en «SECCION CANONICA — VOLVER (direccion 1b-bis)») |
| 5 | RESUMEN-RUTAS | una fila del resumen (slot DISTINTO al de ENV-RUTAS) abre la matriz en SU slot, cruzado con el APVTS; deja el lienzo settle |
| 6 | AGUJA | las agujas del frame pintan sobre las 4 curvas ADSR, coherentes con el motor, con el medidor, las barras y el PANIC (detalle abajo) |
| 7 | NATIVO -> JS | mover masterLevel por el APVTS se ve en el slider de la pagina |
| 8 | JS -> NATIVO | un input real sobre el slider llega al APVTS |
| 9 | GENERAL | los 11 ids de la pestana GENERAL llegan al estado de la pagina |
| 10 | MIDI | una nota de la pagina entra al motor y la rueda de mod nativa se refleja en la pagina |
| 10b | MIDI-CC | el menu MIDI CONTROL del LCD arma el learn de CUTOFF, un CC del motor gana la asignacion y ese mismo CC mueve el parametro (detalle abajo) |
| 11 | ACCIONES | RANDOM por SU boton mueve el APVTS y el pie publica lo mismo; con freezeResonator respeta los congelados |
| 12 | MORPH | esquinas A-D del pad, lectura del estado del pad, gesto pad+aro y hit-testing en la pagina viva |
| 13 | ESQUINA | el clic en la esquina A del pad (ranura 0) abre el cajon de MODELOS con SU ranura resaltada (data-slot-visual), y el clic en una esquina VACIA no abre ni resalta (detalle abajo) |
| 14 | ZRING | la ruta nativa LFO 2 -> Morph Z gira el anillo con arco CON SIGNO; control negativo (fuente Off) en cero |

Encadenado: por DATOS (el slot viaja de ENV-RUTAS a VOLVER y a RESUMEN-RUTAS como avoid), nunca un
guion fijo; todo fallo encadena a la direccion siguiente, nunca dos AGUJAS en paralelo.

**Subsecciones de esta seccion** (guion e invariantes de cada direccion, en orden de corrida):
MATRIZ, MODELOS, ENV-RUTAS, RESUMEN-RUTAS, AGUJA, NATIVO -> JS, JS -> NATIVO, GENERAL, MIDI,
MIDI-CC, ACCIONES, MORPH, ESQUINA, ZRING. Mas cuatro transversales que no son direcciones: las barras
ENV (medidas DENTRO de AGUJA), el medidor-PANIC de la cabecera (tambien dentro de AGUJA: su marcado
es contrato de la fase 4), la fila del LCD (fase 0 de MIDI-CC) y los sondeos compartidos
(`PageWait`, la maquinaria de espera que usan AGUJA y MIDI-CC). VOLVER tiene seccion canonica
propia al final del fichero.

### AGUJA (canon): las CUATRO fases y sus invariantes

La nota entra por el teclado de la pagina (el gesto de un usuario) y el frame de telemetria
(`envelopes: [amp, filter]` a ~15 Hz) pinta la aguja horizontal sobre cada curva ADSR en las DOS
vistas (lienzo `.env-curves`, cajon `.env-blocks`).

1. SILENCIO: las cuatro agujas OCULTAS (el frame viaja; el nivel esta bajo el suelo del dibujo).
2. SOSTENIDO: las cuatro VISIBLES y COHERENTES con el motor (`getEnvelopeLevelsForUI`,
   margen 0.08); el sondeo RETIENE la toma mas coherente (minimo desfase pagina-motor leido EN el
   mismo instante: en la bancada el frame llega 1-2 periodos tarde y "la de nivel mas alto" cazaba
   attack contra sustain). GEMELIDAD de las 4 curvas entre vistas: |lienzo - cajon| < 0.01. El
   medidor de voces acompana: >= 1 led.
3. RELEASE: ocultas, o en cola DESCENDIENDO en coherencia con el motor (la ADSR de la sesion puede
   tener cola larga; no se exige cero seco).
4. PANIC: se RE-ARMA una nota (tras el release el motor ya esta mudo: sin re-arme, un silencio no
   demuestra nada), se espera a que el medidor LLEGE encendido, y el CLIC en el medidor
   (`.voice-meter`, el gesto doble: bridge + worklet) tiene que dejar el medidor APAGADO (0 leds) y
   las cuatro agujas OCULTAS en el frame siguiente (sondeo de 100 tomas x 30 ms). La condicion de
   salida exige medidor apagado Y agujas ocultas: lo segundo solo podria ser la cola natural.

   Las dos esperas que la fase PANIC necesita (medidor apagado ANTES de re-armar, medidor encendido
   DESPUES) no son bucles propios: son predicados sobre el sondeo compartido **`PageWait`** (ver
   "Sondeos compartidos (canon)" mas abajo), que devuelve ademas si el predicado llego a cumplirse.
   Ese flag es lo que permite distinguir "el PANIC no apago el medidor" de "el re-arm no llego a
   sonar", que antes salian con el mismo texto y costaban un buen rato de diagnostico.

   El medidor que esta fase pulsa es contrato del arnes, no pintura libre: su marcado, su label
   dinamico y el paso a paso del corte estan en «Medidor-PANIC (canon)» mas abajo.

Veredicto: el AND de las cuatro fases (`needleOk`), con la linea de log de la direccion como
evidencia: `... medidor 1 led(s), barras 8 (OK, gemelas OK) ... (cantando); nota fuera ... -> OK`.

### Barras ENV (canon): la pieza y sus TRES invariantes

Cada fila de la MATRIZ (cajon y RESUMEN del lienzo) lleva una barra fina (`.drawer-slot__env-level`)
que se rellena con el nivel de la envolvente que su fuente tiene asignada (ENV 1 -> amp, ENV 2 ->
filtro; otras fuentes: sin barra). La DECISION (que filas viven) corre por snapshot en cada paint;
el NIVEL lo repinta el canal de telemetria por frame (`panel.js::paintEnvLevels`; `modSummary.js`
con su propio listener sobre los mismos frames). CSS unico: relleno por `--env-level`, oculta con
`data-live='false'`. Legibles con el cajon CERRADO (el mueble mantiene el DOM con `inert`).

Las tres invariantes, medidas DENTRO de AGUJA (mismas lecturas, entran en su veredicto):

1. COHERENCIA con las agujas (margen 0.08): en el sostenido, toda barra viva pinta el nivel de SU
   envolvente del MISMO frame que las agujas (pintar la hermana o un frame viejo rompe el OK).
2. APAGADO tras el release: las vivas bajan con su envolvente, criterio de cola de las agujas:
   barra < sostenido * 0.5 Y |barra - nativo_ahora| < 0.08 (no se queda clavada).
3. GEMELIDAD entre copias (margen 0.01, el estricto de dibujo): la barra de la fila n del cajon y
   la de la fila n del resumen se repintan del MISMO canal; su desacuerdo solo puede ser de
   PINTURA, no de datos ni de instante. Se compara por PAREJA de fila (ambas vivas, misma
   envolvente), con guardias de al menos una pareja viva por lado (un array partido no da OK
   gratis).

Evidencia en el log: `barras 8 (OK, gemelas OK)` (4 del cajon + 4 del resumen).

### Medidor-PANIC (canon): ocho leds que son un BOTON, y la fase 4 que lo pulsa

Pieza transversal, medida DENTRO de AGUJA (fase 4) como las barras ENV. Vive en la fila de audio de
la cabecera (`panel.js`), fuera del mueble: se ve con el cajon cerrado. Que el arnes lo mida no es un
detalle: el medidor es a la vez el estado ENCENDIDO que la fase 4 tiene que romper y el estado APAGADO
que prueba el corte, asi que su marcado ES contrato del arnes, no pintura libre.

**Marcado**: un unico `<button type="button" class="voice-meter">` con `MAX_VOICES_UI = 8` hijos
`<span class="voice-meter__led" data-led="1..8">`. Tres estados, los tres legibles del DOM sin
preguntarle nada al motor:

| Atributo | Donde | Que dice |
|---|---|---|
| `hidden` | el boton | 0 voces: el medidor desaparece entero (no queda un boton vacio) |
| `data-active` | cada led | `"true"` si `index <= voices`: encendido — es lo que CUENTA el arnes |
| `data-led` | cada led | posicion 1..8: la usa el propio pintado para saber cual enciende (`index <= voices`) |

El conteo del arnes es `children` con `data-active === 'true'` — no lee `data-led` — asi que un led
de mas sin `data-active` no cuenta, y un led encendido con el atributo ausente tampoco.

El tope de 8 es del HOST (el Standalone declara 8 voces), no del motor: si la polifonia real lo
supera, el medidor NO inventa leds — el numero entero va en el label, y al arnes solo le importa
`>= 1`. CSS en `main.css`: `appearance: none`, fondo transparente y sin borde (la afordancia son
los leds, el `cursor: pointer` y el tooltip), con `box-shadow` en el led activo y
`:focus-visible` outlines para el teclado.

**Label dinamico**: `setVoiceMeter(count)` reescribe a la vez `aria-label` y `title` con la ACCION
y el conteo vivo — `PANIC: parar N voz activa` / `PANIC: parar N voces activas`, con el singular
separado en el codigo — y en reposo nace `PANIC: parar todas las voces`. Lo alimenta `frame.voices`:
del frame NATIVO en plugin, del meter del WORKLET en local (mismo patron que la aguja de las curvas
o el anillo morphZ: pintura en vivo, no estado del snapshot). Los dos atributos llevan el MISMO
texto a proposito: el boton no tiene texto visible, asi que el nombre accesible y el tooltip son su
unica descripcion, y si divergieran uno de los dos estaria mintiendo. La intencion queda visible sin
abrir la consola: antes el tooltip era generico y no decia cuantas voces iba a parar.

**El clic es el gesto DOBLE**: el mismo `handlers.onPanic` que el boton `PANIC` de la franja del
teclado — notas apagadas por el bridge Y panico al worklet. Son dos superficies para una sola accion;
el arnes mide ESTA, no la franja.

**La fase 4, paso a paso** (por que vive aqui y no solo dentro de AGUJA):

1. `scriptNoteOff (60)` defensivo + `needleWaitQuiet` (400 tomas x 30 ms ≈ 12 s): la cola del
   release de la fase 3 puede seguir sonando, y apilar el re-arm encima mediria el PANIC sobre dos
   voces. El presupuesto de 12 s es el del panic nativo (`allNotesOff`: las voces mueren dentro de
   SU cola). Si no se calma, lo DICE y sigue — es advertencia, no fallo.
2. `scriptKeysAndNoteOn (60, 0.9f)`; si no devuelve `ON_SENT` → FAIL `PANIC sin nota que parar
   (re-arm: …)`. Sin re-arme no hay voz que cortar.
3. `needleWaitArmed` (100 tomas ≈ 3 s): espera `>= 1` led MEDIDO, no un delay a ciegas. El flag
   viaja a la cadena, que es lo que distingue "el PANIC no apago nada" de "el re-arm no llego a sonar".
4. `scriptPanicPress()`: `document.querySelector('.voice-meter')`; si no existe,
   `{error: 'NO_VOICE_METER'}`. Lee `hidden` y el numero de leds con `data-active === 'true'`
   **ANTES** del clic y lo devuelve como `meterBefore`; si ese objeto falta, `meterWasHidden` se
   toma `true` y la fase falla — no se presume nada. Luego `meter.click()`.
5. `needleSample` (400 tomas ≈ 12 s) con `needlePanicPhase = true`, que manda la lectura a
   `needlePanicReading` y cambia el predicado del sondeo a "medidor apagado Y las cuatro ocultas".

El OK es el AND de seis terminos: clic sin `error`, `meterBefore` NO oculto con `>= 1` led,
despues oculto con 0 leds, y las cuatro agujas ocultas. El que la hace NO vacua es el "estaba
encendido ANTES": sin el, un PANIC que no apague nada pasaria por los otros dos (medidor apagado ya
por la cola natural + agujas ocultas por la misma cola).

Del sondeo, dos detalles que se leen mal si se supone el patron de las fases 1-3: en fase PANIC el
predicado es "medidor apagado Y las cuatro ocultas" (las solas no bastarian) y el bucle SALE en el
primer frame que lo cumple — la lectura que se juzga es ese mismo frame, asi que medidor y agujas son
del MISMO instante, que es la invariante que importa. Los 12 s son el techo del peor caso (un panic
nativo que tarda en cortar), no la duracion normal.

Evidencia en el log:
`PANIC por el clic en el medidor (1 led(es) antes) -> medidor apagado, agujas env=off/flt=off (bloques off/off) (ocultas) -> OK`,
con dos corchetes de diagnostico que solo aparecen cuando algo fallo:
`[el re-arm no llego a encender el medidor en el sondeo]` y `[<error del clic>]`.

Las doce direcciones que no son AGUJA ni MIDI-CC, una por una: que hace el guion y que lo
hace fallar. Las tres piezas con seccion canonica propia mas alla de ellas (VOLVER, y las dos
medidas dentro de AGUJA: las barras ENV y el medidor-PANIC) remiten a donde esta.

### MATRIZ (canon): la ruta se pone por el APVTS y el cajon se queda ABIERTO

Guion: escribe por el APVTS lo que pondria un preset — `mod1Source` = indice de "LFO 1" en
`State::getModSources()`, `mod1Destination` = `destinationIndexFor (filterCutoff)`, `mod1Amount` =
+0.5 real— espera 400 ms y abre el cajon con `scriptOpenMatrixDrawer`. Cuatro invariantes:

1. el cajon abierto y su **velo comparten id**: si no coinciden, lo que se abrio no es el dialogo
   que ese disparador gobierna;
2. 4 filas y 12 celdas (`numMatrixSlots` x `numMatrixCellsPerSlot` = 4 x 3);
3. los dos selects enseñan los indices **esperados**, que salen de las tablas de contrato
   (`getModSources()` y `getModDestinationTable()`), nunca escritos a mano: la tabla de destinos es
   append-only y su orden ES estado de preset, asi que si un destino cambia de sitio la direccion
   sigue midiendo lo mismo;
4. la cantidad supera 0.5 en normalizado (0,5 real en un rango -1..1 son 0,75; el control
   compartido publica normalizado, asi que se compara con margen en vez de exigir el texto).

Deja el cajon ABIERTO a proposito: las trece direcciones siguientes corren con la matriz en uso y
el lienzo tapado, que es donde se rompe una UI. Evidencia (corrida del 28 de septiembre): `MATRIZ: cajon
"drawer-modMatrix" (velo "drawer-modMatrix") ABIERTO con 4 rutas y 12 celdas; ruta 1 = fuente 1 ->
destino 10 (esperado 1 -> 10), cantidad 0.750 -> OK`.

### MODELOS (canon): los SEIS assets pasan por el lector de produccion, los cuatro primeros llenan A-D

Dos pasos y los dos se miden. **1. VALIDACION** (`loadSelfModels`): cada asset embebido
(`NEURONiK_SelftestAssets`, los seis del banco CZ101) se escribe a fichero en el directorio
temporal del arnes y se RELEE con `PresetManager::loadModelFromFile`; se exige `isValid` **y** el
numero de frames que el asset declara (1 en los cuatro estaticos, 4 en los dos temporales), asi
que los dos dialectos del v2 —denso y con f0 por frame— pasan por el lector de verdad. El material
es el real del banco, no JSON sintetico: si el plugin dejase de entender el dialecto del Model
Maker, esta direccion lo diria en vez de dar OK con ranuras vacias. **2. RANURAS**: los cuatro
primeros (los mismos que el preset CZ101-BANK pone en las esquinas del pad) entran por
`processor.loadModel (file, slot)`.

Invariante de pagina: la ficha A-D tiene que mostrar exactamente `expectedSlotNames()` —los
nombres **sin extension** (el processor publica `getFileNameWithoutExtension()`) y en el mismo
orden—. El directorio es el temporal del sistema (`neuronik-selftest-models`): `loadSelfModels` lo
vacía al empezar y `finish()` lo borra al terminar, **tambien con FAIL**, para que al reabrir el
editor la ficha no los marque divergentes por estar a medias. No se toca ningun modelo del usuario. La otra
mitad de "cargar un modelo se ve y suena" —que el motor SUENE— esta en `Tests/ModelSlotTest.cpp`,
porque un proceso con ventana no puede medir su propia salida de audio sin pelearse con el hilo de
audio del host. Evidencia: seis lineas `MODELOS: asset CZ-*.neuronikmodel -> OK (1 frame v2)` /
`-> OK (4 frames v2, f0 por frame)`, cuatro `-> ranura N: cargado` y `MODELOS: la ficha A-D de la
pagina muestra "CZ-BASS1|CZ-HAMOG|CZ-PAD1|CZ-SWEP1"`.

### ENV-RUTAS (canon): el salto sale de lo que PINTA la pagina, y su slot es el input de las dos siguientes

Guion: el script elige una fila entre los `button[data-env-route]` decidiendo cual es de ENV por el
select de fuente que ensena el cajon de la MATRIZ que dejo abierto la direccion 0, la pulsa y
**devuelve su slot**. Cuatro invariantes: fila ENV existente (`NO_ENV_ROUTE` / `NO_ENV_ROW_VISIBLE`
si no la hay), cajon de la MATRIZ abierto con su velo, `data-slot-highlight` = el slot pulsado, y
los selects de ese slot enseñando lo que el APVTS tiene (motor contra pagina). La quinta cosa que
se mide sin querer es `wasOpenBefore`: la direccion 0 deja su cajon abierto, asi que el salto
demuestra ademas que el opener **cierra al hermano y abre el suyo**.

El slot no sale del APVTS (el Standalone restaura el `filterState` de la sesion anterior, que puede
traer la matriz del usuario) sino de la pagina; los indices de ENV salen de `getModSources()` y el
APVTS se lee DESPUES, solo para cruzar. Ese slot viaja a VOLVER y a RESUMEN-RUTAS por los dos
caminos. Detalle completo en el bloque «ENCADENADO POR DATOS» de la cabecera de
`BridgeSelftest.h` y en la seccion canonica de VOLVER, al final de este fichero.

### RESUMEN-RUTAS (canon): la segunda via del mismo salto, en un slot DISTINTO

Guion: `scriptSummaryRouteJump (avoidSlot)` elige la fila `button.mod-summary__row` del slot
`(avoid % 4) + 1` —la que ENV-RUTAS ya midio, para no repetir cobertura— y la pulsa. Invariantes: 4 filas
clicables, cajon de la MATRIZ abierto con su velo, resalte = slot pulsado, y los dos selects de ESE
slot enseñando lo que el APVTS tiene. Si la fila del slot elegido no esta pintada devuelve
`NO_ROW_FOR_SLOT` y la direccion **falla**: no pulsa a ciegas. Al terminar hace el settle del lienzo
(`scriptSettleDrawers`) y encadena AGUJA. Detalle del settle en la seccion canonica de VOLVER.

### NATIVO -> JS (canon): un control nativo se ve en la pagina

Guion: `masterLevel` a 0.25 por el APVTS —el camino que usaria un control nativo— y 400 ms
despues `scriptReadFirstRange` lee el `value` del input. Invariante: valor leido a 0.25 con margen
0.02, y sobre todo que el script distinga `NO_SLIDER` y `NO_RESULT` de un numero: un slider que ha
desaparecido no puede dar OK por ausencia de lectura. `masterLevel` es 0..1 sin skew, asi que
normalizado y real coinciden y el margen es pequeno. Los 400 ms son margen de sobra para un poll de
30 ms del dueno: quedarse corto antes que lento.

### JS -> NATIVO (canon): un arrastre real llega al APVTS

Guion: `scriptSetFirstRange ("0.75")` **no escribe el valor**: despacha un `input` de verdad, el
mismo evento que produce un arrastre del usuario, y la pagina lo trata por su camino normal (no
hay atajo). Invariantes: el script devuelve `DISPATCHED` y el APVTS queda a 0.75 con margen 0.02
tras 600 ms —margen MAYOR que en el resto porque aqui son dos saltos: render de la pagina y luego
poll del dueno—. Evidencia: `JS -> NATIVO: slider de la pagina a 0.75, masterLevel nativo = 0.7500`.

### GENERAL (canon): los ids de la pestana tienen que ser numeros, y estar TODOS

Guion: `envAttack` a 0.5 por el APVTS y `scriptReadGeneralState` lee los ids de la pestana
GENERAL, que devuelve el valor leido y dos listas: `missing` (los ids que la pagina no publica) y
`bad` (los que publica y no son numericos). Invariante: `missing` vacia, `bad` vacia y `envAttack` a
0.5 con margen 0.02. Es la direccion que delata un id anadido a la pagina y no al contrato (sale
en `missing`) o que viaja como texto en vez de numero (`bad`): por eso el veredicto exige las dos
listas vacias y no solo el valor que se pedia. Evidencia: `GENERAL: ids de la pagina = 11, envAttack
= 0.500 (nativo 0.5) -> OK`.

### MIDI (canon): la nota entra, suena y SALE; la rueda vuelve

Guion: rueda de modulacion a 0.5 por `processor.injectController` (la ruta del MIDI externo) y, en
la pagina, cambio a la ficha KEYS y pulsacion del DO central (nota 60, velocidad 0.9) como un
usuario. Tres invariantes de medicion y una de ORDEN:

1. `ON_SENT` **y** la nota aparece en `processor.getHeldNotes()` (el FIFO de notas del motor, no el
   eco de la pagina: es el canal por el que una nota que no suena se delata);
2. la rueda se lee **normalizada** (la pagina pinta 0..127) a 0.5 con margen 0.1, con `NO_MOD` /
   `NO_RESULT` separados del numero;
3. `OFF_SENT` **y** la nota desaparece de las retenidas —una nota atascada deja la sesion colgando y
   se reporta como `STUCK`, no como OK—;
4. el orden importa: MIDI-CC corre detras (necesita la tabla CC en su estado de fabrica) y ACCIONES
   ultima (su sorteo mueve los parametros de su tabla entera y invalidaria a quien mida valores
   concretos antes).

Evidencia: `MIDI: nota 60 de la pagina en nativo = on/off, rueda de modulacion en la pagina = 0.50
(nativo 0.5) -> OK`.

### ACCIONES (canon): el sorteo se mide por su EFECTO, dos veces, y por sus dos caras

La direccion no se cierra en su primera pasada: mide el sorteo con todo suelto y despues lo
contrario, que es la otra mitad de la promesa del boton.

**Pasada 1** (`pressPageActions`): `randomStrength` a 1 y los tres `freeze*` a 0 por el APVTS (1 y 0
son los extremos de lo que lee el sorteo), cierre del cajon de la matriz **por su velo** —el clic
de fuera, porque con un modal delante la ficha no es alcanzable por un usuario— y pulsacion del
BOTON de la pagina, leyendo en ella su etiqueta y su estado real (`enabled`). Invariantes: el
boton habilitado, el cajon cerrado, el APVTS movido en **>= total - 2** objetivos (la holgura de dos
es porque un objetivo puede caer por azar en el valor que ya tenia; un sorteo roto mueve 0 o 1, de
modo que la separacion sigue siendo clara) y el pie de la pagina publicando los **MISMOS** numeros
con 0 discrepancias, admitiendo como mucho `numPadOnlyTargets` (2) ids sin publicar: los dos del
pad XY no estan en la lista del pie, que es el contrato de la pagina (los 70 ids del lienzo), y
ninguno mas —media tabla sin publicar es una pagina que dejo de enterarse—.

**Pasada 2** (`pressFreezeGuard`): `freezeResonator` a 1 y el MISMO boton. Invariantes: **cero**
movimientos en el banco congelado (un congelado no tiene por que "caer" en su valor: no se le
sortea nada, asi que aqui la holgura del azar no aplica) y `>= total - 2` movimientos en los
sueltos. Los otros dos `freeze*` se dejan sueltos A PROPOSITO, para que "no se movio" signifique
"estaba congelado" y no "el sorteo dejo de correr". Al salir devuelve `freezeResonator` a 0 (el
congelado era de la medida, no del usuario); el APVTS se queda sorteado, que es una de las cosas
que el arnes declara en su cabecera.

La tabla de objetivos sale de `State::getRandomizeTargets()`, no de una lista escrita en el arnes:
si el RANDOMIZE cambia de opinion sobre QUE sortea, la direccion mide lo nuevo sin tocarla.
Evidencia: `ACCIONES: RANDOM de la pagina ("RANDOM", habilitado) ... APVTS movido en 23/23
objetivos; el pie publica 23 de esos ids (0 sin publicar, 2 son del pad XY), 0 discrepancia(s)` y
`RANDOM con freezeResonator ... movio 10/10 objetivos SUELTOS y 0 objetivos CONGELADOS`.

### MORPH (canon): pad XY y aro morph-Z con el modelo REAL dentro, y la geometria que nadie mas ve

Carga `CZ-SWEP1.neuronikmodel` en la ranura D —el fichero que instala `installFactoryModels()` y al
que apunta el `modelPath3` del preset CZ101-BANK, no una copia— y mide cuatro cosas:

1. **MOTOR -> PAGINA**: escribe `morphX`/`morphY`/`morphZ` = 0,2 / 0,8 / 0,6 por el APVTS (tres
   valores DISTINTOS, para que una vista que se interchange no pueda pasar) y exige que el pulgar,
   los `aria-valuenow` y el primer numero del `stroke-dasharray` del arco (60) pinten ESOS.
   El `y` se lee des-invertido (`1 - top`) porque la pagina lo pinta al reves.
2. **PAGINA -> MOTOR**: un arrastre del pad a 0,75 / 0,25 y un gesto del aro de 1/4 de vuelta, con
   `PointerEvent` de verdad, y despues el APVTS tiene los tres parametros donde el gesto los dejo.
3. **ECO**: la pagina se queda pintada donde la dejo el gesto (mismo viaje, misma lectura).
4. **GEOMETRIA de la pagina viva**: el centro del pad tiene que caer en el pad y el trazo del aro en
   el aro. Los gestos se despachan sobre `document.elementFromPoint`, o sea lo que hace el
   navegador con un dedo: asi un aro de overlay que se come el pad sale aqui y no en un test de
   manejadores, que pasaria igual con la pagina tapada.

Ademas, las cuatro esquinas del pad y la ficha A-D tienen que enseñar los MISMOS cuatro nombres:
si el morph y las ranuras dejan de hablar del mismo motor, esta direccion lo dice. Las dos lecturas
(antes y despues del gesto) comparten UN recolector (`morphCollectorJs`), porque dos recolectores
paralelos podrian divergir sin que nadie lo notase. El APVTS se queda con CZ-SWEP1 en D: es lo que
el usuario pidio ver y el arnes no lo devuelve.

### ESQUINA (canon): el gesto inverso, con la calle de vuelta abierta

Guion: clic en la esquina A del pad (la ranura 0, cargada desde el arranque) y cuatro invariantes:
el texto de la esquina es el **NOMBRE** del modelo cargado, no una letra (comparar contra
`modelName (0)` es una verdad extra: la esquina y la fila del cajon tienen que seguir hablando del
mismo motor); el cajon de MODELOS abierto con su velo; la ranura resaltada en la numeracion
**0-based del motor** (`data-slot-visual="0"`, mientras que las celdas de la matriz son 1-based) y
el nombre de esa ranura pintado en la fila resaltada. Doble guarda: si la esquina no esta clicable
(sin modelo) o el cajon no abre con el resalte, FAIL.

**La mitad SIN TIMBRE (anadida el 28 de septiembre de 2026).** La direccion mide tambien el clic
en una esquina VACIA, que es la otra mitad de la regla del pad y la que se habia perdido: la
pagina no la pinta siquiera (el componente compartido omite el span; `data-clickable` solo existe
con ranura cargada), asi que el punto donde estaria se deriva por **simetria** —el espejo del
centro de la esquina de enfrente, que comparte fila y alto— y se le manda un clic de verdad por
`document.elementFromPoint`. Cuatro invariantes:

1. la esquina de la ranura vacia **no esta en el DOM** (si aparece, la ranura no estaba vacia y la
   direccion no midio lo que cree: `EMPTY_CORNER_PAINTED`);
2. quedan las otras TRES, y el punto cae en el **pad**, no en una esquina (`hitIsPad=1`,
   `hitIsCorner=0`): sin esa comprobacion, "no abrio nada" podria ser "no habia nada que abrir" —
   y es exactamente lo que fallo en la primera version, que reutilizaba el ancho de la esquina de
   referencia y mandaba el clic encima de la de al lado;
3. no se abre **ningun** cajon (se exige el documento entero, no solo el de MODELOS, y el de
   MODELOS se cierra por su id antes: con el delante, "no abre nada" no seria medida de nada);
4. no queda **ningun** resalte en el documento (`[data-slot-highlight='true']`, que el panel
   escribe en todas las filas).

El veredicto de la direccion es el AND de las dos mitades, con dos lineas de log.

**Para que la esquina vacia exista hubo que poder vaciar una ranura, y eso no existia en ninguna
capa**: el processor solo tenia `loadModel`/`reloadModels` (y `reloadModels` no limpia
`modelNames[]`), y la pagina lo tiene IMPIDIDO a proposito con host —"las ranuras son del PRESET y
el motor las volveria a cargar al recargar el proyecto, asi que vaciarlas desde la pagina seria un
gesto que no se sostiene" (`paramStore.js`, `forgetLocalModel`)—. De ahi `clearModelSlot (int slot)`
en `NEURONiKProcessor`: la operation INVERSA de `loadModel`, que encola el `SpectralModel` por
defecto por la MISMA cola de comandos (audio thread, sin asignar), pone el nombre a `EMPTY` y
`modelPath<slot>` tambien, para que recargar el proyecto no lo resucite. `Resonator::loadModel` no
valida el modelo, asi que un modelo por defecto **vacia la ranura de verdad**: la alternativa
(solo renombrar) habria dejado el timbre viejo sonando con la pagina diciendo EMPTY. Devuelve
false solo si el slot esta fuera de rango.

El arnes vacia la D (la esquina `br`, la que MORPH acaba de llenar con el modelo real) y la
**restaura** al terminar, con su CZ-SWEP1 real de la carpeta de fabrica: la restauracion se
loguea —`la pagina vuelve a pintar CZ-BASS1|CZ-HAMOG|CZ-PAD1|CZ-SWEP1`, con `scriptReadMorph`— pero
NO entra en el veredicto, porque es limpieza del propio arnes y no una invariante del producto.

Al salir deja el lienzo como estaba: cierra el cajon **por su id** (`scriptCloseDrawerById`) y
reabre la matriz por el APVTS con la ruta LFO 2 -> Morph Z, que es el estado que ZRING asume al
arrancar. Detalle honesto: esas dos escrituras son LITERALES (fuente 2, destino 28) mientras que
ZRING deriva los indices de las tablas de contrato; es una incoherencia sin efecto medido hoy
(las dos coinciden), pero es el sitio donde un cambio de orden de la tabla pasaria desapercibido.

**Comprobado que CAZA un defecto real de la pagina** (no solo uno del arnes): con la regla de
`displayableModelName` mutada para que `EMPTY` deje de contar como ranura vacia (una sola linea),
la pagina pinta las cuatro esquinas y la direccion lo dice con su nombre propio:
`quedan 4 esquinas pintadas (esperado 3) ... [EMPTY_CORNER_PAINTED] -> FAIL`. Revertido a
continuacion (`git status` de `WebUI/src/audio/localModels.js` sale limpio, dist reconstruido y
verificacion en verde).

### ZRING (canon): el anillo morph-Z girando de verdad, con control negativo A/B/A

Guion: la ruta se escribe **por el APVTS** como la dejaria un preset —`mod1Source` = indice de
"LFO 2", `mod1Destination` = `destinationIndexFor (morphZ)` (28), `mod1Amount` = 1, LFO 2 a 1 Hz en
modo Free y `morphZ` a 0 (base en reposo: lo que se ve es el arco, no la base)— y luego tres
tomas con muestreo de la pagina viva:

1. **A (540 tomas a 30 ms)**: se reconstruye el PERIODO entre culminaciones del arco
   (`stroke-dasharray` del arco `.zring-mod`, que `renderZ` escribe como "span <n>") y se exige
   `zringPeriodMinMs` (300) < periodo < `zringPeriodMaxMs` (800) y `max > 40` guiones. Ojo al
   numero: con el arco **con signo** pintado, la senoide completa culmina DOS veces por periodo, asi
   que lo correcto a 1 Hz son ~500 ms entre culminaciones. El log lo dice asi desde el 28/09: la
   linea imprime el GATE real (`gate 300..800 ms entre cristas`) y el porque de los ~500, porque un
   texto fijo ("esperado ~1000") que no coincide con el gate es peor que no tener ninguno: se lee
   como que el gate es otro.
2. **CON SIGNO**: ademas de las dos condiciones anteriores, alguna instantanea tiene que caer en el
   lado ANTIHORARIO (`5 < span < 95`), o sea la semionda negativa pintada de verdad.
3. **B (30 tomas)**: la fuente a Off tiene que clavar el arco en 0 (`|max| < 2`). Es el control
   NEGATIVO que hace la medida: un arco que no depende de la ruta no se apaga nunca.
4. **A' (320 tomas)**: LFO 2 vuelve y el arco tiene que reanudarse (`max > 40`). Sin esta reprise un
   arco que se apaga y no vuelve pasaria el control negativo por bueno.

El veredicto (`zringOk`) es el AND de las cinco condiciones, y ZRING es la ULTIMA direccion: ella
cierra el `finish()` con los 16 veredictos. El margen 300..800 ms es para hosts lentos, no una
propiedad del motor (los gates fisicos del LFO de 1 Hz no escalan con `NEURONIK_SELFTEST_BUDGET`;
las tomas si: 540/30/320 x factor). Evidencia: `ZRING: arco con LFO2 -> 28: max 100.0 guiones,
periodo 500.103 ms (gate 300..800 ms entre cristas: |sin| de 1 Hz culmina dos veces por periodo, asi
que ~500, no ~1000) -> OK`, `arco con signo: 198/540 instantaneas en el lado
ANTIHORARIO`, `control negativo (fuente Off): |arco| max 0.0 guiones -> OK` y `A de nuevo: max
100.0 guiones -> OK`, mas dos lineas de diagnostico con los spans y el dt medio de las tomas.

### Sondeos compartidos (canon): `PageWait`, el "espera a que se cumpla X" del arnes

Lo que tienen en comun los tres sondeos de espera del arnes es la misma maquinaria: `evaluate` del
script, predicado sobre la lectura, `afterDelay` entre tomas, salida temprana en cuanto se cumple, y
la guarda de por vida en las dos patadas. Vive en `BridgeSelftest.h::PageWait` (clase anidada, con
una instancia `pageWait`), y su API es una linea:

```cpp
pageWait.run (script, predicado, maxSamples, pageWaitIntervalMs,
              [this] (bool satisfied, const juce::String& lastRaw) { ... });
```

- `pageWaitIntervalMs` = 30 ms: el `evaluate` responde en ~1 ms, asi que sin espera las tomas se
  consumirian antes de que la primera telemetria llegue a la pagina (~66 ms).
- `satisfied` = el predicado se cumplio; `false` = se agotaron las tomas, con la ultima lectura a
  mano. Es DIAGNOSTICO, no veredicto: decidir sigue siendo del llamante.
- Con el arnes muerto, `onDone` **no** se llama: la cadena ya no tiene a quien volver.

**Los tres consumidores** (los tres eran el mismo bucle, copiado):

1. `needleWaitQuiet` (AGUJA/PANIC): espera a que el medidor se apague antes de re-armar, para que la
   cola del release no apile voces encima del re-arm.
2. `needleWaitArmed` (AGUJA/PANIC): espera a que el medidor se encienda tras re-armar, para medir el
   PANIC sobre el estado encendido MEDIDO y no sobre un delay a ciegas.
3. `ccSample` (MIDI-CC): espera a que el cutoff de la pagina BAJE tras la nota CC. Es el unico que
   devuelve un VALOR en vez de un flag, y lo hace parseando la ultima lectura con el mismo lambda.

**Lo que NO se extrajo, y por que**: los sondeos que acumulan (la colecta de AGUJA, las tomas del
arco de ZRING) necesitan saber que guardan y cuando parar, asi que `PageWait` no les sirve y siguen
siendo bucles propios. Extraerlos seria otra clase, no esta.

**Comprobado que el flag hace su trabajo**: con el presupuesto del re-arm recortado a 2 tomas
(experimento revertedido, 0 rastros), la linea de PANIC lo dice explicitamente —
`PANIC por el clic en el medidor (0 led(es) antes  [el re-arm no llego a encender el medidor en el
sondeo])`— donde antes solo decia "medidor ENCENDIDO", que es el sintoma de otro problema.

### MIDI-CC (canon): el LCD arma el learn, un CC del motor lo gana y mueve el parametro

Direccion 10b, entre MIDI y ACCIONES (antes del sorteo, que moveria todos los valores que
esta direccion mide). La tabla CC -> parametro vive en el MOTOR (`MidiMappingManager`, atomics
RT) y la pagina la ensena y la edita, nunca la posee. Cuatro fases, cada una por su camino de
verdad, mas una fase 0 que va antes: la geometria de la fila del LCD en la pagina viva (ver
"Fila del LCD (canon)"). La fase 0 va en paralelo (solo lee el DOM, a 120 ms, antes del
armado) y su veredicto se suma al final: si el D-pad no cabe en su chasis, esta direccion falla
aunque el menu se pueda recorrer con eventos sinteticos.

1. **ARMADO POR EL LCD**: los BOTONES del D-pad (`lcdPanel.js`: `.abd-lcd-panel__btn--*`, gesture
   `pointerdown`) navegan `MENU -> right x4 -> OK -> OK -> right`: en una raiz de cinco items los
   cursores `^`/`v` (+-5) son vuelta completa, asi que la navegacion va por el encoder +1
   (`right`); `MENU` abre la raiz, `OK` entra en MIDI CONTROL y en "CC CUTOFF", y el `right` en
   EDIT dispara el `onEdit` de un item `cc` (`sendMidiCcLearn` -> `enterMidiLearnMode`).
2. **LA NOTA CC GANA**: el `injectController` del arnes (la ruta del MIDI externo, barrida en
   `processBlock` ANTES del filtro de canal) con el CC **75** — libre en la tabla de fabrica
   (CUTOFF nace en 74), asi que la asignacion no se confunde con la de fabrica. Consume el learn y
   no toca el parametro (`continue` en el barrido).
3. **LA NOTA CC MUEVE EL PARAMETRO**: el MISMO CC a valor 0 encola y `applyPendingCcChanges` lo
   aplica por `setValueNotifyingHost`; el cutoff de la pagina (pie normalizado) tiene que BAJAR —
   sondeo con salida temprana (patron AGUJA, 30 tomas x 30 ms, escaladas por la env var).
4. **LA PANTALLA LO PINTA**: el item sigue en EDIT y su linea 2 tiene que decir `CC 75` — la tabla
   que el motor acaba de escribir, de vuelta en la pagina.

Al salir: `resetToDefaults()` (la tabla vuelve a fabrica) y CUTOFF a ABIERTO, que es lo que
asumen el sorteo de ACCIONES y el ZRING.

**REGLA DE PRODUCTO que esta direccion fijo** (no la tenia el puente): la tabla CC no es del
APVTS, asi que un learn completado por HARDWARE —el gesto clasico de MIDI Learn, el mismo que
mide esta direccion— la reescribe en el hilo de audio y nadie avisaba a la pagina: su menu MIDI
CONTROL se quedaba con la tabla vieja (en la primera corrida, `CC --`). Ahora `MidiMappingManager`
lleva un contador de version (`getTableVersion()`, movido por todo mutador: learn, clear, reset y
la carga de estado) y el poll del editor (`NeuronikWebView::poll`) y el de la bancada
(`WebPilotHost`) republian `midiCcState` cuando cambia, antes del sondeo, para que la tabla y
los valores lleguen en el MISMO tick. Sin eso, el menu mentiria justo despues del gesto para el
que existe.

Evidencia en el log: `MIDI-CC: learn del LCD sobre CUTOFF -> CC 75 (OK), nota CC (0/127) al motor:
CUTOFF 0.545 -> 0.000 (baja), pantalla del LCD "CC 75" (esperado "CC 75"), fila del chasis sin
recortes -> OK`, precedida de la fase 0: `MIDI-CC: fila del LCD en la pagina viva (dpr 1.25,
zoom 1, ventana 1440x946): chasis 1420x54 en pantalla, contrato 54, D-pad 6 boton(es) en 2
filas, 0 recortado(s), pantalla 1238x28 con 2 lineas dentro -> OK`.

### Fila del LCD (canon): el D-pad es 3x2 y la pantalla va a 11px

La fila del LCD son 54px de PRESUPUESTO FIJO (`GEOMETRY.lcd = 54` en `src/contracts/sections.js`; los
946 del lienzo son 892 de canvas + 54 de esta fila + 8 de aire, y el test de encaje cuenta ese
aire). Con `padding: 6px 10px` quedan 42px utiles, y ahi dentro conviven la pantalla 16x2 y el
D-pad de seis botones. Los numeros medidos en Chromium sobre el dist, a la medida que recibe el
WebView2 del plugin (1440x946):

- **Chasis**: 1420x54, de y=56 a y=110. Contenido 42, de y=62 a y=104.
- **D-pad**: rejilla de **3 columnas x 2 filas** (la del paquete compartido, `lcdPanel.js` con
  `repeat(3, auto)`), botones de 39x14 en dos filas: y=67 y y=85. La segunda acaba en 99: **11px
  de aire** y NINGUN boton recortado. Orden de DOM `[menu, ok, left, right, up, down]` = fila de
  arriba MENU/OK/< y de abajo >/^/v.
- **Pantalla**: 1239x29 en x=33, con las dos lineas de texto de 14px (11px de fuente x 1.3 de
  line-height = 28.6 de los 29): el texto llena el alto sin desbordar.

**Lo que NO se puede hacer aqui**: el `overflow: hidden` del chasis es una red de seguridad, no el
mecanismo. El override del 27 Sep (2 columnas) lo era: a 2 columnas la rejilla de 6 botones se
vuelve 2x3 = 3 filas de 14 + 2 gaps = 51px en los 42 de contenido, y la tercera (`^` `v`) se salia
del chasis y este la cortaba A MEDIA ALTURA (medido: botones en y=99..113 con el chasis cerrando
en 110 — los cursores de arriba y abajo se veian mutilados, y la direccion MIDI-CC los necesita
para navegar). Volver a 3 columnas deja las dos filas dentro.

Y el texto: `.abd-lcd__line` usaba `var(--text-sm, 11px)`, pero ese token **resuelve a 9px** en
este tema, con lo que la reticula 16x2 dejaba 18 de los 30px de alto vacios y el texto se encogia
en la esquina superior izquierda de una franja verde de 1200px. A 11px fijos el alto se llena
(28.6 de 30) sin desbordar.

Alcance: es CSS de la pagina (`WebUI/src/styles/main.css`), asi que cambia los snapshots del
lienzo entero — las dos referencias de `e2e/visual.spec.js` (tema oscuro y claro) se regeneraron
con el diff **acotado a la fila**: la pantalla (x desde 32) y el D-pad (x 1290..1419). Antes de
aceptarlas se comprobo la linea base revirtiendo `main.css` a HEAD y reconstruyendo: los dos
tests pasan sin el cambio, o sea que el diff es del cambio y solo suyo.

**La invariante que hacia falta (fase 0 de MIDI-CC, `scriptLcdGeometry` +
`checkLcdGeometry`)**: todas las pruebas de arriba son de CAPA — comparan clases, valores o
snapshots — y el D-pad se conduce con eventos SINTETICOS, asi que un boton recortado por el
chasis responde igual. Por eso la fila pudo pasar el selftest entero (15 direcciones), vitest
(398/398) y la E2E (22/22) con los cursores `^` y `v` cortados a media altura: el defecto era
invisible para el arnes. La fase 0 mide **CAJAS** en la pagina VIVA del WebView2 y exige cuatro
cosas, todas en la pagina que el plugin muestra de verdad:

1. el alto del chasis es el del CONTRATO (54, en pixeles de LAYOUT);
2. los SEIS botones del D-pad existen;
3. NINGUNO se sale del chasis (se calcula en la pagina, no comparando floats aqui);
4. las DOS lineas de texto caben dentro de la pantalla (con 9px se recortaban por arriba).

Comprobado que **caza**: se reprodujo el defecto del 27 Sep (una sola linea del CSS, la rejilla
a 2 columnas) y la corrida dio `fila del chasis CON RECORTES` + `RESULT: FAIL`, con el crudo
senalando los culpables: `"clipped":["up","down"]` (filas en y 62, 80.4, 98.8 con el chasis
cerrando en 110 — la tercera se sale 3.2px). Las otras tres fases de MIDI-CC siguieron en verde
en esa misma corrida, que es justo la prueba de que eran ciegas.

**Medido DENTRO del WebView2 del plugin** (no en un navegador de escritorio), que es lo que
aportaba la medicion y no se veia fuera:
`dpr 1.25, zoom 1, ventana 1440x946, chasis 1420x54 en pantalla, contrato 54, D-pad 6 boton(es)
en 2 filas, 0 recortado(s), pantalla 1238x28 con 2 lineas dentro`.

**La bancada escala la pagina, y con razon**: su ventana es de 640x520 y escala el lienzo a
0.44 (640/1440), asi que alla la fila mide 631x24 EN PANTALLA con los mismos 54 de alto en
LAYOUT (por eso el contrato se comprueba con el valor COMPUTADO, `cssH`, y no con el rect). Con
la primera version de la comprobacion —que comparaba 54 contra el rect— la bancada daba FAIL sin
motivo: un falso positivo propio, que es lo que sale de medir la invariante en los dos hosts y
no solo en el que se parece al plugin.

### Estado de verificacion (28 Sep, con MIDI-CC)

Standalone: **las 15 direcciones OK** (MIDI-CC incluida, LCD mostrando `CC 75`), selftest-exit=0.

Bancada: MIDI-CC **OK** (misma cadena, mismo LCD). La corrida completa da **FAIL por AGUJA**:
`barras 8 (OK, gemelas DIVERGENTES)` — la invariante de gemelidad (3) entre la copia del cajon y
la del resumen del lienzo. Es la primera vez que la bancada mide esa invariante (el estado del 27
Sep la dejaba pendiente), AGUJA corre ANTES que MIDI-CC y la direccion nueva no toca barras, asi
que es una falla de la pagina en la bancada, no una regresion de MIDI-CC. Queda abierta.

---

## 2026-09-28 — REVISION DE CALIDAD del codigo de la sesion: un bug real (CZ101) y una regla triplicada

La revision de calidad que quedo a medias en el turno anterior esta cerrada. Recorre todo lo que
escribio la sesion, no el arbol entero: memoria local de ranuras (`localModelCache.js`), su pintado
(`ui/modelSlots.js`), el store (`contracts/paramStore.js`: `forgetLocalModel`, `restoreLocalModels`,
`setLocalMorphRoute`, `seedLocalMorphZRoute`), el distintivo `touched` (`ui/panel.js`:
`liveDrawerBadge`) y la reclamacion de voces (`BaseEngine::reclaimIdleVoices`).

### Lo que estaba mal, y se ha arreglado

1. **CZ101: la regla de skew estaba escrita de tres formas y dos no coincidian** (era el hallazgo
   que ya estaba confirmado, ahora con su arreglo). Vive hoy en UN predicado, `usesSkew(spec)`, en
   el contrato generado `WebUI/src/contracts/registry.gen.js` — y en la PLANTILLA de
   `ABDCZ101/scripts/registry_generator.js`, que es de donde sale ese fichero, para que la
   regeneracion no lo pierda (comprobado: `node scripts/registry_generator.js` reproduce el fichero
   byte a byte y no toca los otros tres artefactos generados). La usan `rawToNormalized`,
   `normalizedToRaw`, `normalizedValueOf` (distintivo de bloques) y las dos ramas de `app.js`.
2. **CZ101: un preset que no mencionaba `MODERN_HPF_CUTOFF` lo reiniciaba a 10 kHz.** El reinicio
   de los parametros no mencionados pasaba el `default` del registro en CRUDO a
   `applyParameterToUI`, que espera el NORMALIZADO: el 20 Hz del paso alto se saturaba a 1.0. No
   era latente, era real. El `default` de la APVTS llega siempre en crudo, asi que ahora pasa
   SIEMPRE por `rawToNormalized`, con skew o sin el (la misma verdad que ya usaba
   `factoryValueOf` en el distintivo). Detalle completo en `ABDCZ101/DOCS/CHANGELOG.md`.
3. **NEURONiK: el distintivo `touched` resolvia los defaults de fabrica celda a celda.**
   `defaultNormalizedState([id])` se llamaba DENTRO del `filter`, una vez por celda y en cada
   paint, para sacar un numero que solo depende del contrato. Ahora se resuelve una vez para toda
   la fila (`const contractDefaults = defaultNormalizedState(ids)`). Sin cambio de resultado: el
   panel.test.js sigue en 83/83 y la regresion visual en 11/11.

### Lo que se reviso y se dejo como estaba (con el porque)

- **`reclaimIdleVoices` compacia y hace `swap` de voces mientras el hilo de audio recorre la
  lista**: es seguro por el cerrojo, y conviene que siga siendo explicito —`NEURONiKProcessor::
  setPolyphony` lo toma con `ScopedLock(getCallbackLock())`— porque el compactado escribe en
  punteros que el audio lee. `targetSize = max(newLimit, numActive)` garantiza `size() >=
  activeVoiceLimit` en los dos caminos, y `noteOff`/`pitchBend` iteran la lista COMPLETA (no solo
  por debajo del limite), asi que una voz que sigue sonando por encima del limite nuevo recibe
  igualmente su note-off.
- **`restoreLocalModels` deja en la memoria el texto que no se pudo volver a parsear**: sale el
  `modelError` en la ficha RANURAS, pero el almacen no se limpia y el aviso vuelve en cada F5. Es
  lo que se quiere (una version nueva del lector podria reparsearlo), asi que se deja escrito en
  vez de borrado por surprise.
- **`localRouteParameters` / `seedLocalMorphZRoute` / `setLocalMorphRoute`**: correctos y ya
  idempotentes; el conmutador devuelve `false` en vez de dejar la fila a medias cuando la tabla de
  fuentes no lleva el label.

### Verificacion de este turno

- `ABDCZ101`: **48/48 ficheros, 387/387 tests** (383 + 4 nuevos en `skew.test.js`),
  `node scripts/registry_generator.js` idempotente, `validate_css_order.js` OK.
- `ABDNeural/WebUI`: **395/395** (vitest), Playwright **9/9** (`localMode`, puerto 5236) y **11/11**
  (`visual`, puerto 5239), con la regresion visual sin tocar referencias.
- C++ sin cambios en este turno: `ctest` sigue en 41/41 desde el turno anterior.

### Entorno: los enlaces de `node_modules` de ABDCZ101 estan ROTOS (no es del codigo)

`npx vitest` y `npx vite` fallan con `Cannot find module .../ABDCZ101/node_modules/vitest/vitest.mjs`:
los symlinks de `ABDCZ101/node_modules` apuntan a entradas del store de pnpm que ya no existen
(`vitest@1.6.1_jsdom@24.1.3` cuando el store tiene `vitest@1.6.1_@types+node@26.6.3_jsdom@24.1.3`; lo
mismo con `vite@5.4.21`). Se salva sin tocar `node_modules` apuntando al store:

    V=/d/desarrollos/ABDSynths/node_modules/.pnpm/vitest@1.6.1_@types+node@26.6.3_jsdom@24.1.3/node_modules/vitest
    node $V/vitest.mjs run

La reparacion de verdad es `pnpm install` en el monorepo; no se ha hecho porque rehace el
`pnpm-lock.yaml` que esta modificado por trabajo ajeno.

### MCP Codebase memory: VUELTO, y los dos repos ya estan indexados

Ocho intentos de la sesion anterior dieron `MCP error -32001: Request timed out`; mas tarde, el
servidor respondia al catalogo pero **toda llamada fallaba con `Not connected`**. El 2026-09-28, al
cerrar el bloque del bundle, `list_projects` respondio y se indexaron los dos repos en `moderate`
(no `full`: las aristas de similitud/semantica son las que tardan):

- `D-desarrollos-ABDSynths-ABDNeural`: **4 236 nodos / 10 837 aristas** (no estaba indexado; el
  exclude se queda con `build-reference`, `build-wasm`, `WebUI/dist`, `WebUI/node_modules` y demas).
- `D-desarrollos-ABDSynths-ABDCZ101`: **12 159 nodos / 31 837 aristas** (ya habia indice, de antes
  de hoy; reindexado para que recoge el distintivo de bloques, `usesSkew` y el bundle).
- `ABDMS2000` sigue sin tocarse, como estaba.

Comprobado que el grafo sirve para lo que se le pregunta: `search_graph("reclaim idle voices")` en
ABDNeural devuelve `BaseEngine::reclaimIdleVoices` (`Source/DSP/BaseEngine.cpp:186-223`) con sus
vecinos (`ensureVoices`, `getNumAllocatedVoices`, `voices`, `kMaxVoices`) y del lado WebUI
`onWorkletVoices`; en CZ101, `usesSkew` aparece en `WebUI/src/contracts/registry.gen.js:1476`, que
es el predicado unico de hoy.asi que no hace falta el indice local de simbolos que se planeaba como
plan B: el grafo esta y se actualiza.

TRAMPA DE LA HERRAMIENTA, para el que venga: los argumentos numericos de este MCP llegan como
CADENA y el servidor los rechaza (`limit: Invalid input: expected number, received string`); la
forma de seguir es **no pasar numeros** y usar los valores por defecto (`search_graph` sin `limit`
va bien).

---

## 2026-09-28 — NEURONIK_SELFTEST_BUDGET: el presupuesto del selftest sube sin recompilar

Cuando un host se queda corto (CPU justa: cada `evaluate` tarda mas y la colecta necesita mas
tomas), el presupuesto del arnes se escala con la env var `NEURONIK_SELFTEST_BUDGET` en
SEGUNDOS, sin tocar codigo. El constructor de BridgeSelftest la lee al arrancar y aplica un
FACTOR (segundos / 150 del default) al watchdog (`defaultTimeoutMs`), a las tres colectas de
ZRING (540/30/320) y a las tomas y sondeos de AGUJA (nota 30, release 150, sondeos del PANIC
y needleWaitQuiet 400 / needleWaitArmed 100, todo con `std::lround`). Los gates de periodo
(300..800 ms) y el pacing de 30 ms NO se escalan: el LFO de 1 Hz es del motor, no del host.
Valor ausente, no numerico o <= 0: defaults, y la linea de arranque
`[selftest] presupuesto por ...` solo sale con la var puesta y valida.

Verificado en el Standalone del dia (build de los 3 objetivos sobre el fix de DspMath.h, ver
abajo): con `NEURONIK_SELFTEST_BUDGET=300` la linea de presupuesto sale con factor 2.00, ZRING
mide `spans[1080]`/`[60]`/`[640]` y RESULT: OK con las 14 direcciones; sin la var, defaults
`spans[540]`/`[30]`/`[320]` y RESULT: OK; con `abc`, defaults y RESULT: OK.

> Canon: la "SECCION CANONICA - las 14 direcciones del selftest" lleva ahora la subseccion
> "Presupuesto (canon): como escalarlo sin recompilar"; esta entrada cuenta solo la
> verificacion.

Dos notas de la pasada:

- Una de las corridas con la var salio (exit 0, sin watchdog de por medio) SIN veredicto:
  se corto en silencio justo tras el log del control negativo B de ZRING, sin RESULT. La
  repeticion inmediata cerro OK entera, igual que la corrida sin var y la de `abc`. Si
  vuelve a aparecer, mirar la cola de cierre del arnes (A de nuevo -> finish) y quien puede
  pedir el quit del Standalone sin pasar por `finish()`.
- La compilacion estuvo bloqueada por un C2124 ajeno: `ABDSharedCode/DspCore/DspMath.h`
  (repo aparte, editado por la sesion concurrente) tenia `log2(float)` devolviendo
  `-87.0f / 0.0f`, y MSVC lo rechaza al plegar la constante. La sesion dueña lo arreglo ella
  misma con `-std::numeric_limits<float>::infinity()` (mismo valor IEEE, ahora documentado
  en el propio fichero); recompilado encima sin mas cambios.

---

## 2026-09-28 — MIDI-CC: la direccion 10b del arnes, y el hueco del puente que hacia falta para medirla

El arnés no media el eslabon CC -> parametro: la pagina gestionaba el CC (LCD, menu MIDI CONTROL)
y el motor lo aplicaba, pero nadie media el camino entero. Anadida la direccion **MIDI-CC** entre
MIDI y ACCIONES (antes del sorteo, que moveria los valores que mide), con las cuatro fases del
canon: learn armado por el D-pad del LCD, la nota CC 75 del motor ganando la asignacion, esa
misma nota moviendo CUTOFF (0.545 -> 0.000 en APVTS y en el pie de la pagina, con sondeo de
salida temprana) y la pantalla del LCD enseñando `CC 75`.

Lo interesante es lo que la direccion encontro al nacer: **el puente no publicaba la tabla cuando
el learn lo completaba HARDWARE**. `sendMidiCcState()` salia solo con un snapshot completo o
tras una accion `midiCc*` de la propia pagina, asi que el menu MIDI CONTROL se quedaba con la
tabla vieja justo despues del gesto clasico de MIDI Learn (en la primera corrida pintaba
`CC --` en vez de `CC 75`). Arreglado de raiz, no en el arnés:

- `MidiMappingManager` lleva ahora un contador de version de la TABLA (`getTableVersion()`), que
  mueve todo mutador — learn, clear, reset y la carga de estado — incluida la reescritura que
  hace el bloque de audio con `setMappingByIndex`.
- El poll del editor (`NeuronikWebView::poll`) y el de la bancada (`WebPilotHost::timerCallback`)
  republican `midiCcState` cuando esa version cambia, ANTES del sondeo de parametros, para que
  la tabla y los valores lleguen en el mismo tick. El primer poll la manda siempre: el menu ya no
  arranca en "CC --".

Verificacion: los tres objetivos compilan; **Standalone 15/15 con RESULT: OK** (exit 0);
**bancada: MIDI-CC OK** con la misma cadena y el mismo LCD; `ctest` de lo tocado en verde
(ParameterBridge, StatePersistence, MidiChannelFilter, MidiPort).

Pendiente que dejo la corrida de la bancada: AGUJA falla ahi con `barras gemelas DIVERGENTES`.
Es la primera medicion de esa invariante fuera del plugin (el estado del 27 Sep la dejaba
pendiente), corre antes que MIDI-CC y la direccion nueva no toca las barras, asi que es una
falla propia de la pagina en la bancada. Queda abierta y sin tocar.

> Canon: la "SECCION CANONICA - las 15 direcciones del selftest" lleva la fila 10b del indice y
> la subseccion "MIDI-CC (canon)", con la REGLA DE PRODUCTO que esta direccion fijo (la tabla CC
> se publica tambien cuando cambia sola, no solo cuando la pagina lo pide).

## 2026-09-28 — mientras haya OTRO hilo en el arbol, los commits van por partes

La entrega anterior (`92f6fac`) metio en un commit tres bloques de trabajo que
no eran mios: lo de ModelMaker, selftest y bridge estaba a medias en el arbol,
y como el encargo era "commitea todo" se fue con el. El resultado no fue un
desastre —todo verde, todo commiteado— pero el commit dice cosas que su autor no
sabia, y el dia que un cambio seculote rompa, la bisect tendra que leer un
mensaje que mezcla tres motivos.

LA REGLA, para cuando haya mas de un hilo en el arbol:

1. **Stagear por rutas, nunca con `git add -A`.** `git add -u` mas la lista
   explicita de lo tuyo es la via que funciona: `git add -u` y luego
   `git add <fichero> <fichero>...` de lo tuyo. Antes de commitear, un
   `git status --porcelain` y a LEER la lista: si hay un `M` de un fichero que
   no has tocado, no se commitea (aunque "este relacionado con lo mio").
2. **`git add -p` cuando un fichero tenga los dos autores.** Es lo normal en
   `panel.js`, `app.js` o `main.css`: se stagea hunk a hunk y el commit lleva
   solo tu parte. Cuesta mas, pero el mensaje del commit miente menos.
3. **NUNCA `git stash -u` para "ver como queda el arbol".** Sin `-u` solo
   aparta tus cambios versionados; con `-u` se lleva tambien TODO lo que este
   sin trackear, que es justo el scratch y el trabajo a medias del otro hilo. Le
   paso aqui el 2026-09-28: un `git stash -u` colado en un comando se trago las
   veinte sondas del otro hilo durante medio minuto y las devolvio con
   `git stash pop`. Nada se perdio, pero fue por poco, y no hacia falta: el
   estado se mira con `git status` y `git log`, que no se comen nada.
4. **Lo que no es tuyo se queda en el arbol, y se dice en el mensaje** si el
   commit lo incluye: una linea que diga que el commit arrastra trabajo ajeno
   en vuelo vale mas que descubrirlo en el `git log`.

Y el porque de escribirlo: la higiene de commits no sale de un estilo, sale del
hecho de que varias sesiones comparten el mismo checkout. Lo que hace
`Tests/referencedFilesExist.mjs` con los ficheros nombrados por el build, esto
lo hace con los commits: que el mensaje diga la verdad de lo que lleva dentro.

## 2026-09-28 — la regresion visual del lienzo puede correr sola (Windows, en cada push)

Las once referencias de `WebUI/e2e/snapshots/` se comparaban solo en esta maquina. Ahora hay
`.github/workflows/webui-visual-qa.yml`: se dispara en cada push a `master` y en cada PR que toque
`WebUI/`, y corre SOLO `e2e/visual.spec.js` en `windows-latest`.

**`windows-latest` es un requisito del test, no una preferencia**: las referencias se generaron en
Chromium/Windows y la pagina usa `system-ui` sin webfont, asi que su rasterizado cambia entre
sistemas y un runner Linux fallaria por antialiasing, no por el codigo.

**No se reusa la accion `ajabadia/ABDSharedCode/.github/actions/pnpm-workspace-bootstrap`** que usan
ABDMS2000 y ABDEep, y el motivo esta escrito en el propio YAML: esa accion instala en la RAIZ del
workspace y verifica `<proyecto>/node_modules/@abdsynths/*`, mientras que el layout de NEURONiK es
otro — su workspace es `WebUI/` (con `WebUI/pnpm-workspace.yaml` y su propio lock), o sea que los
tres checkout siguen siendo hermanos pero el install va ahi y los enlaces caen en
`ABDNeural/WebUI/node_modules/@abdsynths/*`. Adaptar la accion seria tocar un repo compartido; el
workflow replica el layout a mano, con la guardia que hace falta: `pnpm install` puede salir en
verde y dejar los `workspace:*` sin enlazar, y el error sale veinte pasos mas tarde en Vite sin
mencionar el workspace. Aqui el install es `--frozen-lockfile` (hay lock versionado), al reves que
en el hermano.

**Lo que NO entra, escrito para que se lea como exclusion y no como olvido**: los guards de node
(`workletSyncTest`, `localMorphZRouteTest`, `localModelCacheTest`) comparan el WASM de `build-wasm/`,
que en el runner no existe sin compilarlo con emscripten, y el smoke `e2e/localMode.spec.js` mide
el canal del worklet con el reloj del `AudioContext`. Los dos entran en cuanto el runner tenga el
paso de `build-wasm`, que es lo que los guards necesitan.

**Lo que no se puede verificar aqui**: el workflow corre en GitHub, no en local. Lo verificado es el
YAML (parsea, con sus diez pasos), que las rutas del layout coinciden con `WebUI/pnpm-workspace.yaml`
(`../../ABDSharedAssets`, `../../ABDSharedCode/MidiKeyboard`) y que el comando que ejecuta es
exactamente el que pasa en verde en local (`npx playwright test e2e/visual.spec.js` con
`NEURONIK_E2E_PORT=5239`, el mismo puerto que le pasa ctest). El primer push a `master` es lo que lo
demuestra de verdad.

## 2026-09-28 — la fila del LCD: el D-pad estaba cortado a media altura

Encargo: mirar la fila del LCD en el plugin compilado y afinar margenes y chasis si quedaba
apretado. Sale de medir mucho: lo que habia era un **defecto visible**, no una holgura.

**Lo que habia** (override del 27 Sep en `WebUI/src/styles/main.css`): chasis 54px con
`overflow: hidden`, y el D-pad con la rejilla forzada a 2 columnas. Seis botones en 2 columnas son
3 filas, no 2: 3x14 + 2 gaps de 4 = **51px en los 42 de contenido**, y la tercera fila (los
cursores `^` y `v`) se salia del chasis, que la cortaba por la mitad — los botones vivian en
y=99..113 con el chasis cerrando en y=110. En la foto con 3x de aumento se veian los dos cursores
mutilados. El comentario del CSS lo reconocia ("la tercera [...] fuera de vista sin romper el
encaje") como si fuera una decision; era un recorte.

**El arreglo, dos lineas de CSS con su por que:**
1. `grid-template-columns: repeat(3, ...)` — la rejilla del paquete compartido (`lcdPanel.js` ya
   pedia `repeat(3, auto)`; el override local era el que la deshacia). Dos filas de 14 + 4 de gap
   = 32px: los seis botones dentro, 11px de aire.
2. `.abd-lcd__line` a `font-size: 11px` fijo en vez de `var(--text-sm, 11px)`, que **resuelve a
   9px** en este tema. Con 9 la pantalla 16x2 usaba 18 de sus 30px de alto y el texto se encogia
   en la esquina de una franja verde de 1200px de ancho; a 11 el texto llena el alto (28.6 de 30).

**Como se verifico, que es lo que cuesta**: a la medida EXACTA que recibe el WebView2 del plugin
(1440x946, segun `NEURONiKEditor`), midiendo el DOM en Chromium sobre el dist y comprobando que
no hay ningun boton fuera del chasis. Despues, el plugin de verdad: los tres objetivos
reconstruidos (el dist va embebido) y **selftest del Standalone con las 15 direcciones OK, exit 0**
— incluida MIDI-CC, que es la direccion que navega con estos mismos botones y que habria fallado
si el D-pad no respondiera.

**El cliente de los snapshots**: los dos "lienzo entero" de `visual.spec.js` fallaron, y con
razon. Antes de regenerarlos no se acepto el diff a ojo: se midio la caja de pixeles distintos
(fila del LCD y=62..109, x=32..1419 — la pantalla por un lado, el D-pad por otro, nada mas) y se
hizo la **linea base revirtiendo `main.css` a HEAD y reconstruyendo el dist**: los dos tests
pasan sin el cambio. Ese par (medir el diff + comprobar la linea base) es lo que distingue un
cambio legitimo de una referencia que se ha quedado vieja. Luego `--update-snapshots` de esos dos
y la E2E completa en verde: **22/22**, mas vitest **398/398**.

**Bancada, para que conste que no es regresion**: `MIDI-CC OK` con el D-pad nuevo (el learn se
arma y la pantalla pinta `CC 75` igual que en el plugin) y AGUJA sigue fallando por lo de antes,
`barras 8 (OK, gemelas DIVERGENTES)`. La fila del LCD no toca barras: la falla abierta de la
bancada sigue siendo la misma, y el LCD no ha anadido ninguna.

**Lo que faltaba y se ha cerrado en esta vuelta (28 Sep, tarde)**: las medidas de arriba estaban
tomadas en Chromium sobre el dist, no en el WebView2 del plugin compilado, y de ahi salio lo que
no se veia en ningun navegador de escritorio: el **`dpr 1.25`** (el escalado de Windows), la
ventana exacta `1440x946` y la pantalla en `1238x28` en vez de los `1239x29` de Chromium (el
layout cae en fracciones de pixel). Todo encaja, pero solo se puede SOSTENER que encaja
midiendo donde el usuario lo ve. Y de ahi salio tambien el agujero: el D-pad lo empuja el arnes
con eventos sinteticos, de modo que la fila paso las 15 direcciones, vitest y la E2E con los
cursores `^` y `v` cortados. Anadida la fase 0 (geometria por cajas), reproducido el defecto del
27 Sep para comprobar que lo caza (`"clipped":["up","down"]`, FAIL) y arreglo de un falso positivo
propio en la bancada (su pagina va escalada a 0.44, asi que el contrato se mira en pixeles de
LAYOUT). Estado final: **plugin 15/15 RESULT: OK (exit 0)**, bancada con la fase 0 **OK** y el
FAIL de siempre en AGUJA, snapshots del lienzo en verde sin tocarlos otra vez.

Un aviso de esta vuelta: la fase PANIC de AGUJA fallo **una vez de tres** corridas
(`medidor ENCENDIDO` con las agujas ya ocultas: el PANIC llego al motor pero el medidor no se
repinto). No se ha tocado: es un fallo de temporizacion de repintado, no de la fila del LCD, y las
corridas siguientes lo dieron en verde. Queda anotado aqui para que si reaparece se mire el
repintado del medidor y no el D-pad.

> Canon: anadida la subseccion "Fila del LCD (canon)" con las tres medidas (chasis 54, D-pad
> 3x2 de 39x14 con 11 de aire, pantalla 1239x29 con 28.6 de texto) y la regla de que el
> `overflow: hidden` es red de seguridad, no mecanismo.

## 2026-09-28 — NEURONiK_Vst3LoadTest: el VST3 de disco, escaneado como un DAW

Encargo: comprobar rapido que el VST3 recien compilado carga y expone sus parametros. No
existia ninguna comprobacion de eso: los tests de C++ hablan con el codigo, y el selftest habla
con el Standalone y la bancada. **El bundle `.vst3` que se instala en un DAW no lo miraba
nadie**, y es justo el artefacto que puede compilar sin errores y estar muerto.

**`Tests/Vst3LoadTest.cpp` + target `NEURONiK_Vst3LoadTest` (ctest #16, 0.3 s)**: un proceso
 aparte que no sabe nada de `NEURONiKProcessor` —lo unico que comparte con el plugin es el
 `.vst3` de disco— que hace el camino de un host: `VST3PluginFormat` (con
 `JUCE_PLUGINHOST_VST3=1`, que sin el no existe) -> `findAllTypesForFile` ->
 `createPluginInstance` -> lista de parametros -> `setValueNotifyingHost` -> estado ->
 `prepareToPlay` + un bloque. Los parametros esperados salen de `NEURONiK_ParameterContract`
 (el mismo que compila el plugin), asi que un bundle viejo y unas fuentes nuevas no pueden dar
 los dos verde. El target **depende de `NEURONiK_VST3`**: sin eso, un ctest sobre un arbol ya
 construido mediria el bundle de la corrida anterior. Tambien acepta una ruta por `argv[1]`,
 para medir una copia instalada.

Comprobado que **no es vacuo**: con una ruta inexistente y con un fichero que no es VST3 falla
con exit 1 y un motivo claro.

**Lo que salio, y que no se sabia:**

1. **El VST3 declara el fabricante como `yourcompany`.** Es el valor por defecto de JUCE
   (`juce_add_plugin` no lleva `PLUGIN_MANUFACTURER_NAME`), asi que en la lista de plugins de
   un DAW aparece el plugin con ese nombre. El codigo de fabricante (`Nrnk`) si que esta
   puesto. **No se ha tocado**: el nombre del fabricante es una decision de producto.
2. **El host recibe 2154 entradas, no 73.** Al servicio del plugin hay 73 —los del contrato, uno
   a uno por nombre— y el resto (2080 + 1) es del ENVOLTORIO VST3: la familia de automatizacion
   `MIDI CC <n>|<v>` y `Bypass`. Se filtran por nombre en el test, no por indice ni por recuento:
   si el plugin publicara alguna vez una entrada llamada `Bypass`, saldria como diferencia en
   vez de esconderse. Una DAW con vista de automatizacion ve las 2154.
3. **El estado del procesador llega como blob opaco por el borde del VST3**, no como el arbol de
   `PARAM` del APVTS (5348 bytes, tipo ilegible). Por eso el contraste de ids se hace por
   nombre —que es lo que el host ve— y no por estado, y la prueba de que un parametro esta vivo
   se hace por el objeto que automatiza el host, no por el estado.

Lo que si queda verificado del parametro en si: `Master Level` es automatizable desde el host,
`setValueNotifyingHost(0.25)` se relee a 0.25, el estado devuelto por el host (5348 bytes) lo
conserva, y `prepareToPlay` + un bloque de 512 muestras sale sin NaN. El scan declara el plugin
como instrumento, con uid 481681130.

`ctest` queda en **43 tests** registrados (antes 42).

## 2026-09-28 — el resumen final de build.bat dice QUE superficie fallo

Encargo: distinguir el veredicto del plugin del de la bancada cuando las dos corran en la misma
pasada. El problema real no era de formato: **el resumen final solo pintaba el del plugin**.

- El parser (`Scripts\selftest_summary.ps1`) leia el log ACUMULATIVO, que solo escribe el
  Standalone. La bancada su logFn escribe a stdout, nunca al log: su veredicto no llegaba al
  resumen, solo a las lineas en linea del paso 9.
- Peor: si el plugin fallaba, la pasada hacia `goto :finish` ANTES de correr la bancada, asi que
  el resumen no decia ni que la segunda no llego a correr.
- Y si fallaba la BANCA, el cierre era `RESULTADO: CON ERRORES` seguido de un resumen de plugin
  en verde sin una sola linea que dijera quien habia fallado.

**Ahora**: la bancada se captura a su propio transcript de UNA pasada
(`build-reference\neuronik-selftest-bancada.log`, borrado antes de correr para que un transcript
viejo no se imprima como si fuera de esta) y `:finish` imprime UN bloque por superficie, con su
etiqueta, su exit, su veredicto por direccion y su transcript:

```
  PLUGIN (Standalone, exit 0):
    [OK  ] ... 15 direcciones ...
    Veredicto: OK
    Transcript: ...\neuronik-selftest.log

  BANCADA (WebPilotHost, exit 1):
    [OK  ] ...
    [FAIL] AGUJA
           ...notas fuera sale, agujas env=off/flt=off (bloques off/off) (ocultas)
    Veredicto: FAIL
    Transcript: ...\neuronik-selftest-bancada.log
```

Y la superficie que NO se ejecuto se dice con su motivo (`PLUGIN_SKIP` / `PILOT_SKIP`, que se
inicializan al principio del script y no en el paso 9: si no, un `goto :finish` anterior
imprimia "no se ejecuto ()" sin explicar nada).

**El parser acepta ya `<log> [etiqueta] [modo]`**: `log` (acumulativo del plugin, con su aviso
de antiguedad) y `stdout` (transcript de una pasada, acotado por la ULTIMA linea `RESULT:`, que
es lo unico que cierra la corrida de la bancada). Un `[FAIL]` cuya linea acaba en `-> FAIL` sin
texto mas toma ahora el FINAL de la linea como motivo: sin eso el bloque decia `[FAIL] AGUJA` y
nada mas, que es justo el caso para el que se separan las superficies.

**Dos bugs que solo salieron al ejecutar de verdad, no al leer el codigo:**

1. `set "PILOT_ST=%ERRORLEVEL%"` estaba DESPUES del `type` del transcript, y `type` pone
   ERRORLEVEL a 0: una bancada en FAIL salia con `exit 0`, el `if` de abajo no entraba y la
   pasada se cerraba en `RESULTADO: OK`. El exit se lee ahora antes del `type`.
2. `Write-Header` se llamaba antes de estar definida (PowerShell no resuelve funciones definidas
   mas abajo en tiempo de ejecucion).

Y el guard del propio repo (`NEURONiK_ReferencedFiles`) canto en la primera pasada: el
`Tests\Vst3LoadTest.cpp` nuevo estaba SIN VERSIONAR y CMakeLists lo nombra, o sea que un clon
limpio no tendria el fichero. Esta en el indice.

**Estado tras el cambio**: pasada completa con las dos superficies corriendo y los dos bloques en
el resumen. El plugin da 15/15 OK; la bancada da AGUJA FAIL (las barras gemelas, la falla abierta
de siempre) — que antes cerraba la pasada en verde sin que nadie pudiera ver de quien era.

> Canon: el resumen del selftest tiene un bloque por superficie (PLUGIN / BANCADA), cada uno con
> su exit y su transcript; una superficie que no corrio se dice con su motivo. El transcript de
> la bancada es `build-reference\neuronik-selftest-bancada.log` (stdout, una pasada, se borra
> antes de correr) y el del plugin sigue siendo el acumulativo `build-reference\neuronik-selftest.log`.

## 2026-09-28 — la bancada tolera un arranque en frio del WebView2 (reintenta, y espera mas)

Encargo: que el arnés de la bancada reintente o extienda la espera de pagina lista cuando el
WebView2 no carga NINGUN recurso, para no perder la pasada por un pico de carga del sistema.

**El fallo que se tapa.** El timer de la bancada vigilaba la pagina con un presupuesto plano de
20 s: si en ese plazo la pagina no publicaba `__pilotReady`, `finish("timeout")` y FAIL. El caso
que seava a perder no es "la pagina va lenta" —la lenta la cubre el presupuesto— sino que el
WebView2 **no ha pedido nada**: el runtime de Edge arrancando en frio, el antivirus escaneando,
otra pasada de build en paralelo. Ahi no habia ni un segundo intento: 20 s de espera en silencio
y un FAIL sin explicacion.

**Lo que hay ahora** (`Source/WebPilotHost.cpp`):

- `watchColdStart()` se llama en cada tick mientras la pagina no esta lista. Si el proveedor no ha
  servido NINGUN recurso, **vuelve a pedir la pagina** cada `coldStartStallMs` (10 s), hasta
  `maxColdStartRetries` (3) veces, y **loguea cada reintento** (un reintento callado seria un FAIL
  sin explicacion). En cuanto el proveedor sirve algo, la funcion deja de hacer nada para siempre:
  a partir de ahi el problema, si lo hay, no es de arranque.
- `pageDeadlineMs()` da el plazo: sin senal, `20 s + 3 x 10 s = 50 s`; con senal, los 20 s de
  siempre contados desde la PRIMERA senal (no desde el ultimo sondeo, que moveria el plazo sin
  parar) y nunca mas alla del total en frio. El caso patologico se aguanta 50 s; el normal no se
  toca.
- El informe de la corrida (`pilot-startup.log`) tiene una linea nueva: `cold start retries: N / 3`
  y cuando fue la primera senal, para que una corrida con reintentos se vea sin buscar el stdout.

**El umbral va a 10 s y no a 6 s por una medicion, no por un gusto**: con 6 s, una corrida normal
de este equipo (maquina ocupada, primer recurso a **7,1 s**) disparaba un reintento
innecesario. Reintentar no es gratis —la nueva navegacion CANCELA la que estaba en curso—, asi
que el umbral tiene que quedar por encima de una carga lenta pero sana. Medido: con 10 s esa
corrida da `cold start retries: 0 / 3` y carga igual que siempre (ready a 7,4-8,4 s segun la
corrida, 6 recursos servidos).

**La senal es "el proveedor sirvio algo", y NO `readyState`.** La primera version contaba tambien
`document.readyState` y el panel en el DOM, y el experimento lo cazo: una navegacion que falla
deja igualmente un documento en `interactive` (la pagina de error del navegador), asi que tras
el primer reintento la funcion creia que todo iba bien y el proceso se rendia a los 28 s. Con la
senal correcta, el mismo escenario da los tres reintentos y se rinde a los 52 s.

**Como se ha verificado** (que no es "lo compile y me parece bien"): con el proveedor
neutralizado temporalmente —un `if` de tres lineas con una env var, ya revertido, 0 rastros en
el codigo— la corrida da `reintento de navegacion 1/3, 2/3, 3/3` a 10 s de intervalo, se rinde
a los 52 s con `reason=timeout` y exit 1 (sigue siendo FAIL: esto tolera el arranque atascado, no
tapa una pagina rota), y el informe dice `cold start retries: 3 / 3 (la pagina no dio ninguna
senal)`. Sin el experimento, la corrida normal da 0 reintentos y el unico FAIL sigue siendo el de
AGUJA (barras gemelas), que no tiene nada que ver.

Nota de alcance: el reintento es de la **bancada** (`WebPilotHost.cpp`). El editor del plugin
espera la pagina por su propio camino y no se ha tocado; si el problema aparece ahi, el arreglo
es el mismo patron en `NEURONiKEditor` (o mieux, en el `NeuronikWebView` compartido, que ya
centraliza el poll de las dos superficies).

> Canon: la bancada reintenta la navegacion mientras el proveedor no sirva NINGUN recurso (3
> reintentos cada 10 s, cada uno logueado) y solo entonces extiende su plazo de pagina a 50 s; con
> una senal, el plazo es el de siempre desde esa senal. La senal es `servedCount > 0`, nunca
> `readyState` (la pagina de error del navegador tambien lo cumple).

## 2026-09-28 — `cancelRouteReturn` fijado al nivel de la unidad, en los TRES caminos de abrir

> Canon: **«SECCION CANONICA — VOLVER (direccion 1b-bis)»**, al final de este fichero. Esta
> entrada cuenta como se llego; el guion vigente, el ciclo de vida del boton, la regla de
> cancelacion y la fontaneria de cierres estan alla.

Encargo: un test en `panel.test.js` que fije `cancelRouteReturn` al abrir por EDIT / franja /
chip — que el retorno muere y el boton se desmonta.

**Lo que ya estaba y lo que faltaba.** El bloque `panel / VOLVER A LA RUTA` de
`WebUI/tests/panel.test.js` ya fijaba la cancelacion por **EDIT**: dos tests, uno abriendo OTRO
cajon (`[data-drawer-trigger="globalFull"]`) y otro el propio origen. Faltaban los otros dos
caminos, y faltaba lo mas importante: **que la invariante este atada a la lista de caminos**, no
a un caso escrito a mano.

**Los tres caminos salen del MISMO hook.** En `panel.js`, `buildCard` recibe
`onDrawerOpenedByUser: () => cancelRouteReturn()` y lo llama en los tres gesture de abrir por si
mismo: el boton EDIT (`[data-drawer-trigger]`), el distintivo vivo de la cabecera
(`[data-live-badge]`, el `.card__badge`) y la franja de GLOBAL & MASTER (`.global-strip`, que es
un `role=button` con click y keydown). El riesgo real no es que el hook falle hoy —funciona— sino
que alguien abra un cajon desde un sitio NUEVO y se olvide de la linea: el retorno sobrevive y el
boton queda colgando, y nada lo diria.

**El test** (uno por camino, con el MISMO cuerpo, generado de una lista) monta el salto con
retorno pendiente (`clickEnvRouteJump`: ENV 1 -> RUTA 1, matriz abierta, boton colgando) y
comprueba las CUATRO cosas, en este orden:

1. punto de partida: destino vivo (`routeBack.target() === 1`) y boton **en el documento**;
2. `routeBack.target()` es null — **el retorno muere**;
3. `routeBack.element.isConnected === false` y el `.env-block__back` no esta en el cuerpo de
   ENVOLVENTES — **el boton se DESMONTA**, no se esconde (`clear()` hace `element.remove()`);
4. la vuelta que quedaba cobrada **no se ejecuta**: la matriz sigue abierta, cerrar la matriz no
   reabre ENVOLVENTES, y un `paint` posterior no hace revivir el boton.

Los tres abren GLOBAL & MASTER, que no es el origen del retorno, para que la cancelacion signifique
algo. La franja y el chip viven en el cuerpo de la FICHA, no en el cajon, y con la matriz abierta
estan bajo el velo: el clic del test es sobre el nodo del DOM, que es como los encuentra el
usuario al cerrar el cajon.

**Comprobado que NO es vacuo** (que es lo unico que hace que un test valga): quitando el
`onDrawerOpenedByUser?.()` SOLO del handler de la franja, el test de la franja se pone rojo con
`expected 1 to be null` —el retorno sobrevive— y **los otros dos siguen verdes**. Revertido a
continuacion (`git diff` de `panel.js` deja de estar modificado, 0 rastros del experimento).

Suite completa: **vitest 401/401** (antes 398, +3). Nota de convencion: `panel.test.js` esta en
LF, como estaba; no se ha tocado el codigo de la pagina.

## 2026-09-28 — el guion de 1b/1b-bis/1b-ter escrito como lo que es: UNA cadena por datos

> Canon: **«SECCION CANONICA — VOLVER (direccion 1b-bis)»**, al final de este fichero. Esta
> entrada cuenta como se llego; el guion vigente, el ciclo de vida del boton, la regla de
> cancelacion y la fontaneria de cierres estan alla.

La cabecera de `Source/WebUI/BridgeSelftest.h` describia las tres direcciones como si fueran
independientes, y no lo son desde que el slot viaja entre ellas. Documentado como tal, con los
tres datos que costan mas de ver en el codigo:

**De donde sale el slot de ENV-RUTAS.** No del APVTS: el Standalone de JUCE restaura el
`filterState` de la ultima sesion ANTES del selftest, y esa sesion puede traer la matriz del
usuario, asi que "el primer slot con fuente ENV" del motor no seria el que el usuario tiene
delante. Decide **lo que la pagina PINTA**: el script busca una fila entre los
`button[data-env-route]` y deduce cual es de ENV leyendo el select de fuente que ensena el cajon
de la MATRIZ que la direccion 0 dejo abierto. Los indices de `ENV 1`/`ENV 2` salen de
`State::getModSources()`; el APVTS se lee DESPUES del clic, solo para cruzar que el cajon
ensena lo mismo que el motor tiene en ese slot. El slot devuelto es el **argumento** de
`backDirection(slot)` y de `summaryRouteJump(slot)` — por los **dos** caminos: si ENV-RUTAS
falla, la cadena sigue igual con RESUMEN-RUTAS, y en ambos casos RESUMEN-RUTAS encadena el
settle y detras AGUJA.

**Como se repite cobertura sin repetir el mismo slot.** `scriptSummaryRouteJump(avoidSlot)`
elige `(avoid % 4) + 1` sobre las CUATRO filas `button.mod-summary__row`; si esa fila no esta
pintada devuelve `NO_ROW_FOR_SLOT` y la direccion **falla**, en vez de pulsar a ciegas.

**El settle de RESUMEN-RUTAS.** `scriptSettleDrawers()` cierra **POR SU ✕** cada cajon que
quede abierto, en un bucle de **hasta cinco**, y su JSON no se juzga: es limpieza, solo
encadena. No cierra "el que haya" por dos razones concretas: cerrar por el velo podria consumir
un retorno pendiente, y cerrar la matriz puede **REABRIR el cajon de origen** —el mismo
comportamiento que mide VOLVER—, asi que hay que iterar hasta que no quede ninguno. AGUJA
arranca sin modales. La cola `settle -> AGUJA` que existio antes murio con el encadenado: dos
AGUJAs en paralelo se pisan la pagina (aguja "visible" en silencio, notas atascadas en MIDI).

Tambien se arreglo el Doxygen de `envRouteJump()`, que seguia describiendo la derivacion vieja
("se DERIVA del APVTS... el slot esperado es el que el APVTS dice"). Build limpio (solo los
C4100 de siempre) y selftest del Standalone en verde: `ENV-RUTAS: 2 fila(s) ENV pintadas; clic en
la del slot 2 -> slot resaltado 2` / `VOLVER: ... slot resaltado 2 (esperado 2)` /
`RESUMEN-RUTAS: 4 fila(s) clicables; clic en la del slot 3`, `RESULT: OK`, exit 0 — la cadena
visible en el log con los tres slots distintos.

## SECCION CANONICA — VOLVER (direccion 1b-bis): el retorno del salto ENV -> MATRIZ

Referencia unica y vigente de la direccion VOLVER del arnes (`Source/WebUI/BridgeSelftest.h`,
`Stage::back`, `backDirection (int slot)` mas los scripts que la sostienen) y de la pieza de pagina
que la hace posible (`WebUI/src/ui/routeBack.js` + el hook del panel). Las entradas cronologicas
cuentan COMO se llego; donde una de ellas y esta seccion discrepen, **manda esta**. Complementa
la fila 4 de la tabla en «SECCION CANONICA — las 15 direcciones del selftest» (esa da la foto del
conjunto; esta da el detalle de una sola direccion).

### Indice: donde estaba repartido antes de esta seccion

La materia vivia en seis sitios, y con la certeza de que un golpe de grep no los encuentra todos.
Cada fila dice que aporta la entrada y que queda MANDADO por aqui.

| Entrada cronologica | Que aporta | Que queda canonico |
|---|---|---|
| 27 Sep, «VOLVER: el retorno del salto ENV -> MATRIZ se mide (la direccion 1b-bis)» | el guion de cinco medidas y el primer esbozo del ciclo de vida del boton | el punto 5 (derivacion del slot) esta CADUCADO: no sale del APVTS |
| 27 Sep, «VOLVER extendida: la regla de CANCELACION, medida E2E» | las tres aserciones de cancelacion y el `NO_GLOBAL_EDIT_TRIGGER` | — (vigente: nombres de scripts, id `globalFull` y selectores, mas abajo) |
| 27 Sep, «VOLVER en el arnes: encaje tras ENV-RUTAS, el settle de modales y el cierre por cajon abierto» | el encaje por datos, el settle y la fontaneria de selectores | su punto 1 tambien dice «ENV-RUTAS deriva el slot del APVTS»: CADUCADO |
| 27 Sep, «RESUMEN-RUTAS, la esquina clicable y la ayuda de gestos corregida» | el destino del slot al salir de VOLVER (`avoid`) | solo el tramo del encaje; el resto es de RESUMEN-RUTAS |
| 28 Sep, «cancelRouteReturn fijado al nivel de la unidad, en los TRES caminos de abrir» | los tres gestos que cancelan y los cuatro tests que lo fijan | — (vigente) |
| 28 Sep, «el guion de 1b/1b-bis/1b-ter escrito como lo que es: UNA cadena por datos» | por que el slot viaja y no se recalcula | — (vigente) |

### Que es el retorno, y quien lo siembra

El unico gesto que **siembra** retorno es «IR A LA RUTA n» del cajon de ENVOLVENTES
(`scriptBackJump (int slot)`); las filas `.env-route` del lienzo abren la matriz SIN retorno porque
el VOLVER no nace con ellas. El estado esta partido por diseño, con un dueño cada cosa:

- el **dueño del gesto** es el panel (`WebUI/src/ui/panel.js`): `let routeReturn = null` es el
  retorno pendiente, lo arma `openDrawerRoute(sectionId, slot, { returnTo })` y lo **consume una
  sola vez** en el `onClose` de la matriz (`const returnTo = routeReturn; routeReturn = null`), que
  es lo que reabre ENVOLVENTES y reancla el boton. Un salto nuevo sustituye al anterior ANTES de
  cerrar nada: el mueble compartido no tiene cierre silencioso, asi que un `close()` alli
  consumiria un retorno vivo.
- la **vista** es `WebUI/src/ui/routeBack.js` (`createRouteBack`): guarda el ultimo destino
  (`lastTarget`), pinta/oculta y cablea el gesto (`onBack` -> `openDrawerRoute('modMatrix', slot,
  { returnTo })`). La SSOT del texto y de la clase viven alli (`ROUTE_BACK_TEXT = 'VOLVER A LA
  RUTA'`, `ROUTE_BACK_CLASS = 'env-block__back'`); el panel solo juega con `hidden` y
  `textContent`, y `clear()` hace `element.remove()` —el boton se **desmonta**, no se esconde.

### El guion de la direccion (cinco medidas, en orden)

1. **RE-SALTO**: pulsa otra vez la MISMA fila de ruta del slot recibido como argumento; exige
   matriz abierta.
2. **CIERRE POR USUARIO**: el ✕ de la matriz (`scriptCloseMatrixDrawer`), no un ESC programatico
   —es el gesto que PAGA la vuelta, y por eso no se simula.
3. **BOTON PRESENTE** (`scriptReadBackButton`): al reabrirse el origen tiene que estar
   `.env-block__back` con SU numero («VOLVER A LA RUTA n»). Sin boton (oculto o descolgado) el
   gesto no existe y la direccion falla.
4. **CLIC** (`scriptPressBackButton`): la matriz reabierta tiene que quedar con SU velo y el MISMO
   slot resaltado (`data-slot-highlight`).
5. **CANCELACION** (extension posterior): abrir GLOBAL por su cuenta con el retorno pendiente lo
   mata y el boton no revive — ni al cerrar GLOBAL ni al cerrar despues la matriz, que tampoco
   reabre el origen solo.

El slot **viaja por datos**: lo elige ENV-RUTAS por lo que la pagina PINTA y lo recibe
`backDirection` como argumento; ninguna capa lo recalcula ni lo hardcodea. (Correccion respecto a
las entradas viejas: la derivacion ya **no** es «el primer slot con fuente ENV» del APVTS —el
Standalone restaura el `filterState` de la sesion anterior y ese puede traer la matriz del
usuario—; ver el bloque «ENCADENADO POR DATOS» de la cabecera de `BridgeSelftest.h`.)

### La regla de CANCELACION, y los TRES caminos que la disparan

Un solo hook, en `panel.js`: `buildCard` recibe `onDrawerOpenedByUser: () => { cancelRouteReturn(); }`
(la linea 277) y lo llama en los tres gestos de abrir por si mismo —el boton EDIT
(`[data-drawer-trigger]`), el distintivo vivo de la cabecera (`[data-live-badge]`) y la franja de
GLOBAL & MASTER (`.global-strip`, con click y keydown)—. El riesgo que dejó escrito la entrada del
28 de septiembre no es que el hook falle hoy, sino que alguien abra un cajon desde un sitio NUEVO y
olvide la linea: el retorno sobrevive y el boton queda colgando sin que nada lo diga. Por eso hay
un test por camino en `panel.test.js` (los tres, con el MISMO cuerpo generado de una lista) y cada
uno exige las CUATRO cosas en este orden: destino vivo -> `routeBack.target() === null` -> boton
**desmontado** (`element.isConnected === false`, no escondido) -> la vuelta que quedaba cobrada
**no se ejecuta** (la matriz sigue abierta, cerrarla no reabre ENVOLVENTES y un `paint` posterior
no hace revivir el boton).

Scripts con los que se mide en la pagina viva: `scriptOpenGlobalDuringReturn` (el EDIT de GLOBAL,
`[data-drawer-trigger="globalFull"]` — el id de la seccion es `globalFull`, no `global`),
`scriptCloseDrawerById ("drawer-globalFull")` y `scriptReadPostCancelState`.

### Fontaneria: como se cierra un cajon (y por que el ✕)

El mueble deja el ✕ de cada cajon en el DOM aunque este cerrado (inerte, `aria-hidden`), asi que
«cerrar el abierto» es `document.querySelector('.drawer--open .drawer__close')`, y eso es valido
mientras haya **un** cajon abierto. Con dos (GLOBAL y la matriz) ese selector cierra el
EQUIVOCADO, el primero del documento: de ahi `scriptCloseDrawerById`, que cierra por
`[data-drawer="drawer-<id>"]`. Regla que deja esto escrito: el cierre de **usuario** (el ✕) es el
que paga el retorno, el que lo cancela si antes se abrio otro cajon y el que limpia el lienzo en el
settle; el cierre por velo queda fuera porque puede consumir un retorno pendiente por un camino que
la direccion no cubre.

El **settle** (`scriptSettleDrawers`, en RESUMEN-RUTAS y no en VOLVER) cierra por su ✕ cada cajon
que quede abierto, **iterando hasta cinco**, porque cerrar la matriz puede reabrir el cajon de
origen. Es limpieza, no veredicto: su JSON no se juzga, solo encadena AGUJA. La cola
`settle -> AGUJA` que un dia vivio dentro de VOLVER murio con el encadenado — dos AGUJAs en
paralelo se pisan la pagina.

### Estado verificado (28 de septiembre de 2026)

- Standalone `--selftest`: `ENV-RUTAS ... slot 2` -> `VOLVER: el boton reabre "drawer-modMatrix"
  ..., slot resaltado 2 (esperado 2)` -> `RESUMEN-RUTAS ... clic en la del slot 3`, `RESULT: OK`,
  exit 0. Los tres slots distintos en el log **son** la prueba de que la cadena viaja por datos.
- Bancada (WebPilotHost): la direccion pasa; la que sigue fallando en la bancada es AGUJA, por las
  barras gemelas divergentes, y es una falla abierta conocida, no de esta direccion.
- vitest 401/401, con los tres tests de cancelacion de `panel.test.js` incluidos.

## 2026-09-28 — canal `neuronik:voice`: los ocho knobs de envolvente POR FIN llegan al motor local

La limitacion que arrastraba `WebUI/needle-probe/` desde el 27 de septiembre era real y estaba
bien medida: en modo local el worklet solo canalizaba GlobalParams (matriz, LFOs, FX) y el morph.
Los ocho tramos de envolvente —`envAttack/Decay/Sustain/Release` y `filterAttack/Decay/Sustain/
Release`— son **VoiceParams**, no GlobalParams, y VoiceParams no tenian ningun camino al motor en
el navegador: se movian en la pagina, la ENV 1 y la ENV 2 sonaban con los defaults de C++
(sustain 0.7/0.7) y la aguja del navegador no podia distinguir una de otra. Que el *plugin* suene
bien no lo tapaba: ahi los escribe el APVTS.

**POR QUE UN CANAL NUEVO Y NO UN CAMPO MAS EN GlobalParams.** El `AdditiveVoice::Params` (motor
NEURONiK) y el `NeurotikVoice::Params` (motor Neurotik) son structs DISTINTOS entre si —el
neurotik no tiene envolvente de filtro, sus `getFilterEnvelopeLevel()` son 0 fijo—, asi que
cruzar "el struct de voz" por el puente ataria el ABI a uno de los dos motores. Lo que cruza es un
POD neutro de ocho floats (`VoiceEnvelopeWire`, el primer campo = attack de AMP, el cuarto = release
de AMP, el quinto = attack de filtro) con tres exports, exactamente el mismo patron que
GlobalParams: `neuronikVoiceEnvelopeSize` (32 bytes), `neuronikVoiceEnvelopeLayout` (los ocho
offsetof, y con `out==0` el numero de campos) y `neuronikSetVoiceEnvelope(pod, size)`. El POD
carry el layout del *contrato* de la pagina, no el de un struct, y por eso no se mueve cuando uno
de los dos motores crece.

**EL TRAMPA QUE HACE FALTA NOMBRAR, y que esta escrito en el codigo**: el camino NO puede ser
`setVoiceParams`, que reemplaza el struct ENTERO. Como el worklet no tiene APVTS, el unico
constructor de `AdditiveVoice::Params` que hay del lado JS es ese, y llamarlo en cada tecla habria
borrado el `morphX/Y/Z` y los volumenes de las capas 1 y 2 que la pagina ya habia cruzado por
`neuronikSetVoiceMorph`/`SetVoiceLayerMorph`. Por eso el read-modify-write se hace en el MOTOR:
`BaseEngine::setVoiceEnvelope` (los ocho tramos, en ms, con los sustains en 0..1) es virtual y
nadie en la base lo publica, NeuronikEngine lo implementa tocando los ocho campos de
`pendingVoiceParams` y NeurotikEngine solo los cuatro de la ENV de AMP. Los otros cuatro no tienen
destino ahi —y no hacen falta: el worklet los conserva en su espejo y los reenvia cuando se vuelve
a NEURONiK—.

**UNIDADES**: el layout del puente esta en MILISEGUNDOS (las de `AdditiveVoice::Params`) y el
contrato da SEGUNDOS, que es lo que ve el APVTS del plugin. El factor 1000 lo aplica la pagina
(`voiceFieldToReal` en `wasm/audioParams.js`), igual que el `* 1000.0f` de
`synchronizeEngineParameters`. Sin el, el attack llegaria como 10 microsegundos. El clapeo de
tiempos y sustains lo hace `Envelope::setParameters` (>= 0.1 ms, 0..1), igual que en nativo; la
frontera solo rechaza el snapshot entero si trae un no-finitos (un NaNTravels por
structuredClone), y descartarlo entero no pierde nada porque la pagina manda siempre los ocho.

**EL TRABAJO EN EL WORKLET**: espejo `veMirror` (ocho floats) + `vePtr` en el heap, escrito por
indice de layout; `veDirty` marca que la pagina ya hablo. El espejo **no** se manda al arrancar a
propósito (el motor nace con los defaults de su struct y un push de ceros los congelaria en
silencio), y `veDirty` es lo que hace legitimo re-aplicarlo entero tras `neuronik:engine`, que
reconstruye el motor: los modelos y el morph ya se re-aplicaban por esa misma razon, el ADSR se
suma a la lista. El payload de la pagina es por eso un snapshot COMPLETO
(`voiceFieldsFromState` no salta ids ausentes: caen al default de su descriptor), no un delta
como el de GlobalParams.

**LA PRUEBA QUE NO SE PODIA ESCRIBIR, escrita.** La pagina de la aguja tiene un boton nuevo que
empuja sustains 0.8 (amp) y 0.2 (filtro) por el mismo `pushVoiceToWorklet` que llama `app.js` en
cada sync, y el spec exige ahora que las needles MIDAN esos valores: amp > 0.6, filtro < 0.4 y
separacion > 0.3. Sin el canal, el poll no sale nunca (las cuatro se quedan en el 0.7 de C++), y
comprobado esta: muteando el worklet para que no llame a `_neuronikSetVoiceEnvelope` el test cae
rojo y el otro sigue verde. La asercion tiene un detalle que costó un fallo rojo propio: hay que
esperar al **sustain**, no al primer frame visible —el primer frame visible es el pico del attack,
donde las dos needles valen ~1.0 y no distinguen nada—; por eso el predicado del poll es
`settled` y no `allVisible`.

Verificado: `build_wasm.bat` completo en verde (paridad nativo<->WASM muestra a muestra, smoke,
sync a `public/worklet` y guard de hashes contra `dist`), `workletSyncTest` sin drift con
`WebUI/dist` reconstruido, vitest **407/407** (6 nuevos: el layout del POD, las unidades en ms, el
snapshot completo, el canal por el que sale y la llamada desde `app.js` —esta ultima con regex y no
con `toContain`, que una linea comentada con la misma llamada haria pasar), E2E **22/22** con
`needleProbe` 2/2, ctest 42/43 (el unico rojo es `NEURONiK_WebUiLocalModeE2e`, el flake conocido del
reloj de `AudioContext`, que pasa al re-run y en la corrida completa) y medicion directa desde node
del canal entero: empujar 0.8/0.2 deja las needles del navegador clavadas en 0.80 y 0.20.

> La entrada del 27 de septiembre que documente la limitacion queda marcada CADUCADA donde estaba.

## 2026-09-28 — las agujas SOSTENIDAS tienen foto de referencia: el unico bloque visual con audio

La regresion visual del lienzo (`e2e/visual.spec.js`, una foto por ficha) se hizo
con una regla explicita: **sin audio, con estado de fabrica** — con el motor arrancado
el medidor, el anillo y el LCD se mueven con el reloj del `AudioContext` y la foto
seria distinta cada vez. La aguja seguia sin fotografia: `needleProbe.spec.js` afirma
que las cuatro se VEN, pintan nivel y son gemelas, y (desde el canal `neuronik:voice`
de la entrada anterior) que miden 0.8 y 0.2, pero nada fijaba que se **pinten en su
sitio**: un `PAD` que se cayera, una escala mal repartida o una curva que se moviera
un pixel salen verdes en las aserciones de numero.

**EL BLOQUE NUEVO** (`NEURONiK Visual Regression - las agujas (needle-probe)`, al final
de `visual.spec.js`, y por tanto dentro de `pnpm test:visual` y del ctest
`NEURONiK_WebUiVisualRegression` sin tocar CMakeLists) rompe la regla de "sin audio", y
lo que lo hace determinista son cuatro cosas, todas medidas:

- **Se captura ASENTADO, no en vuelo.** El poll espera a que las cuatro agujas esten en
  su sustain con margen 0.03 (0.8 la de amp, 0.2 la de filtro). El primer frame
  visible es el pico del attack, donde las dos valen ~1.0: la version con un
  `waitForTimeout` fijo regenero la referencia con 0.81/0.24 y fallo en rojo. Ahora son
  dos **muestras consecutivas iguales** a dos decimales, con `expect.poll`.
- **El `d` redondea.** `envelopeNeedlePath` escribe la Y con `toFixed(2)`, asi que en el
  sustain los ultimos bits del float no llegan al pixel.
- **Dos elementos, no la pagina.** Se fotografian `.env-curves` y `.env-blocks`: el texto
  del estado lleva los Hz del dispositivo de audio (44100 en un portatil, 48000 en la
  bancada) y haria la referencia dependiente de la maquina.
- **`maxDiffPixels: 0`**, mas estricto que el 20 de la config. Medido: dos corridas
  seguidas con el motor de verdad sonando dan 0 pixeles de diferencia.

Y el boton "ADSR" de needle-probe ahora **repinta las dos vistas con el mismo estado que
empuja al motor** (antes solo lo empujaba), para que la aguja caiga en la altura del
sustain de SU propia curva y la foto signifique algo: si la escala de la aguja o el alto
de la curva se mueven, la referencia se aparta aunque el motor siga perfecto.

**CAZA, y se comprobo dos veces**: `PAD` de 2 a 6 en `envelopeCurve.js` → 1535 pixeles
distintos; y quitar el repintado del boton (las curvas se quedan en el contrato por
defecto) → 1162 pixeles distintos. El primer intento de mutacion no cazo nada y la
leccion es la de siempre: el `str_replace` no habia aplicado porque la linea era
`const PAD = 2;`, sin `export` — un test que pasa sin haber mutado nada no prueba nada.

**DE PASO, un guard triplicado**: `ensureAudioClock` estaba copiado en `localMode.spec.js`
y `needleProbe.spec.js` (y el tercero lo necesitaba el bloque nuevo). Vive ahora en
`e2e/support/audioClock.js` —fuera del `testMatch` de Playwright, asi que no se cuela
como spec— y lo importan los tres. La razon del guard (con `--mute-audio` el
`AudioContext` dice 'running' pero su reloj no avanza: el grafo no se tira, el worklet no
procesa y la pagina miente) queda escrita ahi, no repetida tres veces.

Verificado: visual **12/12** (los 11 de antes intactos + el nuevo), E2E completo **23/23**,
ctest `NEURONiK_WebUiVisualRegression` en verde por su cuenta, vitest 407/407. Las dos
referencias nuevas viven en `e2e/snapshots/` con el nombre que compone el
`snapshotPathTemplate` (`{testName}-{arg}`), versionadas con el resto.

## 2026-09-28 — needle-probe entra en ctest como tercer E2E de Playwright, con sus dos puertos

La pagina de la aguja (`WebUI/needle-probe/`) tenia spec (`e2e/needleProbe.spec.js`) y se
corría a mano, pero **no era un test de ctest**: los otros dos si lo eran
(`NEURONiK_WebUiLocalModeE2e` y `NEURONiK_WebUiVisualRegression`). Una suite que solo
existe en local se acaba creyendo verde por costumbre, que es justo lo que el arnés del
plugin evita con tanto cuidado.

**LA ENTRADA** (`NEURONiK_WebUiNeedleProbeE2e`, `CMakeLists.txt`) copia el patron de las
otras dos al pie de la letra: mismo ejecutable de node, mismo `cli.js` de Playwright,
`WORKING_DIRECTORY` en `WebUI/`, `TIMEOUT 600`, el mismo `RESOURCE_LOCK
"neuronik_webui_dist"` y el `else()` que avisa por status si Playwright no esta instalado
(cuenta: 44 -> 45 tests).

**LOS PUERTOS, Y AQUI ESTA EL TRABAJO DE VERDAD.** Este test es el primero que necesita
**los dos** servidores del `webServer` de `playwright.config.js`: el preview de `dist`
(que su webServer construye antes de servir) y el dev de `needle-probe` (la pagina no
entra en `dist`, es de prueba y necesita los fuentes sueltos). Y el del dev estaba
escrito a pelo, en dos sitios —el `command`/`url` de la config y la URL de cada spec—,
cosa que hacia imposible que dos tests que usen la pagina coexistieran. Ahora:

- `PROBE_PORT = NEURONIK_E2E_PROBE_PORT ?? 5237` en la config, con el comentario que
  explica que es el mismo motivo que `E2E_PORT` (con `--strictPort`, dos servidores en el
  mismo se comen un EADDRINUSE y el fallo aparece a los 13 s hablando de puertos);
- `WebUI/e2e/support/probePage.js` con la URL, que ahora la leen los DOS specs que la
  usan (`needleProbe` y el bloque de agujas de `visual`) —un solo sitio, y
  `NEEDLE_PROBE_URL` sigue mandando sobre las dos por si hay un servidor levantado a mano.

Asi el test nuevo lleva `NEURONIK_E2E_PORT=5240;NEURONIK_E2E_PROBE_PORT=5241` y no
comparte ningun puerto con los otros dos. El candado de `dist` se conserva igual, y con
motivo: aunque los puertos ya no se pisan, el webServer de `dist` lanza `npm run build`
sobre el MISMO directorio de salida, y dos `vite build` a la vez se pisan (medido, no
supuesto).

**LO QUE HAY QUE SABER SI ALGO FALLA**: el primer `ctest -R` de este test dio `Timed out
waiting 120000ms from config.webServer` —los servidores de Playwright de la corrida E2E
anterior seguian levantados y la maquina iba cargada—. Repetido en limpio: 32 s, verde.
No era el cableado; si vuelve a salir, mirar primero si hay un `vite`/`vite preview`
vivo de otra corrida (`netstat` en 5236-5241) antes que la configuracion.

Verificado: el test nuevo en verde solo (32 s) y en la suite completa `ctest -j6` con los
tres E2E de Playwright corriendo en paralelo (45 needles 41.9 s, 43 localMode 57.4 s, 44
visual 39.2 s). Con `-E "ModulationMatrixTest|ReferencedFiles"`, 43/43 en verde.

> Aviso de la casa, ajeno a este trabajo: en `ctest -j6` completo fallan
> `NEURONiK_ModulationMatrixTest` (Not Run: su exe no esta construido) y
> `NEURONiK_ReferencedFiles` (`Tests/ModulationMatrixTest.cpp — SIN VERSIONAR`). Ese
> fichero y `builddeep_tmp.bat` son **nuevos y sin versionar, de otra sesion** (aparecieron
> a mitad de esta, con su bloque en `CMakeLists.txt` ya dentro). No se ha tocado nada de eso:
> no es mio y versionarlo seria decidir por otro hilo. Lo que hace falta es un `git add`
> de `Tests/ModulationMatrixTest.cpp` y un build del target.

## 2026-09-28 — la pista de teclado de las esquinas, en el tooltip Y con UNA sola fuente

El clic en una esquina A–D abre el cajón de MODELOS en esa ranura, y con foco tambien
responden `Enter` y `Space` (el `keydown` delegado de `ui/xyPad.js`). Ese teclado estaba
documentado en **un solo sitio**: el item "Esquinas A-D" del `<details>` "Gestos del pad
XY" del cajón — o sea, solo se leia donde no hace falta. El `title` de la esquina, que es
lo que aparece justo encima del elemento cuando el usuario ya lo está mirando, no lo
decia.

**El texto, una vez.** `CORNER_KEYBOARD_HINT = 'Enter/Space'` exportada por
`ui/xyPad.js` (el modulo que despacha el clic y el `keydown`: el gesto es suyo, la
ayuda solo lo documenta) e interpolada en los dos titles de `paintCorners` —el de la
ranura cargada y el de la divergente, que si no documentarian gestos distintos— y en el
item de la ayuda de `ui/visuals.js`, que ahora la importa. Se corrigieron de paso los
DOS comentarios de `xyPad.js` que ya decian "Enter/Space con foco" en prosa: repetian
la palabra, y un cambio de teclas los dejaria mintiendo, asi que ahora dicen
"las teclas de CORNER_KEYBOARD_HINT".

**Y el test que hace cumplir la fuente unica** (`xyPad.test.js`), que es la parte que
hace que esto no se deshaga en un mes:

- las dos variantes del `title` contienen la constante importada (comparar contra la
  constante, no contra la cadena, para que un cambio de la constante no rompa el test
  sin querer), y el item de la ayuda tambien;
- la palabra aparece **exactamente una vez** en los dos modulos, y la cadena esta
  **fijada en el test** en vez de derivarse de la constante: asi un renombrado de las
  teclas sale rojo y obliga a revisar los dos textos a proposito;
- los dos titles interpolan la constante y el item de la ayuda tambien (conteo de
  usos: 2 y 1).

Comprobado que muerde, por los dos lados: quitar la pista del tooltip pone 2 tests en
rojo, y hacer que la ayuda escriba "(Enter/Space con foco)" a mano en vez de
interpolar la constante tumba el de la fuente unica con el mensaje "'Enter/Space'
aparece 2 veces; debe aparecer 1". Los dos experimentos revertidos, 0 rastros.

Verificado: vitest **408/408**, E2E 22/23 (el rojo es `needleProbe` por el reloj de
`AudioContext` en la corrida completa cargada —5.8 min—: al re-run y en solitario 2/2),
selftest del Standalone `RESULT: OK` exit 0 (las esquinas se pintan tambien en el
WebView2 del plugin; el arnés solo mira los `title` de las rutas de la matriz, no los
de las esquinas). `dist` reconstruido.

## 2026-09-28 — revision de gestos no obvios: la ayuda GLOBAL de gestos, y lo que NO hacia falta

Encargo: revisar si otras fichas con gestos no obvios (franja GLOBAL, chips vivos, filas del
resumen) necesitan ayuda contextual o tooltip. Se hizo con un volcado del DOM REAL montado
(vitest + jsdom, el mismo arnes que `panel.test.js`) de los 29 knobs, 12 `title`, todos los
`role`/`tabindex`/`aria-*`, y despues contra el codigo de cada gesto. Resultado: **una sola
ficha tenia el hueco, y no era ninguna de las tres citadas**.

**LO QUE NO NECESITA NADA (y dos falsos positivos que el volcado produjo).**

- *Franja GLOBAL*: el DOM la muestra como `role=button` + `tabindex=0`, la forma exacta de
  un boton de teclado que no hace nada. El codigo dice lo contrario: tiene su `keydown` con
  `Enter`/`Space` (`ui/panel.js`). Sin hueco. Solo la redaccion es pasiva: el `title` dice
  "Tempo, MIDI y aleatorio se editan en el cajon" sin el verbo de accion, mientras las filas
  del resumen ("RUTA 1: abrir en la MATRIZ...") y las esquinas ("Abrir MODELOS...") si lo
  llevan — y su `aria-label` ("GLOBAL & MASTER: abrir el cajon") si lo lleva. O sea, el
  problema es del texto de raton, no del arbol de accesibilidad.
- *Segmentado del motor*: el volcado dio `aria-label=null` y parecio un `radiogroup` sin
  nombre. Falso positivo: el compartido lo nombra con `aria-labelledby` (`ui/controls.js` le
  pasa `id`). Leccion: el volcado tenia que imprimir tambien ese atributo.
- *Chips vivos* (`.card__badge`) y *filas del resumen* (`button.mod-summary__row`,
  `button.env-route`): llevan recuento y destino en `title` y `aria-label`, son `<button>`
  (teclado gratis) y tienen `cursor:pointer` + hover. Sin hueco.

**EL HUECO REAL: los 29 botones redondos.** El componente compartido les da CUATRO gestos y
ninguno se le contaba a nadie: arrastrar, `Shift` = 0.2x (1/5), rueda = 1/20 de la pista,
flechas = ±0.01, y doble clic. Es la inversion de lo razonable: los controles MAS numerosos
sin documentacion, los dos mas RAROS (pad y aro) con su pista en el `title` y su lista en el
cajon de MODELOS — que es justo donde nadie busca una ayuda general.

**LO NUEVO** (`WebUI/src/ui/gestureHelp.js`, montado en la cabecera, plegado): una sola
ayuda de gestos de la pagina, con la lista como fuente unica del texto. Magnitudes medidas
contra el componente, no de memoria, y hay dos que habria sido un error escribir de memoria:

- el teclado del knob son **solo las cuatro flechas** a paso 0.01. Ni RePag ni Inicio/Fin:
  esos son del ARO, que tiene otro manejador. Ponerlos aqui habria sido mentira;
- el **doble clic va al MINIMO del rango**, no al valor por defecto: `knob.js` hace
  `clamp01(options.value ?? 0)` y la WebUI construye el knob con `value: 0`
  (`ui/controls.js`), y `setValue` no toca `options.value`. O sea, en cualquier parametro se
  va al extremo (silencio en gain, 20 Hz en cutoff). El texto dice lo que hace —la unica cosa
  honesta— y **el defecto es del paquete compartido** (`@abdsynths/shared`, otro repo): no se
  puede arreglar desde aqui. Decision pendiente del dueno: arreglar el knob compartido (que
  el dblclick vaya al default del contrato, o quitarlo) o dejar el gesto como esta.

Lo que NO se repite en la ayuda: el pad y el aro ya llevan su gesto en el `title` y su lista
completa esta en el cajon, asi que aqui solo se remite; y la pista de teclado de las esquinas
se **importa** de `xyPad.js` (`CORNER_KEYBOARD_HINT`), para que la palabra no tenga dos
versiones — la regla de la entrada anterior, ahora en un tercer consumidor.

**LO QUE CUESTO EN GEOMETRIA, y por que hay un E2E midiendo.** La cabecera tiene ALTO FIJO,
asi que la lista es un popover (`position: absolute`) y no una fila mas. Se cayo dos veces
antes de fijarlo, y las dos las cazó medir en Chromium en vez de mirar codigo:

1. el quinto hijo de la rejilla (`auto 1fr`) cae en una **fila implicita nueva**: medido, el
   resumen quedo en y=44..52 con la cabecera en 0..35, o sea fuera de su banda y encima de la
   fila del LCD;
2. al meterlo en la fila 3 (la de la linea de contrato, que ocupa todo el ancho con
   `justify-self: end` y su mitad izquierda vacia) la linea de contrato **bajaba 10 px**.

La solucion es compartir CELDA: los dos con `grid-area: 3 / 1 / 4 / -1`, la ayuda a la
izquierda (`justify-self: start`) y el contrato a la derecha. Medido despues: ayuda
y=33..41 x=100..121, contrato y=33..41 x=1017..1180 — la linea de contrato vuelve
exactamente donde estaba. Y el popover: con `right: 0` se salia por la izquierda
(x=-143 con 265 px de ancho), asi que va anclado con `left: 0` (medido: x=100).

Ojo con la asercion de referencia: la caja CSS de la cabecera (30 px) se queda CORTA con esa
fila **ya antes de este cambio** (la linea de contrato tambien la desborda), asi que el test
compara contra la linea de contrato y no contra la caja. Un primer intento de test que
comparaba contra la caja daba rojo con la ayuda ya correcta.

Comprobado que muerde: sin montar la ayuda en la cabecera, el test de `panel.test.js` cae
rojo. El E2E `localMode.spec.js` mide banda, no-solape, contenido y legibilidad del popover.

Verificado: vitest **412/412** (4 nuevos: 3 de la ayuda + 1 en `panel.test.js` que ata la
ayuda a la cabecera y protege el invariante del host — el primer `input[type=range]` sigue
siendo masterLevel y la ayuda no mete ningun input delante), E2E 23/24 (el rojo es
`needleProbe` por el reloj de `AudioContext` en la corrida cargada; al re-run y en solitario
2/2), referencias visuales regeneradas (solo cambian las 2 de pagina entera: las 9 por ficha
no se movieron, lo que confirma que la ayuda no empujo ninguna banda) y selftest del
Standalone `RESULT: OK` exit 0 (la cabecera tambien existe en el WebView2 del plugin).

## El master volume es un Knob y GLOBAL & MASTER sube a la banda del motor (2026-09-28)

Dos cambios de una vez, porque el segundo se ve en el primero.

**1. El control base deja de ser un fader y pasa a ser un `Knob` del paquete
compartido**, dentro de la ficha GLOBAL & MASTER. El `Knob` es un
`div[role="slider"]` **sin `<input>` dentro**: expone `setValue()`, `getValue()`,
`setModulation()`, `destroy()` y escribe `aria-valuenow` / `aria-valuetext`, pero
no tiene `.value` ni setter de `HTMLInputElement`.

Eso obliga a mover el **ancla del arnés del selftest**. Era
`SelftestPage::firstRange = "input[type=range]"`, y funcionaba por un accidental: el
fader era el primer range del documento y las ruedas del teclado venían detrás. Con
un knob, ese selector pasa a apuntar a la **rueda de modulación** — y devolvería
`0` en un test que crees que está midiendo el master, que es la peor forma de
romper un contrato. Ahora el ancla es **explícito**:

- `SelftestPage::baselineControl = "[data-baseline-control]"`, escrito en la celda
  (no en el dial: la celda es lo que sobrevive a un repintado del knob), y
- la celda expone además un **puente** `baselineControl.{value, setFromSnapshot}`
  — el equivalente honesto de lo que antes eran `.value` del input y su setter.
  `setFromSnapshot` es el camino de `paint` (no empuja al store: si lo hiciera,
  NATIVO→JS seguido de un edit real haría un bucle) y `value = x` el del arnés o el
  de un gesto (`commit`: empuja al store y cierra el gesto, como un dedo).

**2. GLOBAL & MASTER sube a la banda del motor, al lado del LFO.** El reparto de
`SECTIONS` pasa a `oscillator, lfo, globalFull | resonator, filter, modMatrix |
envelopes, models, fx`. Las tres bandas siguen sumando 12 lanes: 6+2+4, 2+2+8 y
5+2+5. Antes GLOBAL cerraba la banda de abajo con la matriz.

### El fallo que la mudanza destapo (y por que NO se arregla en el mueble compartido)

Cada cajón de `createDrawer` (ABDSharedAssets/components/drawer.js →
overlayFocus.js) engancha **un listener de `keydown` en `document` por instancia**,
y el destino del salto de ruta lo es. Con el retorno de ruta vivo, "cerrar la
MATRIZ vuelve a ENVOLVENTES" **se comía a sí mismo**: la misma pulsación de ESC
recorría los dos listeners, la matriz se cerraba, su `onClose` reabría
ENVOLVENTES al instante, y el listener de ENVOLVENTES —que ya lo encontraba
abierto— lo volvía a cerrar **dentro del mismo evento**.

No se nota desde hace años porque el orden de montaje decidía quién corría
primero, y al mover GLOBAL en SECTIONS cambió: la MATRIZ pasó a montarse antes que
ENVOLVENTES y su listener, antes inocuo, pasó a ir el segundo.

**Se probó y se descartó** `queueMicrotask(() => back.open())`: por fin la
reapertura es correcta, pero entra tarde y rompe 5 tests (2 → 5). El
`drawer.js` / `overlayFocus.js` es **otro repositorio** (ABDSharedAssets), no
editable desde aquí.

**Lo que sí funciona** es resolver el ESC en el propio panel, en **fase de
captura** (`handleEscapeRouteReturn` en `WebUI/src/ui/panel.js`): antes que
cualquier listener de burbuja, y solo cuando hay un `routeReturn` vivo, el panel
cierra el destino y hace `stopImmediatePropagation()`. El `onClose` de la matriz
hace su trabajo normal (resalte, consumo único, reapertura del origen) y ya no
existe ningún oyente posterior que pueda cerrar lo que acaba de abrir. Sin retorno
vivo no hace nada: cada cajón conserva su ESC.

### El segundo fallo, encontrado mirando la página y no los tests

Con el knob montado, un **gesto de verdad sobre el dial** dejaba el puente del
arnés desfasado: el store iba a 0.55, el dial marcaba 0.55 y
`baselineControl.value` seguía en **0.4**, el último pintado. El arnés habría
leído un valor que la página ya no muestra. La causa: el `onChange` del knob
empujaba al store pero no pasaba por `applyValue` (la única función que actualiza
memoria, dial y readout). Los tests unitarios no lo cazaban porque en jsdom nadie
arrastra un dial, así que la regresión vive en el **smoke E2E**
(`e2e/localMode.spec.js`): arrastre real de ratón, y se comparan las tres lecturas
—dial, readout y puente— más el `neuronik:params` que cruzó la frontera.

### Un pin que llevaba años midiendo lo que no era

`NEURONiK_WebUiSelftestContract` falló al quitar el comentario de `buildBaselineControl`:
`parameterCellAttribute` pineaba `data-parameter-id` en `panel.js`, y lo único
que había encontrado ahí era **un comentario** — el atributo lo escribe
`dataset.parameterId`. El check llevaba tiempo en verde sin demostrar que la
página escribiera el atributo. Ahora `pageForm` declara la forma real, con el
motivo escrito al lado.

### Verificado

- vitest **421/421** (26 ficheros).
- E2E `localMode.spec.js` **11/11** (uno nuevo: el del knob y su puente).
- Referencias visuales regeneradas y **12/12** en segunda pasada: cambian la de
  `globalFull`, `lfo` y `modMatrix` (las bandas se reordenaron) y las dos de
  página entera.
- `NEURONiK_WebUiSelftestContract` y `NEURONiK_NativePanelParity`: OK.
- Plugin `NEURONiK` compila (los avisos C4100 de `BridgeSelftest.h` son previos).
- En Chromium de verdad: GLOBAL & MASTER a la derecha del LFO y en la misma
  banda (y=123 las dos), knob de 46×46 con `aria-valuenow`/`aria-valuetext`,
  **cero** `<input>` dentro del ancla, solo quedan 2 `input[type=range]` en el
  documento (las dos ruedas), y el ESC del retorno de ruta reabre ENVOLVENTES.

**Controles negativos** (mutar el código y ver que algo cae, no suponerlo):

| Mutación | Lo que cae |
|---|---|
| quitar `stopImmediatePropagation()` de la guarda ESC | los 2 tests del retorno de ruta (`panel.test.js`) |
| quitar `data-baseline-control` | 6 tests (5 de `panel.test.js` + el de `keyboard.test.js`) |
| `dataset.parameterId` → `dataset.parameterID` | `NEURONiK_WebUiSelftestContract` |
| el `onChange` del knob sin `applyValue` | el smoke E2E del master |

Fallos ajenos a este trabajo, sin tocar: `NEURONiK_ReferencedFiles` falla por
`Tests/FxSlotsTest.cpp` **sin versionar** (otra sesión), y `NEURONiK_ModulationContractTest`
sigue "Not Run" porque su exe solo existe en Debug.

> Canon: el ancla del control base del arnés es `[data-baseline-control]`, **no**
> "el primer `input[type=range]`" — ese selector significaba "el master" solo por
> el orden de montaje, y con el knob (sin `<input>`) significaba "la rueda de
> modulación". Los anclajes del contrato se nombran, no se deduplican por
> posición.

## Preparacion para el rack de efectos: que hay, que falta y por que (2026-09-29)

Trabajo de reconocimiento mas la FUNDACION (que ya esta, verificada y en verde).
El resto queda escrito aqui para no perderlo.

### Que se ha encontrado

El sistema de efectos nuevo son TRES capas, y conviene no confundirlas:

| Capa | Donde | Que es |
|---|---|---|
| Vocabulario | `ABDSharedAssets/contracts/fx-effects.json` | 57 ids (0 bypass, 1..56) con nombre, `family`, `engine`, `params`. `generatedFrom`: `ABDEep/Source/DSP/FX/FXSlot_Factory.cpp` |
| Aspecto | `components/fxTheme.js` + `styles/components/fx.css` | 11 temas por FAMILIA (no 57 por efecto) y el chasis `.fx-module[data-fx-theme=...]` con cinco tokens |
| Piezas | `effectLEDButton.js`, `tapeEchoVisual.js` | Boton LED de efecto y visual de eco |

ABDEep lo monta con cuatro slots, cada uno con un selector de tipo
(`.fx-type-select[data-slot=N]`), `fx{N}_gain` y `fx{N}_param1..12` en el APVTS, y
un area que repinta los mandos del efecto elegido leyendo los nombres del
contrato.

### El motor YA esta en NEURONiK. Lo que falta es la pagina.

`Source/DSP/FxSlots.h` usa `FxEngine` + `fxDefaultCatalogue()` con 4 slots en
serie (`kFxNumSlots = 4`). El oyente es la ficha EFECTOS: 12 mandos planos
(`fxSaturation`, `fxDelay*`, `fxChorus*`, `fxReverb*`) sobre una rejilla 6x2. Es
la unica parte del tema que va atrasada.

### Los dos datos que mandan sobre el diseno

**1. La columna `params` del contrato compartido NO es trasladable.** Cuenta los
mandos que acepta la implementacion de ABDEep, que no es la de este motor:

| Efecto | params del contrato | params de NUESTRO motor |
|---|---|---|
| coro | 11 | 2 |
| delay | 12 | 2 |
| reverberacion | 12 | 4 |
| saturacion | 12 | 1 |
| schroeder | 5 | 4 |
| BBD | 4 | 4 |

Asi que la pagina **no puede tomar el numero de mandos del JSON**: tiene que
tomarlo del motor. Por eso la fundacion de abajo exporta el catalogo en vez de
copiar el contrato. Es el mismo descuido que el contrato ya documenta dos veces
sobre los NOMBRES, aplicado ahora a los mandos.

**2. Solo TRES de los seis motores tienen fila en el contrato compartido.** Los
otros 48 de ABDEep son codigo JUCE y no pueden entrar en `DspEffects` sin que el
modulo deje de ser JUCE-free, que es lo que sostiene la paridad nativa <-> WASM
de este producto. Mapa:

| Motor | id | Nombre en el contrato | Familia | Estado |
|---|---|---|---|---|
| chorus | 10 | Stereo Chorus | chorus | alineado |
| delay | 13 | Delay | delay | alineado |
| bbd | 36 | BBD Chorus | chorus | alineado |
| reverb (FreeVerb) | 1 | Hall | reverb | **reserva** |
| saturation | 50 | Oversampled Dist | distortion | **reserva** |
| schroeder | 22 | Deep Verb | reverb | **reserva** |

Una reserva NO es una equivalencia: el id esta ocupado por una reserva declarada
en `Source/DSP/FxCatalogue.h` y no por una fila del contrato. Por eso el flag
`aligned` viaja a la pagina.

### Lo que YA esta hecho y verificado

La fundacion, que es lo que hace barato todo lo demas:

- **`Source/DSP/FxCatalogue.h`** — la tabla que une el motor con el vocabulario
  compartido. Un solo sitio para tocar cuando entren motores nuevos.
- **`Source/DSP/FxCatalogExport.h/.cpp` + `Tests/FxCatalogExportTool.cpp`** — el
  generador `NEURONiK_FxExport`, que escribe
  `WebUI/generated/fx-catalog.generated.{json,js}`. Mismo patron que
  `NEURONiK_ParameterExport` y en el mismo paso de `build.bat`.
- **`Tests/FxCatalogueTest.cpp`** — fija el emparejamiento motor a motor. No
  compara la tabla entera contra si misma porque las dos tablas se unen por
  indice: una permutacion pasaria cualquier comparacion de recuento y dejaria un
  modulo con el tema de un efecto y los knobs de otro.
- **`Tests/fxCatalogContractTest.mjs`** — la paridad entre repositorios, que es lo
  que ningun test de un solo repositorio puede ver. Para las filas alineadas
  exige que id, nombre y familia coincidan con `fx-effects.json`; para las
  reservas exige que el id este ocupado de verdad PERO que se diga otra cosa. Sin
  el contrato a mano se salta con el motivo, nunca en verde.

El bus sale del catalogo, no del modulo: **4 mandos por hueco**, no los
`kFxMaxParams` (12) que son el maximo que el motor ACEPTA. Un bus de 12 para un
catalogo que como mucho pide 4 serian ocho parametros muertos por hueco en cada
preset y en el contrato de la pagina.

**Verificado**: `NEURONiK_FxCatalogueTest` y `NEURONiK_FxSlotsTest` en verde,
`NEURONiK_FxCatalogContract` en verde, plugin compila, vitest 421/421 sin cambios.
**Controles negativos**: permutar las dos primeras filas de identidades -> el
test C++ cae con 4 fallos; renumerar el id 10 como si lo hubiera cambiado ABDEep ->
la paridad cae con 2.

### Lo que falta, en orden

1. **El bus en el APVTS.** Por hueco: 1 tipo + 1 ganancia + 1 mezcla + 4 mandos.
   Son 6 x 4 = 24 en vez de los 12 de hoy, y el contrato pasaria de 73 a 85.
   `setType` CREA y DESTRUYE, asi que el tipo se cambia en el hilo de mensajes
   (`FxSlots.h` lo dice ya: "cuando el panel deje elegir el efecto de cada hueco,
   ese `setType` vivira en el hilo de mensajes"). Hoy los tipos se fijan en
   `prepare` y no se vuelven a tocar.
2. **La migracion de presets**, que es donde estan los dos que no salen solos:
   - `fxReverbMix` NO es el `mix` del hueco (el `mix` se queda en 0.5 y la mezcla
     va en el `levels` de la fila), asi que no hay correspondencia directa;
   - `fxDelaySync` y `fxDelayDivision` no son mandos del motor de retardo, que
     solo tiene `time` y `feedback`: o se pierden o el motor crece.
   Decidir cual de las dos cosas es lo que hay que escribir.
3. **La ficha de la pagina**: un cajon (al estilo ENVOLVENTES) con un modulo por
   hueco, `.fx-module[data-fx-theme=...]` con la familia que da el catalogo, el
   selector de tipo y los mandos del efecto elegido. Los `FxParamSpec.name` del
   motor son tecnicos en minuscula (`rate`, `depth`, `wear`): o se pone una tabla
   de etiquetas, o se pinta en versalitas. Decision de producto, no tecnica.
4. **Lo que hay que pedir al otro repositorio**: tres filas en
   `fx-effects.json` para la reverberacion de FreeVerb, la saturacion de una banda
   y el Schroeder. Hasta entonces son reservas, y el paridad test avisa.

> Canon: el `id` de un efecto es un VOCABULARIO COMPARTIDO y el numero de mandos
> es del PRODUCTO. El contrato compartido fija el primero; el motor fija el
> segundo. Confundirlos es el descuido que el propio contrato ya documenta dos
> veces sobre los nombres, y que se repetiria con los mandos si alguien copiara
> la columna `params`.

> Canon: el ancho del bus de mandos lo decide el catalogo del producto (el mayor
> que pide), no el maximo que el motor acepta. Son cosas distintas, y confundirlas
> mete parametros muertos en cada preset.

## El hueco 1 del rack pasa a bus por hueco en el APVTS (2026-09-29)

El primer hueco migrado de los cuatro. El motor, el catalogo y el exportador ya
estaban (seccion anterior); esto es el APVTS.

### Lo que cambia en el contrato

`fxSaturation` (un mando que hacia DOS cosas) se sustituye por el **bus del
hueco 1**:

| id | que es | rango |
|---|---|---|
| `fx1Type` | el efecto del hueco (choice, del catalogo) | 0 = bypass, 1..6 |
| `fx1Gain` | salida del hueco | 0..2, def 1.0 |
| `fx1Mix` | mezcla mojado/seco | 0..1, def 0.0 |
| `fx1Param1..4` | los mandos del efecto | 0..1 normalizado |

73 - 1 + 7 = **79 parametros**. Los huecos 2, 3 y 4 los manejan TODAVIA los
mandos planos de antes (`fxChorus*`, `fxDelay*`, `fxReverb*`): la migracion va
hueco a hueco.

### Las tres decisiones que no eran obvias

**1. Los mandos del bus van NORMALIZADOS 0..1, no en unidades fisicas.** El rango
depende de que efecto este puesto, y el hueco lo cambia el usuario en caliente;
un parametro del APVTS tiene un rango fijo para siempre (lo guarda un preset, lo
automatiza el host). Publicar "Drive 1..8" seria mentir para los otros cinco
efectos del catalogo. El hueco habla normalizado (`FxSlot::setParameter` toma
0..1) y el sesgo lo aplica la fila. La pagina, que si sabe que efecto hay, pone
la etiqueta leyendo el catalogo exportado.

**2. `GlobalParams::saturationAmt` NO se borra, aunque ya no lo lea nadie.** El
struct tiene un ABI publico: `neuronikGlobalParamsLayout` publica el `offsetof`
de cada campo y la pagina escribe el espejo por INDICES
(`WebUI/src/wasm/audioParams.js`, 0..33). Anadir campos al final es seguro; quitar
o reordenar un campo cambia lo que la pagina escribe sin que ningun aviso lo
diga. El bus entra por el final (34..57) y el campo muerto se queda en el sitio
1. Hay un test que lo vigila (`ningun id mapeado pisa el field 1`).

**3. El destino 17 de la matriz no puede ser `&GlobalParams::fx[0].params[0]`,
porque eso no existe en C++**: un puntero a miembro no puede atravesar un array.
Por eso la tabla de modulacion gano un `Kind::globalFxAdd` y un campo
`busParam` en el DESCRIPTOR, codificado como `hueco * kFxBusParams + mando`.

Y un aviso que se lleva puesto: **anadir un campo a `ModRule` rompe la tabla
entera en silencio.** Se inicializa posicionalmente y con elision de llaves, y el
compilador rellena cada sub-agregado `ModRule` HASTA LLENARLO antes de pasar al
siguiente, asi que un miembro nuevo hace que `env` se coma un inicializador mas de
los que le tocaban. Las cincuenta filas existentes se empiezan a leer como otra cosa.
Por eso el campo va en el descriptor, que se rellena por orden.

### Lo que cambia de sonido

El mando `fxSaturation` hacia el drive (`1 + 4*amt`) Y la mezcla (`mix = 1` en
cuanto `amt > 0`, o sea insercion). Ahora son dos mandos. El bypass por `mix = 0`
se conserva entero y sigue devolviendo la seca bit a bit.

**Y la paridad de sonido con la cadena anterior esta COMPROBADA, no supuesta**:
`Tests/FxSlotsTest.cpp` pone el bus en el valor que reproduce el mando viejo
exactamente (el drive normalizado que da un drive fisico de `1 + 4*amt` sobre la
fila, y el mix a 1) y la banda se queda en **1e-5**, que es la del redondeo del
viaje normalizado <-> fisico. Un bus que no pudiera reproducir el sonido viejo
seria una perdida real, y esa es la comprobacion que lo dice. El helper escribe
ADEMAS `p.saturationAmt`, porque la cadena CONGELADA de ese test (una copia del
envoltorio viejo) lo lee: poner solo el bus dejaria a la referencia en silencio y
la comparacion mediria "una de las dos no hace nada".

### En la pagina

La ficha EFECTOS pinta `fx1Mix` y `fx1Param1` en lugar del mando suelto, y su
rejilla paso de **6 a 7 columnas**: con dos mandos donde habia uno la ficha se
queda con 13 controles, y en 6 columnas eso son TRES filas — y el lienzo es de
alto FIJO, asi que una fila de mas no se encoge, se desborda. Con 7 columnas, 13
controles entran en dos filas y el alto de la ficha no cambia.

Los otros cinco ids del bus **no tienen celda todavia** (son del modulo del hueco,
que no esta montado) pero **el store los POSEE**, y eso no es lo mismo: un id sin
dueno lo descarta el store cuando llega del host, asi que sin ellos mover el
selector de efecto en el host no llegaria a la pagina. `screens.test.js` los
comprueba por nombre, para que la lista se note cuando se vacie.

### La deuda que queda anotada

`setType` se llama desde `parameterChanged` (hilo de mensajes), que es donde
puede, pero ese `engine` se puede estar usando en `processBlock` mientras se
toca. **No es una deuda nueva**: el intercambio de motor entero ya lo hace, y con
el puntero completo. El `setType` de un hueco es un caso mas. Arreglarlo de verdad
es un mutex o una suspension de audio que cubra las dos cosas, y es un cambio
aparte.

Tampoco se ha resuelto el **preset**: un preset guardado con `fxSaturation` ya no
tiene ese id, y la migracion de presets (que es trabajo de la proxima fase)
tiene que escribir el bus. Ver la seccion de preparacion del rack.

### Verificado

- ctest 45/49 en verde. Los que no: `PresetMigrationParityTest` y
  `ModulationContractTest` "Not Run" (ejecutables de otra sesion, sin
  versionar, preexistentes), `ReferencedFiles` falla **por los ficheros nuevos
  sin versionar** —que es exactamente lo que ese test pide: un `git add`— y
  `Vst3LoadTest` (tambien de otra sesion).
- `NEURONiK_FxSlotsTest` 21/21 **con la paridad conservada en 1e-5**.
- vitest 423/423. E2E local 11/11. Visuales 12/12 tras regenerar (cambia la
  ficha `fx` y las dos de pagina entera).

> Canon: el `GlobalParams` tiene un ABI publico por indices, asi que un parametro
> nuevo se AÑADE AL FINAL y uno que deja de usarse NO SE BORRA (se queda muerto en
> su sitio y un test vigila que nadie lo escriba). Un hueco del rack publica sus
> mandos NORMALIZADOS, porque su rango depende del efecto que haya puesto y el
> hueco se puede cambiar en caliente.

> Canon: la tabla de destinos de la matriz se inicializa posicionalmente y con
> elision de llaves, asi que **añadir un campo a `ModRule` no es inocuo**. El
> campo nuevo va en `ModDestinationDescriptor`, que se rellena por orden.

---

## Fase 3 del rack FX: los doce ids planos al bus por hueco

Un preset anterior al rack guarda `fxSaturation`, `fxChorusRate`, `fxDelayTime`...
en el APVTS viejo, con el **normalizado de aquel APVTS**. El bus por hueco guarda
el **normalizado de la FILA del efecto**, y no son el mismo numero. Esos dos
normalizados no se pueden copiar uno en otro: hay que viajar de uno a otro
pasando por el valor fisico, que es donde los dos coinciden.

### Que hace

`Source/Serialization/PresetMigrationFx.cpp` define una **tabla de doce filas con
puerta**. Cada fila dice: de que id plano se lee, en que hueco cae, y con que
rango del APVTS viejo se desnormaliza. La fila se aplica **solo si su id destino
existe hoy en el layout**: la puerta es `el id del bus del hueco esta en el
layout del processor`. Con el layout de hoy solo pasa el hueco 1 (saturacion) --
el coro, el retardo y la reverb se siguen manejando con sus mandos planos, y sus
tres ids viejos se quedan **sin tocar** en el arbol, a la espera de su fase.

Por que la puerta y no "migrar los cuatro ya": si la fila 3 se escribiera hoy,
el preset traeria `fx2Param1` con un numero que el hueco 2 todavia no publica, y
el arbol lo guardaria sin que nadie lo oiga. Un preset que se guarda mal en
silencio es peor que un preset que se guarda viejo. Ademas la puerta hace que
bajar un parche con menos huecos no destruya nada: la fila simplemente no se
aplica.

### La conversion, que es donde esta el dinero

`remapKnob` hace **viejo normalizado -> fisico (`fxDenormalise`) -> fisico de la
fila -> normalizado de la fila (`fxNormalise`)**. Los rangos del APVTS viejo y
las filas estan **congelados** en el fichero (`kRangeSaturation`, `kRangeChorusRate`,
...) con un comentario que explica por que no se leen: `fxSaturation` ya no esta en
el layout, asi que leer su rango de ahi daria un rango inventado.

El hueco 1 es el unico con una conversion que merece nombre. El envoltorio viejo
traducia el amount a `drive = 1 + 4 * amount` (rango 1..8, sesgo 0.5), y ese
`1 + 4 *` esta **congelado aqui tambien**: si el drive fisico de la fila cambiara,
el `1 + 4 *` tendria que cambiar con el, y copiar el amount no seria una
migracion sino un cambio de sonido disfrazado de guardado.

`PresetManager.cpp` llama a `migrateFlatFxToSlotBus` **ANTES** de
`migratePresetState`, y ese orden **es el contrato**, no una preferencia: el
limpiado borra los ids que ya no estan en el layout, asi que al reves se leeria
`fxSaturation` de un arbol donde ya no existe y la migracion no tendria nada que
convertir. Se lee de la fila `id` de cada hijo, no de los atomicos del APVTS: en
el momento de cargar un preset, el APVTS todavia no tiene sus valores.

### Idempotencia

`writeParam` devuelve `true` solo si **anade**, y `alreadyOnBus` salta el hueco
cuyo `fxNType` ya esta en el arbol. Migrar dos veces no escribe nada, y sobre todo
**no pisa el efecto que el usuario haya cambiado desde el host**: migrar un preset
que ya esta en el bus tiene que ser un no-op, no una sobrescritura.

### Verificado

- `NEURONiK_PresetMigrationParityTest` **35 pasan, 2 fallan** con el sabotaje
  puesto y **37 de 37** sin el. Las dos que se ponen rojas son exactamente las de
  la conversion, y ninguna otra: el sabotaje copia el `amount` viejo donde
  deberia ir el normalizado del drive, que es justo el fallo que esta clase de
  migracion comete en silencio.
- ctest **47/49**. Los dos rojos: `ReferencedFiles`, que pide `git add` de los
  ficheros nuevos (es lo que ese test hace), y `ModulationContractTest`.
- `ModulationContractTest` **no es de esta fase**: la fase 2 cambio el destino 17
  de la matriz de `fxSaturation` a `fx1Param1` en C++
  (`ABD_CHECK_MOD_DEST_ID (17, fx1Param1)`) y el contrato compartido
  `../ABDSharedAssets/contracts/neuronik_modulation_matrix.json` sigue diciendo
  `{"label": "Saturation", "parameterId": "fxSaturation"}`. El JSON es el que va
  atrasado y es de OTRO repositorio, asi que no se ha tocado. Se arregla cambiando
  ese un campo por `fx1Param1`.
- vitest 423/423 y `pnpm build` sin cambios (esta migracion no toca la pagina).

> Canon: una fila de migracion solo se aplica si su id destino esta HOY en el
> layout. Escribir un id que el hueco todavia no publica no suena a error, suena
> a un preset que se guarda mal y no se nota hasta que algo lo lee.

> Canon: migrar por unidades FISICAS, no copiando normalizados. El APVTS viejo y
> la fila del efecto tienen rangos y sesgos distintos, asi que sus normalizados
> son numeros distintos por construccion. Y el viaje de ahi se congela junto al
> rango: si el rango se congela pero la conversion no, la fila parece correcta y
> suena mal.

---

## El cajon de EFECTOS: un `.fx-module` por hueco, con el tema por familia

La ficha de EFECTOS pasa a ser de cajon. Sus dos celdas del bus (la mezcla y el
drive del hueco 1) bajan a un modulo, y el cajon lleva **un modulo por hueco del
rack**, con el tema de la FAMILIA del efecto que hay puesto.

### La puerta, y es la misma que la de la migracion

Un modulo se llena si el hueco tiene bus, y la prueba es que el store posea
`fxNType`. Hoy solo el hueco 1 lo tiene, asi que los otros tres se pintan con su
chasis, su LED y su titulo, y en el cuerpo una frase que dice que su bus no esta
publicado. **No se inventan ids para llenarlos, ni se les pone el efecto de la
cadena por defecto**: ese reparto vive en `fxDefaultTypeForSlot`, en C++, y
escribirlo tambien en la pagina seria la segunda copia de una tabla que ya
existe.

La ventaja de que la puerta sea "el store posee el id" y no una lista escrita a
mano es que el hueco 2 se llenara solo cuando publique su bus, sin que nadie
edite la pagina. Un id escrito aqui que el host no declara es un id que el store
ignora al escribirle y un modulo con mandos que no suenan.

### El tema, y por que la pagina NO importa `fx-effects.json`

La hoja compartida (`@abdsynths/shared/styles/components/fx.css`) ya define el
chasis `.fx-module` y los **once temas por familia**; la pagina solo pone el
atributo `data-fx-theme` con la familia que le da el catalogo exportado
(`generated/fx-catalog.generated.js`, que el exportador llena leyendo el
contrato compartido).

Y NO se importa `contracts/fx-effects.json` en la pagina, a proposito: ese
fichero vive en el repositorio hermano y la pagina se embebe en el plugin como
binario, asi que un import ahi haria fallar el build en un clon sin el hermano.
El camino es el que ya funciona: **build-time, no runtime**.

`fx-modules` declara `parameterIds: []` a proposito (una vista no reparte
celdas) y recibe los view-models de la FICHA por `options.controls`.

### Lo que se quedo en la ficha del lienzo, y por que

Los ONCE mandos planos de los huecos 2, 3 y 4, como `frontal`. No por
leftover: son los de los huecos que aun no tienen bus. La rejilla sigue a 7
columnas y el alto de la ficha no cambia ni un pixel (11 celdas en 7 columnas
son dos filas, como eran 13).

Y el mix del hueco 1 **no** se quedo en la ficha: una celda de mezcla sin el
selector al lado es la mezcla de algo que el usuario no puede ver. Partida en
dos superficies, la ficha y el cajon cuentan cosas distintas del mismo hueco.

### Los mandos, y el nombre y las unidades

El bus publica cuatro posiciones para todos los efectos porque el hueco no sabe
cuantos mandos va a necesitar el que le pongas; quien lo sabe es la FILA. Por
eso los knobs que el efecto no declara **se esconden** (la saturacion declara
uno, el Schroeder cuatro), y por eso el nombre del mando lo pone la fila
(`drive`, `decay`) y no el id del host (`FX 1 Param 1`).

El readout va en las **unidades del efecto**: el descriptor del host declara el
bus en 0..1, asi que la celda escribe "38%", y ese 0.38 es el decay de un
Schroeder, que va de 0.10 a 0.98. La conversion usa las MISMAS funciones que el
resto de la pagina (`fromNormalized`/`formatValue`), no una cuenta propia: dos
formulas para el mismo sesgo son dos numeros que acaban discrepando.

### Un fallo que solo se ve preguntando DONDE

Los tests de inventario contaban 77 celdas y cuadraban, y aun asi la pagina
tenia los mandos del bus **en la rejilla en vez de en su modulo**: la vista se
fabricaba con cero controles, ningun modulo se pintaba con bus, `claimBlocks()`
devolvia vacio y las celdas se caian al cuerpo de la ficha por el `?? body` del
panel. Se veia entero y con todos los mandos, y no habia ni un rojo.

`tests/fxModules.test.js` tiene por eso un bloque que pregunta **donde cae cada
celda**, no solo que exista. Es el que habria parado esto.

### Verificado

- vitest **448/448** (25 nuevos en `fxModules.test.js`: la puerta, el tema por
  familia, los knobs que el efecto no declara, y donde caen las celdas).
- **Control negativo**: tres sabotajes, los tres en rojo y solo en las
  aserciones de su caso. Poner el tema por efecto en vez de por familia rompe
  tres (coro y coro BBD dejan de verse igual); no esconder los knobs que el
  efecto no usa rompe dos; ignorar la puerta rompe la que dice que el hueco 1
  es el unico con bus.
- Regresion visual: se anadio una foto **por cajon** (no habia ninguna, y el
  cajon de efectos es superficie nueva). Las seis se generan solas desde el
  contrato.
- ctest **48/49**. El unico rojo sigue siendo `ModulationContractTest`, que es
  de la fase 2 y del repositorio hermano.
- `NEURONiK_WebUiNeedleProbeE2e` appeared rojo en una pasada completa y verde
  en las siguientes. **No es de este cambio**: falla en
  `page.waitForFunction(__probeReady)` —la pagina de prueba no llega a estar
  lista en 60 s— y pasa solo (`ctest -R WebUiNeedleProbeE2e` y
  `-R WebUiVisualRegression`, ambos verdes). Es el servidor de dev de la sonda
  tardando en arrancar cuando viene detras de otra tanda de navegador.

> Canon: una celda se cuenta como cableada cuando esta DONDLE tiene que estar, no
> solo cuando existe. El `?? body` del panel convierte "la vista no reclamo este
> id" en "la celda aparece en otro sitio", y ahi no hay ningun error: la pagina
> se ve entera.

> Canon: la puerta de una vista se deriva de lo que el store POSEE, no de una
> lista escrita a mano. La lista hay que mantenerla; la puerta se sola con que
> aparezca el id.
## ctest 51/51 y el bus del hueco ya viaja al navegador (2026-09-29)

### La medida

```
antes  (2026-09-29 07:05)   40/47 = 85%    35674 s   (9 h 54 min)
ahora (2026-09-29 13:4x)   51/51 = 100%    121.74 s   (ctest -C Release -j 4)
```

Los 9,9 h no eran lentitud de los tests: eran **un test que tardaba 9 h 17 min en
morir** (`NEURONiK_TransposableOffsetsTest`, 33396 s), y el cuello de botella era
una aserción de JUCE del final de la cuenta, no el cálculo. Con eso fuera, los
48 tests que no son E2E de navegador tardan menos de 3 s cada uno; los tres
Playwright (45,96 + 40,73 + 35,03 s) son la cola entera.

### Los 7 fallos del 85%, atribuidos

**Grupo A — cuatro abortos, una sola causa.** `NEURONiK_DSPReferenceTest`,
`NEURONiK_LayerEngineTest`, `NEURONiK_TransposableOffsetsTest` y
`NEURONiK_NeurotikBowTest` morían los cuatro con la misma línea:

```
Assertion failed: isPositiveAndBelow (sampleIndex, size),
  file D:\desarrollos\ABDSynths\ABDSharedCode\DspCore/DspCore.h, line 1846
```

Es un `copyToRawArray` leyendo fuera del buffer: **memoria que el motor creía
puesta a cero y no lo estaba**. Los tres primeros llegaban con sus `[ok]` en
verde y reventaban al final, que es justo lo que convertía un test de segundos
en otro de horas. Lo arregló `2517603` (`alignas(16)` en los tres arrays de pila
de `SIMDWrapper.h` + `{}` en `extraAmps`/`extraOffsets` de `SpectralModel.h`).

**Grupo B — un test con checks rojos, la misma causa por el otro lado.**
`NEURONiK_LayerViewTest`: seis `[FAIL]` seguidos (parciales por capa, índices
vacíos en `-1`, recuento de la leyenda, `frameCountOf`, alturas
global-normalizadas) mientras "la vista declara las 2 capas" pasaba. No era la
vista: es que `SpectralModel.h` **rellena esa vista** con `extraAmps` y
`extraOffsets` sin valor inicial, así que la asignación por parcial salía basura.
Mismo commit, `2517603`.

**Grupo C — un contrato que medía algo que ya no existía.**
`NEURONiK_WebUiSelftestContract`:
`parameterCellAttribute ("data-parameter-id") sigue en su sitio de la pagina`.
El literal ya solo vivía en un comentario del propio panel; la página escribe
`dataset.parameterId`. Un test que se ponía verde por un comentario borrado, que
es peor que rojo. `99c9905` cambió el `pageForm` del contrato y dejó escrito por
qué.

**Grupo D — deriva visual real, no un defecto.** `NEURONiK_WebUiVisualRegression`:
5 de 12 capturas distintas (lienzo entero, ficha lfo, globalFull, modMatrix,
tema claro); 19281 píxeles, ratio 0,02, en la del lienzo entero. Consecuencia del
trabajo de UI que aterrizó después del ctest del 85% (temas de familia del rack,
cajón de EFECTOS, fondos). Las instantáneas se regeneraron al entrar `99c9905` y
`9fe80fb`.

### El recuento también se movió: 47 → 51

- **+2 míos**: `NEURONiK_WasmLayoutOrderTest` (`020f18a`, el orden del bus dentro
  del layout) y `NEURONiK_WorkletMirrorLayout` (`f52bc9b`, el espejo contra el
  `.wasm` que se sirve).
- **+3 del rack de huecos** (`99c9905`): `FxCatalogueTest`, `FxSlotsTest`,
  `FxCatalogContract`.
- **−1 retirado** (`99c9905`): `NEURONiK_DspEffectsParityTest`. No se perdió nada
  por recorte; la paridad de efectos la cubren ahora los tres de arriba.

### Lo del `.wasm`, que era el agujero de verdad

`neuronikModMatrixLayout` ya publicaba el tramo entero (matriz **y** bus) desde
`020f18a`. Lo que no lo publicaba era **el binario**: el `.wasm` versionado en
`WebUI/public/worklet` era del 2026-09-28, anterior a ese commit. Medido sobre
él:

```
gpSize 144 | base fields 22 | mod fields 12   ->  34 campos
```

La página escribe hasta el 39 (`fx1Param1..fx1Mix`), así que **los seis mandos
del hueco 1 se perdían en silencio**: el worklet recibía `[34, 0.7]`, no encontraba
offset, y `writeGpField` hacía `return`. El knob se movía en la página, el motor
no oía nada y no saltaba ningún error. Recompilado con `build_wasm.bat` (paridad
WASM↔nativo bit-exacta en los 9 casos de la matriz) el motor publica **58** y el
bus entra de verdad.

Encima, el consumidor tenía un error de numeración: el worklet concatenaba la
tabla de `neuronikGlobalParamsLayout` con la de `neuronikModMatrixLayout` porque
aquella solo daba los 22 escalares. Ahora la segunda **es la cola de la primera
renumerada desde cero**, no una continuación: concatenar duplicaba matriz y bus
(`paramsFieldCount` decía 94 con un layout de 58) y el índice 58 habría apuntado
a la matriz otra vez. Se traduce con la tabla única.

Ahora el fallo tampoco puede volver a ser invisible: el worklet cuenta los
campos del espejo que su layout no publica y los avisa por el port
(`neuronik:layout`), una vez por push; la página los traduce a **ids** con
`gpIdsBeyondFieldCount` y los deja en `audioEngineState.unreachableFieldIds`.

### Lo que queda pendiente

1. **Regla operativa del `.wasm`**: `build_wasm.bat` compila, sincroniza
   `public/worklet` y al final **falla** si `WebUI/dist/worklet` no cuadra con
   `build-wasm`. Hay que correr `pnpm build` en `WebUI/` detrás de cada
   `build_wasm.bat`, o el `NEURONiK_WorkletSync` queda rojo por el `dist` viejo.
   Hoy está cuadrado (ambos lados en `5a432b902b96`).
2. **`Tests/localMorphZRouteTest.mjs` y `Tests/neuronik_wasm_parity.mjs` siguen
   concatenando `base.concat(mod)`**, la misma duplicación que se quitó del
   worklet. Pasan porque la página solo escribe 0..39, pero llevan una tabla con
   la matriz y el bus repetidos y deberían usar la tabla única.
3. **Solo el hueco 1 está migrado en el contrato**: `fx2Param1`, `fx3…` y
   `fx4…` no existen en `generated/parameters.generated.js`. El motor publica los
   cuatro buses y la página no escribe tres, que es lo correcto, pero significa
   que el layout ya tiene sitio para ellos y el contrato todavía no.
4. **`fx1Type` no viaja por el espejo**: el tipo lo decide el hilo de mensajes
   (crea y destruye la instancia del efecto). Es el único mando del bus fuera del
   espejo, a propósito, pero conviene recordarlo antes de tocar el mapa.
5. **`unreachableFieldIds` es un `console.warn` y nada más.** Con el binario al
   día la lista va vacía, pero si alguien vuelve a subir un `.wasm` viejo, el
   aviso cae en una consola que nadie mira. Debería verse en la página.

### Verificado

- vitest **463/463** en 28 ficheros; ctest **51/51**.
- `NEURONiK_WorkletMirrorLayout` comprobado con **control negativo**: con el
  `.wasm` viejo (34 campos) falla nombrando los 18 campos que se perdían (22..39).
- `NEURONiK_WebUiNeedleProbeE2e`, el flake conocido, pasó en 35 s sin tocar nada.

> Canon: un artefacto binario que se sirve al navegador es parte del contrato
> aunque no aparezca en el `git diff` de quien lo construye ni en el de quien
> toca la página. El `.wasm` versionado iba dos commits por detrás del puente que
> lo produce, y la divergencia no daba ningún error: se perdían seis mandos **en
> silencio**. Un consumidor que no encuentra un campo tiene que **decirlo**, no
> devolver.

> Canon: cuando dos exports publican el mismo tramo, uno es una **renumeración**
> del otro, no una continuación. Concatenarlos duplica el tramo y hace que un
> índice pasado de la raya escriba en el sitio equivocado sin que nada se entere.

> Canon: el orden de un layout publicado tiene tres guardas que se comprueban unas
> a otras — el `offsetof` en C++, el mapa de la página y el binario. Cada una
> puede quedarse atrás por su cuenta; las tres juntas no.

> Canon: cuando una suite pasa de horas a segundos, el tiempo **era** el fallo.
> Un assert al final de una cuenta de 9 h no es lentitud, es un cuelgue con pasos.
