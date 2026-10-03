import { defineConfig } from 'vite';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));

// Raíz del repo (ABDNeural/). El servidor de desarrollo solo sirve ficheros bajo
// `fs.allow`; esta UI importa DOS cosas de fuera de su propia carpeta:
//   - el contrato generado, que vive en WebUI/generated (una sola copia, escrita
//     por NEURONiK_ParameterExport en el paso 2/9 de build.bat);
//   - los tokens/widgets de @abdsynths/shared, que es un paquete del workspace.
const repoRoot = path.resolve(root, '..');

export default defineConfig({
  // Rutas relativas: el host embebe la página y no sirve desde una raíz web.
  base: './',
  // Los assets compartidos (bender.png del wheel, etc.) y el worklet viven en
  // WebUI/public — UNA sola copia, la de esta pagina. Hasta el ticket 8.4 este
  // publicDir apuntaba a WebPilot/public (el piloto era el dueno de los assets
  // compartidos); la retirada los mudo aqui y este camino es el que los copia a
  // `dist/` (y de ahi los embebe el plugin).
  publicDir: path.resolve(root, 'public'),
  server: {
    fs: { allow: [root, repoRoot, path.resolve(repoRoot, '../ABDSharedAssets')] },

    // EL CALENTAMIENTO, y por que sin esto el webServer del probe se pasa de
    // presupuesto (medido el 2026-10-03).
    //
    // El problema no era el presupuesto, que está bien, sino CUÁNDO se paga el
    // trabajo. Sin esto, vite transforma cada módulo la PRIMERA vez que algo lo
    // pide, y la primera petición que llega es la del propio arranque de
    // playwright (`url` del webServer). Es decir: el arranque espera a un GET en
    // frío que tiene que transformar el árbol entero de la página.
    //
    // Y lo caro no es transformar: con `DEBUG=vite:transform` el `transform` de
    // cada fichero tarda 0,1-20 ms, mientras el `load [fs]` del MISMO fichero
    // tarda 1,5 s. O sea que el 98% es leer el fichero del disco, que en esta
    // máquina va lento (medido con `cat` a pelo: 88-121 ms por un .js de 10 KB).
    // Medido en el GET en frío de /needle-probe/ con el servidor recién
    // arrancado, tres veces seguidas:
    //
    //     vuelta 1:  GET /needle-probe/ = 62238 ms
    //     vuelta 2:  GET /needle-probe/ = 11406 ms
    //     vuelta 3:  GET /needle-probe/ =  8958 ms
    //
    // La primera es 7 veces la tercera, y esa dispersión ES la intermitencia del
    // NEURONiK_WebUiLocalModeE2e: el mismo test tardaba 130 s una vez y 56 s otra,
    // con los 11 tests del spec sumando 54 s en ambos casos. Lo que variaba no
    // era el test, era cuánto tardaba el GET en frío que precedía a todos.
    //
    // Con `warmup` el trabajo se paga al ARRANCAR el servidor, que es donde hay
    // presupuesto de sobra (120 s), y el GET en frío que espera playwright sale
    // ya caliente. El arranque sigue tardando lo mismo: lo que cambia es que no
    // se lo cobra al primero que pregunta, que es el que decide si el test pasa
    // o se queda sin tiempo.
    //
    // El glob es el de la página de la aguja y el de la app: son las dos que se
    // sirven en el E2E, y las dos arrastran el mismo árbol de `src/`.
    warmup: {
      clientFiles: ['needle-probe/**/*.js', 'src/**/*.js'],
    },
  },
  build: {
    // Salida propia: `WebUI/dist` es lo que embebe el PLUGIN (juce_add_binary_data),
    // lo que sirve la bancada WebView2 y lo que sirve start.bat. Ya no hay una segunda
    // exportacion a la que apuntar: el piloto se retiro (ticket 8.4).
    outDir: 'dist',
    emptyOutDir: true,
    assetsDir: 'assets',
  },
});
