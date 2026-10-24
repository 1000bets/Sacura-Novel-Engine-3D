import {readFile,writeFile} from 'node:fs/promises';
import {runInNewContext} from 'node:vm';
import {moveBody} from '../engine-web-prototype/frontend/src/gamePhysics.js';

const Load = async Name => runInNewContext((await readFile(new URL('../engine-web-prototype/frontend/templates/components/'+Name+'.js',import.meta.url),'utf8'))+'\ncomponent;');
const Room = await Load('HauntedRoom');
const State = Object.fromEntries(Object.entries(Room.state.properties).map(([Name,Schema])=>[Name,Schema.default]));
const Visible = {shadow:false,lamp:false};
let Message = '';
let DoorSeconds = 0;
const Engine = {
  async operation(Name,Arguments) {
    if(Name==='sacura.game.message'){Message=Arguments.message;}
    if(Name==='sacura.object.visible'){Visible[Arguments.object]=Arguments.visible;}
  },
  async emit(){},
  async invoke(Target,Method) {if(Method==='open'){DoorSeconds+=1;}}
};
const Context = {state:State,params:{requireRecord:true},refs:{shadow:'shadow',lamp:'lamp',exit:'exit',ending:'ending'},engine:Engine};
const Horror = [];
for(const Method of ['power','listen','escape','takeFuse','takeFuse','power','power','mirror','mirror','listen','escape']) {
  const Result = await Room.methods[Method].run(Context);
  Horror.push({method:Method,result:Result??null,state:{...State},message:Message,shadow:Visible.shadow,doorSeconds:DoorSeconds});
}
const Body={id:'player',type:'Персонаж',collision:{custom:true,size:[0.45,1.6,0.45],offset:[0,0.8,0]},transforms:{scene:{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}};
const Colliders=[{id:'wall',box:{min:[1,0,-5],max:[2,3,5]}},{id:'floor',box:{min:[-10,-1,-10],max:[10,0,10]}}];
const Movement=[];
for(const Delta of [[2,0,2],[0,-1,0],[-1,0,-2],[0,1,0]]){
  Movement.push({delta:Delta,position:moveBody(Body,[0,0,0],Delta,Colliders,'scene').position});
}
await writeFile(new URL('../Root/Verification/WebTemplateOracle/traces.json',import.meta.url),JSON.stringify({horror:Horror,movement:Movement},null,2)+'\n');
