import React from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {GAME_COMMANDS} from './gameplayModel.js';
export function GameField({label,children}){return <label className="game-field"><span>{tr(label)}</span>{children}</label>;}
export function GameSelect({label,value='',options,onChange}){return <GameField label={label}><select value={value} onChange={e=>onChange(e.target.value)}>{options.map(([id,name])=><option key={id} value={id}>{tr(name)}</option>)}</select></GameField>;}
export function GameNumber({label,value=0,min=0,step=.1,onChange}){return <GameField label={label}><input type="number" value={value} min={min} step={step} onChange={e=>{if(e.target.value!=='')onChange(Number(e.target.value));}}/></GameField>;}
export function GameVector({label,value=[0,0,0],onChange,min}){return <GameField label={label}><div className="game-vector">{value.map((v,i)=><input key={i} aria-label={tr(label)+' '+['X','Y','Z'][i]} type="number" step=".1" min={min} value={v} onChange={e=>{const next=[...value];next[i]=Number(e.target.value);onChange(next);}}/>)}</div></GameField>;}
export function JsonValue({label,value,onChange}){const [text,setText]=React.useState(()=>JSON.stringify(value,null,2)),[error,setError]=React.useState('');React.useEffect(()=>setText(JSON.stringify(value,null,2)),[JSON.stringify(value)]);return <GameField label={label}><textarea rows={3} spellCheck={false} value={text??''} aria-invalid={!!error} onChange={e=>setText(e.target.value)} onBlur={()=>{try{onChange(JSON.parse(text));setError('');}catch{setError(tr('Проверьте JSON значения.'));}}}/>{error&&<small role="alert">{error}</small>}</GameField>;}
export default function GameplayCommandFields({project,command='damage',value={},onChange}){
 useLocale();const v=value&&typeof value==='object'?value:{},patch=x=>onChange({command,value:{...v,...x}}),ids=(array)=>[['','Выберите…'],...array.map(x=>[x.id,x.name])];
 return <><GameSelect label="Команда" value={command} options={Object.entries(GAME_COMMANDS)} onChange={command=>onChange({command,value:command==='startTimer'?{seconds:2,timerId:'timer'}:command==='impulse'?{velocity:[0,5,0]}:{}})}/>
 {['damage','heal','impulse'].includes(command)&&<GameSelect label="Объект" value={v.target||''} options={[["","Главный герой"],...project.objects.map(o=>[o.id,o.name])]} onChange={target=>patch({target})}/>}
 {['damage','heal'].includes(command)&&<GameNumber label="Количество" value={v.amount??25} onChange={amount=>patch({amount})}/>}
 {['addItem','removeItem','equip','useItem'].includes(command)&&<GameSelect label="Предмет" value={v.itemId} options={ids(project.gameplay?.items||[])} onChange={itemId=>patch({itemId})}/>}
 {['addItem','removeItem'].includes(command)&&<GameNumber label="Количество" min={1} step={1} value={v.count??1} onChange={count=>patch({count})}/>}
 {command==='goTo'&&<GameSelect label="Нода истории" value={v.beatId} options={[["","Выберите…"],...project.chapters.flatMap(c=>c.beats).filter(b=>b.kind==='gameplay'||b.kind==='dialogue'||b.kind==='end').map(b=>[b.id,b.text||b.id])]} onChange={beatId=>patch({beatId})}/>}
 {command==='combine'&&<GameSelect label="Рецепт" value={v.recipeId} options={ids(project.gameplay?.recipes||[])} onChange={recipeId=>patch({recipeId})}/>}
 {command==='impulse'&&<GameVector label="Импульс" value={v.velocity||[0,5,0]} onChange={velocity=>patch({velocity})}/>}
 {['startTimer','stopTimer'].includes(command)&&<GameField label="Имя таймера"><input value={v.timerId||'timer'} onChange={e=>patch({timerId:e.target.value})}/></GameField>}
 {command==='startTimer'&&<><GameNumber label="Секунды" value={v.seconds??2} onChange={seconds=>patch({seconds})}/><GameSelect label="Объект правила" value={v.objectId} options={[["","Без объекта"],...project.objects.map(o=>[o.id,o.name])]} onChange={objectId=>patch({objectId})}/><label><input type="checkbox" checked={!!v.repeat} onChange={e=>patch({repeat:e.target.checked})}/> {tr('Повторять')}</label></>}
 {command==='callFunction'&&<><GameSelect label="Функция Blueprint" value={v.functionId} options={ids(project.functions||[])} onChange={functionId=>patch({functionId})}/><JsonValue label="Параметры функции" value={v.args||{}} onChange={args=>patch({args})}/></>}
 </>;
}
