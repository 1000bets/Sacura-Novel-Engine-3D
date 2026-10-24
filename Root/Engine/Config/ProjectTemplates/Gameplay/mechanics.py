import math


def CaptureState(Value):
    if isinstance(Value,set):
        return sorted(Value)
    if isinstance(Value,list):
        return [CaptureState(Entry) for Entry in Value]
    if isinstance(Value,dict):
        return {Name:CaptureState(Entry) for Name,Entry in Value.items()}
    if hasattr(Value,'__dict__'):
        return {Name:CaptureState(Entry) for Name,Entry in vars(Value).items() if Name not in ('Destination','Trace','CanSee','Move')}
    return Value


def RestoreState(Target, Saved):
    for Name,Value in Saved.items():
        Current = getattr(Target,Name)
        if isinstance(Current,set):
            setattr(Target,Name,set(Value))
        elif hasattr(Current,'__dict__'):
            RestoreState(Current,Value)
        elif isinstance(Current,list) and Current and hasattr(Current[0],'__dict__'):
            for Entry,Snapshot in zip(Current,Value):
                RestoreState(Entry,Snapshot)
        else:
            setattr(Target,Name,Value)


class Health:
    def __init__(Self, Maximum=100.0, Armor=0.0):
        Self.Maximum = Maximum
        Self.Armor = Armor
        Self.Current = Maximum

    @property
    def Dead(Self):
        return Self.Current <= 0.0

    def Damage(Self, Amount):
        if Self.Dead:
            return False
        Self.Current = max(0.0, Self.Current - max(0.0, Amount - Self.Armor))
        return True

    def Heal(Self, Amount):
        if Self.Dead:
            return False
        Self.Current = min(Self.Maximum, Self.Current + max(0.0, Amount))
        return True


class Counter:
    def __init__(Self, Value=0.0):
        Self.Value = Value

    def Add(Self, Amount):
        Self.Value += Amount
        return Self.Value


class Reward:
    def __init__(Self, Destination, Base=1.0, Multiplier=1.0):
        Self.Destination = Destination
        Self.Base = Base
        Self.Multiplier = Multiplier
        Self.Awards = 0

    def Award(Self, Factor=1.0):
        Self.Awards += 1
        return Self.Destination.Add(Self.Base * Self.Multiplier * Factor)


class Inventory:
    def __init__(Self):
        Self.Items = {}
        Self.Collected = set()

    def Count(Self, Item):
        return Self.Items.get(Item, 0)

    def Pickup(Self, ObjectId, Item):
        if ObjectId in Self.Collected:
            return False
        Self.Collected.add(ObjectId)
        Self.Items[Item] = Self.Count(Item) + 1
        return True

    def Remove(Self, Item):
        if Self.Count(Item) < 1:
            return False
        Self.Items[Item] -= 1
        return True


class LockedDoor:
    def __init__(Self, RequiresKey=False, ConsumeKey=False, OpenSeconds=0.5):
        Self.Locked = True
        Self.RequiresKey = RequiresKey
        Self.ConsumeKey = ConsumeKey
        Self.OpenSeconds = OpenSeconds
        Self.Progress = 0.0
        Self.Opening = False
        Self.Elapsed = 0.0
        Self.StartProgress = 0.0

    def SetLocked(Self, Value):
        Self.Locked = Value

    def CanUnlock(Self, Items):
        return Self.RequiresKey and Items.Count('key') > 0

    def Interact(Self, Items):
        if Self.Locked:
            if not Self.CanUnlock(Items):
                return False
            if Self.ConsumeKey:
                Items.Remove('key')
            Self.SetLocked(False)
        Self.Open()
        return True

    def Open(Self):
        if Self.Opening:
            return
        Self.StartProgress = Self.Progress
        Self.Elapsed = 0.0
        Self.Opening = True

    def Tick(Self, DeltaTime):
        if Self.Opening:
            Self.Elapsed += DeltaTime
            if Self.OpenSeconds <= 0:
                Self.Progress = 1.0
            else:
                Self.Progress = Self.StartProgress + (1.0 - Self.StartProgress) * min(1.0, Self.Elapsed / Self.OpenSeconds)
            if Self.Elapsed >= Self.OpenSeconds:
                Self.Opening = False


class Weapon:
    def __init__(Self, Damage=25.0, Magazine=6, Reserve=40, Cooldown=0.3,
                 ReloadTime=1.0, Projectile=False, ProjectileSpeed=20.0, Range=40.0):
        Self.Damage = Damage
        Self.Magazine = Magazine
        Self.Ammo = Magazine
        Self.Reserve = Reserve
        Self.Cooldown = Cooldown
        Self.ReloadTime = ReloadTime
        Self.Projectile = Projectile
        Self.ProjectileSpeed = ProjectileSpeed
        Self.Range = Range
        Self.NextShot = 0.0
        Self.ReloadUntil = None

    def Shoot(Self, Clock, OwnerHealth):
        if OwnerHealth.Dead or Self.Ammo < 1 or Clock < Self.NextShot or Self.ReloadUntil is not None:
            return False
        Self.Ammo -= 1
        Self.NextShot = Clock + Self.Cooldown
        return True

    def Reload(Self, Clock, OwnerHealth):
        if OwnerHealth.Dead or Self.Ammo >= Self.Magazine or Self.Reserve <= 0 or Self.ReloadUntil is not None:
            return False
        Self.ReloadUntil = Clock + Self.ReloadTime
        return True

    def Tick(Self, Clock):
        if Self.ReloadUntil is not None and Clock >= Self.ReloadUntil:
            Amount = min(Self.Magazine - Self.Ammo, Self.Reserve)
            Self.Ammo += Amount
            Self.Reserve -= Amount
            Self.ReloadUntil = None


class EnemyAgent:
    def __init__(Self, Position, Maximum=75.0, Speed=1.6, Damage=9.0):
        Self.Position = list(Position)
        Self.Health = Health(Maximum)
        Self.Speed = Speed
        Self.Damage = Damage
        Self.Detection = 30.0
        Self.Range = 1.8
        Self.Windup = 0.3
        Self.Interval = 1.0
        Self.NextAttack = 0.0
        Self.AttackAt = None
        Self.Rewarded = False
        Self.State = 'Idle'

    def Tick(Self, DeltaTime, Clock, TargetPosition, TargetHealth, Visible=True):
        if Self.Health.Dead or TargetHealth.Dead:
            Self.State = 'Stopped'
            Self.AttackAt = None
            return
        Horizontal = TargetPosition[0] - Self.Position[0]
        Depth = TargetPosition[2] - Self.Position[2]
        Distance = math.dist(TargetPosition,Self.Position)
        if Distance > Self.Detection:
            Self.State = 'Idle'
            Self.AttackAt = None
            return
        if Distance > Self.Range or not Visible:
            Self.State = 'Chase'
            Self.AttackAt = None
            Travel = min(Distance, Self.Speed * DeltaTime)
            if Distance > 0:
                Self.Position[0] += Horizontal / Distance * Travel
                Self.Position[2] += Depth / Distance * Travel
            return
        Self.State = 'Attack'
        if Self.AttackAt is None and Clock >= Self.NextAttack:
            Self.AttackAt = Clock + Self.Windup
        if Self.AttackAt is not None and Clock >= Self.AttackAt:
            TargetHealth.Damage(Self.Damage)
            Self.NextAttack = Clock + Self.Interval
            Self.AttackAt = None


def RaySphere(Origin, Direction, Center, Radius, Maximum):
    Offset = [Origin[Axis] - Center[Axis] for Axis in range(3)]
    Projection = sum(Offset[Axis] * Direction[Axis] for Axis in range(3))
    Discriminant = Projection * Projection - sum(Value * Value for Value in Offset) + Radius * Radius
    if Discriminant < 0:
        return None
    Distance = -Projection - math.sqrt(Discriminant)
    if Distance < 0:
        Distance = -Projection + math.sqrt(Discriminant)
    if 0 <= Distance <= Maximum:
        return Distance
    return None


def RayMesh(Origin, Direction, Center, Scale, Rotation, Geometry, Maximum):
    if any(abs(Value) < 0.000001 for Value in Scale):
        return None
    LocalOrigin = [Origin[Axis] - Center[Axis] for Axis in range(3)]
    LocalDirection = list(Direction)
    for Vector in (LocalOrigin, LocalDirection):
        for First, Second, Angle in ((0,2,Rotation[1]),(1,2,-Rotation[0]),(0,1,-Rotation[2])):
            Cosine = math.cos(Angle)
            Sine = math.sin(Angle)
            Vector[First], Vector[Second] = Cosine*Vector[First]-Sine*Vector[Second], Sine*Vector[First]+Cosine*Vector[Second]
        for Axis in range(3):
            Vector[Axis] /= Scale[Axis]
    Positions = Geometry['positions']
    Indices = Geometry['indices']
    Closest = None
    for Offset in range(0,len(Indices),3):
        Vertices = [Positions[Index*3:Index*3+3] for Index in Indices[Offset:Offset+3]]
        FirstEdge = [Vertices[1][Axis]-Vertices[0][Axis] for Axis in range(3)]
        SecondEdge = [Vertices[2][Axis]-Vertices[0][Axis] for Axis in range(3)]
        Perpendicular = [LocalDirection[1]*SecondEdge[2]-LocalDirection[2]*SecondEdge[1], LocalDirection[2]*SecondEdge[0]-LocalDirection[0]*SecondEdge[2], LocalDirection[0]*SecondEdge[1]-LocalDirection[1]*SecondEdge[0]]
        Determinant = sum(FirstEdge[Axis]*Perpendicular[Axis] for Axis in range(3))
        if Determinant <= 0.000000001:
            continue
        Difference = [LocalOrigin[Axis]-Vertices[0][Axis] for Axis in range(3)]
        FirstWeight = sum(Difference[Axis]*Perpendicular[Axis] for Axis in range(3))/Determinant
        if FirstWeight < 0 or FirstWeight > 1:
            continue
        Cross = [Difference[1]*FirstEdge[2]-Difference[2]*FirstEdge[1], Difference[2]*FirstEdge[0]-Difference[0]*FirstEdge[2], Difference[0]*FirstEdge[1]-Difference[1]*FirstEdge[0]]
        SecondWeight = sum(LocalDirection[Axis]*Cross[Axis] for Axis in range(3))/Determinant
        if SecondWeight < 0 or FirstWeight+SecondWeight > 1:
            continue
        Distance = sum(SecondEdge[Axis]*Cross[Axis] for Axis in range(3))/Determinant
        if 0 <= Distance <= Maximum:
            Closest = Distance
            Maximum = Distance
    return Closest


def RayBox(Origin, Direction, Center, HalfExtents, Maximum):
    Near = 0.0
    Far = Maximum
    for Axis in range(3):
        Offset = Origin[Axis] - Center[Axis]
        if abs(Direction[Axis]) < 0.000001:
            if abs(Offset) > HalfExtents[Axis]:
                return None
            continue
        First = (-HalfExtents[Axis] - Offset) / Direction[Axis]
        Second = (HalfExtents[Axis] - Offset) / Direction[Axis]
        Near = max(Near,min(First,Second))
        Far = min(Far,max(First,Second))
        if Near > Far:
            return None
    return Near


def MoveBox(Position, Delta, Size, Offset, Colliders):
    Result = list(Position)
    for Axis in (0,2,1):
        Step = Delta[Axis]
        if Step == 0:
            continue
        Minimum = [Result[Index]+Offset[Index]-Size[Index]/2 for Index in range(3)]
        Maximum = [Result[Index]+Offset[Index]+Size[Index]/2 for Index in range(3)]
        Allowed = Step
        for OtherMinimum,OtherMaximum in Colliders:
            if not all(Maximum[Index]>OtherMinimum[Index]+0.00001 and Minimum[Index]<OtherMaximum[Index]-0.00001 for Index in range(3) if Index != Axis):
                continue
            Gap = OtherMaximum[Axis]-Minimum[Axis]
            if Step > 0:
                Gap = OtherMinimum[Axis]-Maximum[Axis]
            if (Step>0 and Gap>=-0.0001 and Gap<=Allowed) or (Step<0 and Gap<=0.0001 and Gap>=Allowed):
                Allowed = Gap
        Result[Axis] += Allowed
    return Result


class Shooter:
    def __init__(Self, EnemyPositions):
        Self.Health = Health()
        Self.Inventory = Inventory()
        Self.Inventory.Items['medkit'] = 1
        Self.Score = Counter()
        Self.Reward = Reward(Self.Score, 100.0)
        Self.Weapons = [Weapon(), Weapon(60.0, 2, 8, 0.8, 1.5, True)]
        Self.Selected = 0
        Self.Enemies = []
        for Index, Position in enumerate(EnemyPositions):
            Maximum = 75.0
            Damage = 9.0
            Speed = 1.6
            if Index == 4:
                Maximum = 125.0
                Damage = 15.0
            if Index == 3:
                Speed = 2.8
            Self.Enemies.append(EnemyAgent(Position, Maximum, Speed, Damage))
        Self.Clock = 0.0
        Self.Running = False
        Self.ObjectiveActive = False
        Self.Outcome = ''
        Self.Projectiles = []
        Self.Tracer = None
        Self.Trace = None
        Self.CanSee = None
        Self.Move = None

    def Start(Self):
        if not Self.Outcome:
            Self.Running = True
            Self.ObjectiveActive = True

    def Stop(Self):
        Self.Running = False
        for Enemy in Self.Enemies:
            Enemy.AttackAt = None
            Enemy.State = 'Stopped'

    def UseMedkit(Self):
        if Self.Inventory.Count('medkit') < 1 or not Self.Health.Heal(35.0):
            return False
        Self.Inventory.Remove('medkit')
        return True

    def Hit(Self, Origin, Direction, Maximum, Damage):
        if Self.Trace is not None:
            Distance, TargetIndex = Self.Trace(Origin, Direction, Maximum)
            Target = None
            if TargetIndex is not None:
                Target = Self.Enemies[TargetIndex]
                Target.Health.Damage(Damage)
            return Distance, Target
        Nearest = Maximum
        Target = None
        for Enemy in Self.Enemies:
            if Enemy.Health.Dead:
                continue
            Distance = RaySphere(Origin, Direction, Enemy.Position, 0.8, Nearest)
            if Distance is not None:
                Target = Enemy
                Nearest = Distance
        if Target is not None:
            Target.Health.Damage(Damage)
        return Nearest, Target

    def Fire(Self, Origin, Direction):
        WeaponInstance = Self.Weapons[Self.Selected]
        if not WeaponInstance.Shoot(Self.Clock, Self.Health):
            return False
        if WeaponInstance.Projectile:
            Self.Projectiles.append({'Position':list(Origin), 'Direction':list(Direction),
                                     'Remaining':WeaponInstance.Range, 'Speed':WeaponInstance.ProjectileSpeed,
                                     'Damage':WeaponInstance.Damage})
        else:
            Distance, Target = Self.Hit(Origin, Direction, WeaponInstance.Range, WeaponInstance.Damage)
            Self.Tracer = [list(Origin), [Origin[Axis] + Direction[Axis] * Distance for Axis in range(3)], Self.Clock + 0.08]
        Self.CheckOutcome()
        return True

    def CheckOutcome(Self):
        for Enemy in Self.Enemies:
            if Enemy.Health.Dead and not Enemy.Rewarded:
                Enemy.Rewarded = True
                Self.Reward.Award()
        if Self.Health.Dead:
            Self.Outcome = 'Game Over'
            Self.Stop()
        elif Self.ObjectiveActive and Self.Enemies and all(Enemy.Health.Dead for Enemy in Self.Enemies):
            Self.Outcome = 'Победа'
            Self.Stop()

    def Tick(Self, DeltaTime, PlayerPosition):
        Self.Clock += DeltaTime
        for WeaponInstance in Self.Weapons:
            WeaponInstance.Tick(Self.Clock)
        for Projectile in list(Self.Projectiles):
            Travel = min(Projectile['Remaining'], Projectile['Speed'] * DeltaTime)
            Distance, Target = Self.Hit(Projectile['Position'], Projectile['Direction'], Travel, Projectile['Damage'])
            Projectile['Position'] = [Projectile['Position'][Axis] + Projectile['Direction'][Axis] * Distance for Axis in range(3)]
            Projectile['Remaining'] -= Travel
            if Target is not None or Distance < Travel or Projectile['Remaining'] <= 0:
                Self.Projectiles.remove(Projectile)
        for Index,Enemy in enumerate(Self.Enemies):
            if not Self.Running:
                continue
            Visible = True
            if Self.CanSee is not None:
                Visible = Self.CanSee(Enemy.Position, PlayerPosition)
            Previous = list(Enemy.Position)
            Enemy.Tick(DeltaTime, Self.Clock, PlayerPosition, Self.Health, Visible)
            if Self.Move is not None:
                Enemy.Position = Self.Move(Index,Previous,Enemy.Position)
        Self.CheckOutcome()


class Novel:
    def __init__(Self):
        Self.Relationship = Counter()
        Self.Reward = Reward(Self.Relationship)
        Self.Snapshot = 0.0
        Self.Stage = 'reply'
        Self.Text = 'Как ответить?'

    def Choose(Self, Choice):
        if Self.Stage == 'reply' and Choice in ('support', 'cold'):
            if Choice == 'support':
                Self.Reward.Award()
            Self.Snapshot = Self.Relationship.Value
            Self.Stage = 'continue'
            Self.Text = 'Продолжим разговор?'
            return True
        if Self.Stage == 'continue':
            if Choice == 'support' and Self.Relationship.Value >= 1:
                Self.Reward.Award()
                Self.Stage = 'end'
                Self.Text = 'Теперь доверия ещё больше. Разговор окончен.'
                return True
            if Choice == 'goodbye':
                Self.Stage = 'end'
                Self.Text = 'Вы попрощались. Разговор окончен.'
                return True
        return False


class HauntedRoom:
    def __init__(Self, RequireRecord=True):
        Self.RequireRecord = RequireRecord
        Self.Fuse = False
        Self.Power = False
        Self.Record = False
        Self.MirrorSeen = False
        Self.Shadow = False
        Self.Escaped = False
        Self.Ending = ''
        Self.Door = LockedDoor()
        Self.Objective = 'Найдите предохранитель на столе у входа.'
        Self.Message = ''
        Self.Visits = {}

    def Examine(Self, Identifier, Kind, Text, Distance, Maximum=3.2):
        if Distance > Maximum:
            Self.Message = 'Подойдите ближе: WASD. Зажмите правую кнопку мыши, чтобы осмотреться.'
            return False
        Self.Visits[Identifier] = Self.Visits.get(Identifier, 0) + 1
        if Kind == 'examine':
            Self.Message = Text
            return True
        if Kind == 'tape':
            Kind = 'record'
        return Self.Interact(Kind)

    def Interact(Self, Action):
        if Self.Ending:
            return False
        if Action == 'fuse':
            if Self.Fuse:
                return False
            Self.Fuse = True
            Self.Objective = 'Установите предохранитель в красный щит у дальней стены.'
            Self.Message = 'Керамика ещё тёплая. Кто-то вынул предохранитель перед вашим приходом.'
        elif Action == 'power':
            if not Self.Fuse:
                Self.Message = 'Пустое гнездо. Нужен предохранитель со стола смотрителя.'
                return False
            if Self.Power:
                Self.Message = 'Щит гудит. Теперь работает магнитофон.'
                return True
            Self.Power = True
            Self.Shadow = True
            Self.Objective = 'Прослушайте магнитофон на синем столе. Не оборачивайтесь на стук.'
            Self.Message = 'Свет вспыхнул. В конце коридора стоит человек в вашей форме. У него нет лица.'
        elif Action == 'record':
            if not Self.Power:
                Self.Message = 'Катушки неподвижны. Сначала восстановите питание щита.'
                return False
            Self.Record = True
            Self.Door.SetLocked(False)
            Self.Shadow = False
            Self.Objective = 'Выход разблокирован. Вернитесь к зелёной двери и завершите обход.'
            Self.Message = 'Ваш голос на ленте: «Смена закончилась тридцать лет назад. Не отвечай, если назовут по имени». Замок выхода щёлкнул.'
        elif Action == 'mirror':
            Self.MirrorSeen = not Self.MirrorSeen
            Self.Shadow = Self.MirrorSeen
            Self.Message = 'Отражение исчезло. На стекле изнутри проступило: «Не оставайся на вторую смену».'
            if Self.MirrorSeen:
                Self.Message = 'В зеркале вы всё ещё стоите у входа. Отражение поднимает руку, хотя вы не двигались.'
        elif Action == 'exit':
            if not Self.Power or (Self.RequireRecord and not Self.Record):
                Self.Message = 'Заперто. В журнале сказано: восстановить питание и прослушать последнюю запись.'
                return False
            Self.Door.SetLocked(False)
            Self.Door.Open()
        else:
            return False
        return True

    def Tick(Self, DeltaTime):
        Self.Door.Tick(DeltaTime)
        if not Self.Door.Opening and Self.Door.Progress >= 1.0:
            Self.Escaped = True
            Self.Ending = 'Рассвет'
            Self.Objective = 'Вы выбрались. Сегодня в журнале впервые появилась новая дата.'
