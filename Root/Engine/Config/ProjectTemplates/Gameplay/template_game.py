import json
import math
from pathlib import Path
import engine
from template_data import TemplateData, PickingGeometry
from mechanics import Inventory, LockedDoor, Novel, Shooter, HauntedRoom, RayBox, RayMesh, MoveBox, CaptureState, RestoreState


@engine.register_class('game.TemplateController')
class TemplateController(engine.ScriptComponent):
    Scenario = engine.field(str, default='quest')
    ObjectIds = engine.field(str, default='{}')
    MoveSpeed = engine.field(float, default=4.0)
    LookSensitivity = engine.field(float, default=0.003)
    RequireRecord = engine.field(bool, default=True)

    def OnCreate(Self):
        Self.Objects = {Name:engine.FindSceneObject(Self.owner, Identity)
                        for Name, Identity in json.loads(Self.ObjectIds).items()}
        Self.InitialPositions = {Name:Self.Position(Name) for Name in Self.Objects}
        Self.PlayerPosition = list(Self.InitialPositions['player'])
        Self.Yaw = 0.0
        Self.Pitch = 0.0
        if Self.Scenario == 'horror':
            Self.Pitch = -0.16
        if Self.Scenario in ('quest','novel'):
            Camera = TemplateData['scenes'][0]['cameras'][0]
            Difference = [Camera['target'][Axis]-Camera['position'][Axis] for Axis in range(3)]
            Self.Yaw = math.atan2(-Difference[0],-Difference[2])
            Self.Pitch = math.atan2(Difference[1],math.hypot(Difference[0],Difference[2]))
        Self.Message = ''
        Self.StorageStatus = ''
        Self.Inventory = Inventory()
        Self.KeyDoor = LockedDoor(True)
        Self.WarehouseDoor = LockedDoor()
        Self.Novel = Novel()
        Self.Horror = HauntedRoom(Self.RequireRecord)
        Self.Horror.Door.OpenSeconds = 1.0
        EnemyNames = sorted(Name for Name in Self.Objects if Name.startswith('enemy') and Name[5:].isdigit())
        Self.EnemyNames = EnemyNames
        Self.Shooter = Shooter([Self.InitialPositions[Name] for Name in EnemyNames])
        Self.Shooter.Trace = Self.Trace
        Self.Shooter.CanSee = Self.CanSee
        Self.Shooter.Move = Self.MoveEnemy
        Self.StoryStage = 0
        Self.LastOutcome = ''
        Self.AnimationClock = 0.0
        Self.MessageSeconds = 0.0
        Self.PreviousMessage = ''
        Self.JumpVelocity = 0.0
        Self.GroundHeight = Self.PlayerPosition[1]
        Self.SubscriptionIds = []
        if Self.Scenario in ('quest', 'shooter', 'horror'):
            for Action, Callback in [('Fire', Self.Fire), ('Interact', Self.Interact),
                                     ('Reload', Self.Reload), ('Medkit', Self.Medkit),
                                     ('Pistol', Self.Pistol), ('Launcher', Self.Launcher),
                                     ('Jump', Self.Jump)]:
                Self.SubscriptionIds.append(engine.BindAction(Self, '/Game/Input/' + Action + '.inputaction',
                                            engine.InputPhase.Triggered, Callback))
        Self.UpdateScene()
        Self.InitialState = Self.CaptureGame('intro')

    def CaptureGame(Self, StoryNode):
        Values = {Name:CaptureState(getattr(Self,Name)) for Name in ('Inventory','KeyDoor','WarehouseDoor','Novel','Horror','Shooter','PlayerPosition','Yaw','Pitch','StoryStage','LastOutcome','JumpVelocity','AnimationClock')}
        return {'version':1,'scenario':Self.Scenario,'node':StoryNode,'values':Values}

    def RestoreGame(Self, Snapshot):
        if Snapshot['version'] != 1 or Snapshot['scenario'] != Self.Scenario:
            raise ValueError('Save belongs to a different template or version')
        RestoreState(Self,Snapshot['values'])
        engine.RequestStoryNode(Snapshot['node'])
        Self.Place('player',Self.PlayerPosition)
        Self.Place('camera',Self.CameraPosition())
        engine.SetObjectLook(Self.Objects['camera'],Self.Yaw,Self.Pitch)
        Self.UpdateScene()

    def Trace(Self, Origin, Direction, Maximum):
        Filter = engine.PhysicsQueryFilter()
        Filter.LayerMask = 3
        Hit = engine.Raycast(Self.owner, Origin, Direction, Maximum, Filter)
        if Hit is None:
            return Maximum, None
        for Index, Name in enumerate(Self.EnemyNames):
            if Hit['Object']._id == Self.Objects[Name]._id and not Self.Shooter.Enemies[Index].Health.Dead:
                return Hit['Distance'], Index
        return Hit['Distance'], None

    def CanSee(Self, Origin, Target):
        Distance = math.dist(Origin, Target)
        if Distance <= 0.01:
            return True
        Direction = [(Target[Axis] - Origin[Axis]) / Distance for Axis in range(3)]
        Filter = engine.PhysicsQueryFilter()
        Filter.LayerMask = 1
        return engine.Raycast(Self.owner, Origin, Direction, Distance, Filter) is None

    def MoveEnemy(Self, Index, Previous, Desired):
        return Self.MoveCharacter('enemy-'+str(Index+1),Previous,Desired)

    def MoveCharacter(Self, Identifier, Previous, Desired):
        Colliders = []
        Owner = next(Entry for Entry in TemplateData['objects'] if Entry['id']==Identifier)
        for Source in TemplateData['objects']:
            if Source['id']==Identifier or not Source.get('collision',{}).get('enabled',False):
                continue
            Position = Self.Position(Source['id'])
            if Source['id'].startswith('enemy-'):
                Enemy = Self.Shooter.Enemies[int(Source['id'].split('-')[1])-1]
                if Enemy.Health.Dead:
                    continue
                Position = Enemy.Position
            if Source['id'] in ('shooter-hero','horror-player'):
                Position = Self.PlayerPosition
            Placement = Source['transforms'][TemplateData['scenes'][0]['id']]
            Size = [Value*0.5 for Value in Placement['scale']]
            Offset = [0,0,0]
            if Source['type']=='Персонаж':
                Size = [0.6,1.6,0.6]
                Offset = [0,0.8,0]
            if Source['collision'].get('custom'):
                Size = Source['collision']['size']
                Offset = Source['collision'].get('offset',[0,0,0])
            Colliders.append(([Position[Axis]+Offset[Axis]-Size[Axis]/2 for Axis in range(3)],
                              [Position[Axis]+Offset[Axis]+Size[Axis]/2 for Axis in range(3)]))
        Size = Owner['collision'].get('size',[0.6,1.6,0.6])
        Offset = Owner['collision'].get('offset',[0,0.8,0])
        return MoveBox(Previous,[Desired[Axis]-Previous[Axis] for Axis in range(3)],Size,Offset,Colliders)

    def OnDestroy(Self):
        for Subscription in Self.SubscriptionIds:
            engine.Unbind(Subscription)

    def Position(Self, Name):
        Placement = Self.Objects[Name].get_local_transform()
        return [Placement.position.x, Placement.position.y, Placement.position.z]

    def Place(Self, Name, Position):
        Placement = Self.Objects[Name].get_local_transform()
        Placement.position = engine.Vector3(*Position)
        Self.Objects[Name].set_local_transform(Placement)

    def Show(Self, Name, Visible):
        engine.SetObjectActive(Self.Objects[Name], Visible)

    def Direction(Self):
        return [-math.sin(Self.Yaw) * math.cos(Self.Pitch), math.sin(Self.Pitch),
                -math.cos(Self.Yaw) * math.cos(Self.Pitch)]

    def CameraPosition(Self):
        if Self.Scenario in ('quest','novel'):
            return Self.InitialPositions['camera']
        Position = list(Self.PlayerPosition)
        Position[1] += TemplateData['gameplay']['cameraSettings']['eyeHeight']
        if Self.Scenario == 'horror':
            Direction = Self.Direction()
            Position = [Position[Axis] - Direction[Axis] * TemplateData['gameplay']['cameraSettings']['distance'] for Axis in range(3)]
            Origin = [Self.PlayerPosition[0], Self.PlayerPosition[1] + 1.6, Self.PlayerPosition[2]]
            Distance = math.dist(Origin, Position)
            CameraDirection = [(Position[Axis] - Origin[Axis]) / Distance for Axis in range(3)]
            Filter = engine.PhysicsQueryFilter()
            Filter.LayerMask = 1
            Hit = engine.SphereSweep(Self.owner, Origin, 0.15, CameraDirection, Distance, Filter)
            if Hit is not None:
                Position = [Origin[Axis] + CameraDirection[Axis] * max(0.15, Hit['Distance'] - 0.05) for Axis in range(3)]
        return Position

    def Fire(Self, Value):
        if Self.Scenario == 'shooter':
            if Self.Shooter.Fire(Self.CameraPosition(), Self.Direction()):
                Self.Pitch = min(1.3, Self.Pitch + 0.025)
        elif Self.Scenario in ('quest', 'horror'):
            Self.Interact(Value)

    def Reload(Self, Value):
        Self.Shooter.Weapons[Self.Shooter.Selected].Reload(Self.Shooter.Clock, Self.Shooter.Health)

    def Medkit(Self, Value):
        if Self.Shooter.UseMedkit():
            Self.Message = 'Здоровье восстановлено'

    def Pistol(Self, Value):
        Self.Shooter.Selected = 0

    def Launcher(Self, Value):
        Self.Shooter.Selected = 1

    def Jump(Self, Value):
        if Self.Scenario != 'shooter':
            return
        if Self.PlayerPosition[1] <= Self.GroundHeight:
            Self.JumpVelocity = 5.0

    def Interact(Self, Value):
        if Self.Scenario == 'shooter':
            if engine.GetStoryNode() == 'battle' and Self.Shooter.Inventory.Pickup('medkit', 'medkit'):
                Self.Message = 'Предмет в инвентаре'
            return
        if Self.Scenario not in ('quest', 'horror'):
            return
        if Self.Scenario == 'horror' and (Self.StoryStage == 0 or Self.Horror.Ending):
            return
        Point = engine.GetInputValue('/Game/Input/Point.inputaction')
        Forward = Self.Direction()
        Right = [math.cos(Self.Yaw),0,-math.sin(Self.Yaw)]
        Up = [math.sin(Self.Yaw) * math.sin(Self.Pitch),math.cos(Self.Pitch),math.cos(Self.Yaw) * math.sin(Self.Pitch)]
        FieldOfView = TemplateData['scenes'][0]['cameras'][0]['fov']
        if Self.Scenario == 'horror':
            FieldOfView = TemplateData['gameplay']['cameraSettings']['fov']
        Direction = [Forward[Axis] + (Right[Axis] * Point.Horizontal + Up[Axis] * Point.Vertical) * math.tan(math.radians(FieldOfView/2)) for Axis in range(3)]
        Length = math.sqrt(sum(Value * Value for Value in Direction))
        Direction = [Value / Length for Value in Direction]
        Closest = None
        Distance = 400.0
        for Source in TemplateData['objects']:
            Name = Source['id']
            if Source['type'] == 'Источник света' or Name == 'horror-player':
                continue
            if Name == 'horror-fuse' and Self.Horror.Fuse:
                continue
            if Name == 'horror-shadow' and not Self.Horror.Shadow:
                continue
            if Name == 'quest-key' and 'key' in Self.Inventory.Collected:
                continue
            Placement = Source['transforms'][TemplateData['scenes'][0]['id']]
            HalfExtents = [Value * 0.25 for Value in Placement['scale']]
            Center = Self.Position(Name)
            if Source['type'] == 'Персонаж':
                Center[1] += 0.75
                HalfExtents = [0.37,0.75,0.23]
                Hit = RayBox(Self.CameraPosition(),Direction,Center,HalfExtents,Distance)
            else:
                Rotation = [math.radians(Value) for Value in Placement['rotation']]
                Doors = {'quest-door':Self.KeyDoor,'story-door':Self.WarehouseDoor,'horror-exit':Self.Horror.Door}
                if Name in Doors:
                    Rotation = [0,-1.25*Doors[Name].Progress,0]
                Geometry = PickingGeometry['Template'+Source.get('primitive','box').title()]
                Hit = RayMesh(Self.CameraPosition(),Direction,Center,Placement['scale'],Rotation,Geometry,Distance)
            if Hit is not None:
                Closest = Source
                Distance = Hit
        if Closest is not None:
            if Self.Scenario == 'quest':
                Action = {'quest-key':'pickup','quest-door':'door','story-door':'warehouse'}.get(Closest['id'])
                if Action is not None:
                    Self.ExecuteGameAction(Action)
            else:
                Interaction = Closest.get('interactionCall',{})
                for Mechanic in TemplateData['gameplay']['mechanics']:
                    if Mechanic['id'] == Interaction.get('parameters',{}).get('instance',{}).get('id'):
                        Parameters = Mechanic['values']
                        Owner = Mechanic['scope']['id']
                        Self.Horror.Examine(Owner,Parameters['kind'],Parameters.get('text',''),math.dist(Self.PlayerPosition,Self.Position(Owner)),Parameters.get('distance',3.2))

    def OnUpdate(Self, DeltaTime):
        Self.AnimationClock += DeltaTime
        if Self.Scenario in ('shooter', 'horror'):
            CanMove = not Self.Shooter.Outcome
            if Self.Scenario == 'horror':
                CanMove = Self.StoryStage > 0 and not Self.Horror.Ending
            if CanMove:
                Look = engine.GetInputValue('/Game/Input/Look.inputaction')
                if Self.Scenario != 'horror' or engine.GetInputValue('/Game/Input/LookEnable.inputaction').Horizontal != 0:
                    Self.Yaw -= Look.Horizontal * Self.LookSensitivity
                    Self.Pitch = max(-1.5, min(1.5, Self.Pitch - Look.Vertical * Self.LookSensitivity))
                PreviousPosition = list(Self.PlayerPosition)
                Move = engine.GetInputValue('/Game/Input/Move.inputaction')
                Magnitude = max(1.0, math.hypot(Move.Horizontal, Move.Vertical))
                Self.PlayerPosition[0] += (math.cos(Self.Yaw) * Move.Horizontal - math.sin(Self.Yaw) * Move.Vertical) * Self.MoveSpeed * DeltaTime / Magnitude
                Self.PlayerPosition[2] += (-math.sin(Self.Yaw) * Move.Horizontal - math.cos(Self.Yaw) * Move.Vertical) * Self.MoveSpeed * DeltaTime / Magnitude
                Self.JumpVelocity -= 16.0 * DeltaTime
                Self.PlayerPosition[1] = max(Self.GroundHeight, Self.PlayerPosition[1] + Self.JumpVelocity * DeltaTime)
                Identifier = 'shooter-hero'
                if Self.Scenario == 'horror':
                    Identifier = 'horror-player'
                Self.PlayerPosition = Self.MoveCharacter(Identifier,PreviousPosition,Self.PlayerPosition)
                if Self.PlayerPosition[1] <= Self.GroundHeight:
                    Self.JumpVelocity = 0.0
            Self.Place('player', Self.PlayerPosition)
            Self.Place('camera', Self.CameraPosition())
            engine.SetObjectLook(Self.Objects['camera'], Self.Yaw, Self.Pitch)
        Self.KeyDoor.Tick(DeltaTime)
        Self.WarehouseDoor.Tick(DeltaTime)
        Self.Horror.Tick(DeltaTime)
        if Self.Scenario == 'shooter':
            Self.Shooter.Tick(DeltaTime, Self.PlayerPosition)
            if Self.Shooter.Outcome and Self.LastOutcome != Self.Shooter.Outcome:
                Self.LastOutcome = Self.Shooter.Outcome
                Destination = 'victory'
                if Self.Shooter.Health.Dead:
                    Destination = 'defeat'
                engine.RequestStoryNode(Destination)
        CurrentMessage = Self.Message
        if Self.Scenario == 'horror':
            CurrentMessage = Self.Horror.Message
        if CurrentMessage != Self.PreviousMessage:
            Self.PreviousMessage = CurrentMessage
            Self.MessageSeconds = 3.0
        Self.MessageSeconds = max(0.0,Self.MessageSeconds-DeltaTime)
        Self.UpdateScene()

    def UpdateScene(Self):
        if Self.Scenario == 'quest':
            Self.Show('key', 'key' not in Self.Inventory.Collected)
            for Name, Door in [('door', Self.KeyDoor), ('warehouse', Self.WarehouseDoor)]:
                Position = list(Self.InitialPositions[Name])
                engine.SetObjectLook(Self.Objects[Name],-1.25 * Door.Progress,0)
        if Self.Scenario == 'shooter':
            Self.Show('player',False)
            for Name, Enemy in zip(Self.EnemyNames, Self.Shooter.Enemies):
                Self.Place(Name, Enemy.Position)
                Self.Show(Name, not Enemy.Health.Dead)
                if Enemy.State == 'Chase':
                    engine.SetObjectLook(Self.Objects[Name],math.atan2(Self.PlayerPosition[0]-Enemy.Position[0],Self.PlayerPosition[2]-Enemy.Position[2]),0)
            Self.Show('medkit', 'medkit' not in Self.Shooter.Inventory.Collected)
            Tracer = Self.Shooter.Tracer
            Self.Show('tracer', Tracer is not None and Self.Shooter.Clock < Tracer[2])
            if Tracer is not None:
                Origin, Target, Expiration = Tracer
                Length = math.dist(Origin, Target)
                if Length > 0.001:
                    Placement = Self.Objects['tracer'].get_local_transform()
                    Placement.position = engine.Vector3(*[(Origin[Axis] + Target[Axis]) * 0.5 for Axis in range(3)])
                    Placement.scale = engine.Vector3(0.02, 0.02, Length)
                    Self.Objects['tracer'].set_local_transform(Placement)
                    engine.SetObjectLook(Self.Objects['tracer'], math.atan2(Origin[0] - Target[0], Origin[2] - Target[2]), math.asin((Target[1] - Origin[1]) / Length))
            for Index in range(8):
                Name = 'projectile' + str(Index)
                Visible = Index < len(Self.Shooter.Projectiles)
                Self.Show(Name, Visible)
                if Visible:
                    Self.Place(Name, Self.Shooter.Projectiles[Index]['Position'])
        if Self.Scenario == 'horror':
            Move = engine.GetInputValue('/Game/Input/Move.inputaction')
            engine.SetObjectLook(Self.Objects['player'],Self.Yaw+math.pi,0)
            Self.Show('fuse', not Self.Horror.Fuse)
            Self.Show('shadow', Self.Horror.Shadow)
            Self.Show('lamp', Self.Horror.Power)
            Position = list(Self.InitialPositions['exit'])
            engine.SetObjectLook(Self.Objects['exit'],-1.25 * Self.Horror.Door.Progress,0)

    @engine.method(Parameters={'Action':str})
    def ExecuteGameAction(Self, Action):
        if Action in ('save','load','restart'):
            Self.StorageStatus = ''
            SavePath = Path(__file__).resolve().parents[2] / 'Saved' / 'Gameplay.json'
            if Action == 'restart':
                Self.RestoreGame(json.loads(json.dumps(Self.InitialState)))
                return
            try:
                if Action == 'save':
                    SavePath.parent.mkdir(parents=True,exist_ok=True)
                    Temporary = SavePath.with_suffix('.tmp')
                    Temporary.write_text(json.dumps(Self.CaptureGame(engine.GetStoryNode()),ensure_ascii=False),encoding='utf-8')
                    Temporary.replace(SavePath)
                    Self.Message = 'Игра сохранена.'
                    Self.StorageStatus = 'saved'
                else:
                    Self.RestoreGame(json.loads(SavePath.read_text(encoding='utf-8')))
                    Self.Message = 'Игра загружена.'
                    Self.StorageStatus = 'loaded'
            except FileNotFoundError:
                Self.Message = 'Сохранение не найдено.'
            except (OSError,ValueError,KeyError,TypeError) as Failure:
                Self.Message = 'Не удалось прочитать или записать сохранение.'
                print('Gameplay save failed: '+str(Failure))
            Self.PreviousMessage = Self.Message
            Self.MessageSeconds = 3.0
            return
        if Self.Scenario == 'quest':
            if Action == 'pickup':
                if Self.Inventory.Pickup('key', 'key'):
                    Self.Message = 'Предмет в инвентаре'
            elif Action == 'door':
                if not Self.KeyDoor.Interact(Self.Inventory):
                    Self.Message = 'Заперто'
            elif Action == 'warehouse':
                if not Self.WarehouseDoor.Interact(Self.Inventory):
                    Self.Message = 'Заперто'
        elif Self.Scenario == 'shooter':
            if Action == 'stop':
                Self.Shooter.Stop()
            elif Action == 'medkit':
                Self.Medkit(None)
        elif Self.Scenario == 'horror':
            if Action == 'interact':
                Self.Interact(None)
        Self.UpdateScene()

    @engine.method(Parameters={'Action':str}, Result=str)
    def RunStoryAction(Self, Action):
        if Action == 'BeginExploration':
            Self.StoryStage = 1
        elif Action == 'UnlockWarehouse':
            Self.WarehouseDoor.SetLocked(False)
        elif Action == 'BeginOpenWarehouse':
            Self.WarehouseDoor.Open()
        elif Action == 'OpenWarehouse':
            if Self.WarehouseDoor.Opening:
                return 'Waiting'
        elif Action == 'StartBattle':
            Self.StoryStage = 1
            Self.Shooter.Start()
        elif Action == 'WaitBattle':
            if not Self.Shooter.Outcome:
                return 'Waiting'
            if Self.Shooter.Health.Dead:
                return 'Defeat'
        elif Action == 'WaitEscape':
            if not Self.Horror.Ending:
                return 'Waiting'
            if not Self.Horror.Escaped:
                return 'Defeat'
            if Self.Horror.Door.Progress < 1.0:
                return 'Waiting'
        elif Action == 'AwardSupport':
            Self.Novel.Reward.Award()
        elif Action == 'Snapshot':
            Self.Novel.Snapshot = Self.Novel.Relationship.Value
        elif Action == 'CheckRelationship':
            if Self.Novel.Relationship.Value >= 1:
                return 'Available'
            return 'Unavailable'
        return 'Done'

    @engine.method(Result=str)
    def GetGameView(Self):
        View = {'title':'', 'values':[], 'actions':[], 'text':Self.Message}
        if Self.Scenario == 'quest':
            View['title'] = 'Две двери'
            View['values'] = [{'label':'Ключи', 'value':Self.Inventory.Count('key')}]
        elif Self.Scenario == 'novel':
            View['title'] = 'Отношение'
            View['values'] = [{'label':'Сейчас', 'value':Self.Novel.Relationship.Value},
                              {'label':'Запомнено', 'value':Self.Novel.Snapshot}]
        elif Self.Scenario == 'shooter':
            WeaponInstance = Self.Shooter.Weapons[Self.Shooter.Selected]
            View['title'] = 'Учебный бой'
            View['reticle'] = True
            View['pointerLock'] = not Self.Shooter.Outcome
            View['text'] = Self.Shooter.Outcome
            View['values'] = [{'label':'HP', 'value':Self.Shooter.Health.Current},
                              {'label':'Патроны', 'value':str(WeaponInstance.Ammo) + '/' + str(WeaponInstance.Reserve)},
                              {'label':'Очки', 'value':Self.Shooter.Score.Value},
                              {'label':'Аптечки', 'value':Self.Shooter.Inventory.Count('medkit')}]
            if WeaponInstance.ReloadUntil is not None:
                View['text'] = 'Перезарядка'
        elif Self.Scenario == 'horror':
            View['title'] = 'Последний обход'
            View['text'] = Self.Horror.Message
            View['objective'] = Self.Horror.Objective
            Labels = {True:'да', False:'нет'}
            View['values'] = [{'label':'Предохранитель', 'value':Labels[Self.Horror.Fuse]},
                              {'label':'Питание', 'value':Labels[Self.Horror.Power]},
                              {'label':'Запись', 'value':Labels[Self.Horror.Record]}]
        View['variables'] = {
            'Health':float(Self.Shooter.Health.Current), 'Score':float(Self.Shooter.Score.Value),
            'Ammunition':Self.Shooter.Weapons[Self.Shooter.Selected].Ammo,
            'Relationship':float(Self.Novel.Relationship.Value), 'Snapshot':float(Self.Novel.Snapshot),
            'Objective':Self.Horror.Objective
        }
        View['actions'] = [{'id':Action, 'label':Action, 'enabled':True}
                           for Action in ('pickup', 'door', 'warehouse', 'medkit', 'stop')]
        for Action,Label in [('save','Сохранить'),('load','Загрузить'),('restart','Начать заново')]:
            View['actions'].append({'id':Action,'label':Label,'enabled':True,'allowWhilePaused':True})
        View['projectTitle'] = TemplateData.get('name','')
        Endings = [Beat for Chapter in TemplateData['chapters'] for Beat in Chapter['beats'] if Beat['kind'] == 'end']
        Ending = Endings[0]
        if Self.Scenario == 'shooter' and Self.Shooter.Health.Dead:
            Ending = next(Beat for Beat in Endings if Beat['id'] == 'shooter-game-over')
        View['ending'] = Ending.get('ending','')
        View['input'] = TemplateData['input']
        View['controllerMode'] = TemplateData['gameplay']['controller']['mode']
        View['storageStatus'] = Self.StorageStatus
        if Self.Scenario == 'novel' and engine.GetStoryNode() == 'cold':
            View['dialogueChoices'] = [{'id':'-1','label':'Поддержать ещё раз','enabled':False},{'id':'0','label':'Попрощаться','enabled':True}]
        View['message'] = ''
        if Self.MessageSeconds > 0:
            View['message'] = Self.PreviousMessage
        if not View['message'] and Self.Scenario == 'shooter' and engine.GetStoryNode() == 'battle':
            View['message'] = 'Победите цели задания.'
        View['reticle'] = Self.Scenario == 'shooter'
        return json.dumps(View, ensure_ascii=False)


@engine.register_node(type_id='game.TemplateAction', label='Действие примера')
class TemplateAction(engine.NarrativeNode):
    Inputs = {'Controller':engine.pin('engine.ObjectHandle'), 'Action':engine.pin(str, Default='')}
    Exits = ('Done', 'Defeat', 'Available', 'Unavailable')

    def Execute(Self, Context, Controller, Action):
        if Action == 'OpenWarehouse':
            Context.Invoke(Controller, 'RunStoryAction', Action='BeginOpenWarehouse')
        Result = Context.Invoke(Controller, 'RunStoryAction', Action=Action)
        while Result == 'Waiting':
            yield Context.Wait(0.05)
            Result = Context.Invoke(Controller, 'RunStoryAction', Action=Action)
        return engine.NarrativeResult(Result)
