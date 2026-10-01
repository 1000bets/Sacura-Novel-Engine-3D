import React from 'react';
import './buildVersion.css';
export default function BuildVersion(){
 const build=__SACURA_BUILD__;
 return <span className="build-version" aria-label={`Версия ${build.version}, ${build.environment}, сборка ${build.commit}`} title={`Сборка: ${build.commit}`}>
  v{build.version} · {build.environment} · {build.commit.slice(0,7)}
 </span>;
}
