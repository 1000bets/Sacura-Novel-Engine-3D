const Presentation = {
  'editor.CreateEmptyObjectAction': ['Пустой объект', 'Box', 'Основные'],
  'editor.CreateCameraObjectAction': ['Камера', 'Video', 'Основные'],
  'editor.CreateStaticMeshObjectAction': ['Статический меш', 'FileBox', 'Основные'],
  'editor.CreateDirectionalLightObjectAction': ['Directional Light', 'Sun', 'Освещение'],
  'editor.CreatePointLightObjectAction': ['Point Light', 'Lightbulb', 'Освещение'],
  'editor.CreateSpotLightObjectAction': ['Spot Light', 'Flashlight', 'Освещение'],
  'editor.CreateSkySphereObjectAction': ['Sky Sphere', 'CloudSun', 'Освещение'],
  'editor.CreateSkyLightObjectAction': ['Sky Light', 'Sun', 'Освещение'],
  'editor.CreatePhysicsBodyObjectAction': ['Физическое тело', 'Box', 'Физика'],
  'editor.CreateCharacterObjectAction': ['Контроллер персонажа', 'PersonStanding', 'Физика'],
  'editor.CreateNavMeshBoundsObjectAction': ['Область навигации', 'Scan', 'Навигация'],
  'editor.CreateNavigationModifierObjectAction': ['Модификатор навигации', 'Route', 'Навигация'],
  'editor.CreateNavigationLinkObjectAction': ['Связь навигации', 'Link', 'Навигация'],
};

export function nativePlacementMenuItems({actions: Actions = [], disabled: Disabled, command: Command, select: Select}) {
  const Items = Actions.filter(Action => Action.category === 'Objects').map(Action => {
    let Label = Action.label;
    let Icon = 'Box';
    let Group = 'Другие';
    const Entry = Presentation[Action.id];
    if (Entry) { [Label, Icon, Group] = Entry; }
    if (Action.assetId) { Group = 'Примитивы'; }
    return {
      id: Action.id, label: Label, icon: Icon, group: Group, disabled: Disabled,
      searchText: Action.label + ' ' + Action.id,
      action: async () => {
        let Result;
        if (Action.assetId) { Result = await Command('assets.place', {assetId:Action.assetId,subAssetId:Action.subAssetId}); }
        else { Result = await Command('editor.executeAction', {actionId: Action.id}); }
        if (Result?.id) { await Select(Result.id); }
      },
    };
  });
  const Groups = [['Основные','basic','Box'],['Освещение','lighting','Lightbulb'],['Примитивы','primitives','Shapes'],['Физика','physics','Box'],['Навигация','navigation','Route'],['Другие','other','Plus']];
  return Groups.map(([Label, Identifier, Icon]) => ({id:'placement.'+Identifier,label:Label,icon:Icon,children:Items.filter(Item=>Item.group===Label),disabled:Disabled})).filter(Group=>Group.children.length>0);
}
