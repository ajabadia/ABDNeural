# tools/verificacion

Scripts que miden cosas del repositorio. Estaban sueltos en `D:/tmp/rev`, que es
temporal y no se commitea: un dato que solo vive ahi se pierde con el proximo
`pnpm store prune`. Aqui viven con nombre, con su porque y con sus pruebas.

## Correrlo todo

```bash
bash tools/verificacion/correr_todas.sh
```

Sale 0 si todo pasa y 1 si algo falla, diciendo que. Son tres pasos y tardan
medio minuto.

## Que hay

### `convenciones.py` — como esta escrito cada fichero

Mide los finales de linea, los vertical tabs, las vallas de codigo de los
`.md`, el CJK y lo que se salga de la lista de los tres caracteres que los `.bat`
ya traian. Sale 1 si encuentra algo.

```bash
python tools/verificacion/convenciones.py --trackeados   # lo versionado
python tools/verificacion/convenciones.py                 # el arbol entero
python tools/verificacion/convenciones.py CMakeLists.txt  # lo que le digas
```

`--trackeados` es el modo normal. El arbol entero tiene temporales de otras
sesiones sin versionar que no son fuente de nadie y que nadie puede arreglar, y
una puerta que se abre con ellas es una puerta que todo el mundo abre con
`--no-verify`.

### `detectar_tildes.py` — que ficheros van con tildes y cuales sin

Cuenta palabras cuya forma acentuada es otra palabra (`esta` de verbo, `mas`,
`dia`) y dice el grupo de cada fichero, con los numeros. No decide cual es el
grupo correcto: eso lo dice el `.gitattributes` y la cabecera del fichero. Este
solo cuenta, para que la decision se tome mirando y no de memoria.

```bash
python tools/verificacion/detectar_tildes.py           # el repo entero
python tools/verificacion/detectar_tildes.py un.md     # lo que le digas
```

Sale en tres grupos: CON tildes, SIN tildes, y SIN SENAL, que son los que no
traen ninguna de las palabras contadas. El tercero esta porque filtrarlos en
silencio era peor que equivocarse: `build.bat` desaparecia del informe sin
dejar rastro de si se habia leido.

### `banco_convenciones.py` — que los dos de arriba AVISAN

Un verificador que solo se ha visto en verde no prueba nada. Este monta un
fichero por cada fallo, comprueba que se ve, y monta ficheros limpios y
comprueba que NO se ven, que es la otra mitad y la que se olvida.

```bash
python tools/verificacion/banco_convenciones.py
```

### `suite_bajo_carga.sh` — correr algo con la maquina ocupada

```bash
bash tools/verificacion/suite_bajo_carga.sh -- npx vitest run --no-color
```

Satura los nucleos, comprueba que la carga **ha mordido de verdad** con un
calibrado antes y despues, y lanza el comando. Sale con su codigo de salida.

El aviso de "LA CARGA NO HA MORDIDO" importa: sin el, un `rc=0` con la maquina
libre se lee como "el arreglo aguanta" cuando en realidad no se ha reproducido la
condicion. MEDIDO el 2026-10-03: la primera version de este script lanzaba los
procesos y ya estaba, y el comando tardaba 0,96 s contra 1,00 s en reposo. Nada.

No lo llama `correr_todas.sh`, y es a proposito: satura la maquina y lo que
mide depende de lo que se le pase.

### `rc.bat` — que un `.bat` devuelva su codigo de salida

```bat
call tools\verificacion\rc.bat otro.bat argumentos
if errorlevel 1 echo [ROJO] otro.bat ha fallado
```

`cmd //c "otro.bat & echo RC=%ERRORLEVEL%"` MIENTE: `%ERRORLEVEL%` se expande al
montar la linea, antes de que corra el otro script. Un banco armado asi da verde
de un script que ha fallado, que es lo peor que puede pasarle a un banco.

## Lo que se dejo fuera, y por que

De los ~199 scripts que habia en `D:/tmp/rev`, la mayoria no ha entrado:

| Grupo | Por que no |
|---|---|
| `arregla_*`, `edit_*`, `acentuar_*`, `fix*` | One-shot de una sesion. Su valor esta en el commit que hicieron, no en el fichero. Repetirlos no reproduce nada. |
| `build41.sh`, `mutar15b.sh`, `m16.py`, `b3.sh` | Nombres que solo tienen sentido dentro de la sesion que los escribio. |
| `aislar_crlf.sh`, `comparar_suite.sh` | Atados a un cambio concreto que ya esta commiteado. La IDEA que habia detrás (medir con y sin el cambio) sigue viva en el `.sh` y en el banco. |
| Los 74 `.log` y las 114 salidas | No son scripts, son salidas. |
| `.guard`, `CMakeCache.txt.bak`, `*_antes.*` | Copias de seguridad de ficheros que ya estan en el repo. |

Un repo con 199 scripts de sesion es un repo donde nadie sabe cual vale.

## Requisitos

Python 3, que ya hace falta para el build. `suite_bajo_carga.sh` y
`correr_todas.sh` necesitan bash, que viene con git for Windows.
