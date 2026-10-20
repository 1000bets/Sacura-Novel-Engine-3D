import {DatabaseSync,backup} from 'node:sqlite';
import {mkdirSync} from 'node:fs';
const directory='/data/backups';mkdirSync(directory,{recursive:true});
const filename=`${directory}/projects-${new Date().toISOString().replaceAll(':','-')}.sqlite`;
const db=new DatabaseSync(process.env.DB_PATH||'/data/projects.sqlite');
try{await backup(db,filename);console.log(filename);}finally{db.close();}
