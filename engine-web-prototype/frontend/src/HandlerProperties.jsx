import React from 'react';
import {GameField,GameSelect,GameNumber,GameVector} from './GameplayCommandFields.jsx';
import {signalDefinitions} from './signalModel.js';
import {GAME_TRIGGERS} from './gameplayModel.js';

// Admission metadata; the program itself is edited on the canvas.
export default function HandlerProperties({project,rule,onChange}){
 const type=rule.signal?.type||rule.trigger,signals=[...signalDefinitions(),...project.gameplay.signals||[]],role=['interact','hit','death','enter','exit','collision'].includes(type)?'target':'source';
 const refs=[
  [{kind:'reference',type:'project',mode:'direct',id:project.id},'Проект · '+project.title],
  ...(project.subscenes||[]).map(s=>[{kind:'reference',type:'scene',mode:'direct',id:s.id},'Сцена · '+s.name]),
  ...(project.objects||[]).map(o=>[{kind:'reference',type:'object',mode:'direct',id:o.id},'Объект · '+o.name]),
  ...(project.widgets||[]).flatMap(w=>[[{kind:'reference',type:'widget',mode:'direct',id:w.id},'Виджет · '+w.name],...(w.elements||[]).filter(e=>e.type==='button').map(e=>[{kind:'reference',type:'widget',mode:'direct',id:w.id,elementId:e.id},'Кнопка · '+(e.name||e.text)+' · '+w.name])]),
  ...(project.input?.actions||[]).map(a=>[{kind:'reference',type:'inputAction',mode:'direct',id:a.id},'Ввод · '+a.name]),
  ...[...new Set((project.objects||[]).flatMap(o=>o.tags||[]))].map(tag=>[{kind:'reference',type:'object',mode:'tag',tag},'Группа · '+tag]),
 ];
 if(!rule.scope||rule.scope.kind==='object')refs.unshift([{kind:'reference',type:'object',mode:'context',role:'self'},'Этот объект']);
 const selector=(filterRole,label)=>{const current=rule.filter?.[filterRole],selected=current?JSON.stringify(current):'',options=[['','Любой источник'],...refs.map(([r,name])=>[JSON.stringify(r),name])];if(selected&&!options.some(([id])=>id===selected))options.push([selected,'Недоступная связь сохранена']);return <GameSelect key={filterRole} label={label} value={selected} options={options} onChange={value=>{const filter={...rule.filter};if(value)filter[filterRole]=JSON.parse(value);else delete filter[filterRole];onChange({filter});}}/>;};
 return <details><summary>Сигнал и правила запуска</summary>
  <GameField label="Название обработчика"><input value={rule.name||''} onChange={e=>onChange({name:e.target.value})}/></GameField>
  <GameSelect label="Входной сигнал" value={type||''} options={signals.map(s=>[s.id,s.name||GAME_TRIGGERS[s.id]||s.id])} onChange={type=>onChange({signal:{...rule.signal,type}})}/>
  {selector(role,role==='target'?'У объекта / цели':'У источника')}
  {['tick','timer'].includes(type)&&<><GameNumber label="Интервал в секундах" value={rule.seconds??2} onChange={seconds=>onChange({seconds})}/><label className="game-check"><input type="checkbox" checked={!!rule.repeat} onChange={e=>onChange({repeat:e.target.checked})}/>Повторять</label></>}
  {['enter','exit'].includes(type)&&<><GameVector label="Центр области" value={rule.center||[0,0,0]} onChange={center=>onChange({center})}/><GameVector label="Размер области" value={rule.size||[3,2,3]} min={.1} onChange={size=>onChange({size})}/></>}
  <GameSelect label="Повторный запуск" value={rule.reentry||'queue'} options={[["queue","В очередь"],["skip","Пропустить занятый"],["restart","Остановить прежний и начать"]]} onChange={reentry=>onChange({reentry})}/>
  <label className="game-check"><input type="checkbox" checked={!!rule.once} onChange={e=>onChange({once:e.target.checked})}/>Один раз за запуск</label>
  <GameNumber label="Пауза между запусками" value={rule.cooldown??0} onChange={cooldown=>onChange({cooldown})}/>
  <details><summary>Очередь и дополнительные фильтры</summary>
   <GameNumber label="Одновременных запусков правила" min={1} max={8} step={1} value={rule.maxConcurrent??1} onChange={maxConcurrent=>onChange({maxConcurrent})}/>
   <GameNumber label="Сигналов в очереди правила" min={0} max={16} step={1} value={rule.maxQueued??16} onChange={maxQueued=>onChange({maxQueued})}/>
   <GameSelect label="Источник времени" value={rule.timeSource||'game'} options={[["game","Игровое время"],["native","Системное время Event"]]} onChange={timeSource=>onChange({timeSource})}/>
   {['source','target','initiator'].filter(r=>r!==role).map(r=>selector(r,({source:'Источник',target:'Цель',initiator:'Инициатор'})[r]))}
  </details>
 </details>;
}
