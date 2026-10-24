import React from 'react';
import WidgetRenderer from './WidgetRenderer.jsx';
import './nativeRuntimeWidgets.css';

export default function NativeRuntimeWidgets({State, Execute}) {
  return <div className="native-runtime-widgets">
    {(State?.widgetInstances || []).map(Instance => {
      if (Instance.error) {
        return <p role="alert" key={Instance.assetId}>{Instance.error}</p>;
      }
      if (!Instance.visible) {
        return null;
      }
      return <div className={'native-runtime-widget native-runtime-widget-' + Instance.kind} key={Instance.instanceId}>
        <WidgetRenderer controlled widget={Instance.widget} context={{variables:Instance.fields}}
          onElementEvent={(Element, Value = null) => Execute({instanceId:Instance.instanceId, elementId:Element.id, value:Value})}/>
      </div>;
    })}
  </div>;
}
