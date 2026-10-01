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

## Lo que sigue sin arreglarse

Nada de lo de esta pagina. Los `WebUi*E2e` y los dos vitest estan arreglados, y
la lista de conocidos (`Scripts/verify_all_known.json`) esta **vacia**: no queda
ningun rojo conocido.

Lo que queda es el store v10, que sigue ahi y sigue bloqueado. No molesta
mientras nadie instale con pnpm 10, y seArrange en cuanto se reinstale todo con
el pnpm del `packageManager`.
