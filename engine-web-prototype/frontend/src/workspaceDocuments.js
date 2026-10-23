const Owners = new Set();
const Listeners = new Set();
let ActiveDocument;

export function ActiveWorkspaceDocument() {
  return ActiveDocument || document;
}
export function RegisterWorkspaceDocument(Owner) {
  if (Owner !== window) {
    Owners.add(Owner);
  }
  const Activate = () => { ActiveDocument = Owner.document; };
  Owner.addEventListener('pointerdown', Activate, true);
  Owner.addEventListener('focus', Activate, true);
  for (const Listener of Listeners) {
    Owner.addEventListener(Listener.Type, Listener.Callback);
  }
  return () => {
    Owner.removeEventListener('pointerdown', Activate, true);
    Owner.removeEventListener('focus', Activate, true);
    for (const Listener of Listeners) {
      Owner.removeEventListener(Listener.Type, Listener.Callback);
    }
    Owners.delete(Owner);
    if (ActiveDocument === Owner.document) {
      ActiveDocument = document;
    }
  };
}
export function ListenWorkspaceEvent(Type, Callback) {
  const Listener = {Type, Callback};
  Listeners.add(Listener);
  window.addEventListener(Type, Callback);
  for (const Owner of Owners) {
    Owner.addEventListener(Type, Callback);
  }
  return () => {
    Listeners.delete(Listener);
    window.removeEventListener(Type, Callback);
    for (const Owner of Owners) {
      Owner.removeEventListener(Type, Callback);
    }
  };
}
