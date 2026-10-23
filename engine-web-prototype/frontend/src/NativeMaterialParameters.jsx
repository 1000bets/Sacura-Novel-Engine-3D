import React, {useState} from 'react';

export default function NativeMaterialParameters({document: Document, edit: Edit, disabled: Disabled}) {
  const [Identifier, SetIdentifier] = useState('');
  const [ParameterType, SetParameterType] = useState('Float');
  const Types = ['Float', 'Integer', 'Boolean', 'Vector2', 'Vector3', 'Vector4', 'Color', 'Texture2D', 'StaticSwitch'];
  function Add(Event) {
    Event.preventDefault();
    let Default = 0;
    if (['Boolean', 'StaticSwitch'].includes(ParameterType)) { Default = false; }
    if (ParameterType === 'Texture2D') { Default = null; }
    if (ParameterType === 'Vector2') { Default = [0, 0]; }
    if (ParameterType === 'Vector3') { Default = [0, 0, 0]; }
    if (['Vector4', 'Color'].includes(ParameterType)) { Default = [0, 0, 0, 1]; }
    Edit({...Document, parameters: [...Document.parameters, {id: Identifier, displayName: Identifier, group: 'Parameters', type: ParameterType, default: Default}]});
    SetIdentifier('');
  }
  function Change(IdentifierValue, Property, Value) {
    Edit({...Document, parameters: Document.parameters.map(Parameter => {
      if (Parameter.id === IdentifierValue) { return {...Parameter, [Property]: Value}; }
      return Parameter;
    })});
  }
  return <details><summary>Определения параметров</summary>
    {Document.parameters.map(Parameter => <fieldset disabled={Disabled} key={Parameter.id}><legend>{Parameter.id} ({Parameter.type})</legend>
      <label>Название<input defaultValue={Parameter.displayName || ''} onBlur={Event => {if (Event.target.value !== Parameter.displayName) { Change(Parameter.id, 'displayName', Event.target.value); }}}/></label>
      <label>Группа<input defaultValue={Parameter.group || ''} onBlur={Event => {if (Event.target.value !== Parameter.group) { Change(Parameter.id, 'group', Event.target.value); }}}/></label>
      {Parameter.type === 'Texture2D' && <>
        <label><input type="checkbox" checked={!!Parameter.srgb} onChange={Event => Change(Parameter.id, 'srgb', Event.target.checked)}/>sRGB</label>
        <label>Адресация<select value={Parameter.address || 'Wrap'} onChange={Event => Change(Parameter.id, 'address', Event.target.value)}><option>Wrap</option><option>Clamp</option></select></label>
        <label>Фильтрация<select value={Parameter.filter || 'Linear'} onChange={Event => Change(Parameter.id, 'filter', Event.target.value)}><option>Linear</option><option>Point</option></select></label>
      </>}
      <button onClick={() => Edit({...Document, parameters: Document.parameters.filter(Value => Value.id !== Parameter.id)})}>Удалить параметр</button>
    </fieldset>)}
    <form onSubmit={Add}><fieldset disabled={Disabled}><legend>Новый параметр</legend>
      <label>Имя в HLSL<input required pattern="[A-Za-z_][A-Za-z0-9_]*" value={Identifier} onChange={Event => SetIdentifier(Event.target.value)}/></label>
      <label>Тип<select value={ParameterType} onChange={Event => SetParameterType(Event.target.value)}>{Types.map(Value => <option key={Value}>{Value}</option>)}</select></label>
      <button type="submit" disabled={!Identifier || Document.parameters.some(Parameter => Parameter.id === Identifier)}>Добавить параметр</button>
    </fieldset></form>
  </details>;
}
