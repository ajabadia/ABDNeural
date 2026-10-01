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

**`selftest_verify_all_node.js`** (el tercero) es el rápido: 74 comprobaciones
sobre la lista de conocidos y sobre lo que se rompe en silencio, sin compilar
nada. Tarda segundos. Si has tocado la lista, `Scripts/known/` o
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
solo el resumen), así que un `3` no lo hace fallar: eso es justo lo que lo hace
seguro.

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
