import json
import uuid
from pathlib import Path
from WebTemplateContent import BuildReferenceObjects
import GeneratePrimitiveModels


Repository = Path(__file__).resolve().parents[1]
TemplateRoot = Repository / 'Root/Engine/Config/ProjectTemplates'


def Identity(Name):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, 'sacura-template/' + Name))


def Write(PathValue, Value, AssetType=None):
    PathValue.parent.mkdir(parents=True, exist_ok=True)
    PathValue.write_text(json.dumps(Value, ensure_ascii=False, indent=2), encoding='utf-8')
    if AssetType:
        Metadata = {'schemaVersion':1, 'guid':Identity(str(PathValue.relative_to(TemplateRoot))),
                    'assetType':AssetType, 'loadSettings':{}, 'subAssets':[], 'importInfo':{}}
        PathValue.with_name(PathValue.name + '.meta').write_text(json.dumps(Metadata, indent=2), encoding='utf-8')


def Generate(Scenario):
    Destination = TemplateRoot / ('builtin-' + Scenario)
    Content = Destination / 'Content'
    Reference = json.loads((Repository / 'Root/Verification/WebTemplateOracle' / (Scenario + '.json')).read_text(encoding='utf-8'))
    Objects = []
    ObjectIds = {}
    Palette = {'floor':[0.18,0.2,0.23,1], 'wall':[0.34,0.38,0.4,1],
               'door':[0.3,0.53,0.38,1], 'key':[0.85,0.65,0.25,1],
               'enemy':[0.7,0.22,0.2,1], 'player':[0.4,0.55,0.8,1],
               'white':[0.8,0.82,0.78,1], 'blue':[0.2,0.35,0.65,1]}
    for Color in ('#dfc6af','#343139','#775150'):
        Palette[Color] = [int(Color[Start:Start+2],16)/255 for Start in (1,3,5)] + [1]
    for Entry in Reference['objects']:
        Color = Entry.get('color','#94785b')
        Palette[Color] = [int(Color[Start:Start+2],16)/255 for Start in (1,3,5)] + [1]
    Materials = {}
    for Name, Color in Palette.items():
        Parent = 'StandardPBR'
        if Name in ('#343139','#775150'):
            Parent = 'Unlit'
        Converted = []
        for Channel in Color[:3]:
            if Channel <= 0.04045:
                Converted.append(Channel/12.92)
            else:
                Converted.append(((Channel+0.055)/1.055)**2.4)
        ParentMetadata = json.loads((Repository / ('Root/Engine/Content/Materials/'+Parent+'.material.meta')).read_text())
        Material = {'schemaVersion':1,'parent':{'assetId':ParentMetadata['guid'],'subAssetId':None},
                    'overrides':{'BaseColor':{'type':'Color','value':Converted+[1]}}}
        if Parent == 'StandardPBR':
            Material['overrides']['Roughness'] = {'type':'Float','value':0.8}
        PathValue = Content / 'Materials' / (Name + '.materialinstance')
        Write(PathValue, Material, 'MaterialInstance')
        Materials[Name] = Identity(str(PathValue.relative_to(TemplateRoot)))
        for Suffix in ('.material','.material.meta'):
            Previous = Content / 'Materials' / (Name+Suffix)
            if Previous.is_file():
                Previous.unlink()

    def Component(Name, Type, Properties):
        return {'id':Identity(Scenario + '/component/' + Name), 'type':Type, 'typeVersion':1,
                'bEnabled':True, 'properties':Properties}

    def Object(Name, Position, Scale=(1,1,1), Color='wall', Mesh=True):
        ObjectId = Identity(Scenario + '/object/' + Name)
        ObjectIds[Name] = ObjectId
        Components = []
        if Mesh:
            Components.append(Component(Name + '/mesh', 'engine.MeshRendererComponent', {
                'mesh_asset_id':'5bcbb1a6-db24-4248-b7d9-40555c563724',
                'mesh_sub_asset_id':'997a9fc2-2f8e-4e5d-82a1-af384af37962',
                'material_asset_id':Materials[Color], 'visible':True}))
        Result = {'id':ObjectId, 'name':Name, 'parent':None, 'bActive':True, 'bVisual':True,
                  'transform':{'position':Position, 'rotation':[0,0,0,1], 'scale':Scale}, 'components':Components}
        Objects.append(Result)
        return Result

    ModelDirectory = Content / 'Models'
    ModelDirectory.mkdir(parents=True,exist_ok=True)
    GeneratePrimitiveModels.CONTENT = ModelDirectory
    GeneratePrimitiveModels.NAMESPACE = uuid.uuid5(uuid.NAMESPACE_URL,'sacura-template/'+Scenario+'/models')
    Geometries = json.loads((Repository / 'Root/Verification/WebTemplateOracle/geometry.json').read_text())
    for Name, Geometry in Geometries.items():
        Vertices = [(Geometry['positions'][Offset:Offset+3],Geometry['normals'][Offset:Offset+3],Geometry['coordinates'][Offset//3*2:Offset//3*2+2]) for Offset in range(0,len(Geometry['positions']),3)]
        GeneratePrimitiveModels.write_model(Name,(Vertices,Geometry['indices']))
    Camera = BuildReferenceObjects(Scenario, Reference, ModelDirectory, Objects, ObjectIds, Object, Component)
    ControllerId = Identity(Scenario + '/component/controller')
    Camera['components'].append(Component('controller', 'game.TemplateController', {
        'Scenario':Scenario, 'ObjectIds':json.dumps(ObjectIds), 'MoveSpeed':Reference['gameplay']['controller']['speed'],
        'LookSensitivity':Reference['gameplay']['cameraSettings']['sensitivity'], 'RequireRecord':True}))
    Write(Content / 'Scenes/Main.scene', {'format':'sakura.scene','formatVersion':1,'name':Scenario,'objects':Objects}, 'Scene')

    ScriptDirectory = Content / 'Scripts'
    ScriptDirectory.mkdir(exist_ok=True)
    PickingGeometry = {Name:{Field:Geometry[Field] for Field in ('positions','indices')} for Name,Geometry in Geometries.items() if Name in ('TemplateBox','TemplateSphere','TemplateCylinder')}
    (ScriptDirectory / 'template_data.py').write_text('import json\nTemplateData = json.loads(' + repr(json.dumps(Reference,ensure_ascii=False)) + ')\nPickingGeometry = json.loads(' + repr(json.dumps(PickingGeometry)) + ')\n',encoding='utf-8')

    Nodes = []
    def Line(Identifier, Text, Next):
        Nodes.append({'id':Identifier,'kind':'Line','speaker':'Рассказчик','text':Text,'nextNodeId':Next})
    def Choice(Identifier, Text, Options):
        Nodes.append({'id':Identifier,'kind':'Choice','speaker':'Рассказчик','prompt':Text,'choices':[
            {'id':Identity(Scenario + '/' + Identifier + '/' + Label),'label':Label,'nextNodeId':Next} for Label, Next in Options]})
    def Action(Identifier, Operation, Next, **Exits):
        Routes = {'Done':Next, **Exits}
        Nodes.append({'id':Identifier,'kind':'Narrative','narrativeType':'game.TemplateAction',
                      'inputs':{'Controller':{'object':ControllerId},'Action':Operation}, 'sources':{},
                      'choices':[{'id':Name,'label':Name,'nextNodeId':Target} for Name, Target in Routes.items()]})
    Beats = {Entry['id']:Entry for Entry in Reference['chapters'][0]['beats']}
    if Scenario == 'quest':
        Line('intro','Левая дверь открывается ключом. Правая пока заперта.','unlock')
        Action('unlock','UnlockWarehouse','unlocked')
        Line('unlocked','Смотритель разблокировал склад. Теперь можно открыть правую дверь.','next')
        Line('next','Замок не сбросился. Следующая реплика отдельно открывает склад.','open')
        Action('open','OpenWarehouse','opened')
        Line('opened','Дверь открыта после завершения анимации.','end')
        Line('end',Beats['quest-end']['text'],'finished')
    elif Scenario == 'shooter':
        Line('intro',Beats['shooter-intro']['text'],'start')
        Action('start','StartBattle','combat')
        Line('combat',Beats['shooter-combat']['text'],'battle')
        Action('battle','WaitBattle','victory',Defeat='defeat')
        Nodes[-1]['text'] = Beats['shooter-watch']['text']
        Line('victory',Beats['shooter-win']['text'],'end')
        Line('defeat',Beats['shooter-game-over']['text'],'end')
    elif Scenario == 'novel':
        Choice('intro','Как ответить?',[('Поддержать','support'),('Ответить холодно','gate')])
        Action('support','AwardSupport','snapshot')
        Action('snapshot','Snapshot','thanks')
        Line('thanks','Спасибо за поддержку. Я запомню этот момент.','gate')
        Action('gate','CheckRelationship','end',Available='personal',Unavailable='cold')
        Choice('personal','Продолжим разговор?',[('Поддержать ещё раз','second'),('Попрощаться','goodbye')])
        Choice('cold','Продолжим разговор?',[('Попрощаться','goodbye')])
        Action('second','AwardSupport','warm')
        Line('warm','Теперь доверия ещё больше.','end')
        Line('goodbye',Beats['novel-end']['text'],'finished')
        Line('end',Beats['novel-end']['text'],'finished')
    elif Scenario == 'horror':
        Line('intro',Beats['horror-intro']['text'],'enable')
        Action('enable','BeginExploration','explore')
        Action('explore','WaitEscape','escaped')
        Line('escaped',Beats['horror-ending']['text'],'end')
    if Scenario not in ('quest','novel'):
        Nodes.append({'id':'end','kind':'End'})
    Nodes.append({'id':'finished','kind':'End'})
    Write(Content / 'Stories/Opening.story', {'format':'sakura.story','formatVersion':1,'name':Scenario,'startNodeId':'intro','nodes':Nodes}, 'Story')

    if Scenario in ('quest','shooter','horror'):
        Bindings = []
        ActionDefinitions = [('Move',2,3),('Look',2,3),('Point',2,3),('LookEnable',0,3),('Fire',0,0),('Interact',0,0),
                             ('Reload',0,0),('Medkit',0,0),('Pistol',0,0),('Launcher',0,0),('Jump',0,0)]
        ActionIds = {}
        for Name, ValueType, Trigger in ActionDefinitions:
            PathValue = Content / 'Input' / (Name + '.inputaction')
            Write(PathValue, {'Version':1,'Action':{'Name':Name,'ValueType':ValueType,'Trigger':Trigger,'bClampMagnitude':Name not in ('Look','Point'),'bRequireNeutral':Name != 'Point'}}, 'InputAction')
            ActionIds[Name] = Identity(str(PathValue.relative_to(TemplateRoot)))
        Mapping = {'move':'Move','interact':'Interact','point':'Fire','fire':'Fire','reload':'Reload','medkit':'Medkit','select-0':'Pistol','select-1':'Launcher','look':'Look','horror-look':'Look'}
        Controls = [('Point','Mouse','Position',[1,1,1],[0,1,2]),('LookEnable','Mouse','Secondary',[1,1,1],[0,1,2])]
        Buttons = ['South','East','West','North','LeftShoulder','RightShoulder','LeftTrigger','RightTrigger','Back','Start','LeftStick','RightStick','DpadUp','DpadDown','DpadLeft','DpadRight']
        for Context in Reference['input']['contexts']:
            if Context['id'] in ('system','dialogue'):
                continue
            for Binding in Context['bindings']:
                Name = Mapping.get(Binding['actionId'])
                if Name is None:
                    continue
                Device = 'Keyboard'
                Control = Binding['control']
                if Binding['device']=='touch':
                    Device = 'Touch'
                if Binding['device']=='pointer':
                    Device = 'Mouse'
                    Control = {'primary':'Primary','delta':'Delta'}[Control]
                if Binding['device']=='gamepad-axis':
                    Device = 'Gamepad'
                    Control = ['LeftX','LeftY','RightX','RightY'][int(Control)]
                if Binding['device']=='gamepad-button':
                    Device = 'Gamepad'
                    Control = Buttons[int(Control)]
                Scale = [1,1,1]
                AxisOrder = [0,1,2]
                SourceScale = Binding.get('scale',1)
                if isinstance(SourceScale,list):
                    if SourceScale[0]==0:
                        AxisOrder = [1,0,2]
                        Scale[1] = SourceScale[1]
                    else:
                        Scale[0] = SourceScale[0]
                        if len(SourceScale)>1 and SourceScale[1]!=0:
                            Scale[1] = SourceScale[1]
                Controls.append((Name,Device,Control,Scale,AxisOrder))
        for Name, Device, Control, Scale, AxisOrder in Controls:
            Bindings.append({'Identifier':Name + Device + Control,'Action':ActionIds[Name], 'Device':Device,
                             'Control':Control,'Modifiers':{'Scale':dict(zip(['Horizontal','Vertical','Depth'],Scale)), 'AxisOrder':AxisOrder}})
        Write(Content / 'Input/Gameplay.inputcontext', {'Version':1,'Context':{'Name':'Gameplay','Bindings':Bindings}}, 'InputMappingContext')
    (Destination / 'README.md').write_text('''# Gameplay template

Open Content/Stories/Opening.story to edit the story and its Python actions.
Content/Scripts/mechanics.py contains the game rules. Content/Scripts/template_game.py
connects those rules to scene objects, Enhanced Input and the shared gameplay HUD.
Stop Play and reopen the project in a new process after editing Python.
The template source is copied into this project; it is not a runtime engine preset.
''', encoding='utf-8')


if __name__ == '__main__':
    for Scenario in ('quest','shooter','novel','horror'):
        Generate(Scenario)
