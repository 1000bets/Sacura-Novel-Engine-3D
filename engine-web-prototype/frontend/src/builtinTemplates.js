import {readAuthoringPackage} from './mechanicPackages.js';
const guide=['Откройте шаблон и выберите «Игровые механики». Возможности и правила редактируются обычными средствами.','Для изменения завершения выберите механику задания и настройте группу целей. Для смерти, награды и аптечки откройте соответствующие правила.','Камера, управление, оружие и HUD подключаются независимо. Функции доступны в существующем графовом редакторе.','Для применения к другой сцене экспортируйте отдельную механику и назначьте свои привязки в предварительном просмотре импорта.'];
export const builtinTemplates=[
 {id:'builtin-quest',name:'Point & click · Квест',description:'Ключ, запертая дверь, инвентарь и диалог после открытия.',builtin:true,path:'point-and-click'},
 {id:'builtin-shooter',name:'Шутер · FPS, HP и Game Over',description:'Движение, оружие, аптечки и явные правила задания по выбранной группе целей.',builtin:true,path:'shooter',guide},
 {id:'builtin-novel',name:'Новелла · Отношение и ответы',description:'Выбор меняет отношение и открывает следующий ответ.',builtin:true,path:'novel'},
 {id:'builtin-clicker',name:'UI-кликер · Монеты',description:'Кнопка даёт монеты, улучшение увеличивает скорость получения. Без героя и физики.',builtin:true,path:'ui-clicker'}
];
export async function loadBuiltinTemplate(id){const t=builtinTemplates.find(t=>t.id===id);if(!t)throw Error('Шаблон не найден.');const response=await fetch(`${import.meta.env?.BASE_URL||'/'}game-templates/${t.path}.sacura-template.json`);if(!response.ok)throw Error('Не удалось загрузить стандартный шаблон.');const pkg=await readAuthoringPackage(await response.json());if(pkg.roots.kind!=='template')throw Error('В библиотеке ожидался пакет шаблона.');return pkg;}
