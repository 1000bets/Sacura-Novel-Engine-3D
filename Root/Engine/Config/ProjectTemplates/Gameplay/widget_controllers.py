import engine


@engine.register_widget(type_id='game.ui.TemplateHud', kind='hud')
class TemplateHud(engine.HudWidget):
    Fields = {'Health': 100.0, 'Score': 0.0, 'Ammunition': 0, 'Relationship': 0.0, 'Snapshot': 0.0, 'Objective': ''}

    def OnEvent(Self, Event, Value):
        if Event in ('pickup', 'door', 'warehouse', 'medkit', 'stop'):
            Self.Command('GameAction', Event)
