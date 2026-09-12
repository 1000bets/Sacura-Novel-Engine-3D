// Stable authoring identities for the geometry supplied with each location.
// Compound props stay together; their transform is stored per subscene.
export const DECORATION_CATALOG = [
  {id:'room-floor',name:'Пол',kind:'living',color:'#49404a',position:[0,-.15,0]},
  {id:'room-back-wall',name:'Задняя стена',kind:'living',color:'#515c63',position:[0,1.3,-3.2]},
  {id:'room-left-wall',name:'Левая стена',kind:'living',color:'#515961',position:[-4.5,1.3,0]},
  {id:'room-right-wall',name:'Правая стена',kind:'living',color:'#666067',position:[4.5,1.45,0]},
  {id:'room-ceiling',name:'Потолок',kind:'living',color:'#8b8180',position:[0,3.04,0]},
  {id:'room-window',name:'Окно',kind:'living',color:'#7b9eac',position:[1.35,1.53,-3.06]},
  {id:'room-rug',name:'Ковёр',kind:'living',color:'#776370',position:[-.6,.01,.8]},
  {id:'room-side-table',name:'Столик у лампы',kind:'living',color:'#7a6655',position:[-3.65,0,.85]},
  {id:'room-lamp',name:'Настольная лампа',kind:'living',color:'#d6b886',position:[-3.65,.84,.85]},
  {id:'room-cup',name:'Чашка',kind:'living',color:'#adb6b3',position:[.2,.85,1.72]},
  {id:'garden-ground',name:'Земля сада',kind:'garden',color:'#3c544b',position:[0,-.15,0]},
  {id:'garden-path',name:'Садовая дорожка',kind:'garden',color:'#899286',position:[0,0,-10]},
  {id:'garden-fence',name:'Ограда',kind:'garden',color:'#6e8272',position:[-2,.3,-3]},
  ...[[-3,-2],[3,-2],[-3,2],[5,1],[-5,-7],[0,-9],[5,-8],[-6,4]].map(([x,z],i)=>({id:`garden-tree-${i+1}`,name:`Дерево ${i+1}`,kind:'garden',color:'#527b68',position:[x,0,z]})),
  {id:'station-platform',name:'Платформа',kind:'station',color:'#687575',position:[0,-.1,0]},
  {id:'station-tracks',name:'Рельсы и шпалы',kind:'station',color:'#b7b6ab',position:[0,0,-2]},
  {id:'station-canopy',name:'Навес платформы',kind:'station',color:'#9aaba3',position:[0,0,1]},
  {id:'station-luggage',name:'Чемодан',kind:'station',color:'#aaa39a',position:[1.4,.4,1.2]},
];

export const DECORATION_TRANSFORMS = Object.fromEntries(DECORATION_CATALOG.map(o=>[o.id,o.position]));
export const DECORATION_KINDS = Object.fromEntries(DECORATION_CATALOG.map(o=>[o.id,o.kind]));
