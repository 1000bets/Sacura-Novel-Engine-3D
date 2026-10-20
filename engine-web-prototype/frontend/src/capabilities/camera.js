import {registerCapability} from '../capabilityRegistry.js';
import {clamp} from '../gamePhysics.js';
import {operation,session} from './shared.js';
registerCapability({id:'sacura.camera',enter(){this.state.camera=this.config.camera;this.state.cameraParameters=this.config.cameraSettings;this.state.controller=this.config.controller;},methods:{look(value,device){if(!['firstPerson','thirdPerson'].includes(this.config.camera?.mode)||!Array.isArray(value)||value.length!==2||!value.every(Number.isFinite))return;const speed=device==='pointer'?this.config.cameraSettings.sensitivity:.03;this.state.yaw-=value[0]*speed;this.state.pitch=clamp(this.state.pitch-value[1]*speed,-1.5,1.5);}},
 operations:[operation('sacura.camera.look','Повернуть камеру',{delta:{type:'vector',size:2,label:'Поворот',default:[0,0]},device:{type:'string',enum:['pointer','axis','gamepad-axis','gamepad','touch'],options:{pointer:'Мышь',axis:'Ось', 'gamepad-axis':'Ось геймпада',gamepad:'Геймпад',touch:'Сенсорный ввод'},default:'pointer',label:'Источник'}},{type:'void'},(p,c)=>session(c).look(p.delta,p.device))]});

registerCapability({id:'sacura.crosshair'});
