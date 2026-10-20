import React from 'react';
export default function GameplayMigrationReport({project}){
 const report=project.gameplayMigration;if(!report)return null;
 const download=()=>{const url=URL.createObjectURL(new Blob([JSON.stringify(report.source,null,2)],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download='Sacura-до-преобразования.sacura.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
 return <details><summary>Отчёт о преобразовании игровых механик</summary><p>Версия {report.from} → {report.to}. Исходные данные сохранены в резервной копии. ID объектов, событий и функций сохраняются.</p>{report.changes.map((text,i)=><p key={i}>{text}</p>)}{report.diagnostics.length?<section aria-label="Настройки для проверки">{report.diagnostics.map((text,i)=><p role="alert" key={i}>{text}</p>)}</section>:<p>Непереносимых настроек не обнаружено. Проверьте проект в Play.</p>}<button type="button" onClick={download}>Скачать исходный проект</button></details>;
}
