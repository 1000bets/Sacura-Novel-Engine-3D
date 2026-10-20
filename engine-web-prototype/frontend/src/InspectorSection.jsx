import React from 'react';
import {Icon} from './StudioParts.jsx';
import {t as tr,useLocale} from './i18n.jsx';

export default function InspectorSection({title,children,open=true,extra,className=''}){
 useLocale();
 return <details className={'inspector-section '+className} open={open}>
  <summary><Icon name="ChevronRight" size={12}/><span>{tr(title)}</span>{extra}</summary>
  <div className="inspector-fields">{children}</div>
 </details>;
}
