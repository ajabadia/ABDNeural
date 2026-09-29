#!/usr/bin/env python3
"""
ABDNeural — auditor de referencias de HANDOFF.md contra el codigo REAL.

HANDOFF.md son ~8.100 lineas written a lo largo de nueve dias. Cada entrada cita
ficheros, funciones y constantes del momento en que se escribio. Cuando el codigo
se mueve, esas citas CADUCAN en silencio: el documento sigue leyendo "esto vive en
ui/panel.js" cuando el fichero ya no existe, y el lector pierde el tiempo (o peor,
edita donde no hay nada que editar).

Este script extrae las referencias del documento y las contrasta con el arbol:

  - RUTAS  (`WebUI/src/app.js`, `Source/DSP/Engine.h`, `build.bat`): se comprueba
           que el fichero exista. Sin extension se acepta que sea un DIRECTORIO,
           porque asi se citan las carpetas (`ui/`, `Source/ModelMaker/`).
  - SIMBOLOS (`onWorkletVoices`, `destinationIndexFor`, `IvyPad::destroy`): se
           busca el IDENTIFICADOR en el corpus de codigo. Es busqueda de texto,
           no de simbolo: basta con que el nombre exista en algun sitio, que es lo
           que un lector puede resolver con un grep.
  - EXTERNOS (`@abdsynths/shared`): se buscan tambien en node_modules, porque son
           de otro repo (ABDSharedAssets) pero existen de verdad. Se reportan
           aparte para no mezclarlos con el codigo nuestro.

Que NO hace, a proposito: no juzga si una afirmacion es VERDAD (si el codito hace
lo que el documento dice). Solo comprueba que lo citado exista. Un documento que
cita un fichero real puede seguir siendo falso; esto solo atrapa la forma mas
barata de desactualizacion, la que no se nota al leer.

Uso:
    python Scripts/handoff_refs_audit.py            # informe, sale 0 siempre
    python Scripts/handoff_refs_audit.py --fail     # sale 1 si hay referencias rotas
    python Scripts/handoff_refs_audit.py --paths-only
"""

import io
import os
import re
import sys
import unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HANDOFF = os.path.join(ROOT, 'HANDOFF.md')

# Arboles donde vive el codigo. `build*` y `node_modules` se tratan aparte.
CODE_DIRS = ['Source', 'WebUI/src', 'WebUI/tests', 'WebUI/e2e', 'WebUI/public',
             'WebUI/scripts', 'Tests', 'Scripts', 'Assets', '.github',
             'DOCS', 'DOC', 'DocsAnalyzed', 'wasm']
CODE_FILES = ['CMakeLists.txt', 'build.bat', 'build_wasm.bat', 'start.bat',
              'WebUI/package.json', 'WebUI/vite.config.js', 'WebUI/playwright.config.js',
              'WebUI/index.html', 'ROADMAP.md', 'package.json']
EXTERNAL_ROOTS = ['WebUI/node_modules/@abdsynths']

SOURCE_EXT = {'.h', '.hpp', '.cpp', '.cc', '.cxx', '.c', '.mjs', '.js', '.ts',
              '.tsx', '.jsx', '.css', '.html', '.json', '.ps1', '.bat', '.py',
              '.md', '.txt', '.yml', '.yaml', '.cmake', '.preset', '.wasm'}

# Lo que NO es una referencia aunque vaya entre acentos graves: banderas, numeros,
# fragmentos de codigo con espacios y comandos de una linea. Un token con espacio
# suele ser texto citado ("el boton RANDOM"), no un nombre de fichero.
SKIP_EXACT = {
    'grep', 'cmake', 'ctest', 'node', 'pnpm', 'npm', 'npx', 'powershell', 'cmd',
    'bash', 'git', 'pip', 'python', 'echo', 'build.bat nopause', 'pnpm build',
    'CMAKE_BUILD_TYPE', 'JUCE', 'JUCE_WEB_BROWSER', 'WebUI', 'NEURONiK',
}

# Raices que NO son de este arbol: otro repo hermano (ABDSharedAssets), otro
# paquete (WebPilot, el Next.js retirado), o rutas del disco del usuario. Sus
# entradas en HANDOFF son historia, no afirmaciones sobre el codigo de aqui, asi
# que se listan aparte en vez de contarlas como rotas.
EXTERNAL_PREFIXES = ('abdshared', 'abdsharedassets', 'webpilot', 'documents',
                     'demo/', '404', 'www.', 'oneDrive', 'd:/', 'c:/',
                     '../', 'abdcz101', 'abdsharedcode')


def strip_accents(text):
    return ''.join(c for c in unicodedata.normalize('NFD', text)
                   if unicodedata.category(c) != 'Mn')


# Simbolos que NO son codigo nuestro aunque no esten en el arbol: APIs de JUCE,
# de vitest/Playwright, comandos de shell y palabras de relleno. No se reportan
# como rotos porque "no aparece en nuestro codigo" es la respuesta correcta para
# ellos y reportarlos solo haria que la lista de simbolos fuera ruido.
EXTERNAL_SYMBOLS = {
    # JUCE y el C de JUCE embebido
    'MouseListener', 'ERR_NETWORK_IO_SUSPENDED', 'onEditorHide', 'ResizableWindow',
    # runners de test y su API
    'str_replace', 'testMatch', 'moderate', 'expect', 'describe', 'it', 'beforeEach',
    # comandos y herramientas del shell
    'netstat', 'pip', 'choco', 'where', 'tasklist',
    # relleno tipico de plantillas
    'yourcompany', 'PLUGIN_MANUFACTURER_NAME', 'TODO', 'FIXME',
}


def looks_like_url(token):
    return '://' in token or token.startswith('www.')


def looks_like_glob_list(token):
    """`morphX/morphY`, `setValue/getValue/destroy`, `injectNoteOn/Off/`,
    `lfo1/2SyncMode`: son LISTAS de nombres, no rutas.

    Sin esta distincion medio documento aparece como "ruta rota" y el informe
    deja de servir para lo que importa. El caso `lfo1/2SyncMode` se cuela porque
    `2SyncMode` no empieza por letra, asi que la regla mira que NO haya ninguna
    extension de fichero en el token: `CoreModules/Foo.cpp` si es una ruta,
    `lfo1/2SyncMode` no.
    """
    parts = [p for p in token.replace('\\', '/').split('/') if p]
    if len(parts) < 2:
        return False
    _, ext = os.path.splitext(parts[-1])
    if ext.lower() in SOURCE_EXT:
        return False
    identifiers = [re.match(r'^[A-Za-z_$][A-Za-z0-9_$]*$', p) for p in parts]
    return all(identifiers) or not identifiers[0] or not identifiers[-1]


HEX_HASH = re.compile(r'^[0-9a-fA-F]{7,40}$')
BUILD_HASHED = re.compile(r'^[a-z]+-[A-Za-z0-9_-]{6,}\.(js|css)$')


def strip_path_suffix(token):
    """`ui/panel.js::liveDrawerBadge` y `app.js:186-223` son ruta + puntero.

    La parte que hay que comprobar es la ruta: el sufijo es donde mirar DENTRO del
    fichero, no otro fichero.
    """
    clean = token.split('::')[0]
    match = re.match(r'^(.*?):\d+(?:[-,]\d+)*$', clean)
    return match.group(1) if match else clean


def is_external(token):
    return token.lower().replace('\\', '/').startswith(EXTERNAL_PREFIXES)


def looks_like_path(token):
    """Ruta si lleva separador de directorios o una extension de fichero conocida."""
    if ' ' in token or token.startswith('-') or looks_like_glob_list(token):
        return False
    if '/' in token or '\\' in token:
        return True
    _, ext = os.path.splitext(token)
    return ext.lower() in SOURCE_EXT


def looks_like_symbol(token):
    """Identificador: foo(), Class::metodo, CONSTANTE o camelCase, sin espacios."""
    if ' ' in token or token.startswith('-'):
        return False
    if token in SKIP_EXACT:
        return False
    if token.replace('.', '').isdigit():
        return False
    if token.startswith('@'):          # @abdsynths/shared: paquete, no simbolo
        return False
    core = token.split('(')[0].split('::')[-1].strip()
    if not core or not re.match(r'^[A-Za-z_$][A-Za-z0-9_$]*$', core):
        return False
    # Un identificador tiene al menos una letra minuscula o un _ o un digito:
    # "OnWorklet" (clase) o "IDs" (constexpr) tambien cuentan, pero "OK" o "FAIL"
    # son palabras del log, no codigo.
    return bool(re.search(r'[a-z_0-9]', core))


MAX_BYTES = 4 * 1024 * 1024   # por encima, el fichero no es codigo fuente


def collect_source_files():
    files = []
    for rel in CODE_FILES:
        path = os.path.join(ROOT, rel)
        if os.path.isfile(path):
            files.append(path)
    for rel in CODE_DIRS:
        base = os.path.join(ROOT, rel)
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in ('node_modules', 'build', '.git')]
            for name in filenames:
                if os.path.splitext(name)[1].lower() in SOURCE_EXT:
                    path = os.path.join(dirpath, name)
                    try:
                        if os.path.getsize(path) <= MAX_BYTES:
                            files.append(path)
                    except OSError:
                        pass
    return files


IDENTIFIER = re.compile(r'[A-Za-z_$][A-Za-z0-9_$]*')


def build_corpus(files):
    """CONJUNTO de identificadores del codigo, en minusculas.

    No se guarda el texto: se guardan los nombres. Buscar un simbolo en el
    documento es entonces una consulta al conjunto (O(1)) en vez de un `in` sobre
    varios megas de texto por cada token — con 1.000 referencias, la diferencia
    son segundos frente a minutos.
    """
    names = set()
    for path in files:
        try:
            with io.open(path, 'r', encoding='utf-8', errors='ignore') as handle:
                names.update(IDENTIFIER.findall(handle.read()))
        except OSError:
            pass
    return {name.lower() for name in names}


def build_external_corpus():
    return build_corpus(collect_external_files(read_contents=True))


EXTERNAL_MAX_FILES = 200000      # para NOMBRES: el recorrido es barato
EXTERNAL_READ_MAX = 4000         # para el corpus de identificadores
EXTERNAL_READ_MAX_BYTES = 512 * 1024
# Los paquetes son enlaces: lo que se busca es el nombre del fichero (barato) y,
# para los simbolos, solo el codigo fuente legible. Los bundles minificados y sus
# .map no aportan un solo identificador util.
EXTERNAL_SKIP_DIRS = ('/dist/', '/build/', '.next/', '/coverage/')


def collect_external_files(read_contents=False):
    """Ficheros del paquete compartido.

    Dos modos, y la distincion es la que hacia que el informe dijera que
    `widgets.css` no existia (SI existe): con un tope plano de ficheros, el
    recorrido se comia los primeros directorios del paquete y nunca llegaba a
    styles/. Para NOMBRES el tope es alto y no se lee nada; para el CORPUS se
    leen solo fuentes legibles, con un tope bajo y salto de dist/build.
    """
    files = []
    limit = EXTERNAL_READ_MAX if read_contents else EXTERNAL_MAX_FILES
    for rel in EXTERNAL_ROOTS:
        base = os.path.join(ROOT, rel)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base, followlinks=True):
            normalised = dirpath.replace('\\', '/')
            if read_contents:
                dirnames[:] = [d for d in dirnames
                               if not any(part in ('dist', 'build', '.next',
                                                    'coverage')
                                          for part in normalised.split('/'))]
            for name in filenames:
                if name.endswith('.map'):
                    continue
                if os.path.splitext(name)[1].lower() not in SOURCE_EXT:
                    continue
                path = os.path.join(dirpath, name)
                if read_contents:
                    try:
                        if os.path.getsize(path) > EXTERNAL_READ_MAX_BYTES:
                            continue
                    except OSError:
                        continue
                files.append(path)
                if len(files) >= limit:
                    return files
    return files


def resolve_path(token):
    """Normaliza una ruta citada: separadores, barra inicial y './'."""
    clean = token.replace('\\', '/').lstrip('./')
    return os.path.join(ROOT, clean.replace('/', os.sep))


def build_name_index():
    """Rutas relativas y nombres simples de todo lo que hay en el arbol.

    Permite aceptar `app.js` (citado sin carpeta) o `ui/panel.js` (citado con una
    carpeta de mas): se busca por sufijo. Sin esto, medio documento se declararia
    roto por nombres que SI existen.
    """
    index = set()
    for rel in CODE_DIRS + ['WebUI']:
        base = os.path.join(ROOT, rel)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in ('node_modules', 'build', '.git')]
            rel_dir = os.path.relpath(dirpath, ROOT).replace('\\', '/')
            for name in dirnames + filenames:
                index.add(name)
                index.add((rel_dir + '/' + name).lstrip('./'))
    return index


def resolve_reference(token, names, external_names):
    """'ok' si la ruta existe de alguna forma razonable; 'rota' si no."""
    if os.path.exists(resolve_path(token)):
        return 'ok'
    parts = [p for p in token.replace('\\', '/').split('/')
             if p and p not in ('.', '..', '*', '?')]
    if not parts:
        return 'ok'
    # Un token SIN carpetas (`tokens.css`) se acepta por nombre simple, incluido
    # el del paquete compartido. Uno CON carpetas solo se acepta si un sufijo suyo
    # (con carpeta) existe de verdad: si no, el fichero se movio y eso hay que
    # reportarlo.
    if len(parts) == 1:
        return 'ok' if (parts[0] in names or parts[0] in external_names) else 'rota'
    for start in range(len(parts)):
        if '/'.join(parts[start:]) in names:
            return 'ok'
    return 'rota'


def extract_references(text):
    """Los tokens entre acentos graves, con la linea en la que se citan.

    Se ignoran los bloques de codigo cercados: un `grep` de ejemplo dentro de un
    ```bash no es una referencia al fichero grep.
    """
    references = []
    in_fence = False
    for number, line in enumerate(text.split('\n'), 1):
        if line.lstrip().startswith('```'):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for token in re.findall(r'`([^`\n]+)`', line):
            references.append((number, token.strip()))
    return references


def main():
    argv = sys.argv[1:]
    paths_only = '--paths-only' in argv
    fail_on_broken = '--fail' in argv

    with io.open(HANDOFF, 'r', encoding='utf-8') as handle:
        text = handle.read()

    references = extract_references(text)
    source_files = collect_source_files()
    corpus = build_corpus(source_files)
    external = build_external_corpus() if not paths_only else set()
    # Los ficheros del paquete compartido cuentan como existentes: many entradas
    # citan `widgets.css` o `lcdPanel.js` a pelo, aunque viven en
    # node_modules/@abdsynths. Sin sumarlos aqui, media lista de "rotas" serian
    # ficheros que estan a la vista en el editor.
    names = build_name_index()
    external_files = collect_external_files()
    # Dos tipos de indice para el paquete compartido, y la distincion importa:
    #   - nombre simple (`widgets.css`) -> vale como equivalente;
    #   - ruta relativa DENTRO del paquete (`styles/components/backgrounds.css`)
    #     -> vale como equivalente.
    # Un nombre simple NO puede validar una ruta CON carpetas: si la entrada dice
    # `WebUI/src/ui/fitStage.js` y ese path ya no existe pero el fichero vive hoy
    # en el paquete compartido, la afirmacion esta CADUCADA (dice donde vive y es
    # falso), y aceptarla por nombre borraria justo el hallazgo.
    external_names = {os.path.basename(path) for path in external_files}
    for path in external_files:
        marker = '@abdsynths'
        if marker in path:
            tail = path.split(marker, 1)[1].lstrip('/\\')
            parts = tail.split(os.sep)[1:]      # sin el nombre del paquete
            if parts:
                names.add('/'.join(parts))
                names.add('/'.join(parts[-2:]))

    broken_paths = []
    broken_symbols = []
    external_only = []
    stale_history = []
    skipped_urls = 0

    # Corte de eras: la entrada que RETIRO el piloto. Todo lo anterior describe un
    # arbol que ya no existe (WebPilot/, Next.js, page.jsx, su snapshot), asi que
    # sus referencias caducas son historia esperada y no un fallo del codigo. Lo
    # que se busca de verdad son las rotas POSTERIORES: esas describen el codigo
    # vigente y han dejado de ser ciertas.
    pilot_marker = '## Retirada del piloto'
    pilot_line = text.find(pilot_marker)
    pilot_line = len(text) if pilot_line < 0 else text.count('\n', 0, pilot_line) + 1

    for number, token in references:
        if not token:
            continue
        if looks_like_url(token):
            skipped_urls += 1
            continue
        if '?' in token or '*' in token or '{' in token or '}' in token:
            continue      # glosa o expansion de llaves (`ParameterPanel.{h,cpp}`)
        if HEX_HASH.match(token) or BUILD_HASHED.match(token):
            continue      # hash de commit, o fichero de dist con hash de build
        if token.startswith('@'):
            external_only.append((number, token))
            continue      # paquete de otro repo (@abdsynths/shared)
        if token.startswith('/'):
            continue      # fragmento absoluto: `/W4`, `/O2`, `/demo/demo.html`
        if is_external(token):
            external_only.append((number, token))
            continue

        era = 'historica' if number < pilot_line else 'vigente'

        if looks_like_path(token):
            path_token = strip_path_suffix(token)
            if resolve_reference(path_token, names, external_names) == 'rota':
                (broken_paths if era == 'vigente' else stale_history).append(
                    (number, token))
            continue

        if paths_only:
            continue

        if looks_like_symbol(token):
            core = token.split('(')[0].split('::')[-1].strip().lower()
            if core in corpus or core in EXTERNAL_SYMBOLS:
                continue
            if core in external:
                external_only.append((number, token))
                continue
            (broken_symbols if era == 'vigente' else stale_history).append(
                (number, token))

    def report(title, rows):
        if not rows:
            return
        print('\n%s (%d)' % (title, len(rows)))
        for number, token in rows:
            print('  HANDOFF.md:%d  %s' % (number, token))

    print('corte de eras (retirada del piloto, linea %d)' % pilot_line)

    print('=' * 72)
    print('AUDITOR DE REFERENCIAS DE HANDOFF.md')
    print('=' * 72)
    print('referencias entre acentos graves : %d' % len(references))
    print('ficheros de codigo leidos        : %d' % len(source_files))
    print('rutas rotas                     : %d' % len(broken_paths))
    print('simbolos rotos                  : %d'
          % (0 if paths_only else len(broken_symbols)))
    print('referencias en @abdsynths (otro repo): %d' % len(external_only))

    report('RUTAS QUE NO EXISTEN (vigente) — Comprobado: el fichero no esta',
           broken_paths)
    print('\nNOTA: la lista de RUTAS es un hecho (se comprueba la existencia).')
    print('      La de SIMBOLOS es un CANDIDATO: se busca el identificador en el')
    print('      texto del codigo, asi que un simbolo puede vivir en un fichero que')
    print('      este auditor no recorre, o ser cosa de otro repo (JUCE, otro')
    print('      sintetizador). Se listan para revisar, no como veredicto.')

    report('SIMBOLOS QUE NO APARECEN EN EL CODIGO (vigente, revisar)', broken_symbols)
    report('CADUCADAS ANTES DE LA RETIRADA DEL PILOTO (esperado: historia)',
           stale_history)

    if external_only:
        print('\nNOTA: %d referencias apuntan a @abdsynths/shared (otro repo): existen,'
              % len(external_only))
        print('      pero no son codigo de este arbol.')

    broken = len(broken_paths) + (0 if paths_only else len(broken_symbols))
    if fail_on_broken and broken:
        print('\nFAIL: %d referencias rotas (--fail).' % broken)
        return 1
    print('\nOK: informe generado (exit 0). Con --fail devuelve 1 si hay rotas.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
