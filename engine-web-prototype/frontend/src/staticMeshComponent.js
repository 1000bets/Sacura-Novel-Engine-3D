// Older scenes store mesh data directly on objects. An explicit false removes the
// render component without discarding that data, so undo and re-adding restore it.
export function hasStaticMesh(object) {
  return !!object && (object.staticMesh === true || (object.staticMesh !== false && object.type !== 'Персонаж' && object.type !== 'Источник света' && object.type !== 'Пустой объект'));
}

export function canAddStaticMesh(object) {
  return object?.type !== 'Персонаж' && object?.type !== 'Источник света';
}
