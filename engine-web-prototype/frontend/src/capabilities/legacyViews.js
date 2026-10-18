import {registerCapability} from '../capabilityRegistry.js';
// Read views for old Events/widgets/saves. Gameplay definitions remain the source;
// no variable name is written here: story bridges are explicit stateBindings.
registerCapability({id:'sacura.compatibility',sync(){const s=this.state;
 if(this.config.legacyCompatibility!=='gameplay-v1')return;
 s.weaponId=s.selectedWeapons[this.config.legacyOwnerId]||'';
 s.kills=s.resources[this.config.legacyCounterId||'legacy-kills']||0;
}});
