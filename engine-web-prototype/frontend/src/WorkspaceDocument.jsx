import React, {useMemo} from 'react';
import NativeAssetEditorRoot from './NativeAssetEditorRoot.jsx';
import {CreateDocumentDesktopApi} from './documentDesktopApi.js';

export default function WorkspaceDocument({descriptor:Descriptor}) {
  const Desktop = useMemo(() => CreateDocumentDesktopApi(Descriptor), [Descriptor.id]);
  return <NativeAssetEditorRoot desktop={Desktop}/>;
}
