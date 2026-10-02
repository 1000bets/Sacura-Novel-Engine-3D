import {t} from './i18n.js';

// React Flow's keyboard instructions and canvas controls share the editor locale.
export function flowAriaLabels() {
 return {
  'node.a11yDescription.default':t('Enter или пробел — выбрать узел. Delete — удалить, Escape — отменить.'),
  'node.a11yDescription.keyboardDisabled':t('Enter или пробел — выбрать узел. Стрелки — переместить. Delete — удалить, Escape — отменить.'),
  'node.a11yDescription.ariaLiveMessage':({direction,x,y})=>t('Узел перемещён: {0}. Новая позиция: x {1}, y {2}.',[t(({left:'влево',right:'вправо',up:'вверх',down:'вниз'})[direction]||direction),x,y]),
  'edge.a11yDescription.default':t('Enter или пробел — выбрать связь. Delete — удалить, Escape — отменить.'),
  'controls.ariaLabel':t('Управление графом'),
  'controls.zoomIn.ariaLabel':t('Увеличить масштаб'),
  'controls.zoomOut.ariaLabel':t('Уменьшить масштаб'),
  'controls.fitView.ariaLabel':t('Показать граф целиком'),
  'controls.interactive.ariaLabel':t('Переключить взаимодействие'),
  'minimap.ariaLabel':t('Миникарта графа'),
  'handle.ariaLabel':t('Пин соединения'),
 };
}
