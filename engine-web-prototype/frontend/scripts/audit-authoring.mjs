import '../src/gameplayModel.js';
import {operationDefinitions} from '../src/operationRegistry.js';
import {capabilityDefinitions} from '../src/capabilityRegistry.js';
import {mechanicDefinitions} from '../src/mechanicsModel.js';
import {signalDefinitions} from '../src/signalModel.js';
import {TYPES} from '../src/model.js';
import {GAME_COMMANDS} from '../src/gameplayModel.js';
// Installed code inventory, without creating a project or executing author content.
console.log(JSON.stringify({
 mechanics:mechanicDefinitions().map(({id,name,scopes})=>({id,name,scopes})),
 capabilities:capabilityDefinitions(),signals:signalDefinitions(),
 operations:operationDefinitions(),nativeActions:Object.keys(TYPES),legacyCommands:GAME_COMMANDS,
},null,2));
