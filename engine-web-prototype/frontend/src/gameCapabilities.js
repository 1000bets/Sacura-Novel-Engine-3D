// Public installed modules, shared by editor preview and standalone player.
// Registration order preserves the existing update order: intent, bodies, shots, timers.
import './capabilities/health.js';
import './capabilities/behavior.js';
import './capabilities/movement.js';
import './capabilities/weapons.js';
import './capabilities/inventory.js';
import './capabilities/interaction.js';
import './capabilities/timers.js';
import './capabilities/camera.js';
import './capabilities/resources.js';
import './capabilities/legacyViews.js';

import './capabilities/turns.js';
import './extensions/reward.js';
