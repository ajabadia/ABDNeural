# Propuesta: tres filas nuevas en el contrato compartido de efectos

Fichero al que se propone tocar: `../ABDSharedAssets/contracts/fx-effects.json`.
Documento vivo en: `ABDNeural/DOCS/FX_EFFECTS_CONTRACT_PROPOSAL.md` (este repo es donde
se razona; el fichero que se edita es del hermano).

Motivo de la propuesta: el catálogo de NEURONiK tiene **tres motores sin fila propia**
en el contrato compartido —la reverb de FreeVerb, la saturación y el reverberador de
Schroeder— y los lleva como **reservas** sobre filas que son de otros efectos
(`Source/DSP/FxCatalogue.h`, tabla `fxNeuronikIdentities()`). Una reserva es un
parche: funciona, pero significa "aqui vendria esto" sobre un id que hoy habla de
otra cosa. Esta propuesta es para cerrar eso de verdad.

---

## 1. Qué se propone

Tres filas al **final** de `effects`, con ids nuevos, y el resto del fichero
ajustado para que siga siendo cierto lo que dice de sí mismo:

```jsonc
    { "id": 57, "name": "FreeVerb Reverb",    "engine": "Reverb",               "variant": null,      "family": "reverb",     "params": 4 },
    { "id": 58, "name": "Saturation",         "engine": "Saturation",           "variant": null,      "family": "distortion", "params": 1 },
    { "id": 59, "name": "Schroeder Reverb",   "engine": "SchroederReverb",      "variant": null,      "family": "reverb",     "params": 4 }
```

El `params` de la fila 58 es **1** porque el motor de saturación solo expone el
drive, y es justo el campo que rompe la convención del fichero. Véase §3.2.

(Van dentro del array `effects`, no son un JSON por sí solas. Con el mismo relleno por columnas que las existentes —verificado
que la 52 se reproduce carácter a carácter con el mismo formateo—, y la 59 se queda
sin coma porque es la última del array.)

Resumen del cambio:

| Fichero | Qué pasa |
|---|---|
| `ABDSharedAssets/contracts/fx-effects.json` | +3 filas, 2 `notes` tocadas, +1 nota nueva, `generatedFrom` matizado, `$schema` arreglado |
| `ABDNeural/Source/DSP/FxCatalogue.h` | 3 filas de la tabla de identidades: `aligned` a `true` y `sharedId` a 57/58/59 |
| `ABDNeural/Tests/fxCatalogContractTest.mjs` | sin tocar: **se pone rojo solo** (§5.2) |

---

## 2. Por qué ids nuevos y no reutilizar las reservas

Las reservas actuales son el id **1** ("Hall"), el **50** ("Oversampled Dist") y el
**22** ("Deep Verb"). Reescribir esas tres filas **borraría tres efectos reales** del
catálogo de ABDEep, y ese fallo no suena a error: suena a que el usuario abre el
hueco 1 y ya no hay reverb de hall. Un id que se reutiliza es un id que un día
significa dos cosas, y el síntoma aparece en el producto ajeno, no en el que cambió
el fichero.

Numerar al final tampoco es pereza. Insertar en medio renumera todo lo de después, y
los ids de este fichero son **dirección, no orden**: el mismo criterio que ya está
escrito en `FxDefaultCatalogue.h` ("si se inserta, se inserta al final"), y con la
razón puesta: el "4" dejaría de significar una cosa el lunes y otra el martes.

La cabecera de `FxCatalogue.h` ya anticipa exactamente este paso: *"Cuando el
contrato compartido gane esas tres filas, se cambia el flag **y el número** en ESTE
fichero — un solo sitio — y el test de paridad pasa a exigir la coincididencia."*

**Consecuencia asumida**: los ids 57, 58 y 59 los produce un motor que **no es** el
de `generatedFrom`. Véase §4.2 y §6.1.

---

## 3. Las tres filas, campo a campo

### 3.1 `family`

| id | `family` | Por qué |
|---|---|---|
| 57 | `reverb` | obvious |
| 58 | `distortion` | **no hay familia "saturation"**, y no se crea. `distortion` es la correcta: el motor es `atan(x · drive)`, una curva de saturación. |
| 59 | `reverb` | obvious |

Las dos familias elegidas **ya existen** en el array `families`. Eso es lo que
persigue el diseño de once familias: un tema por familia, y una variante nueva
hereda el aspecto de su familia sin escribir CSS. Añadir "saturation" como familia
sería crear un tema entero para que lo use un único efecto.

### 3.2 `params`: aquí hay que romper la convención del fichero, y decirlo

La nota 4 del propio contrato dice: *"`params` es el número de controles que ACEPTA
el motor, no el número que se pinta"*. Hasta hoy eso ha querido decir siempre lo
mismo: **la implementación de ABDEep**. El coro tiene 11 ahí y 2 en el motor
compartido; el delay, 12 y 2; la reverberación, 12 y 4.

Para estas tres filas no hay implementación de ABDEep. La única implementación que
existe es la del módulo compartido, así que el único número honesto es el de esa:
`FxEffectInfo::numParams` de la fila del catálogo.

| id | motor | `kNumParams` |
|---|---|---|
| 57 | `adapters::ReverbFx` | 4 (`size`, `damping`, `width`, `levels`) |
| 58 | `adapters::SaturationFx` | 1 (`drive`) |
| 59 | `adapters::SchroederFx` | 4 (`decay`, `damping`, `diffusion`, `predelay`) |

La alternativa —poner el número de mandos de la implementación de ABDEep, o el de
otro efecto parecido— sería inventar un número que no corresponde a nada. **Una fila
que miente con `params` es peor que una fila que rompe la convención, y por eso el
cambio tiene que ir acompañado de una nota que lo diga en el fichero, no solo aquí.

⚠️ **Consecuencia a vigilar**: los ids 13 ("Delay") y 10 ("Stereo Chorus") son filas
**alineadas** cuyo motor compartido tiene menos mandos que su implementación de
ABDEep, y ahí el `params` del contrato cuenta la de ABDEep. A partir de las 57..59
la columna va a significar dos cosas a la vez según la fila. El test de paridad ya
lo sabe y **no compara `params`** a propósito (`Tests/fxCatalogContractTest.mjs`,
bloque de cabecera). Lo que hay que evitar es que alguien lo compare "para arreglarlo".

### 3.3 `engine` y `variant`

La nota 2 dice: *"`engine` + `variant` juntos describen la clase que se
instancia"*. Los 42 valores de `engine` que hay hoy son clases de ABDEep
(`SimpleReverb`, `MidasEQ`, `ZitaReverb`…). Para estas tres filas la clase que se
instancia **sí existe**, y es del módulo compartido:

| id | clase real | fichero |
|---|---|---|
| 57 | `abd::dsp::Reverb` | `DspEffects/DspReverb.h:70` |
| 58 | `abd::dsp::Saturation` | `DspEffects/DspSaturation.h:54` |
| 59 | `abd::dsp::SchroederReverb` | `DspEffects/DspSchroederReverb.h:84` |

Los tres nombres están **libres** en el contrato (verificado: la lista de `engine`
no contiene `Reverb`, `Saturation` ni `SchroederReverb`), y siguen el estilo de la
casa: el nombre de la clase, sin namespace. `variant` va a `null` porque el motor
no tiene variantes: es una clase, no una familia con constantes.

> **Decisión pendiente (la única de verdad).** La alternativa honesta también sería
> `"engine": null`, con la nota de que no hay equivalente en la fábrica de ABDEep.
> **Se recomienda el nombre de la clase**, porque `null` + `null` significa hoy
> "bypass" en la única fila que lo usa, y un lector que encuentre tres filas con
> `engine: null` y una de ellas bypass no tiene forma de saber cuál es cuál. El
> nombre de la clase no miente y no necesita nota para entenderse.

### 3.4 `name`

`name` es lo que pinta la interfaz cuando el efecto se nombra desde el vocabulario
compartido, así que es texto de cara al usuario, y ahí no hay que escatimar:

| id | `name` propuesto | alternativas descartadas |
|---|---|---|
| 57 | `FreeVerb Reverb` | "Hall" (es otro motor), "FreeVerb" (a secas, junto a las quince de reverb que ya hay se pierde de qué es) |
| 58 | `Saturation` | — el motor ya se llama así y no hay ambigüedad posible |
| 59 | `Schroeder Reverb` | "Schroeder" (a secas, junto a las de arriba: dos "Verb" que no dicen cuál) |

Nota: el `displayName` de producto (`FxEffectInfo::name` del motor) sigue siendo
`Reverb` / `Saturation` / `Schroeder`. Son dos campos distintos a propósito: el
`name` es **vocabulario compartido** y el `displayName` es **decisión de producto**.
Para la fila 58 coinciden, y no es un problema.

---

## 4. Lo demás del fichero que tiene que cambiar con las filas

Añadir tres filas sin tocar nada más deja el fichero diciendo cosas falsas. Estas son
las que habría que cambiar en el mismo commit:

### 4.1 Las `notes`

- **Nota 1**: *"los ids 1..56 son efectos"* → **1..59**.
- **Nota 6 (AVISO CONOCIDO 2)**: el recuento de filas del final ("57 ids… 0..56 sin
  huecos") → **60 filas, 0..59 sin huecos**, y conviene **reescribir la frase**,
  porque tal como está es ambigua: cuenta filas, no ids de efecto, y con las tres
  filas nuevas ese desliz se vuelve a colar.
- **Nota nueva** (la importante), para que nadie tenga que reconstruirlo leyendo el
  diff:

  > ids 57..59 los produce el módulo compartido (`ABDSharedCode/DspEffects`), no la
  > fábrica de ABDEep. Son las primeras filas del fichero que `generatedFrom` no
  > produce. En esas tres filas `params` cuenta los mandos del motor compartido, que
  > es la única implementación que existe; en el resto cuenta los de la
  > implementación de ABDEep.

### 4.2 `generatedFrom`

Hoy es una cadena: `"ABDEep/Source/DSP/FX/FXSlot_Factory.cpp"`. Con las filas 57..59
deja de ser cierto, y un `generatedFrom` falso es la clase de mentira que hace que
alguien de confianza regenere el fichero desde la fabrica y borre tres efectos sin
avisar. Opciones:

1. **Recomendada**: dejarlo como está y que la nota nueva lo matice.
2. Pasarlo a lista: `["ABDEep/Source/DSP/FX/FXSlot_Factory.cpp", "ABDSharedCode/DspEffects/FxDefaultCatalogue.h"]`.
   Más preciso, pero cambia el **tipo** del campo, y cualquier consumidor que lo lea
   como cadena se rompe. Como no se ha encontrado ninguno (§6.1), el riesgo es bajo,
   pero sigue siendo un cambio de tipo en un contrato compartido.

### 4.3 `$schema` apunta a un fichero que no existe

El contrato declara `"$schema": "./fx-effects.schema.json"` y **ese fichero no está**
en `ABDSharedAssets/contracts/`. Consecuencia directa de esta propuesta: **nada
valida la forma de las filas que se añadan**, ni de estas tres ni de las siguientes.
Escribirlo son unas 40 líneas y es lo que convierte "añadir una fila" en algo
auditable en vez de en un acto de fe. No tiene nada que ver con las tres filas, pero
aprovechar el mismo commit es lo lógico: es la primera vez que se toca el fichero.

### 4.4 Orden de las filas dentro del array

El array está hoy agrupado por familia, que es **orden de lectura**. Insertar por
familia rompe los ids; apendar rompe la vista por familias. **Apendar**: los ids son
la verdad, y el agrupado es decorativo. Si aun así se quiere conservar la lectura
por familias, la respuesta correcta es un *sort* en el consumidor, no ids
renumerados.

---

## 5. Qué se desbloquea en este repo

### 5.1 `Source/DSP/FxCatalogue.h`, tres filas

```diff
-        {  10, "Stereo Chorus",      "chorus",     true  },   // chorus
-        {  13, "Delay",              "delay",      true  },   // delay
-        {   1, "Hall",               "reverb",     false },   // reverb     RESERVA
-        {  50, "Oversampled Dist",   "distortion", false },   // saturation RESERVA
-        {  22, "Deep Verb",          "reverb",     false },   // schroeder  RESERVA
-        {  36, "BBD Chorus",         "chorus",     true  },   // bbd
+        {  10, "Stereo Chorus",      "chorus",     true  },   // chorus
+        {  13, "Delay",              "delay",      true  },   // delay
+        {  57, "FreeVerb Reverb",    "reverb",     true  },   // reverb
+        {  58, "Saturation",         "distortion", true  },   // saturation
+        {  59, "Schroeder Reverb",   "reverb",     true  },   // schroeder
+        {  36, "BBD Chorus",         "chorus",     true  },   // bbd
```

También se puede borrar el bloque "**LAS TRES FILAS SIN ALINEAR**" entero de la
cabecera de ese fichero (unas 30 líneas), porque pasa a ser historia: la explicación
de por qué esas reservas existen ya no le sirve a nadie, y una explicación que ya no
explica nada es la forma más lenta de mentir.

### 5.2 El test de paridad se pone rojo solo

`Tests/fxCatalogContractTest.mjs` tiene dos ramas, y el cambio de §5.1 hace que cada
una mire lo contrario de lo que miraba:

- Las **alineadas** exigen que `row.name === effect.name` y `row.family ===
  effect.family` contra el JSON de verdad. Con `aligned: true` y los ids nuevos,
  exige exactamente las tres filas propuestas. Si un nombre o una familia no cuadran,
  cae.
- Las **reservas** exigen que el contrato diga **otra cosa** que nuestro
  `displayName` (la comprobacion que hoy es `row.name !== effect.displayName`). Con
  cero reservas, **ese bloque deja de comprobar cualquier cosa sin decir nada**.

Ese segundo punto es un agujero que conviene cerrar en el mismo commit: que el test
**diga** que no queda ninguna reserva, en vez de recorrer un array vacío en
silencio. Un parity check que se apaga porque se quedó sin filas es peor que no
tenerlo: el día que alguien reintroduzca una reserva, el test sigue en verde.

---

## 6. Riesgos y preguntas abiertas

### 6.1 El `description` del contrato declara consumidores que no lo referencian

El `description` dice que lo consumen *"la WebUI de ABDEep, la de ABDNeural y el
registro de temas (`components/fxTheme.js`)"*. Buscando `fx-effects` y
`buildFxThemeIndex` en `ABDEep` y en `ABDSharedAssets` (js/mjs/html/json) **no
aparece ninguna referencia**. Lo que sí se ha comprobado:

- `fxTheme.js` no lee el fichero: recibe el catálogo ya parseado
  (`buildFxThemeIndex(catalogue)`) y solo usa `id`, `family` y `name`. Añadir filas
  no le puede hacer nada: un `Map` más grande.
- El único código que **abre el fichero hoy** es el test de paridad de este repo.

Conclusión: el riesgo de romper a otro producto por añadir filas es **bajo**, y el
beneficio es que el `description` y el `generatedFrom` están describiendo un
consumo que hoy no existe. Eso no es un motivo para no hacerlo, pero sí para no
dar por hecho que "el contrato lo consume la WebUI de ABDEep" al revisar el cambio.

### 6.2 Si algún día un consumidor construye un menú iterando `effects`

Un menú que recorra el array y pida a la fábrica un motor por `effect.id` ofrecería
**57, 58 y 59 como entradas sin motor detrás**. No se ha encontrado ese consumidor
(§6.1), así que hoy no es un problema; queda escrito porque es el modo de fallo
natural de este fichero si el día que viene se le da un consumidor nuevo.

### 6.3 El `name` es texto de otro producto

Los nombres de las 57..59 los pinta quien use el vocabulario compartido. Con el
vocabulario de hoy solo es este repo, así que el coste de equivocarse es cero; si
mañana entra ABDEep, un "FreeVerb Reverb" en su menú es una decisión que se tomó
aquí sin mirarlo. Por eso §3.4 propone los nombres **y sus alternativas**: es una
decisión de dos minutos que conviene tomar **con el otro repo delante**, no sola.

---

## 7. Orden de ejecución

1. Añadir las tres filas y las notas en `ABDSharedAssets/contracts/fx-effects.json`.
2. Cambiar las tres filas de `fxNeuronikIdentities()` en `Source/DSP/FxCatalogue.h`.
3. Correr `npx vitest run` en `WebUI/` y `ctest -R FxCatalogContract`: el test debe
   pasar **solo** si las tres filas del JSON coinciden con el C++ en nombre y familia.
   Ese es el momento de la prueba de fuego: si el paso 1 y el 2 se hicieron de
   memoria en vez de con este documento delante, hay algún nombre mal escrito.
4. Solo entonces, borrar el bloque "LAS TRES FILAS SIN ALINEAR" de la cabecera, y
   hacer que el test diga que ya no queda ninguna reserva.
5. `git add` de lo nuevo, que es lo que `NEURONiK_ReferencedFiles` viene pidiendo
   desde la fase 1.

Los pasos 1 y 2 están en **repositorios distintos**, así que el 3 es el primero que
puede fallar y el 2 no se puede aplicar solo: esa es la buena noticia del diseño del
test de paridad, y la razón por la que existe.
