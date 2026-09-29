/**
 * GENERADO POR NEURONiK_FxExport — NO EDITAR.
 *
 * El catalogo de efectos de NEURONiK: que efectos hay, con que id del
 * vocabulario compartido, de que familia (y por tanto con que tema) y con que
 * mandos. La pagina pinta un hueco de efecto leyendo solo esto.
 *
 * El numero de mandos sale del MOTOR, no del contrato compartido: la columna
 * `params` de ese contrato cuenta los mandos de la implementacion de ABDEep.
 */

export const FX_CATALOG = Object.freeze({
  "generatedFrom": "Source/DSP/FxCatalogue.h + ABDSharedCode/DspEffects/fxDefaultCatalogue()",
  "note": "El id y la familia vienen del vocabulario compartido de efectos; el numero y las tablas de los mandos vienen del MOTOR, que es el unico que sabe cuantos acepta. La columna params del contrato compartido cuenta los mandos de la implementacion de ABDEep y no es trasladable.",
  "numSlots": 4,
  "maxParams": 4,
  "effects": [
    {
    "id": 0, "sharedId": 0, "name": "Bypass", "displayName": "Bypass", "family": "bypass", "aligned": true, "technicalName": null, "params": [] },
    { "id": 1, "sharedId": 10, "name": "Stereo Chorus", "displayName": "Chorus", "family": "chorus", "aligned": true, "technicalName": "chorus", "params": [
        { "index": 0, "name": "rate", "min": 0.1, "max": 8, "default": 0.85, "skew": 0.55, "steps": 0 },
        { "index": 1, "name": "depth", "min": 0, "max": 1, "default": 0.5, "skew": 1, "steps": 0 }
      ] },
    { "id": 2, "sharedId": 13, "name": "Delay", "displayName": "Delay", "family": "delay", "aligned": true, "technicalName": "delay", "params": [
        { "index": 0, "name": "time", "min": 0.01, "max": 2, "default": 0.375, "skew": 0.3, "steps": 0 },
        { "index": 1, "name": "feedback", "min": 0, "max": 0.95, "default": 0.35, "skew": 1, "steps": 0 }
      ] },
    { "id": 3, "sharedId": 57, "name": "FreeVerb Reverb", "displayName": "Reverb", "family": "reverb", "aligned": true, "technicalName": "reverb", "params": [
        { "index": 0, "name": "size", "min": 0, "max": 1, "default": 0.5, "skew": 1, "steps": 0 },
        { "index": 1, "name": "damping", "min": 0, "max": 1, "default": 0.5, "skew": 1, "steps": 0 },
        { "index": 2, "name": "width", "min": 0, "max": 1, "default": 0.9, "skew": 1, "steps": 0 },
        { "index": 3, "name": "levels", "min": 0, "max": 1, "default": 0.33, "skew": 1, "steps": 0 }
      ] },
    { "id": 4, "sharedId": 58, "name": "Saturation", "displayName": "Saturation", "family": "distortion", "aligned": true, "technicalName": "saturation", "params": [
        { "index": 0, "name": "drive", "min": 1, "max": 8, "default": 2, "skew": 0.5, "steps": 0 }
      ] },
    { "id": 5, "sharedId": 59, "name": "Schroeder Reverb", "displayName": "Schroeder", "family": "reverb", "aligned": true, "technicalName": "schroeder", "params": [
        { "index": 0, "name": "decay", "min": 0.1, "max": 0.98, "default": 0.6, "skew": 1, "steps": 0 },
        { "index": 1, "name": "damping", "min": 0, "max": 1, "default": 0.3, "skew": 1, "steps": 0 },
        { "index": 2, "name": "diffusion", "min": 0, "max": 1, "default": 0.7, "skew": 1, "steps": 0 },
        { "index": 3, "name": "predelay", "min": 0, "max": 0.12, "default": 0.02, "skew": 1, "steps": 0 }
      ] },
    { "id": 6, "sharedId": 36, "name": "BBD Chorus", "displayName": "Juno BBD Chorus", "family": "chorus", "aligned": true, "technicalName": "bbd", "params": [
        { "index": 0, "name": "mode", "min": 0, "max": 3, "default": 1, "skew": 1, "steps": 4 },
        { "index": 1, "name": "rate", "min": 0.1, "max": 8, "default": 0.513, "skew": 0.6, "steps": 0 },
        { "index": 2, "name": "depth", "min": 0, "max": 1, "default": 0.6, "skew": 1, "steps": 0 },
        { "index": 3, "name": "wear", "min": 0, "max": 1, "default": 0.35, "skew": 1, "steps": 0 }
      ] },
    { "id": 7, "sharedId": 60, "name": "Shelf Filter", "displayName": "Shelf EQ", "family": "filter", "aligned": true, "technicalName": "shelf", "params": [
        { "index": 0, "name": "mode", "min": 0, "max": 1, "default": 0, "skew": 1, "steps": 2 },
        { "index": 1, "name": "freq", "min": 20, "max": 20000, "default": 1000, "skew": 0.5, "steps": 0 },
        { "index": 2, "name": "gain", "min": -12, "max": 12, "default": 3, "skew": 1, "steps": 0 }
      ] },
    { "id": 8, "sharedId": 9, "name": "Stereo Phaser", "displayName": "Phaser", "family": "modulation", "aligned": true, "technicalName": "phaser", "params": [
        { "index": 0, "name": "rate", "min": 0.02, "max": 15, "default": 0.6, "skew": 0.45, "steps": 0 },
        { "index": 1, "name": "depth", "min": 0, "max": 1, "default": 0.5, "skew": 1, "steps": 0 },
        { "index": 2, "name": "feedback", "min": 0, "max": 1, "default": 0.25, "skew": 1, "steps": 0 }
      ] }
  ]
}
);

/** Los ids del vocabulario compartido, indexados por id de fila. */
export const FX_CATALOG_BY_SHARED_ID = Object.freeze(
  Object.fromEntries (FX_CATALOG.effects.map ((effect) => [effect.sharedId, effect])),
);
