# Verificar este proyecto

Todo lo que hace falta para saber si el proyecto está bien, en un fichero. Lo
escribí porque estaba repartido entre varios mensajes de commit y un par de
`README` por carpeta, que es donde nadie lo encuentra a los dos meses.

## Lo primero: el entorno

En esta máquina el build **no arranca** si no se le dicen las tres rutas.
Está escrito entero, con el porqué de cada parte, en
[COMO-ARREGLAR-EL-BUILD.md](COMO-ARREGLAR-EL-BUILD.md). El resumen son tres
`-D`: la instancia de Visual Studio, `JUCE_PATH`, y
`JUCE_WEBVIEW2_PACKAGE_LOCATION`.

Si vienes de otro repositorio o de otra máquina, **eso primero**. Sin eso no
compila nada y los rojos del paso 1 no son de compilación.

Y lo segundo, que no está en ningún sitio escrito:

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
```

CMake **no** está en el PATH del sistema en esta máquina. Sin eso, `ctest` no
existe, el paso 2 se salta entero sin decirlo del todo, y los scripts se leen
ficheros de la última corrida: rojos viejos contados como si fueran de hoy. Es
el peor fallo posible del verify, y es silencioso.

## Los tres comandos

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"

# 1. Lo rapido: la parte compartida y los dos gemelos, sobre el mismo build
bash Scripts/verify_all_check.sh ; echo "rc=$?"

# 2. Un solo script, el de tu terminal (.bat en Windows)
bash Scripts/verify_all.sh

# 3. Solo la logica de la lista de conocidos, sin build ni tests
node Scripts/selftest_verify_all_node.js
```

## Qué mira cada uno, y cuál es el que quieres

**`selftest_verify_all_node.js`** (el tercero) es el rápido: 265 comprobaciones
sobre la lista de conocidos y sobre lo que se rompe en silencio, sin compilar
nada. Tarda unos minutos. Si has tocado la lista, `Scripts/known/` o
`verify_all_node.js`, **este es el que quieres**.

**`verify_all_check.sh`** (el primero) es el completo: pasa el selftest,
lanza los dos gemelos uno detrás de otro sobre el mismo build y compara lo que
dicen. Es la pregunta «¿dicen lo mismo el `.sh` y el `.bat`?», y por construcción
tarda lo que tardan **dos** verificaciones.

**`verify_all.sh`** (el segundo) es la verificación en sí, por pasos:

| Paso | Qué hace |
| --- | --- |
| 1 | Compila los 39 tests nativos + 2 programas de apoyo (no todo: «todo» arrastra el WASM) |
| 2 | `ctest` con timeout, y separa los rojos conocidos de los nuevos |
| 3 | `vitest` de la WebUI |
| 4 | `vitest` de ABDSharedAssets |
| 5 | Contratos cruzados entre los dos repositorios |

Sirve con `--only=2`, `--no-build` y `--help`. Las dos cosas dan lo mismo en
Windows y en bash, y se puede comprobar:

| Lo que escribes | Lo que hace | Por qué |
| --- | --- | --- |
| `--no-build` o `--NO-BUILD` | salta la compilación del paso 1 | las opciones no distinguen mayúsculas |
| `--only=2` o `--only 2` | un solo paso, de 1 a 5 | las dos formas funcionan en los dos scripts |
| `--solo=1` (typo) | avisa y se sigue | un flag de más no puede tumbar un verify |
| `--only=9`, `--only=`, `--only=1 --only=9` | **error, sale con 2** | es el flag que decide *qué* se verifica |

Lo último estaba mal antes, y en las dos direcciones: `verify_all.sh --only=9`
no ejecutaba **nada** y salía con 0 diciendo que todo estaba en verde, y
`verify_all.bat --only=9` se iba a los **cinco pasos** (70 s) por un typo.

## El resultado: qué significa cada cosa

Salir en **rojo** es lo normal aquí, y no es un fallo del script. El verify
se niega a salir en verde con rojos dentro, a propósito: un verify que dice «ok»
con lo que hay dentro enseña a mirar el código de salida sin mirar la salida, y
el día que aparezca un rojo nuevo se confunde con el ruido de siempre.

La línea que importa es la del final:

```bash
bash Scripts/verify_all.sh 2>&1 | tail -3
```

```
  Dicen lo mismo: 10 line(s) de detalle, 10 recuento(s), sin una sola diferencia.
```

Y en cada paso, la cuenta de rojos con su clasificación:

```
  53 tests, 8 en rojo (los verdes no se listan: taparian los rojos)
  ROJO    NEURONiK_PresetMigrationParityTest
          ROTO DE ORIGEN AJENO: ajeno: del modulo compartido a medias...
```

`MIO` es de este trabajo y hay que arreglarlo. `AJENO` es del trabajo de al
lado: se lee y se decide uno a uno. **`SIN CLASIFICAR` es lo que hay que
mirar**: un rojo nuevo, o un conocido cuyo motivo se ha quedado sin prefijo.

## Los tres códigos de salida

| Código | Qué significa |
|---|---|
| `0` | Todo en verde, y **todo lo que se ha ejecutado es de esta pasada**. |
| `1` | Hay algún paso con fallos. Sale en rojo aunque los rojos sean del otro trabajo. |
| `2` | Uso incorrecto (un `--only` que no es un paso, otro verify corriendo). |
| `3` | **Sin rojos, pero con tests sin medir**: su `.exe` no es de esta pasada. |

El `3` es nuevo. Antes, 35 binarios rancios y cero rojos salían con `0`, y un
`0` con la mitad de la batería sin medir es un `0` que no se ha ganado. No es
`1` porque no ha fallado nada, y `--no-build` es una opción legítima: quien la
usa quiere correr la batería sobre el build que ya hay, y por eso el aviso es el
más fuerte de los tres en vez de un error.

Sale junto a una línea del informe que también va con `--only=2`, aunque entonces
la cuenta salga a cero:

```
  39 test(s) SIN MEDIR: no se han compilado en esta pasada, asi que de ellos
  no se puede decir ni que fallen ni que pasan. Si no hay rojos, el comando
  sale con 3, no con 0: lo que se ha medido esta en verde, y lo que no,
  no se ha mirado. Recompila lo que falte y vuelve a pasar la bateria.
```

`verify_all_check.sh` **no mira** el código de salida de los dos verify (compara
solo el resumen), así que un `3` o un `4` no lo hacen fallar: eso es justo lo que
los hace seguros.

### Los cuatro veredictos del check de gemelos

El check lanza los dos scripts uno detrás de otro sobre el mismo build y compara
sus resumenes. La comparación tiene **cuatro** salidas, no dos. Las dos últimas se
añadieron el 2026-10-02 porque las de antes mentian en casos reales:

| Veredicto | Qué ha pasado | Qué hacer |
|---|---|---|
| `0` | Dicen lo mismo. | Nada. |
| `3` | Dicen lo mismo **salvo en un rojo intermitente**: algún test ha salido rojo en una de las dos pasadas y verde en la otra. | Relanzar ese test (`ctest -R <nombre>`). Si sale verde otra vez, era eso. |
| `4` | **Cuentan igual, pero una vuelta ha ido peor**: ctest ha matado tests por tiempo en una de las dos, y los tests que salieron lentos en esa vuelta y no en la otra. | **Repetir el check entero** con la máquina descargada. Si sale igual otra vez, ya no es la máquina: es el umbral. |
| `1` | **No** dicen lo mismo: hay una regla en un gemelo y no en el otro. | Mirar el `diff` que imprime el check y cambiar la regla en los dos. |

El `3` y el `4` del check salen como **`0`** a propósito: comparar dos vueltas en
las que la máquina ha ido distinta no dice nada de los dos scripts, solo de la
máquina. Y salen **muy visible**: el `3` nombra los tests que se han movido y por
qué lado, y el `4` nombra el lado que fue peor, cuántos timeouts hubo en cada
vuelta y los tests que perdona por eso. La orden suelta
(`node Scripts/verify_all_node.js compara ...`) sí devuelve `3` y `4`, para que
quien la llame desde otro script los distinga del `0` sin leer el texto.

**Por qué no es una lista de nombres.** Los dos veredictos son una prueba
aritmética: si se le suman al lado que no vio las líneas que le sobran, los dos
resúmenes tienen que coincidir línea a línea **y los recuentos tienen que cuadrar
con esas líneas**. Si no cuadran, sale con `1` como siempre. Eso es lo que impide
que un error de conteo en un gemelo se esconda: si un gemelo contara mal, los
números no saldrían cuadrados.

**Por qué son dos veredictos y no uno.** El intermitente tiene una sola causa
posible (el test va a veces) y una sola acción (relanzar el test). El degradado
tiene **dos** causas indistinguibles con dos vueltas: la máquina fue más lenta, o
al gemelo que fue más lento le cambiaron el umbral de «lento». La segunda es
justo el bug que este check existe para cazar. Por eso el `4` no dice «es la
máquina»: dice que hay un timeout que lo respalda y que **hay que repetir el
check**. Un `3` se resuelve relanzando los tests que nombra; un `4` solo se
resuelve relanzando el check entero.

**Por qué un timeout perdona y un `lento` no.** Un test que ctest mata por tiempo
es un hecho de *aquella* vuelta: no hay ningún umbral escrito en estos dos
scripts que lo produzca o lo quite. Un `lento` de más, en cambio, tiene las dos
causas de arriba. Perdonar el `lento` sin el timeout sería una tautología («los
lentos no cuadran, así que los lentos no cuentan») y dejaría pasar justo el
umbral cambiado que se quiere cazar.

Tres cosas que **no** se perdonan, aunque lo otro cuadre:

- **Un `lento` que solo ve uno de los dos sin ningún timeout que lo respalde.** El
  umbral de «lento» es una regla, y el check existe para cazar reglas distintas.
- **Un recuento que no cuenta rojos ni lentos** (`tocoTimeout`, `huerfanos`,
  `pasos`, `conocidos`, `tests`). Un rojo intermitente no los toca: si se mueven,
  hay otra causa, y el texto dice cuál.
- **Una cuenta rota** (un `fallos` que no es la diferencia de las líneas, una
  clasificación que no sube lo que sube su número de líneas), aunque haya un
  timeout de por medio. Un timeout de más perdona los lentos; no perdona una
  aritmética que no cuadra.

### `--estricto`: el mismo caso, juzgado como divergencia

```bash
bash Scripts/verify_all_check.sh --estricto
```

Quita los dos veredictos que perdonan: todo lo que no cuadra sale con `1`, como
antes del 2026-10-02. Es para cuando el `3` o el `4` te parecen generosos y
quieres el rojo de verdad. Es también la única vía para no depender de la buena
fe con el `4`, que por su propia construcción no puede saber si la máquina fue
más lenta o si le movieron el umbral.

El flag **no se pasa a los dos verify** (no lo entenderían y se negaría a
arrancar): cambia cómo se juzga la diferencia, y ese juicio es del check.

Con `--comparar` (que compara dos resúmenes ya escritos, sin lanzar nada) el `3`
y el `4` **se propagan tal cual**, sin pasarlos a `0`: ese modo es el que se usa
para mirar dos logs viejos y preguntar por código.

### Cómo se prueba un veredicto que sale una vez de cada mil

La sección 15 del selftest monta el caso con los datos del fallo real
(`NEURONiK_WorkletSync` rojo en los dos, `NEURONiK_WebUiLocalModeE2e` rojo en
uno) y, sobre todo, los casos que **no** pueden salir con `3`. La sección 16 hace
lo mismo con el `4`, con los datos del otro fallo real del mismo día (el `.sh` con
`tocoTimeout 0` y el `.bat` con `tocoTimeout 2`, y los dos E2E como lentos solo
en el `.bat`). Eso de que una prueba de una regla que casi nunca se dispara tiene
que ser sobre todo una lista de lo que tiene que seguir saliendo con `1`: la
regla nueva es una puerta que se abre, y lo que hay que vigilar es que no se abra
de más.

Las reglas se han medido una a una, rompiéndolas en `verify_all_node.js` y
contando cuántas comprobaciones caen. Tabla del 2026-10-02 (265 comprobaciones
en verde sin mutar):

| Mutación | Comprobaciones que caen | Lee |
|---|---|---|
| No cuadrar `fallos` con las líneas que sobran | 1 | **Muerde**: es el que decide. |
| No comprobar la clasificación (`mios`/`ajenos`/`sinClasificar`) | 2 | **Muerde**. |
| Dejar pasar cualquier recuento, tb los que no cuentan rojos | 3 | **Muerde**. |
| Ignorar `--estricto` | 1 | **Muerde**. |
| La condición de «un solo lado» escrita al revés | 6 | **Muerde** (fue el fallo del primer intento). |
| Salir con `0` en vez de con `3` | 3 | **Muerde**. |
| Perdonar un `lento` de más sin timeout que lo respalde | 11 | **Muerde** (sección 16). |
| Inventar un timeout cuando falta el recuento | 6 | **Muerde** (sección 16). |
| Tratar la ausencia del recuento como si fuera un cero | 5 | **Muerde** (sección 16). |
| Dejar que la máquina excuse un rojo que no cuadra | 11 | **Muerde** (sección 16). |
| Perdonar también los `lento` sin mirar los timeouts | 0 | **Redundante**: ya los descarta la cuenta de `fallos`. |
| Perdonar líneas que sobran en los dos lados | 0 | **Redundante**, y por aritmética. |

Los números de la tabla son de 2026-10-02 y **varían** entre mutaciones, y no es
un error: cada una cambia el número de comprobaciones que el propio selftest llega
a ejecutar (una aserción cuyo valor cambia puede leerse de otra forma), así
que el denominador no es el mismo. Lo que se compara es cuántas caen, no el
denominador.

Dos de estas mutaciones están **en el propio selftest** (sección 16): se copia el
programa, se rompe la regla en el fichero copiado y se corre el caso. Se comprueba
también que el mutante ha cambiado de verdad el fichero, porque un `.replace` que
no encuentra su texto deja el fichero intacto y las comprobaciones siguientes darían
verde por no haber roto nada — que es un fallo que ya pasó aquí: el ancla de la
mutación del timeout tenía un `TimeoutsMas` con mayúscula de más y el `.replace`
no encontraba nada.

Las dos últimas se quedan en el código igualmente: son la **explicación en voz
alta** de lo que la aritmética ya implica. Una regla que se deduce de otra
cuenta es una regla que se lee en un sitio y se mantiene en otro, que es
justo lo que `diferencia()` se ha puesto en medio para que las dos mitades sean
el mismo código y no dos cuentas parecidas.

## Los rojos conocidos

La lista está partida en dos, y las dos mitades se escriben a mano:

- [`Scripts/verify_all_known.json`](verify_all_known.json) — **solo los
  nombres**, de los tests que se sabe que están rojos.
- [`Scripts/known/`](known/) — el **motivo** de cada uno, en
  `<nombre del test>.txt`, con el prefijo `MIO:` o `ajeno:` en la primera
  línea.

La convención entera está en [`known/README.md`](known/README.md), y ahí está
también qué hacer cuando un test se renombra, se borra o se arregla.

Un rojo conocido que **pasa** lo avisa el propio script con `ARREGLO`, y dice
los dos pasos que hay que hacer a mano: quitar el nombre del índice y borrar el
fichero de motivo. No los quita él.

Y un rojo conocido cuyo `.exe` **no se ha compilado en esta pasada** no puede
decir `ARREGLO`, porque no se ha ejecutado: sale como `SIN MEDIR`, sin pedir que
se borre nada. Antes se pisaban los dos avisos, separados por el `ctest`
entero, y el segundo («borra su motivo») mandaba sobre el primero («no se sabe
nada de este test»), con lo que un `--no-build` podía hacer borrar entradas de
la lista de conocidos por tests que nadie había mirado:

```
  BINARIO RANCIO  1 test(s) se ejecutan con el .exe de una pasada anterior:
        NEURONiK_EjemploTest
  ...
  ARREGLO  NEURONiK_EjemploTest
            quita "NEURONiK_EjemploTest" del indice y borra known/NEURONiK_EjemploTest.txt
```

Ahora el primero dice los nombres que no se han medido, y el segundo los
respeta. Y si la lista no ha llegado (el paso 1 no se ha ejecutado, por ejemplo
con `--only=2`), **no dice `ARREGLO` de ninguno**: lo que no se sabe no se
anuncia como bueno, el mismo principio que ya se aplica cuando el log de rojos
no se puede leer.

Y cada uno dice **por qué** no se ha medido, que no es lo mismo en todos los
casos y en el peor de ellos cambia lo que hay que hacer:

```
  SIN MEDIR  NEURONiK_ModulationMatrixTest
            su .exe no se ha compilado en esta pasada, asi que no se sabe si falla: no se puede decir que este arreglado,
            y por eso NO hay que quitarlo del indice ni borrar su motivo
  SIN MEDIR  NEURONiK_ModulationDest17DriveTest
            no tiene .exe, asi que no se ha ejecutado nunca; recompila su target y no solo su .exe: no se puede decir que este arreglado,
            y por eso NO hay que quitarlo del indice ni borrar su motivo
```

El primero tiene el `.exe` de antes: recompilando su target, el rojo se va. El
segundo **no tiene `.exe`**, así que no se ha ejecutado nunca: recompilando su
target tampoco desaparece, porque lo que hay que arreglar es por qué no se
construye. Con un texto único («su .exe no se ha compilado») los dos sonaban
igual, y quien leyera el segundo iba al sitio equivocado con seguridad.

El motivo lo escribe el aviso de binarios rancios, que es el único que ha
mirado los `.exe`, y viaja con el nombre en un fichero temporal con una línea
por test (`nombre`, un tabulador, el motivo). Los dos scripts solo **cuentan**
las líneas de ese fichero, para lo del código de salida 3; el motivo lo
muestra el aviso de `ARREGLO`.

## Cómo se prueba un aviso que no sale nunca

`verify_all_known.json` tiene `entradas: []`: no hay ningún rojo conocido, así
que el aviso de `ARREGLO` / `SIN MEDIR` **no aparece en el verify de verdad, ni
siquiera con `--no-build`**. No se ve porque no hay nada que lo dispare, no
porque esté roto. Ese es un problema para el que lo lee y para el que lo
escribe: un aviso que no sale nunca no se puede revisar ni por la ortografía ni
por la claridad, y se escribe bien, pasa las comprobaciones, y el día que
aparece por primera vez nadie lo ha leído nunca.

Por eso el selftest tiene dos secciones que **montan el aviso y enseñan la
pantalla entera**, con entradas reales de esta batería:

| Sección | Qué enseña |
| --- | --- |
| 13 | Una entrada real de verdad: un test reconstruido (`ARREGLO`), uno con el `.exe` de antes y uno sin `.exe` (los dos `SIN MEDIR`), con los motivos redactados como se redactan de verdad |
| 14 | Las dos pantallas, con build y con `--no-build`, y los **tres motivos distintos** uno al lado del otro |

Las dos imprimen la salida tal cual, entre líneas, con los códigos de color de
verdad. Al ejecutarlas se lee el aviso como lo verías en la consola, que es
justo lo que no se puede hacer con el verify real.

### Por qué una entrada de mentira no basta

Las secciones 12 y anteriores comprueban la **regla** con nombres inventados
(`T1`, `T2`), que es lo correcto para una prueba automática: la regla no
depende de que hoy haya un rojo en concreto. El problema es que un nombre de dos
letras deja fuera justo lo que hay que mirar en un aviso:

- Si el nombre **cabe** en la línea y si el motivo entero se entiende debajo.
- Si el texto dice lo que hay que hacer, no solo lo que pasó.
- Si los dos `SIN MEDIR` que parecen iguales **son** iguales, o si tienen que
  decir cosas distintas.

Y hay una razón más, que es la que de verdad manda: **la lista de verdad no se
puede usar como banco de pruebas.** Es el sitio donde viven los rojos que se
saben que hay. Añadir una entrada para probar un aviso significaría que un rojo
que no existe se anunciaría como `ARREGLO` de verdad, con el consejo de borrar
un fichero de `known/` que no habría que borrar. El banco de pruebas tiene que
estar en un directorio temporal, y por eso `known/` tiene su copia propia con
`entradas: []`.

### Qué se comprueba, y qué no

La diferencia importa, porque es donde está el límite de lo que el selftest
puede decir por sí solo:

- **Las reglas se comprueban con `ok`**, y cada comprobación nueva se verifica
  **por mutación**: se rompe el código a propósito y se mira que la comprobación
  caiga. Una comprobación que no se ha visto caer no se sabe si mira algo.
- **La ortografía y la claridad se miran leyendo**, que es lo único que sirve.
  Por eso la sección 13 imprime la pantalla: para eso está.

Un detalle que parece menor y no lo es: la sección 14 cuenta las líneas del
fichero de no medidos con cada uno de los dos comandos de verdad, `grep -c .` y
el `for /f` del `.bat`, y los compara. Añadir el tabulador al formato podría
romper el código de salida 3 en silencio —si un gemelo se quedara contando una
línea o ninguna, saldría con cualquier cuenta y la línea del informe del script
mentiría— así que eso se mide, no se supone.

## Cuando algo falla de verdad

| Lo que ves | Lo que casi siempre es |
| --- | --- |
| `CTEST NO ESTA EN EL PATH` | Falta el `export PATH` de arriba |
| `SIN TEST EN LA BATERIA` | El test se renombró o se borró, y hay que mover su nombre **y** su fichero de motivo |
| `MOTIVO SIN ENTRADA` | Hay un `.txt` en `known/` que el índice no nombra, y no lo lee nadie |
| `AVISO` sobre un prefijo | Un motivo sin `MIO:` ni `ajeno:`; el rojo sale `SIN CLASIFICAR` |
| Los dos gemelos NO coinciden | Uno de los dos se ha tocado y el otro no. El `diff` de arriba da el nombre del rojo que difiere |
| `AVISO: ... no se ha podido cargar` | La lista no se puede cargar; el motivo concreto sale en la validación de más abajo |
| `BINARIO RANCIO` | El test se está ejecutando con el `.exe` de una pasada anterior, no con el código de ahora. Ver abajo |

## El aviso de binarios rancios

Sale en el paso 1, siempre, y es lo primero que hay que leer antes de fiarse de
ningún resultado de la batería.

El paso 1 compila **los 39 tests nativos que ve `ctest`**, más los dos
programas que la propia verificación usa para sus fixtures (`FxExport` y
`ModulationParityDump`, que no son tests y por eso no salen de la lista). De los
53 tests que ve `ctest`, 39 son nativos (los otros 14 son de Node: Playwright y
contratos). Al compilar los 39, **el encabezado dice `39 reconstruidos` y `0
rancios`**, y el aviso de binarios rancios **no dice nada**: es la señal de que
la batería ha medido el código de ahora.

Antes compilaba seis targets, de los que solo cuatro eran tests, y los otros 35
nativos se ejecutaban con el `.exe` de la pasada anterior. Por eso la cuenta
tenía tres cifras: `39 nativos = 4 reconstruidos + 35 rancios`. Ahora la cuenta
es de dos, y si aparece un rancio es que algo se ha compilado por fuera entre
una verify y otra, que es justo lo que hay que mirar.

La lista de targets **no está escrita en el script**: sale de
`CTestTestfile.cmake`, que es donde CMake declara los tests. Añadir un test al
proyecto no obliga a tocar el verify, y si la lista se queda corta el aviso lo
dice en vez de dejarlo pasar en silencio.

Y los 41 targets van en **una sola invocación** de `cmake --build`, no una por
target: el generador de Visual Studio recompila la librería de JUCE cuando algo
la toca, y con 41 invocaciones eso se paga 41 veces. Medido en esta máquina, un
target solo tarda 20 s en frío (por `juce_core_CompilationTime.cpp`, que se
regenera siempre) y 1 s en caliente; los 41 juntos, en caliente, son del orden de
dos minutos, y ese tiempo es de **cargar 41 proyectos de MSBuild**, no de
compilar: con la caché al día no se compila ni un fichero.

Esto no es un detalle: durante días se leyeron cuatro rojos que eran binarios del
29/09, y un test que se daba por bueno y no lo estaba. Por eso el aviso es
**fuerte**, y por eso **no se calla con `--no-build`**: con `--no-build` no se ha
compilado nada, así que los 39 nativos van con el binario de la fecha que tengan,
y el encabezado pasa a ser `NO SE HA CONSTRUIDO NADA`.

Si un rojo sale de la lista de rancios, recompila ese target y vuelve a mirar
antes de tocar código:

```bash
cmake --build build-reference --config Release --target NOMBRE
```

## Lo que hoy está rojo, y por qué

**Nada. La batería está entera en verde: 53 de 53**, y la lista de conocidos
(`Scripts/verify_all_known.json`) tiene `entradas` vacío.

Hubo tres, y están arreglados. Lo que interesa es **por qué estaban**, porque
son dos fallos encimados y el segundo estaba tapado por el primero:

**Los `.js` de `node_modules` que no se abrían.** Eran 199 ficheros que daban
`EPERM` (error 5 de Win32, `ACCESS_DENIED`), y rompían a la vez los tres
`WebUi*E2e` y los dos `vitest` de los pasos 3 y 4.

Estaba escrito que era el antivirus, y **no lo era**: `Get-MpThreatDetection` y
`Get-MpThreat` salían vacíos, el ACL del directorio era idéntico al de un paquete
que se leía bien, y en la misma carpeta se podía crear y leer un fichero nuevo. El
bloqueo estaba en el inodo, no en la ruta: los mismos inodos, bloqueados a la vez
en el store de pnpm y en los tres repos que lo comparten por hardlink. La causa
era el store `v10`, de marzo, que ya no deja leer; el `v11` se lee entero. Todo
el diagnóstico y el rodeo están en
[`COMO-ARREGLAR-EL-BUILD.md`](COMO-ARREGLAR-EL-BUILD.md).

**Y el navegador de Playwright, que nadie sabía que faltaba.** Con los `.js`
arreglados, los tres tests seguían en rojo y el motivo escrito ya no tenía nada
que ver: no había **ningún** navegador instalado (`Executable doesn't exist at
...ms-playwright/chromium_headless_shell-1243/...`).

Eso es lo que hace dangerous un motivo de rojo conocido: es una hipótesis sobre
por qué falla, y si está equivocada **tapa la causa real**. El arreglo hecho a
partir de la hipótesis equivocada no quita el rojo, y entonces parece que no tiene
arranjo. Por eso los motivos se comprueban antes de apuntarlos, y por eso los
pasos 3 y 4 (que no son tests sino pasos enteros) no tienen lista: un rojo ahí
pide decisión aunque el motivo esté escrito.

Hubo ocho, y aquí está el resto de la historia porque es el ejemplo de para qué
existe el aviso de binarios rancios: cinco del motor (`PresetMigrationParity`,
`ParameterDescriptor`, `StatePersistence`, `ParameterBridge`,
`ParameterRandomizer`). Cuatro estaban **bien**: sus `.exe` eran del 29/09, y al
recompilarlos pasaron sin tocar una línea. Uno era un bug de verdad
(`ParameterRandomizer`: RANDOMIZE movía `fxChorusMix` y `fxReverbMix`, que no
están cableados a nada). `PresetMigrationParity` con rc=139 no era un SIGSEGV: era
el mismo binario rancio.

## Lo que este verify NO hace

- **No juzga el build.** Sale en rojo por un fallo que no es tuyo, y el código
  de salida no lo propaga al `check`, que solo compara los dos scripts.
- **No compara tiempos.** Dos corridas seguidas dan tiempos distintos, y eso
  haría fallar el check por el motivo equivocado. Solo mira si un test fue
  lento, no cuánto.
- **No compila «todo»**: solo los 39 tests nativos del paso 1 y los 2 programas de
  apoyo. El plugin y los targets de WASM no son de esta verificación. Todos los
  tests nativos se compilan, así que ninguno sale con el binario de la pasada
  anterior; lo que no se compila es lo que no es un test.
