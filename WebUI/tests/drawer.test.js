/**
 * El cajón lateral: abrir, cerrar y no dejar rastro.
 *
 * Lo que se pincha aquí es lo que distingue este cajón del `slideDrawer` de los
 * hermanos (ver la cabecera de src/ui/drawer.js): el contenido NO se reconstruye al
 * abrir. Si alguien lo cambia por el patrón de re-render, las celdas dejarían de
 * existir con el cajón cerrado — y con ellas el recuento de los 70 del selftest.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { createDrawer } from '../src/ui/drawer.js';

const mount = (options = {}) => createDrawer({ id: 'drawer-test', title: 'MATRIZ', badge: '4 RUTAS', ...options });

afterEach(() => {
  document.body.innerHTML = '';
});

describe('drawer', () => {
  it('nace cerrado y cuelga del documento (position: fixed)', () => {
    const drawer = mount();

    expect(drawer.isOpen()).toBe(false);
    expect(drawer.element.parentElement).toBe(document.body);
    expect(drawer.element.dataset.drawer).toBe('drawer-test');
    expect(drawer.element.getAttribute('aria-hidden')).toBe('true');
    expect(drawer.element.classList.contains('drawer--open')).toBe(false);

    drawer.destroy();
  });

  it('abre y cierra con el botón, con el fondo y con ESC', () => {
    const drawer = mount();
    const backdropVisible = () => drawer.backdrop.classList.contains('drawer-backdrop--visible');

    drawer.open();
    expect(drawer.isOpen()).toBe(true);
    expect(drawer.element.classList.contains('drawer--open')).toBe(true);
    expect(drawer.element.getAttribute('aria-hidden')).toBe('false');
    expect(backdropVisible()).toBe(true);

    drawer.element.querySelector('.drawer__close').click();
    expect(drawer.isOpen()).toBe(false);
    expect(backdropVisible()).toBe(false);

    drawer.open();
    drawer.backdrop.click();
    expect(drawer.isOpen()).toBe(false);

    drawer.open();
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);

    // Cerrar un cajón ya cerrado no es un error ni un cambio de estado.
    drawer.close();
    expect(drawer.isOpen()).toBe(false);

    drawer.destroy();
  });

  it('ESC con el cajón cerrado no hace nada (no se secuestra el teclado)', () => {
    const drawer = mount();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));

    expect(drawer.isOpen()).toBe(false);
    drawer.destroy();
  });

  it('lo que el llamador ponga en el cuerpo sigue ahí tras abrir y cerrar', () => {
    const drawer = mount();
    const cell = document.createElement('div');

    cell.dataset.parameterId = 'mod1Source';
    drawer.body.append(cell);

    drawer.open();
    drawer.close();

    // El contenido no se reconstruye: es el MISMO nodo (ver la cabecera).
    expect(drawer.body.contains(cell)).toBe(true);
    expect(drawer.body.querySelector('[data-parameter-id="mod1Source"]')).toBe(cell);

    drawer.destroy();
  });

  it('destroy lo quita todo, incluido el listener de teclado', () => {
    const drawer = mount();
    const onKeyDown = vi.fn();

    document.addEventListener('keydown', onKeyDown);
    drawer.destroy();

    expect(document.querySelector('[data-drawer="drawer-test"]')).toBeNull();
    expect(document.querySelector('[data-drawer-backdrop="drawer-test"]')).toBeNull();

    drawer.open();
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));

    // El objeto ya no responde: sin listeners no hay estado que cambiar.
    expect(drawer.isOpen()).toBe(false);
    expect(onKeyDown).toHaveBeenCalledTimes(1);

    document.removeEventListener('keydown', onKeyDown);
  });
});
