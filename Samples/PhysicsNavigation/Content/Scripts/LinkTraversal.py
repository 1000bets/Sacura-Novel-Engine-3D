import math
import engine


@engine.register_class("game.LinkTraversal")
class LinkTraversal(engine.ScriptComponent):
    def on_create(Self):
        engine.SetNavigationAreaCost(Self.owner, 2, 5.0)
        Self.Elapsed = 0.0
        Self.Start = None
        Self.End = None

    def on_update(Self, DeltaTime):
        Owner = Self.owner
        if engine.GetNavigationState(Owner) != engine.NavigationAgentState.WaitingForLink:
            Self.Start = None
            return
        if Self.Start is None:
            Self.Start = Owner.get_local_transform().position
            Self.End = engine.GetPendingNavigationLinkEnd(Owner)
            Self.Elapsed = 0.0
        Self.Elapsed += DeltaTime
        Fraction = min(1.0, Self.Elapsed / 0.6)
        Placement = Owner.get_local_transform()
        Placement.position = engine.Vector3(
            Self.Start.x + (Self.End.x - Self.Start.x) * Fraction,
            Self.Start.y + (Self.End.y - Self.Start.y) * Fraction + math.sin(Fraction * math.pi) * 0.8,
            Self.Start.z + (Self.End.z - Self.Start.z) * Fraction)
        Owner.set_local_transform(Placement)
        if Fraction >= 1.0:
            engine.CompleteNavigationLink(Owner, True)
            Self.Start = None
