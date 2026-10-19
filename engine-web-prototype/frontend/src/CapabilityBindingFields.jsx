import {availableComponentBindings} from './componentValueBindings.js';
import React from 'react';
import {availableCapabilityBindings} from './capabilityBindings.js';
import {GameSelect} from './GameplayCommandFields.jsx';
export default function CapabilityBindingFields({project,value,onChange,label='Значение возможности',valueTypes}){
 const choices=[...availableCapabilityBindings(project),...availableComponentBindings(project)].filter(c=>!valueTypes||valueTypes.includes(c.binding.valueType)),key=b=>JSON.stringify([b?.capability,b?.owner,b?.field,b?.recordId,b?.instance,b?.source]),selected=choices.findIndex(c=>key(c.binding)===key(value));
 return <GameSelect label={label} value={selected<0?'':String(selected)} options={[["","Выберите владельца и значение…"],...choices.map((c,i)=>[String(i),c.name])]} onChange={id=>onChange(id===''?null:choices[Number(id)].binding)}/>;
}
