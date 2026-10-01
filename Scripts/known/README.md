# Motivos de los rojos conocidos

Esta carpeta guarda **el por que** de cada test que se sabe que esta rojo y por
que se deja rojo. Sin esto, la lista de conocidos es una lista de excepciones
sin explicación, y dentro de tres meses nadie sabe si un rojo es mio, del
trabajo de al lado, o un forgot que hay que arreglar ya.

Aqui no hay codigo. Lo unico que hay que saber para anadir o quitar una entrada
son las cuatro reglas de abajo, y las dos mitades que tienen que quedar
sincronizadas.

## Donde esta cada mitad

| Que | Donde |
| --- | --- |
| Que tests se sabe que estan rojos | `Scripts/verify_all_known.json`, en la lista `entradas` |
| Por que esta ese rojo | `known/<nombre del test>.txt`, aqui al lado |

El JSON es **solo el indice**: una lista de nombres y nada mas. Por eso `entradas`
tiene dentro los nombres pelados, sin motivo al lado, y no el par nombre-motivo.

El nombre del fichero de motivo es **exactamente** el nombre de la entrada del
indice, mas `.txt`. Si los dos no se llaman igual, el motivo no se lee: no es que
avise, es que no existe para el script. Y al reves tambien: un `.txt` aqui cuyo
nombre no este en el indice no lo lee nadie (ver *MOTIVO SIN ENTRADA*, mas abajo).

Un test de la lista de ctest es, por ejemplo, `NEURONiK_StatePersistenceTest`. En
`entradas` va ese nombre, y aqui va `NEURONiK_StatePersistenceTest.txt`.

## Las cuatro reglas de un fichero de motivo

1. **El prefijo va en la PRIMERA linea, y decide la clasificacion.**

   ```
   MIO:   el rojo es de este trabajo, y hay que arreglarlo.
   ajeno: el rojo es del trabajo de al lado, y hay que leerlo pero no tocarlo.
   ```

   El prefijo se lee antes que nada. `MIO:` cuenta como mio, `ajeno:` cuenta como
   ajeno, y cualquier otra cosa sale **SIN CLASIFICAR**. Ojo: sin prefijo NO es
   mio por ser mio. Un rojo del que no se sabe de quien es tiene que quedar sin
   clasificar, que es lo unico honesto. Si el motivo empieza por otra palabra
   (`Nota:`, `Pendiente:`) no cuenta: el prefijo tiene que ir el primero.

2. **El resto del texto es libre, y se puede partir en lineas.** Se aplana al
   leer: las lineas se unen con un espacio y los saltos de linea se convierten en
   espacios. En el informe sale **una sola linea por rojo**, asi que un motivo
   partido en tres lineas sale en una. Escribe el motivo como quede mejor de
   leer.

3. **Las lineas que empiezan por `#` son comentarios y NO salen en el informe.**
   Ese es el sitio para el por que, el contexto y la historia, que es lo que de
   verdad hay que escribir y lo que no debe ensuciar la linea del rojo. La
   primera linea tambien puede ser un comentario, siempre que la linea que lleva
   el prefijo quede la primera de verdad.

4. **UTF-8, sin BOM, con salto de linea final.** Como todos los ficheros de texto
   del proyecto. Un BOM no se ve y rompe el prefijo de la primera linea, asi que
   el rojo sale SIN CLASIFICAR sin que se entienda por que.

Un fichero de motivo **vacio, o solo con comentarios, es un error**: la carga
falla ruidosamente y los dos scripts avisan con el motivo concreto. Es
deliberado: una entrada con el motivo en blanco clasifica el rojo sin decir nada
y eso es peor que no clasificarlo.

## Un ejemplo, tal cual esta ahora

`NEURONiK_PresetMigrationParityTest.txt`:

```
ajeno: del modulo compartido a medias (otro hilo): SEGFAULT, el motor no llega a imprimir

# El test revienta con un fallo de segmentacion ANTES de que su codigo llegue
# a escribir nada. Por eso el motivo no puede ser "lo que dice el log": no dice
# nada. Es lo que se sabe del rojo, que es lo unico que se puede decir.
```

La primera linea es el motivo que sale en el informe. Todo lo demas son notas
para quien lo lea dentro de seis meses.

## Los dos avisos que vigilan que las mitades cuadren

Los dos scripts comprueban la lista en el banner, con la orden `bateria` de
`verify_all_node.js`, y avisan de las dos desincronizaciones posibles.

**SIN TEST EN LA BATERIA.** Una entrada del indice cuyo test no existe en la
bateria de ctest. Ese test no se ejecuta, no falla, y por tanto su entrada nunca
llega a la lista de rojos: no clasifica nada. No se arregla sola, porque mientras
el test no se ejecute no vuelve a fallar y la entrada se queda para siempre.
Arreglo, en dos pasos porque son dos mitades:

- si el test se **renombro**: el nombre del indice pasa al nombre nuevo, **y el
  fichero de motivo tambien**, o el motivo se queda huerfano.
- si el test se **borro**: el nombre sale del indice **y el fichero de motivo se
  borra con el**.

**MOTIVO SIN ENTRADA.** Un `.txt` en esta carpeta cuyo nombre no esta en
`entradas`. Al reves: el fichero existe, con su texto escrito, y no lo lee
nadie. Pasa al copiar el motivo de un test y renombrar solo el indice. Con el
indice bien, el fichero que sobra no molesta (se avisa para que no se
acumulen), asi que el arreglo puede ser en un solo paso: o el nombre entra en
el indice, o el fichero se borra.

Junto a estos dos hay dos avisos mas que no son de la carpeta:

- **AVISO** cuando un motivo no empieza por `MIO:` ni por `ajeno:`. No rompe nada
  (el rojo sale SIN CLASIFICAR, que es lo correcto), pero casi siempre es un
  prefijo olvidado.
- **ARREGLO** cuando un test de la lista vuelve a pasar. No es un error: un rojo
  arreglado se quita solo de la lista, con su nombre y su fichero de motivo, en
  cuanto se comprueba que pasa.

## Por que esta partida en dos, y no todo en el JSON

Porque con cinco entradas cabia entero. A treinta, el JSON deja de ser una lista
y pasa a ser un muro de texto del que no se ve que hay dentro. Y un motivo largo
en una linea no se puede partir, no se puede comentar, y cualquier cambio suyo
enseña el texto entero en el diff.

En un fichero de texto si: se parte en lineas, se comenta con `#`, y el diff
enseña solo lo que cambio.

## Quien lee esto

`verify_all.sh` y `verify_all.bat`, los dos gemelos, leen el JSON y los motivos
por la orden `conocidos` de `verify_all_node.js`, que es el **unico** sitio donde
esta la logica de cargar la lista (en `cargar()`, con su carpeta de motivos
resuelta **relativa al fichero JSON**, no al directorio de trabajo). Ni el `.sh`
ni el `.bat` tocan el JSON ni los motivos: solo lo llaman.

Y por eso esta carpeta no tiene reglas propias para anadir un motivo: hay que
anadir el nombre al indice Y crear el fichero. El script no puede anadir el
nombre solo, porque entonces el otro script leeria un nombre sin motivo y
fallaria. Las dos mitades se escriben juntas, a mano, y el aviso comprueba que
hayan quedado juntas.

Para mirar el resultado sin leer los scripts:

```
node Scripts/verify_all_node.js conocidos Scripts/verify_all_known.json
node Scripts/verify_all_node.js motivo   Scripts/verify_all_known.json NEURONiK_StatePersistenceTest
node Scripts/verify_all_node.js cuenta   Scripts/verify_all_known.json
```
