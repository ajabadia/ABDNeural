# Cómo arreglar el build en esta máquina

Este repo estuvo **días sin poder compilar**, y no por el código: por tres
rutas del `CMakeCache.txt` que apuntan a cosas que ya no están en este
ordenador. Aquí está lo que había que cambiar y cómo se comprueba.

## Los tres atranques del build, en orden

El error que sale es siempre el primero que falta, así que se veins en
cadena. Los tres se arreglan a la vez, que si no se corrigen uno a uno se
tarda tres recargas.

**1. La instancia de Visual Studio.** El cache pedía
`C:/Program Files/Microsoft Visual Studio/18/Community`, que no existe. Lo
instalado es el BuildTools de 18, en otro sitio:

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
"C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" \
  -products '*' -format value -property installationPath
# -> C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
```

**2. JUCE.** El proyecto lo busca por `JUCE_PATH`, y si no, por `C:/JUCE`
(`CMakeLists.txt`, bloque *FIND OR INCLUDE JUCE*). `C:/JUCE` no existe; el
checkout real está en `D:/desarrollos/JUCE`. Ojo con `JUCE_DIR`: **no** es lo
mismo. `find_package(JUCE CONFIG)` busca un `JUCEConfig.cmake` que este
checkout no trae, porque un checkout no es un paquete instalado. Poner
`JUCE_DIR` a la raíz de JUCE no arregla nada y encima cambia el error a
"Could not find a package configuration file", que no dice que el problema es
la ruta.

**3. WebView2.** Falta el paquete NuGet `Microsoft.Web.WebView2`, y sin él
JUCE aborta la configuración en `juce_add_plugin` (el `NEEDS_WEBVIEW2 TRUE`
de la línea 245). No está en la caché de NuGet de esta máquina, así que hay
que traerlo. JUCE busca `build/native/include/WebView2.h` y
`build/native/x64/WebView2LoaderStatic.lib` dentro del paquete.

Y aquí está la trampa, que es lo que hace que esta ruta no se entienda a la
primera: **la variable que hay que poner NO es
`JUCE_WEBVIEW2_PACKAGE_LOCATION`**. El `CMakeLists.txt` de JUCE la declara con
un `option(...)`, así que en el `CMakeCache` queda como
`JUCE_WEBVIEW2_PACKAGE_LOCATION:BOOL`. Un `-D` con una ruta dentro se guarda
igual como BOOL, y el `FindWebView2.cmake` la lee con un `if(...)` que compara
el contenido... con un BOOL, `D:/tmp/wv2/pkg` no es verdadero ni falso: no es
un 0 ni un 1, así que la rama no se toma y la búsqueda sigue su curso. Medido
en el `CMakeCache` de este repositorio:

```
JUCE_WEBVIEW2_PACKAGE_LOCATION:BOOL=D:/tmp/wv2/pkg     <- guardada, ignorada
WebView2_root_dir:PATH=D:/tmp/wv2/pkg                  <- esta si se lee
```

La que funciona es **`WebView2_root_dir`**, que es la variable estándar del
`find_package` y la que el propio `FindWebView2.cmake` documenta como
"override the default directory where our FindWebView2 script is looking".
Con ella, el include del plugin pasa a ser
`D:/tmp/wv2/pkg/build/native/include` y el plugin compila.

## El comando, entero

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
cd /d/desarrollos/ABDSynths/ABDNeural
cmake -S . -B build-reference \
  -G "Visual Studio 18 2026" \
  -DCMAKE_GENERATOR_INSTANCE="C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools" \
  -DJUCE_PATH=D:/desarrollos/JUCE \
  -DWebView2_root_dir=D:/tmp/wv2/pkg
```

Cuatro cosas de este comando que no son obvias:

- **`CMAKE_GENERATOR_INSTANCE` va como `-D`, no como `-G`.** El generador se
  resuelve antes de leer ninguna `-D`, así que poner el path dentro de `-G`
  no funciona y el error sale igual.
- **Sin `-A x64`.** El árbol se	configuró con `CMAKE_GENERATOR_PLATFORM` vacía,
  y CMake se niega a cambiarlo: *"generator platform: x64 does not match the
  platform used previously"*. Hay que dejarlo como estaba.
- **`WebView2_root_dir`, y no `JUCE_WEBVIEW2_PACKAGE_LOCATION`.** La segunda
  existe, la documenta JUCE, y no funciona: el `option()` de JUCE la deja en
  el caché como BOOL y una ruta no es un booleano. La primera es la que lee el
  `find_package`. El síntoma de equivocarse es que el `CMakeCache` muestra la
  ruta puesta y el build sigue pidiendo el paquete, que es desconcertante
  porque el caché dice que está.
- **`CMAKE_GENERATOR_INSTANCE` en el `-D` sí reescribe el cache**, y con él la
  ruta de JUCE y la de WebView2, así que las tres cosas quedan guardadas y
  los builds siguientes ya no necesitan el comando entero.

## Y el WebView2 de dónde sale

```bash
mkdir -p /d/tmp/wv2 && cd /d/tmp/wv2
curl -sSL -o wv2.nupkg \
  "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3485.44"
unzip -q -o wv2.nupkg -d pkg
```

La versión `1.0.3485.44` es la que JUCE está pidiendo en su propio mensaje de
error, y no hay que buscarla en ningún `CMakeLists`. **No va en el repo**: son
9 MB, y la ruta de `/d/tmp` es de esta máquina. En otra, o se descarga otra
vez o se deja que JUCE lo pida con su mensaje.

## Cómo se comprueba que ha quedado bien

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
# 1. Configurar: tiene que salir "Build files have been written to"
cmake --build build-reference --config Release --target NEURONiK_FxExport
# 2. Los seis targets del paso 1 del verify, que son los que importan
bash Scripts/verify_all_check.sh ; echo "rc=$?"
```

El paso 1 del verify tiene que dar los seis `PASA`. Antes daba cinco `ROJO` de
`build` que **no eran fallos de compilación**: eran el generador inexistente, y
cada target caído contaba como un fallo. El `check` acaba en 0 y los dos
gemelos dicen lo mismo.

Si el `check` sale en 0 pero en medio pone `INTERMITENTE`, no es que el build
esté mal: es que algún test (los `WebUi*E2e` de Playwright, casi siempre) ha
salido rojo en una de las dos pasadas y verde en la otra. Salen los nombres
justo encima. Si se sospecha que sea de verdad, `--estricto` lo vuelve a juzgar
como divergencia y sale en 1. Está en
[VERIFICAR.md](VERIFICAR.md#los-tres-veredictos-del-check-de-gemelos).

Con el build arreglado, la batería corre en serio: **53 tests, 8 rojos**. Los
cinco conocidos siguen rojos (y ahora con el motivo verificado sobre el
binario, no supuesto), y los otros tres son los `WebUi*E2e` de Playwright.

## Cuarto atranco, este del 2026-10-01: los `.js` de `node_modules` que no se abren

Este se ha diagnosticado aqui, y **no es el antivirus**, aunque durante dias se
escribo que si. La distincion importa, porque cada causa tiene su rodeo y solo
uno de los dos existe.

### Que falla, exactamente

199 `.js` de `WebUI/node_modules` dan `EPERM` al abrirlos. Con `CreateFileW` de
Win32, error **5**, `ERROR_ACCESS_DENIED`, y cinco intentos seguidos dan lo
mismo, asi que no es un bloqueo de momento. Entre los paquetes afectados estan
`@playwright/test` (los tres `.js` de su `cli.js`, `index.js` y `reporter.js`),
`cross-spawn`, `acorn`, `jsdom` y 41 mas.

Los ficheros que bloquean el paso 3, el paso 4 y los tres `WebUi*E2e` de ctest,
todos con el mismo error:

```
WebUI/node_modules/@playwright/test/cli.js
ABDSharedAssets/node_modules/vitest/...
node_modules/.pnpm/<paquete>/node_modules/<paquete>/*.js
```

### Por que NO es el antivirus

- `Get-MpThreatDetection` y `Get-MpThreat` salen **vacios**: no hay ninguna
  deteccion que atribuirle. Un antivirus que bloquea sin detectar deja rastro en
  los dos.
- El ACL del directorio es **identico** al de un paquete que se lee bien
  (`BUILTIN\Administradores:(I)(F)`, `Usuarios:(I)(RX)`, y asi). No hay un
  `Deny` en ninguna parte.
- En la **misma carpeta** del fichero bloqueado se puede **crear y leer** un
  fichero nuevo. El bloqueo no es del directorio.
- `cipher /c` dice `U` en los dos: no es EFS.

### De donde viene de verdad

El bloqueo esta en el **inodo**, no en la ruta ni en el nombre. El mismo inodo
esta bloqueado a la vez en tres sitios:

```
WebUI/node_modules/.pnpm/cross-spawn@7.0.6/.../index.js
ABDSharedAssets/node_modules/.pnpm/cross-spawn@7.0.6/.../index.js
ABDSynths/node_modules/.pnpm/cross-spawn@7.0.6/.../index.js
D:\.pnpm-store\v10\files\de\dd49c1eb...   <- el store, la copia unica
```

Que sean el mismo inodo es lo normal: pnpm hace **hardlinks** desde el store, y un
hardlink son el mismo fichero con cuatro nombres. Hacer un hardlink **nuevo** a
uno de esos ficheros tambien falla, lo que confirma que la bloqueo esta en el
contenido y no en la entrada del directorio.

Y el store es el culpable, porque hay dos y uno esta bien:

| store | de quien | estado |
| --- | --- | --- |
| `D:\.pnpm-store\v10` | `pnpm@10.25.0` (el `packageManager` del `package.json`) | **bloqueado**, 190 de 192 inodos |
| `D:\.pnpm-store\v11` | `pnpm@12.8.1` (el instalado) | se lee entero |

O sea que `node_modules` se instalo con pnpm 10 contra el store v10, ese store
quedo en un estado que el sistema ya no deja leer, y desde entonces todos los
`node_modules` de la casa (10 repos, porque comparten store) tienen los mismos
199 ficheros muertos. `pnpm install --frozen-lockfile` tampoco arregla nada:
falla con `ERR_PNPM_PACKAGE_MANAGER_REMOVE_MODULES_DIR`, porque no puede borrar
el `.pnpm` viejo.

### El rodeo

Ni `icacls /reset` ni mas exclusiones: no es un permiso. Lo que hay que hacer es
**no usar el store v10**. En `WebUI` se puede porque el `.pnpm` viejo se puede
ignorar en vez de arreglarlo, con `node-linker=hoisted`:

```bash
cd /d/desarrollos/ABDSynths/ABDNeural/WebUI
pnpm install \
  --store-dir 'D:\tmp\pnpm-store-ok' \
  --config.node-linker=hoisted \
  --ignore-scripts \
  --no-frozen-lockfile
```

Con `hoisted`, pnpm escribe los paquetes en `node_modules/<nombre>/` en vez de en
`node_modules/.pnpm/`. Esos sitios estan vacios, y vacio quiere decir que no hay
nada bloqueado: se puede escribir. El `.pnpm` viejo se queda ahi, sin leer y sin
estorbar, porque con `hoisted` nadie lo mira.

Lo medido: rc 0, **5102 `.js` y 0 ilegibles**, y los enlaces a los paquetes del
workspace (`@abdsynths/shared` y `@abdsynths/midi-keyb`) **siguen funcionando**,
porque son enlaces de simbolo a los repos y no al store. Con eso:

- paso 3 (WebUI): **619 tests, 38 ficheros, 0 fallos**
- paso 4 (ABDSharedAssets): **1716 tests, 40 ficheros, 0 fallos**

### Y el quinto atranco, que estaba tapado por el cuarto

Con los `.js` arreglados, los tres `WebUi*E2e` **seguian en rojo**, pero por
otra cosa: **no habia ningun navegador de Playwright instalado**.

```
Error: browserType.launch: Executable doesn't exist at
C:\Users\ajaba\AppData\Local\ms-playwright\chromium_headless_shell-1243\...
Looks like Playwright was just installed or updated.
```

Esto es lo peligroso del asunto: el motivo que estaba escrito en la lista de
conocidos (`el .js de Playwright no se puede leer`) era verdad en su momento, y
tapaba esta segunda causa. Un motivo de un rojo conocido es una hipotesis sobre
por que falla; si la hipotesis esta equivocada y el arreglo se hace a partir de
ella, el rojo no se quita nunca y parece que no tiene arreglo.

El arreglo es una linea:

```bash
cd /d/desarrollos/ABDSynths/ABDNeural/WebUI
npx playwright install chromium
```

Con el, la bateria entera pasa: **53 de 53**.

### Como se comprueba

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
cd /d/desarrollos/ABDSynths/ABDNeural

# Que no queda ni un .js sin leer (tiene que decir 0 ilegibles)
node -e "
const fs=require('fs'),path=require('path');
let t=0,m=0;
(function b(d){let e;try{e=fs.readdirSync(d,{withFileTypes:true})}catch(x){return}
for(const x of e){const p=path.join(d,x.name);
if(x.isDirectory()){b(p);continue}
if(!x.name.endsWith('.js'))continue
t++;try{const fd=fs.openSync(p,'r');fs.closeSync(fd)}catch(q){m++}}})('node_modules/.pnpm');
console.log('js:',t,'ilegibles:',m);"

# Y que los pasos 3 y 4 arrancan
bash Scripts/verify_all.sh --only=3 --only=4
```

### Si algum dia hay que dar marcha atras

El arbol viejo **no se puede borrar** (los mismos ficheros dan error 5 al `rm`),
pero el **directorio** si se puede renombrar, que es lo que hace falta. Asi que
la salida esta siempre:

```bash
cd /d/desarrollos/ABDSynths/ABDNeural/WebUI
mv node_modules node_modules_roto_v10
```

Eso devuelve las 199 descargas a su sitio sin tocar nada del store.

## Los dos avisos que se pisaban, y el codigo 3

No es de este atranco, pero sale de aqui: el aviso de binarios rancios y el de
los conocidos que ya no fallan se contradecian, y el segundo era el que mandaba.

Con `--no-build`, el mismo test salia dos veces en la misma pantalla, separados
por el `ctest` entero:

```
  NO SE HA CONSTRUIDO NADA  1 test(s) se ejecutan sin haber sido compilados nunca en esta pasada:
        NEURONiK_EjemploTest
  ...
  ARREGLO  NEURONiK_EjemploTest
            quita "NEURONiK_EjemploTest" del indice y borra known/NEURONiK_EjemploTest.txt
```

El primero dice «de este test no se sabe nada». El segundo dice «esta arreglado,
borra su motivo», y eso es un consejo destructivo nacido de no saber que se ha
ejecutado. Con la lista de conocidos de hace unos dias y `--no-build` la trampa
estaba armada.

Ahora el aviso de rancios deja una lista con los que **no** se han medido, y
`arreglados` la lee: los de esa lista salen `SIN MEDIR`, sin pedir borrar nada.
Y si la lista no ha llegado, no dice `ARREGLO` de ninguno.

Cada uno dice ademas **su** motivo, y aqui se separaron tres casos que antes
salian con el mismo texto: binario rancio (su `.exe` es de antes), `SIN
BINARIO` (no tiene `.exe` y no se ha ejecutado nunca) y `--no-build` (no se ha
compilado nada en esta pasada). El segundo es el que importa: recompilando su
target el rojo tampoco se va, porque lo que hay que arreglar es por que no se
construye. Decirle «su `.exe` no se ha compilado» a un test que no tiene `.exe`
manda al sitio equivocado con seguridad. La seccion 14 del selftest los imprime
juntos en pantalla.

Ademas el verify sale con **3** si hay no medidos y ningun rojo, y con una linea
en el informe que los cuenta. Antes salia con 0. Lo cubren las secciones 12, 13
y 14 de `Scripts/selftest_verify_all_node.js`: la lista que deja `rancios`, el
choque montado, la pantalla con una entrada real, y los motivos distintos.

## El sexto atranco, este del 2026-10-02: los enlaces del workspace que no resuelven

Este es el que mas caro salio, y no por lo que rompe: por **cuanto** rompe.

### Que falla, exactamente

El paso 4 de `build.bat` (exportar la WebUI) se para con esto:

```
[vite]: Rollup failed to resolve import "@abdsynths/shared/components/wheel.js" from
  "D:/desarrollos/ABDSynths/ABDSharedCode/MidiKeyboard/src/keyboard.js".
```

Es decir: nueve minutos de compilación para un error de un enlace de cuatro
lineas, en un `node_modules` que no es el de la pagina.

### De donde viene

Los paquetes del workspace (`@abdsynths/shared` y `@abdsynths/midi-keyb`) no se
bajan: **son enlaces a los repos de al lado**. Los escribe `pnpm`, y los escribe
con la ruta tal cual la ve el shell, que en esta maquina es POSIX. Medido:

```
WebUI/node_modules/@abdsynths/shared -> /d/tmp/ws12/ABDSharedAssets
```

`/d/tmp/ws12` **no existe** (es un arbol de pnpm fantasma de otra sesion), y
aunque existiera Node leeria `/d/...` como `D:\d\...`. El enlace esta ahi, la
carpeta parece, y no resuelve.

Lo que lo hace confuso es que **el mismo paquete se resuelve bien y mal segun
quien mire primero**. La pagina entra por `MidiKeyboard/src`, y ahi el enlace
roto gana a la junction buena de `WebUI/node_modules`. Un paquete que funcionaba
desde la pagina se dejo de resolver en el mismo build, sin que nadie tocase una
linea de codigo: cambio quien gaina el orden de resolucion.

### El rodeo: junctions, y por que junction

`mklink /D` (symlink) **pide privilegios de administrador y falla** en esta
maquina (medido: "Carece de privilegios suficientes"). `mklink /J` (junction) no
los pide y es lo que Windows usa de nativo para esto.

Eso es un `Scripts/junctions-workspace.bat`, que recorre los seis repos del
workspace y **se ejecuta después de cada `pnpm install`**, que es lo que los
vuelve a escribir como symlink. `build.bat` ya lo llama solo antes del paso 3, y
corta si sale con un código distinto de 0.

Es idempotente: si un enlace ya resuelve, no lo toca. Solo crea los enlaces que
el `package.json` de cada repo **declara como dependencia**, y esos son (medido
el 2026-10-02):

| repo | `@abdsynths/shared` | `@abdsynths/midi-keyb` |
|---|---|---|
| `ABDNeural/WebUI` | si | si |
| `ABDSharedAssets` | no | no |
| `ABDSharedCode/MidiKeyboard` | si | no |
| `ABDMS2000` | si | si |
| `ABDCZ101` | si | si |
| `ABDEep` | si | si |

Que el filtro se base en el `package.json` y no en una lista de enlaces a mano no
es cosmético. Sin el, el script se enlazaba a sí mismo: `ABDSharedAssets` (que
**es** `@abdsynths/shared`) se ponía un enlace a sí mismo, y `MidiKeyboard` (que
**es** `@abdsynths/midi-keyb`) otro. Un repo enlazado a sí mismo a través de su
`node_modules` es un bucle de resolución de módulos esperando a que algo lo
recorra. Los dos casos se notaron al medir, y se quitaron a mano.

El detalle fino está en cómo se busca la dependencia: el nombre se busca como
clave, o sea `"@abdsynths/shared":` **con los dos puntos de detrás**. Sin ellos
`findstr` encuentra también el campo `"name"` del propio repo, que es el mismo
texto, y el filtro no filtra nada.

```bash
cd /d/desarrollos/ABDSynths/ABDNeural
cmd //c "Scripts\junctions-workspace.bat"
```

```
=== Enlaces del workspace como junctions ===

  [REPO]   D:\desarrollos\ABDSynths\ABDNeural\WebUI
  [REPO]   D:\desarrollos\ABDSynths\ABDSharedAssets
  [REPO]   D:\desarrollos\ABDSynths\ABDSharedCode\MidiKeyboard
  [REPO]   D:\desarrollos\ABDSynths\ABDMS2000
  [REPO]   D:\desarrollos\ABDSynths\ABDCZ101
  [REPO]   D:\desarrollos\ABDSynths\ABDEep

RESULTADO: OK, 6 repos revisados y todos los enlaces del
         workspace resuelven.
```

Cuando todo está bien **no imprime nada por repo**: seis líneas de `[REPO]` y el
resultado. El ruido se paga solo, porque los que se quedan con avisos son los
que hay que mirar.

Sale con **1** si algo se queda sin resolver, y avisa de cuál. Los casos que se
midieron: `[NUEVO]` (no estaba, se crea), `[ROTO]` (existe pero no resuelve) y
`[LISTO]` (creado). En el roto borra el enlace con `rmdir` —que quita junctions
y symlinks sin pedir nada— y solo si eso falla intenta un `ren`; si tampoco,
avisa de que la causa probable es el store bloqueado del cuarto atranco.

Los repos que aun no tienen `node_modules` se saltan en silencio: todavía no se
ha instalado, y eso no es un problema. Lo que **no** se salta en silencio es un
`node_modules` que existe pero no es una carpeta de módulos; ese caso es el
siguiente.

### Y ahora también en CI, desde el 2026-10-03

El workflow (`.github/workflows/webui-visual-qa.yml`) llama al script en **los tres
jobs** (`visual-regression`, `js-tests` y `native-tests`), justo después de su
`pnpm install` y antes de cualquier paso de la WebUI:

```yaml
- name: Fix the workspace links as junctions (junctions-workspace.bat)
  shell: cmd
  working-directory: ABDNeural
  run: |
    set NEURONIK_SUITE=%GITHUB_WORKSPACE%
    set NEURONIK_CI=1
    Scripts\junctions-workspace.bat
```

Tres cosas que hicieron falta, y las tres se midieron probando el script contra un
layout de CI antes de meterlo:

**1. `NEURONIK_SUITE`**, porque el layout de CI no es el de local. En local la suite
está en `D:\desarrollos\ABDSynths` y ese valor es el que trae el script por
defecto. En el runner los hermanos son subdirectorios sueltos de
`$GITHUB_WORKSPACE`. MEDIDO sin la variable: el script resolvía `%SUITE%` a la ruta
local, no encontraba a los hermanos del runner, **y aun así imprimía
`RESULTADO: OK, 6 repos revisados`**. Un verde que no ha mirado nada.

**2. `NEURONIK_CI=1`**, porque CI clona solo los hermanos que cada job necesita. En
`visual-regression` no están `ABDMS2000`, `ABDCZ101` ni `ABDEep`. Con el modo
estricto el paso habría fallado **siempre**, con un rojo de "falta el repo" en un
job donde ese repo no hace falta: un rojo siempre es un job caído. En modo CI los
que faltan se **saltan y se cuentan** (`3 repos saltados por NEURONIK_CI`), que es
distinto de no aparecer. Los que **están** se comprueban igual que en local, que es
lo que importa: el enlace roto es del repo que está.

**3. Un repo ausente ahora es un ROJO en el modo de por defecto.** Eso se cambio
justo al medir lo de arriba: sin ello, un layout equivocado se pasaba en verde.

**Lo que el paso hace de verdad**, medido sobre un enlace roto hecho a propósito
(`mklink /J` a un destino inexistente, que es como queda un enlace cuando la ruta
que se escribió no era la de Windows):

```
  ANTES: NO_RESUELVE
  [ROTO]    shared en ...\node_modules no resuelve.
  [LISTO]   shared en ...\node_modules -> ...\ABDSharedAssets
  DESPUES: RESUELVE
  RC_EN_MODO_CI=0
```

Y el modo por defecto, en el mismo layout equivocado, da `RC=1` diciendo que
`ABDCZ101` y `ABDEep` no están. Las dos cosas que se querían: en CI arregla, y si
la máquina no es la que cree, lo dice.

### La otra mitad: el `node_modules` entero era un symlink

El mismo día, en `ABDSharedAssets`, el symlink POSIX no había caído donde tocaba
el paquete: había caído sobre la **carpeta `node_modules` completa**, y apuntaba
a `node_modules_ok/node_modules`, o sea una **copia del proyecto entero**. Todo
parecia en su sitio y no había ni vitest dentro, así que el paso 4 del
`verify_all` caía con un rojo que no señalaba ni al enlace ni al store.

Un enlace de esa clase **no lo arregla el script**: arreglarlo es reinstalar. Lo
que si se puede, y es lo que hace, es **avisar antes de que cueste**. El testigo
no es buscar un `package.json` dentro de `node_modules` —no lo tiene ninguna
instalación buena; pnpm deja ahí `.pnpm`, `.modules.yaml` y `.bin`—, sino si la
carpeta **en sí** es un punto de reanálisis:

```bash
fsutil reparsepoint query "D:\ruta\node_modules"
# 0  -> es un enlace (junction o symlink, los dos)
# error 4390 -> es una carpeta de verdad
```

Medido el 2026-10-02 sobre los seis repos: los seis dan 4390, o sea ninguno
tiene el `node_modules` enlazado. El aviso **cuenta como fallo a propósito**, y
por eso `build.bat` corta: cuando se llega al paso 4 ya se han gastado nueve
minutos, y un corte aquí con el motivo a la vista sale más barato que un rojo sin
pistas al final. Y el script **no** intenta meter junctions dentro de ese
`node_modules`: se harian en un sitio que no es el que parece, y el fallo
seguiente ("la junction no ha quedado bien") no señalaría la causa.

El rodeo a mano, si aparece:

```bash
# 1. Quitar el enlace. rmdir quita junctions y symlinks sin pedir nada.
rmdir /q "D:\desarrollos\ABDSynths\ABDSharedAssets\node_modules"

# 2. Reinstalar SIN tocar el store de la raiz, que esta bloqueado (cuarto
#    atranco), y con el store propio que ya se sabe que funciona.
cd /d/desarrollos/ABDSynths/ABDSharedAssets
pnpm install --ignore-workspace \
  --store-dir "D:\tmp\pnpm-store-ok" \
  --config.node-linker=hoisted

# 3. Volver a poner los enlaces y comprobar que el paso 4 pasa.
cd /d/desarrollos/ABDSynths/ABDNeural
cmd //c "Scripts\junctions-workspace.bat"
```

### Que se mide al quitarlo

El rodeo de junctions tapa un sintoma que **tambien aparece en el servidor de
desarrollo**, y ese no lo arregla:

```
Pre-transform error: Failed to resolve import "@abdsynths/shared/components/wheel.js"
  from ".../ABDSharedCode/MidiKeyboard/src/keyboard.js"
```

Con el enlace roto, el primer GET a `/needle-probe/` tardaba **20 s** (despues
9 s con las junctions), y los presupuestos de `WebUI/playwright.config.js` son
**15 s** de navegacion y **120 s** de arranque de servidor. Por ahi se caian
`NEURONiK_WebUiLocalModeE2e` y `NEURONiK_WebUiVisualRegression`, con un
"Timed out waiting 120000ms from config.webServer" que no decia nada de
enlaces. Medido el 2026-10-02: los dos en verde con las junctions puestas.

### Como se comprueba

```bash
cd /d/desarrollos/ABDSynths/ABDNeural

# Que los seis repos están revisados y sin avisos (tiene que salir 6)
cmd //c "Scripts\junctions-workspace.bat" | grep -c "^  \[REPO\]"

# Que ningun node_modules es un enlace (no tiene que salir nada)
for r in WebUI ../ABDSharedAssets ../ABDSharedCode/MidiKeyboard \
         ../ABDMS2000 ../ABDCZ101 ../ABDEep; do
  fsutil reparsepoint query "$r/node_modules" 2>&1 | grep -c "0xa0"
done

# Y que el paso 4 del build pasa
cd WebUI && pnpm build
```

Ojo al medir el código de salida desde Git Bash: `cmd //c "algo.bat & echo
RC=%ERRORLEVEL%"` **miente**, porque `%ERRORLEVEL%` se expande al analizar la
línea, antes de que el `.bat` corra, y siempre sale 0. Para el código real hace
falta un `call` con expansión retardada.

## El intermitente del webServer del probe, este del 2026-10-03

Este es el único que **no rompia nada**: el test pasaba. Lo que hacia era pasar a
veces, y por eso costaba más que los otros.

### Que pasaba, exactamente

`NEURONiK_WebUiLocalModeE2e` tardaba **130 s** en una pasada y **56 s** en la
siguiente, con los 11 tests del spec sumando **54 s en los dos casos**. Es decir,
lo que variaba no eran los tests: eran los ~76 s de más de una pasada. El budget
del `webServer` del probe son 120 s, y un GET en frío medido a mano eran 7-9 s, que
no cuadra con ninguno de los dos números — y por ahí se empezó.

### De donde venía (medido, no supuesto)

Cuatro mediciones, cada una descartando una causa:

| Sospecha | Medido | Veredicto |
|---|---|---|
| `npx` por delante del binario | 2,65 s con `npx` contra 1,31 s directo | 1,3 s. No es el caso. |
| `vite build` del webServer 1 | 3,4-3,7 s | Descartado. |
| Transformar el árbol de `src/` | `vite:transform` de cada fichero: 0,1-20 ms | Descartado. |
| **Leer los ficheros** | `vite:load [fs]` del MISMO fichero: **1,5 s** | **Aquí está.** |

El `cat` de un `.js` de 10 KB, sin vite de por medio, tarda **88-121 ms** en esta
máquina. Es un disco lento, y vite hace 83 cargas (`load`) de ficheros de `src/`.

**Y el disco lento es solo la mitad.** La otra mitad es **CUÁNDO** se paga: sin
`warmup`, vite transforma cada módulo la primera vez que algo lo pide, y la primera
petición que llega es la del propio arranque de playwright (el `url` del
`webServer`). O sea que **el arranque espera a un GET en frío que tiene que
transformar el árbol entero de la página**. El GET de `/needle-probe/` con el
servidor recién arrancado, tres veces seguidas:

```
vuelta 1:  62238 ms
vuelta 2:  11406 ms
vuelta 3:   8958 ms
```

La primera es **7 veces** la tercera. Esa dispersión ES el intermitente: la primera
vez el disco y la cache de OS están fríos, y las siguientes ya están calientes.

### El rodeo: `server.warmup` en `WebUI/vite.config.js`

Pre-transformar los ficheros **al arrancar el servidor**, que es donde hay
presupuesto de sobra, en vez de en el primer GET, que es donde no lo hay:

```js
warmup: {
  clientFiles: ['needle-probe/**/*.js', 'src/**/*.js'],
},
```

El presupuesto de 120 s **no se toca**, y esa es la parte importante: el problema
no era el presupuesto, era que se le cobraba al primero que preguntaba, que es el
que decidía si el test pasaba. Con el warmup el trabajo se paga en el arranque y el
GET en frío sale caliente.

### Que se mide al quitarlo

El mismo presupuesto (tiempo hasta el primer 200 del probe), con y sin warmup, con
el mismo método y tres vueltas cada uno:

| | vuelta 1 | vuelta 2 | vuelta 3 |
|---|---|---|---|
| **Sin warmup** | 56,4 s | 13,2 s | 13,9 s |
| **Con warmup** | 12,3 s | 12,2 s | 12,2 s |

La primera vuelta baja de 56 s a 12 s y la **dispersión se va de 43 s a 0,09 s**.

Y el test de verdad, tres veces seguidas:

```
vuelta 1: 41 s, rc=0
vuelta 2: 43 s, rc=0
vuelta 3: 41 s, rc=0
```

contra los 130 s / 56 s de antes. Los otros dos specs que arrancan el mismo
servidor (`NEURONiK_WebUiVisualRegression` 43 s y
`NEURONiK_WebUiNeedleProbeE2e` 153 s) siguen en verde.

**Sobre el de las agujas (153 s):** es el que de verdad carga el motor WASM y
levanta la página entera con las dos vistas de ADSR, y su trabajo es otro. No se
ha tocado su presupuesto y **no está medido** que 153 s sea su valor estable; si
alguna vez se pasa de los 600 s de `TIMEOUT` del test, el sitio a mirar es este y
no el warmup.

### Como se comprueba

```bash
cd /d/desarrollos/ABDSynths/ABDNeural/build-reference

# El test que era intermitente, tres veces seguidas
for i in 1 2 3; do
  export PATH="/d/desarrollos/cmake/bin:$PATH"
  ctest -C Release -R NEURONiK_WebUiLocalModeE2e
done
```

En VERDE ctest **no imprime las líneas del webServer**, y por ahí el intermitente
no se veía: hay que correrlo con `-V` para ver cuándo arranca cada servidor.

## El séptimo atranco, este del 2026-10-03: el Standalone mudo por no declarar buses

Este no rompia el build: lo hacia **sonar mal**, que es peor porque el build
sale en verde y el rojo es de oídos.

### Que falla, exactamente

El Standalone no suena. Las agujas se quedan en 0, la nota MIDI no arranca ninguna
voz y el arco del LFO sobre Morph Z no se mueve. En el selftest del paso 9 se
manifestaba como «tres direcciones no se mueven», sin decir de donde.

### De donde viene

El processor **no declaraba buses de salida**. Sin
`BusesProperties().withOutput(...)`, `getMainBusNumOutputChannels()` vale CERO
—JUCE no inventa buses: sale de `getChannelCountOfBus`, que es null si no hay
bus— y ahí sale el mudo:

```
StandalonePluginHolder  ->  pide getMainBusNumOutputChannels() salidas = 0
AudioDeviceManager      ->  initialise (0, 0, ...) no abre NINGUN dispositivo
                          ->  no hay processBlock
                          ->  el motor no suena
```

Sin device no hay `processBlock`, y sin `processBlock` el motor no arranca. Todo
lo demás estaba en verde: 53/53 en ctest, la export de la WebUI bien, los E2E en
verde. El único síntoma era que no sonaba.

### El rodeo que se puso

`BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)`
en el inicializador del processor. Es stereo de salida y SIN entrada, que es lo
que es un synth.

### Y ahora el guard, que es lo que evita que vuelva: `NEURONiK_OutputBusDeclaredTest`

Un test que **pregunta al processor real**, no un grep del fuente, y esa
diferencia es el motivo de que sea un test y no una línea de script:

- Un `withOutput` escrito en un **comentario** haria pasar el grep con el plugin
  igual de mudo. El fallo que se caza es de comportamiento y un grep no lo ve.
- El día que el bus se declare en otro sitio (una fábrica de buses, una clase
  base, un `.h`), el grep sigue buscando en el `.cpp` y pasa **en verde con el
  bus sin declarar otra vez**. Ese es el bucle que el test corta.

El test instancia el `NEURONiKProcessor` de verdad (compila las mismas fuentes
que `StatePersistenceTest`) y comprueba cuatro cosas:

| Que mira | Por que |
|---|---|
| `getMainBusNumOutputChannels() > 0` | La pregunta que hace el host. Es el aserto que cierra el fallo. |
| Que sea **stereo**, no mono | No habría cazado este fallo, pero un bus mono donde el host espera stereo es la misma sorpresa un día más tarde. |
| Que `getBus(false, 0)` exista | El bus está declarado y el processor lo describe. |
| Que se pueda **escribir** en el, tras `prepareToPlay` | Declarar el bus y no poder escribir en el es el mismo mudo con otro nombre: el host abre el dispositivo y el `processBlock` escribe en la nada. |

No comprueba el AUDIO, y a propósito: que suene de verdad lo dice el selftest del
paso 9, que corre el arnés real contra la página. Este guard solo afirma el
**contrato de canales**, que se puede comprobar en 30 líneas y sin abrir un
dispositivo.

**La prueba negativa.** Un guard que solo se ha visto en verde no prueba nada:
puede que nunca llegue a fallar. Este se comprobó quitando el `BusesProperties`,
recompilando y ejecutándolo:

```
[FAIL] el bus principal declara salidas (ahora: 0) — con 0 el Standalone pide 0 salidas, no abre dispositivo y el plugin sale mudo
[FAIL] el bus de salida es stereo (ahora: 0) ...
  RESULT: FAIL (3)
TEST_NEGATIVO_RC=1
```

y tras restaurar el processor, `RESULT: OK` y `rc=0`. El fallo sale con el
**motivo del mudo escrito en el mensaje**, que es lo que un rojo de este tipo
necesita: no basta con que falle, tiene que decir que sin esto el plugin no abre
dispositivo.

### Que se mide al quitarlo

El rojo del paso 8 con el mensaje del bus, en vez de un Standalone que carga y no
suena. Y el propio `build.bat` compila el target en su paso 8, así que un
`BusesProperties` borrado rompe la compilación de las pruebas antes de que nadie
tenga que escuchar nada.

### Como se comprueba

```bash
cd /d/desarrollos/ABDSynths/ABDNeural

# El guard, solo (0,04 s)
ctest --test-dir build-reference -C Release -R OutputBusDeclaredTest --output-on-failure

# Y que esta en la bateria
ctest --test-dir build-reference -C Release -N | tail -2   # Total Tests: 54
```

Ojo al contar: paso de 53 a 54 tests al añadir este. Si `ctest -N` dice 53, el
`add_test` no está, o el build no se ha reconfigurado.

## Lo que sigue sin arreglarse

Nada de lo de esta pagina. Los `WebUi*E2e` y los dos vitest estan arreglados, y
la lista de conocidos (`Scripts/verify_all_known.json`) esta **vacia**: no queda
ningun rojo conocido.

**El intermitente del `webServer` del probe también está cerrado** (medido el
2026-10-03): era el `server.warmup` que faltaba en `vite.config.js`, y con el
arreglo el `LocalModeE2e` da 41/43/41 s en tres pasadas seguidas contra los 130 s
y 56 s de antes. El budget de 120 s no se toco.

Lo que queda por debajo es **el disco**: un `cat` de un `.js` de 10 KB tarda
88-121 ms en esta máquina, y vite hace 83 cargas de ficheros al arrancar el
servidor de desarrollo. El warmup quita la intermitencia (mueve el coste al
arranque y elimina la dispersión), pero no hace el disco más rápido. Si alguna
vez un E2E se acerca a su `TIMEOUT` de 600 s, la causa más probable es esto y no
el código del test.

Lo que queda es el store v10, que sigue ahi y sigue bloqueado. No molesta
mientras nadie instale con pnpm 10, y se arregla en cuanto se reinstale todo con
el pnpm del `packageManager`.

Y el Store del Standalone mudo, que ya no es pendiente pero conviene no perder de
vista: el bus de salida está declarado y `NEURONiK_OutputBusDeclaredTest` falla en
rojo si vuelve a desaparecer. La causa de fondo —JUCE no inventa buses y el
Standalone pide exactamente los que el processor declara— no se arregla, se
vigila.

Y ahora tambien los enlaces del workspace: `junctions-workspace.bat` los deja
bien, pero hay que acordarse de el **después de cada `pnpm install`**. El
`build.bat` se encarga solo (lo llama antes del paso 3 y corta si falla), así que
un build normal ya no lo necesita; lo que lo necesita es el `pnpm install` por
separado, que es justo el movimiento que rompe el paso 4.

El arreglo de verdad sería que pnpm los escribiera con `--config.symlink=false`.
Mientras no, el script es el rodeo reproducible, y para eso está: se puede
ejecutar las veces que haga falta sin consecuencias.
