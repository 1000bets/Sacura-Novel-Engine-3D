class WidgetController:
    Fields = {}

    def SetField(Self, Name, Value):
        if Name not in Self._Fields:
            raise KeyError("Undeclared widget field: " + Name)
        Default = Self.Fields[Name]
        if type(Value) is not type(Default) and not (type(Default) is float and type(Value) is int):
            raise TypeError("Invalid widget field type: " + Name)
        Self._Fields[Name] = Value

    def GetField(Self, Name):
        return Self._Fields[Name]

    def SetVisible(Self, Visible):
        Self._Visible = bool(Visible)

    def Command(Self, Name, Value=None):
        _widget_command(Self.InstanceId, Name, Value)

    def OnCreate(Self):
        pass

    def OnUpdate(Self, Context):
        for Name, Value in Context.get('Variables', {}).items():
            if Name in Self.Fields:
                Self.SetField(Name, Value)

    def OnEvent(Self, Event, Value):
        pass

    def OnDestroy(Self):
        pass

def register_widget(*, type_id, kind):
    def Register(Class):
        if not issubclass(Class, WidgetController):
            raise TypeError("Widget controller must derive from WidgetController")
        import sys
        _register_widget(Class, type_id, kind, Class.Fields, sys._getframe(1).f_code.co_filename)
        return Class
    return Register

@register_widget(type_id='engine.ui.Dialogue', kind='dialogue')
class DialogueWidget(WidgetController):
    Fields = {'Speaker':'', 'Text':'', 'Status':'', 'Choices':[], 'CanAdvance':False, 'CanChoose':False, 'SelectedChoiceId':''}

    def OnUpdate(Self, Context):
        Dialogue = Context['Dialogue']
        Self.SetVisible(Dialogue['Phase'] in ('line', 'choice'))
        Self.SetField('Speaker', Dialogue['Speaker'])
        Self.SetField('Text', Dialogue['Text'])
        Self.SetField('Status', '')
        Self.SetField('Choices', Dialogue['Choices'])
        Self.SetField('SelectedChoiceId', Dialogue['SelectedChoiceId'])
        Self.SetField('CanAdvance', Dialogue['CanAdvance'] and not Context['Paused'])
        Self.SetField('CanChoose', Dialogue['CanChoose'] and not Context['Paused'])

    def OnEvent(Self, Event, Value):
        if Event == 'Advance':
            Self.Command('Advance')
        elif Event == 'Choose':
            Self.Command('Choose', int(Value))

@register_widget(type_id='engine.ui.Hud', kind='hud')
class HudWidget(WidgetController):
    def OnUpdate(Self, Context):
        super().OnUpdate(Context)
        Self.SetVisible(not Context['Paused'])

@register_widget(type_id='engine.ui.MainMenu', kind='mainMenu')
class MainMenuWidget(WidgetController):
    Fields = {'Title':''}

    def OnUpdate(Self, Context):
        Self.SetField('Title', Context['Title'])
        Self.SetVisible(Context['MainMenu'])

    def OnEvent(Self, Event, Value):
        if Event in ('Start', 'Resume', 'MainMenu'):
            Self.Command(Event)
        elif Event in ('save', 'load', 'restart'):
            Self.Command('GameAction', Event)
            if Event in ('load', 'restart'):
                Self.Command('Resume')

@register_widget(type_id='engine.ui.PauseMenu', kind='pauseMenu')
class PauseMenuWidget(MainMenuWidget):
    def OnUpdate(Self, Context):
        Self.SetField('Title', Context['Title'])
        Self.SetVisible(Context['Paused'] and not Context['MainMenu'])
