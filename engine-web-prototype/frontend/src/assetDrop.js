export function hasDroppedFiles(event){
 const transfer=event.dataTransfer;
 return Array.from(transfer.types||[]).includes('Files')||Array.from(transfer.items||[]).some(item=>item.kind==='file')||Boolean(transfer.files?.length);
}
export function dropAssetFiles(event,{dragDepth,onDragging,onFiles,disabled}){
 if(!hasDroppedFiles(event))return;
 event.preventDefault();event.stopPropagation();dragDepth.current=0;onDragging(false);
 if(disabled)return;
 const files=Array.from(event.dataTransfer.files||[]);
 if(files.length)onFiles(files);
}
