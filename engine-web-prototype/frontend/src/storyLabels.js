export const beatIcon=node=>({choice:'GitFork',branch:'GitFork',end:'Flag',gate:'MousePointerClick',merge:'Merge',variable:'Database','set-variable':'Pencil'})[node?.kind]||'MessageSquare';
export function beatPreview(node,limit=4){
 if(node.kind==='branch')return 'If · Если';
 if(node.kind==='set-variable')return 'Set · '+(node.variable||'Переменная');
 const text=(node.kind==='end'?node.ending||node.text:node.text)?.trim()||({choice:'Выбор игрока',gate:'Взаимодействие',merge:'Схождение веток'})[node.kind]||'Пустая реплика';
 const words=text.split(/\s+/);return words.slice(0,limit).join(' ')+(words.length>limit?'…':'');
}
