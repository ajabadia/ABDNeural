# REGRESION VISUAL DEL LIENZO — corrida completa

## Fecha: 2026-09-30
## Repo: ABDNeural, `master` en `157da1b`
## Comando: `npx playwright test e2e/visual.spec.js`

---

## RESULTADO

**18 de 18 pasan. 1,8 minutos.** Sin saltados, sin artefactos de fallo.

```
el lienzo entero                        12,0s
ficha oscillator                         2,9s
ficha lfo                                3,0s
ficha globalFull                         2,7s
ficha resonator                          2,6s
ficha filter                             2,8s
ficha modMatrix                          1,7s
ficha envelopes                          1,7s
ficha models                             1,5s
ficha fx                                 1,9s
cajon lfo                                2,9s
cajon globalFull                         2,4s
cajon modMatrix                          1,9s
cajon envelopes                          1,9s
cajon models                             2,2s
cajon fx                                 2,0s
el lienzo entero con el tema claro       2,4s
las cuatro agujas, en el sustain         9,7s
```

El ultimo, el de las agujas, se auto-salta cuando el reloj de audio de Chromium
no avanza en la maquina, que es lo que pasa sin salida de audio. En esta corrida
no se salto. Por eso el numero de esta suite oscila entre 17 y 18 sin que
haya cambiado nada del codigo: conviene no leer un 17 como un fallo.

---

## CON QUE SE COMPARO

| | |
|---|---|
| Lienzo de diseno (SSOT) | 1440 x 946, de `CANVAS` en `src/contracts/sections.js` |
| `maxDiffPixels` | 20 |
| `threshold` | 0,2 de diferencia por pixel |
| Viewport | igual al tamano de diseno, y escala 1 exigida antes de capturar |
| Chromium | Playwright 1.56.0, Windows |

Los puertos son los propios de esta sesion, `NEURONIK_E2E_PORT=5336` y
`NEURONIK_E2E_PROBE_PORT=5337`, para no chocar con la otra sesion que usa
5236/5237.

---

## LAS DIECINUEVE REFERENCIAS

Ninguna sobra y ninguna falta: los 18 tests piden 19 fotos (el de las agujas
pide dos) y hay 19 ficheros. La correspondencia es de una a una.

| grupo | cuantas | tamano |
|---|---|---|
| fichas (una por seccion) | 9 | 229x206, 467x206, 586x206, 705x206 |
| cajones (columna completa) | 6 | 640x946 |
| pagina entera, dos temas | 2 | 1440x946 |
| agujas del needle-probe | 2 | 616x391 y 616x165 |

---

## DERIVA CERO

La huella SHA-256 de las 19 referencias, tomada antes y despues de la corrida,
es **identica**. La suite no escribe nada al pasar: compara y nada mas. Eso es
lo que distingue una referencia estable de una que se regenera sola, y por eso
se comprueba con huella y no a ojo.

---

## QUE TIENE QUE VER CON EL ARCO DEL KNOB

Comparadas pixel a pixel contra `dc72578`, el commit anterior al del arco:

**Doce cambian, siete siguen byte-identicas.** Las siete que no se tocan son las
que no tienen un solo mando: `cajon lfo`, `cajon models`, `ficha envelopes`,
`ficha modMatrix`, `ficha models` y las dos de las agujas. Un sitio donde no hay
knob, la referencia no se toca.

| referencia | px distintos |
|---|---|
| pagina entera, tema claro | 36,8 % |
| pagina entera, tema oscuro | 28,7 % |
| ficha fx | 7,4 % |
| ficha lfo | 4,6 % |
| ficha resonator | 3,3 % |
| ficha oscillator | 2,5 % |
| ficha filter | 2,1 % |
| ficha globalFull | 0,7 % |
| cajon modMatrix | 0,5 % |
| cajon envelopes | 0,4 % |
| cajon fx | 0,2 % |
| cajon globalFull | 0,1 % |

La ficha fx es la que mas se mueve porque es la que mas mandos tiene, y el
orden de todo el resto sigue el numero de mandos. Las de pagina entera
diluyen el cambio en 1400 px de alto.

**Un matiz que cambia la lectura de esas dos cifras.** Las dos referencias de
pagina entera dicen 36,8 % y 28,7 %, y el commit del arco declaraba 1,5 %. No
es que el arco se haya movido: es que esas dos se regeneraron **dos veces**. La
primera por el arco, y la segunda —aun sin commitear— por la seccion de ejes del
S950 que esta trabajando la otra sesion, que ocupa 54 px del lienzo y desplaza
todo lo de arriba. Medido por separado: contra `157da1b` (ya con el arco) el
cambio es de 28,8 % y 37,4 %, que es practicamente el mismo total. El
desplazamiento domina y el arco aporta poco al conjunto.

Las doce referencias del arco **no** se han vuelto a tocar al regenerar las dos
de pagina. Siguen byte-identicas a las de `157da1b`, y eso es la prueba de que
el arreglo del arco esta asentado y no se esta moviendo con cada corrida.

---

## LO QUE ESTA SIN COMMITEAR Y TOCA A ESTA SUITE

Cuatro cosas del arbol de trabajo mueven esta suite, y conviene nombrarlas
porque ninguna es de la sesion que la ejecuto:

1. `WebUI/e2e/visual.spec.js`: el arranque con timeouts propios (15 s de
   navegacion, 10 s de arranque) en vez del timeout del test.
2. `WebUI/e2e/snapshots/`: las dos referencias de pagina entera regeneradas.
3. `WebUI/src/`: la funcion S950 del otro hilo, incluida `createS950Axes()` en
   `src/ui/panel.js`, que anade un bloque al lienzo.
4. `WebUI/src/ui/envelopeCurve.js` borrado y las reglas locales de la curva
   retiradas de `src/styles/main.css`, que ahora la pinta la hoja compartida.

El punto 4 merecia comprobacion y salio limpio: las quince referencias de
ficha y cajon **pasan**, y una de ellas es `ficha envelopes`, que es
exactamente la que pinta la curva. Si la convergencia hacia la hoja compartida
hubiera cambiado un solo pixel, ese test estaria en rojo. Lo que se absorbs es
el desplazamiento de las de pagina, no un cambio de la curva.

---

## COMO REGENERAR

Solo tras un cambio INTENCIONAL de pintura:

```
npx playwright test e2e/visual.spec.js --update-snapshots
```

`--update-snapshots` pasa por las diecinueve, pero solo escribe donde hay
diferencia real: en la corrida que produjo este informe tocaba dos ficheros de
diecinueve, y los otros diecisiete salieron byte-identicos.

Las referencias son de Chromium en Windows y hay que compararlas en Windows:
la pagina usa las fuentes del sistema y su rasterizado cambia entre sistemas
operativos. Una referencia de Windows sobre un runner Linux fallaria por la
fuente, no por el codigo.
