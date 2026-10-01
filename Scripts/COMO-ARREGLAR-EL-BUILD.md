# Cómo arreglar el build en esta máquina

Este repo estuvo **días sin poder compilar**, y no por el código: por tres
rutas del `CMakeCache.txt` que apuntan a cosas que ya no están en este
ordenador. Aquí está lo que había que cambiar y cómo se comprueba.

## Los tres atranques, en orden

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
`build/native/x64/WebView2LoaderStatic.lib` dentro del paquete, y JUCE da la
variable `JUCE_WEBVIEW2_PACKAGE_LOCATION` para decirle dónde está.

## El comando, entero

```bash
export PATH="/d/desarrollos/cmake/bin:$PATH"
cd /d/desarrollos/ABDSynths/ABDNeural
cmake -S . -B build-reference \
  -G "Visual Studio 18 2026" \
  -DCMAKE_GENERATOR_INSTANCE="C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools" \
  -DJUCE_PATH=D:/desarrollos/JUCE \
  -DJUCE_WEBVIEW2_PACKAGE_LOCATION=D:/tmp/wv2/pkg
```

Tres cosas de este comando que no son obvias:

- **`CMAKE_GENERATOR_INSTANCE` va como `-D`, no como `-G`.** El generador se
  resuelve antes de leer ninguna `-D`, así que poner el path dentro de `-G`
  no funciona y el error sale igual.
- **Sin `-A x64`.** El árbol se	configuró con `CMAKE_GENERATOR_PLATFORM` vacía,
  y CMake se niega a cambiarlo: *"generator platform: x64 does not match the
  platform used previously"*. Hay que dejarlo como estaba.
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
9 MB, y el `JUCE_WEBVIEW2_PACKAGE_LOCATION` de `/d/tmp` es de esta máquina. En
otra, o se descarga otra vez o se deja que JUCE lo pida con su mensaje.

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

## Lo que sigue sin arreglarse

- Los `WebUi*E2e` siguen en rojo. No es el motor: es Playwright sin navegador
  o sin permiso (`EPERM` en el log).
- `ABDSharedAssets` sale en rojo por un error de sintaxis en
  `components/skins/index.js`, que es del otro hilo.
- La lista de conocidos (`Scripts/known/`) sigue en pie: si un rojo conocido se
  arregla, el propio script avisa con `ARREGLO` y dice los dos pasos.
