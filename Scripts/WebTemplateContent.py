import json
import math


def BuildReferenceObjects(Scenario, Reference, ModelDirectory, Objects, ObjectIds, Object, Component):
    Objects.clear()
    ObjectIds.clear()
    Aliases = {
        'quest':{'quest-key':'key','quest-door':'door','story-door':'warehouse'},
        'shooter':{'shooter-hero':'player','arena-medkit':'medkit',**{'enemy-'+str(Index+1):'enemy'+str(Index) for Index in range(5)}},
        'horror':{'horror-player':'player','horror-fuse':'fuse','horror-power':'power','horror-tape':'record','horror-exit':'exit','horror-shadow':'shadow','horror-alarm-light':'lamp','horror-mirror':'mirror','horror-journal':'journal','horror-cabinet':'cabinet','horror-bell':'bell','horror-desk':'desk'},
        'novel':{}
    }[Scenario]
    Scene = Reference['scenes'][0]
    CameraData = Scene['cameras'][0]
    Camera = Object('camera',CameraData['position'],Mesh=False)
    Difference = [CameraData['target'][Axis]-CameraData['position'][Axis] for Axis in range(3)]
    Yaw = math.atan2(-Difference[0],-Difference[2])
    Pitch = math.atan2(Difference[1], math.hypot(Difference[0],Difference[2]))
    Camera['transform']['rotation'] = Quaternion(Yaw,Pitch,0)
    FieldOfView = CameraData['fov']
    if Scenario in ('shooter','horror'):
        FieldOfView = Reference['gameplay']['cameraSettings']['fov']
    Camera['components'].append(Component('camera','engine.CameraComponent',{'primary':True,'field_of_view':FieldOfView}))
    Sun = Object('sun',[-4,9,3],Mesh=False)
    Sun['transform']['rotation'] = Quaternion(-0.927,-1.064,0)
    SunColor = [1,0.965,0.875,1]
    SunIntensity = 3.2
    SkyIntensity = 1.7
    if Scene.get('time') == 'Ночь':
        SunColor = [0.471,0.592,0.816,1]
        SunIntensity = 0.65
        SkyIntensity = 0.85
    Sun['components'].append(Component('sun/light','engine.LightComponent',{'light_type':'Directional','intensity':SunIntensity,'light_color':SunColor,'cast_shadows':False}))
    Sky = Object('sky',[0,0,0],Mesh=False)
    Sky['components'].append(Component('sky/light','engine.SkyLightComponent',{'intensity':SkyIntensity,'light_color':[0.773,0.843,0.918,1]}))
    Background = '#89bddb'
    if Scene.get('time') == 'Ночь':
        Background = '#182a43'
    if Scene.get('time') == 'Рассвет':
        Background = '#a6b9c6'
    if Scene.get('weather') == 'Туман':
        Background = '#899da4'
    BackgroundColor = [BackgroundRadiance(int(Background[Start:Start+2],16)/255) for Start in (1,3,5)]+[1]
    Sky['components'].append(Component('sky/background','engine.SkySphereComponent',{'zenith_color':BackgroundColor,'horizon_color':BackgroundColor,'ground_color':BackgroundColor,'sun_intensity':0,'brightness':1}))
    for Source in Reference['objects']:
        Name = Aliases.get(Source['id'],Source['id'])
        Placement = Source['transforms'][Scene['id']]
        Character = Source['type'] == 'Персонаж'
        Light = Source['type'] == 'Источник света'
        Scale = list(Placement['scale'])
        Primitive = Source.get('primitive','box')
        Native = Object(Name,Placement['position'],Scale,Source.get('color','#94785b'),not Light and not Character)
        Native['name'] = Source['name']
        Rotation = [math.radians(Value) for Value in Placement['rotation']]
        Native['transform']['rotation'] = Quaternion(Rotation[1],Rotation[0],Rotation[2])
        ObjectIds[Source['id']] = Native['id']
        if Light:
            Color = Source.get('color','#ffffff')
            Native['components'].append(Component(Name+'/light','engine.LightComponent',{
                'light_type':'Point','intensity':Source.get('light',{}).get('intensity',1.0),
                'range':Source.get('light',{}).get('distance',10.0),
                'cast_shadows':Source.get('light',{}).get('castShadow',False),
                'light_color':[int(Color[Start:Start+2],16)/255 for Start in (1,3,5)]+[1]}))
        elif Character:
            CharacterRoot = ModelDirectory.parents[4] / 'Content/Characters/StarterHumanoid'
            CharacterMetadata = json.loads((CharacterRoot/'StarterHumanoid.character.meta').read_text())
            Visual = Object(Name+'/visual',[0,0,0],[0.83,0.83,0.83],Mesh=False)
            Visual['parent'] = Native['id']
            Slots = []
            for Index in range(2):
                Metadata = json.loads((CharacterRoot/('HumanoidMaterial'+str(Index)+'.materialinstance.meta')).read_text())
                Slots.append({'slot':Index,'material':{'assetId':Metadata['guid'],'subAssetId':None}})
            Visual['components'].append(Component(Name+'/character','engine.CharacterComponent',{
                'description':Source['name'],'character':{'assetId':CharacterMetadata['guid'],'subAssetId':None},
                'material_slots':{'entries':Slots},'automatic_walk':True}))
        else:
            Model = 'Template'+Primitive.title()
            Metadata = json.loads((ModelDirectory / (Model+'.glb.meta')).read_text())
            Native['components'][0]['properties'].update({'mesh_asset_id':Metadata['guid'],'mesh_sub_asset_id':Metadata['subAssets'][0]['id']})
        Collision = Source.get('collision',{})
        if Collision.get('enabled',False):
            Motion = 'Static'
            Layer = 1
            if Character:
                Motion = 'Kinematic'
                Layer = 2
                if Name == 'player':
                    Layer = 4
            HalfExtents = [0.25,0.25,0.25]
            Center = [0,0,0]
            if Collision.get('custom'):
                HalfExtents = [Value / 2 for Value in Collision['size']]
                Center = Collision.get('offset',Center)
            Native['components'].append(Component(Name+'/physics','engine.PhysicsBodyComponent',{
                'use_mesh_defaults':False,'motion_type':Motion,'shape_type':'Box',
                'half_extents':HalfExtents,'center':Center,'collision_layer':Layer,'use_gravity':False}))
    if 'player' not in ObjectIds:
        Object('player',[0,0,0],Mesh=False)
    if Scenario == 'shooter':
        Object('tracer',[0,-10,0],[0.02,0.02,1],'key')
        for Index in range(8):
            Object('projectile'+str(Index),[0,-10,0],[0.12,0.12,0.12],'key')
    return Camera


def Quaternion(Yaw, Pitch, Roll):
    return [math.cos(Yaw/2)*math.sin(Pitch/2)*math.cos(Roll/2)+math.sin(Yaw/2)*math.cos(Pitch/2)*math.sin(Roll/2),
            math.sin(Yaw/2)*math.cos(Pitch/2)*math.cos(Roll/2)-math.cos(Yaw/2)*math.sin(Pitch/2)*math.sin(Roll/2),
            math.cos(Yaw/2)*math.cos(Pitch/2)*math.sin(Roll/2)-math.sin(Yaw/2)*math.sin(Pitch/2)*math.cos(Roll/2),
            math.cos(Yaw/2)*math.cos(Pitch/2)*math.cos(Roll/2)+math.sin(Yaw/2)*math.sin(Pitch/2)*math.sin(Roll/2)]


def BackgroundRadiance(Channel):
    Linear = Channel/12.92
    if Channel>0.04045:
        Linear = ((Channel+0.055)/1.055)**2.4
    Quadratic = 2.43*Linear-2.51
    FirstOrder = 0.59*Linear-0.03
    return (-FirstOrder-math.sqrt(FirstOrder*FirstOrder-4*Quadratic*0.14*Linear))/(2*Quadratic)
