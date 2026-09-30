import React, {useEffect, useRef, useState} from 'react';
import {Icon, Button} from './StudioParts.jsx';

const fallback={title:'Подсказки',body:'Нажмите на кнопку, поле или рабочую область — здесь появится описание выбранного элемента. Наведение курсора не меняет подсказку.'};
const descriptions={
 'Файл':'Создать, открыть или сохранить проект. Ctrl+S сохраняет текущий проект; Ctrl+Shift+S — сохранить под новым именем.',
 'Правка':'Отмените последнее изменение (Ctrl+Z) или восстановите расположение панелей редактора.',
 'Окна':'Откройте историю или рабочую панель в отдельном плавающем окне. Окно можно перетаскивать за заголовок и менять размер за нижний правый угол.',
 'Камеры':'Сохраните ракурс сцены, настройте кадр мышью или включите слежение за персонажем. Основная камера используется для общего плана.',
 'Сабсцены':'Настройте локацию, начальные погоду и время суток, входную реплику и переходы между частями истории.',
 'Звук и окружение':'Откройте аудиотеку, настройте начальное окружение или проверьте работающие эффекты. Пауза и остановка доступны при запуске сцены.',
 'Поток истории':'Один холст с репликами, их событиями и переходами. В каждом блоке: подготовка → текст и параллельные события → ответ игрока → завершение. Линии ведут к продолжению и другим сабсценам.',
 'Создать':'Добавьте объект, реплику, сабсцену или шаблон действия в проект.',
 'В реплику':'Добавить выбранный звуковой файл и его настройки к текущей реплике сценария.',
 'Восстановить раскладку':'Вернуть размеры сцены, иерархии и инспектора по умолчанию; закрыть плавающие окна.',
};
const areas=[
 ['.scene-viewport','Сцена','Alt + ЛКМ — вращение вида; СКМ — сдвиг; ПКМ + WASD / QE — полёт. Выберите объект и используйте W / E / R для перемещения, поворота и масштаба. F — приблизить объект.'],
 ['.hierarchy-tree','Иерархия','Выберите объект, чтобы изменить его свойства справа. Значок глаза скрывает объект. Двойной щелчок приближает его в сцене.'],
 ['.inspector-panel','Инспектор','Свойства выбранного объекта, реплики или действия. Выберите элемент в сцене, иерархии или сценарии.'],
 ['.global-timeline','Таймлайн','Общий маршрут истории. Откройте сабсцену или реплику, чтобы настроить её содержимое и переходы.'],
 ['.sound-workspace','Звуковые файлы','Прослушайте музыку или озвучку, настройте громкость и нажмите «В реплику», чтобы добавить файл к выбранному блоку сценария.'],
 ['.subscene-workspace','Сабсцены','Настройте локацию, начальную погоду, время суток и точку входа. Переходы связывают сабсцены в историю.'],
];
export function InfoView(){
 const [info,setInfo]=useState(fallback),[collapsed,setCollapsed]=useState(false);
 useEffect(()=>{
  const update=e=>{
   const target=e.target instanceof Element?e.target:null;
   if(!target||target.closest('.info-view'))return;
   const control=target.closest('button,input,select,textarea,summary,[role="button"]');
   const help=target.closest('[data-help]');
   const name=control?.getAttribute('aria-label')||control?.closest('label')?.querySelector('span')?.textContent?.trim()||control?.textContent?.trim()||control?.getAttribute('title');
   const area=areas.find(([selector])=>target.closest(selector));
   const next=descriptions[name]?{title:name,body:descriptions[name]}:help?{title:name||help.dataset.helpTitle||'Подсказка',body:help.dataset.help}:area?{title:name||area[1],body:control?.getAttribute('title')||area[2]}:name?{title:name,body:control?.disabled?'Сейчас недоступно. Остановите предпросмотр или выберите подходящий элемент.':control?.getAttribute('title')||'Дополнительная справка для этого элемента пока не задана.'}:fallback;
   setInfo(old=>old.title===next.title&&old.body===next.body?old:next);
  };
  document.addEventListener('click',update,true);
  return()=>{document.removeEventListener('click',update,true);};
 },[]);
 return <section className={'info-view'+(collapsed?' collapsed':'')} aria-label="Контекстная справка"><button className="info-heading" aria-expanded={!collapsed} onClick={()=>setCollapsed(v=>!v)}><Icon name="Info" size={15}/>Подсказки<Icon name={collapsed?'ChevronUp':'ChevronDown'} size={13}/></button>{!collapsed&&<div><strong>{info.title}</strong><p>{info.body}</p></div>}</section>;
}

export function FloatingWindow({active,title,onClose,children}){
 const [position,setPosition]=useState({x:Math.min(260,window.innerWidth*.12),y:90});
 const drag=useRef(null),panel=useRef(null);
 useEffect(()=>{if(!active)return;const clamp=()=>setPosition(p=>({x:Math.max(0,Math.min(p.x,window.innerWidth-160)),y:Math.max(0,Math.min(p.y,window.innerHeight-60))}));window.addEventListener('resize',clamp);return()=>window.removeEventListener('resize',clamp);},[active]);
 if(!active)return children;
 return <section ref={panel} className="floating-editor-window" role="dialog" aria-label={title} style={{left:position.x,top:position.y}} onKeyDown={e=>{if(e.key==='Escape'){e.stopPropagation();onClose();}}}>
  <header onPointerDown={e=>{if(e.target.closest('button'))return;drag.current={x:e.clientX-position.x,y:e.clientY-position.y};e.currentTarget.setPointerCapture(e.pointerId);}} onPointerMove={e=>{if(drag.current)setPosition({x:Math.max(0,Math.min(window.innerWidth-160,e.clientX-drag.current.x)),y:Math.max(0,Math.min(window.innerHeight-60,e.clientY-drag.current.y))});}} onPointerUp={()=>{drag.current=null;}} onLostPointerCapture={()=>{drag.current=null;}}><Icon name="AppWindow" size={15}/><strong>{title}</strong><span>Перетащите за заголовок</span><Button icon="X" title="Закрыть окно / вернуть рабочую область на место" onClick={onClose}/></header>
  <div className="floating-window-body">{children}</div>
 </section>;
}
