# NEURONiK Web UI Pilot

## Objetivo

Validar si Next.js es una base razonable para una interfaz web embebida en WebView2 sin migrar todavía la interfaz JUCE completa ni conectar todo el DSP.

Este experimento debe ser pequeño, reversible y evaluable.

## Alcance del piloto

El piloto tendrá únicamente:

- una pantalla/panel de parámetros;
- `masterLevel`;
- `morphX`;
- `morphY`;
- `engineType` como selector;
- estado local simulado;
- indicador de cambios de parámetros;
- exportación estática de Next.js;
- carga en navegador y WebView2.

No incluirá inicialmente:

- Bank Manager;
- MIDI real;
- WASM;
- AudioWorklet;
- presets completos;
- bridge JUCE real;
- migración de `ABDSharedCode` completa.

## Arquitectura del experimento

```text
Next.js export estático
        ↓
Panel React del piloto
        ↓
modelo de parámetros simulado
        ↓
WebView2 de prueba
```

El modelo simulado debe conservar los IDs reales definidos en:

```text
Source/State/ParameterDefinitions.h
DSP_PARAMETERS.md
```

No se deben inventar IDs alternativos.

## Criterios de éxito

El piloto continúa si cumple todos estos puntos:

1. `next build` produce una exportación estática.
2. La página funciona sin servidor Node en producción.
3. El panel se carga en navegador con rutas relativas.
4. El mismo contenido se carga en WebView2.
5. Los cambios de parámetros actualizan el estado sin listeners duplicados.
6. El bundle y el arranque son razonables para una interfaz embebida.
7. La estructura permite sustituir el mock por un adaptador JUCE sin reescribir el panel.

## Criterios para abandonar Next.js

Se volverá a React/Vite si el piloto muestra que:

- la exportación estática complica innecesariamente los recursos;
- WebView2 necesita hacks específicos para cargar la aplicación;
- el ciclo de vida de React no encaja con el bridge;
- el bundle o el arranque son excesivos;
- el adaptador JUCE queda más complejo que el panel que resuelve;
- las ventajas de Next.js no aparecen en esta escala de aplicación.

Abandonar Next.js en este punto no invalida los componentes React ni el contrato de parámetros.

## Estado actual

- [x] Aplicación Next.js aislada creada en `WebPilot/`.
- [x] Configurado `output: 'export'`.
- [x] Modelo simulado de cuatro parámetros implementado.
- [x] Panel mínimo implementado.
- [x] `next build` y exportación estática verificados.
- [x] Crear un host JUCE/WebView2 mínimo separado.
- [x] Ejecutar el host y verificar visualmente la carga de `out/`.
- [ ] Registrar la decisión final en `ROADMAP.md` y `HANDOFF.md`.

## Resultado de la verificación manual (2026-09-16)

El host `NEURONiK Web Pilot` arranca en WebView2 y muestra el panel Next.js completo:

- `Master Level`;
- `Morph X`;
- `Morph Y`;
- `Engine Type`;
- indicador `STATIC EXPORT`;
- contador de cambios y JSON de estado;

y los controles responden (los sliders y el selector actualizan el estado del panel).

Esto valida la cadena completa sin ningún hack específico de WebView2:

```text
next build (output: 'export')
    → WebPilot/out/
    → ResourceProvider JUCE (backend WebView2)
    → https://juce.backend/
    → panel React servido y funcional
```

Conclusión: la exportación estática de Next.js **es compatible con WebView2** y las rutas absolutas que genera (`/_next/static/...`) se resuelven contra el origen `https://juce.backend/`, que es precisamente el origen que intercepta `WebBrowserComponent`.

## Contrato real de parámetros (sustituye al mock)

El panel ya no usa una lista escrita a mano. Lee los descriptores generados desde el APVTS:

```text
lib/parameters.js                        (adaptador: lookup, escalado, formato, validación)
generated/parameters.generated.js        (70 parámetros reales)
generated/parameters.generated.d.ts      (tipos para TypeScript)
generated/parameters.generated.json      (instantánea de datos)
```

El estado sigue siendo local: todavía no hay bridge JUCE. Lo que ya es real son los IDs, rangos,
intervalos, `skew`, defaults y listas de opciones.

Estos tres ficheros **se versionan a propósito** (no están en `.gitignore`): el test anti-drift los
compara contra una exportación nueva, así que tenerlos en el repositorio es lo que da valor a esa
comprobación en cualquier clone. Detalle en `HANDOFF.md`, sección del contrato de parámetros.

Regeneración e integridad:

```bash
cd ABDNeural
./build-reference/Release/NEURONiK_ParameterExport.exe WebPilot/generated
ctest --test-dir build-reference -C Release -R NEURONiK_ParameterDescriptorTest --output-on-failure
```

El contrato incluye además el estado real de cada parámetro (`dspStatus`, `engines`, `dspNote`):

```text
70 parámetros · 65 conectados al DSP · 4 solo UI · 1 sin ruta · 1 fuera del layout
```

El panel muestra ese resumen y marca con un asterisco cualquier control que no llegue al motor.

## Medición del host

Peso de `WebPilot/out`: 23 ficheros, 628 KB en crudo, 189 KB gzip, de los cuales unos 572 KB de JS
en crudo (172 KB gzip) son casi todo runtime de framework.

Arranque real en WebView2 (Release, 2026-09-16, medición repetida):

```text
                                   panel en DOM      react ready
5 ejecuciones en caliente            886 - 1166 ms    965 - 1293 ms
  backend WebView2 construido        341 - 515 ms
  recursos                           8 peticiones, 476 KB, 0-1 miss (favicon.ico)

control frio/caliente                1112 ms          1198 ms     (copia recien escrita del exe)
original en caliente                 1004 ms          1079 ms

outlier medido al compilar           2308 ms          2438 ms     (una sola vez)
```

Lectura de los datos:

- **No hay regresión sostenida.** Las cinco ejecuciones en caliente son incluso mejores que el
  rango medido antes (1232-1518 ms), y el bundle no ha crecido (8 peticiones, 476 KB).
- El outlier de 2308 ms apareció **una vez**, en el primer arranque tras recompilar.
- Intenté reproducirlo escribiendo una copia nueva del exe (fichero frío, misma carpeta, mismos
  recursos): 1112 ms, solo ~110 ms más que en caliente. Eso **descarta la caché de ficheros del exe**
  como causa.
- Queda por tanto como causa más probable la creación del entorno/perfil de WebView2 en esa
  primera sesión. No se ha comprobado aislando el perfil porque vive fuera del repositorio
  (`%LOCALAPPDATA%`); una medición limpia requeriría reiniciar o vaciar la standby list.

Borrar el perfil de WebView2 no cambia el resultado de forma apreciable (observación previa).
Reproducible con:

```bash
cd ABDNeural
"./build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe" --auto-quit
# resultado en pilot-startup.log, junto al ejecutable
```

Nota sobre el aviso de Turbopack en el build de la WebUI:

```text
Warning: Next.js ignored pnpm-workspace.yaml in D:\desarrollos\ABDSynths because it is outside
the current Git repository (D:\desarrollos\ABDSynths\ABDNeural).
```

Es esperado y **no hay que "arreglarlo"**: el piloto está instalado a propósito fuera del workspace
(`pnpm install --ignore-workspace`), así que Next encuentra el `pnpm-workspace.yaml` del monorepo,
comprueba que está fuera de este repositorio y lo ignora. Apuntar `turbopack.root` a ese directorio
haría justo lo contrario de lo que buscamos (arrastrar el workspace a un piloto que queremos
aislado). Se deja como está a propósito; revisitarlo solo si algún día el piloto pasa a ser parte
del workspace.

## Orden de implementación

1. ~~Crear una aplicación Next.js aislada en `WebPilot/`.~~
2. ~~Configurar `output: 'export'`.~~
3. ~~Implementar el modelo simulado de cuatro parámetros.~~
4. ~~Crear el panel mínimo.~~
5. ~~Verificar `next build` y exportación estática.~~
6. ~~Ejecutar `NEURONiK_WebPilotHost` y verificar visualmente la carga.~~
7. ~~Sustituir el mock por el adaptador de parámetros real.~~
8. ~~Medir el arranque real dentro de WebView2 (instrumentación del host).~~
9. Repetir la medición con el host en caliente y actualizar la tabla de arriba.
10. Conectar el bridge de parámetros real (JUCE -> WebView2) y comparar contra la UI JUCE.
11. Registrar la decisión final sobre Next.js en `ROADMAP.md` y `HANDOFF.md`.

## Regla de alcance

No añadir nuevas pantallas, presets ni conexión de audio hasta cerrar formalmente este punto de decisión.
