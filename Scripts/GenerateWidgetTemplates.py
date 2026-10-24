import copy
import json
import uuid
from pathlib import Path


Repository = Path(__file__).resolve().parents[1]
TemplateRoot = Repository / 'Root/Engine/Config/ProjectTemplates'
EngineContent = Repository / 'Root/Engine/Content'
ControllerNames = {'dialogue': 'engine.ui.Dialogue', 'hud': 'engine.ui.Hud',
                   'mainMenu': 'engine.ui.MainMenu', 'pauseMenu': 'engine.ui.PauseMenu'}
BindingNames = {'hero-hp': 'Health', 'score': 'Score', 'ammo': 'Ammunition',
                'relationship-text': 'Relationship', 'snapshot-text': 'Snapshot', 'horror-objective': 'Objective'}
GameEvents = {'take-key': 'pickup', 'key-door-button': 'door', 'story-door-button': 'warehouse',
              'medkit-button': 'medkit', 'stop-battle': 'stop'}


def Identity(Name):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, 'sacura-widget/' + Name))


def WriteAsset(Destination, Document, AssetType, Name):
    Destination.parent.mkdir(parents=True, exist_ok=True)
    Destination.write_text(json.dumps(Document, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    Metadata = {'schemaVersion': 1, 'guid': Identity(Name), 'assetType': AssetType,
                'loadSettings': {}, 'subAssets': [], 'importInfo': {}}
    Destination.with_name(Destination.name + '.meta').write_text(json.dumps(Metadata, indent=2) + '\n', encoding='utf-8')
    return Metadata['guid']


def Convert(Source, Controller):
    Widget = copy.deepcopy(Source)
    Widget.pop('builtinDefault', None)
    for Element in Widget['elements']:
        Role = Element.get('role')
        Field = {'speaker': 'Speaker', 'dialogueText': 'Text', 'title': 'Title'}.get(Role)
        if Element['id'] in BindingNames:
            Field = BindingNames[Element['id']]
        if Field:
            Element['binding'] = {'source': 'variable', 'path': Field,
                                  'format': Element.get('binding', {}).get('format', '')}
        if Element['type'] == 'choices':
            Element['binding'] = {'source': 'variable', 'path': 'Choices'}
            Element['event'] = 'Choose'
            Element['enabledField'] = 'CanChoose'
            Element['selectedField'] = 'SelectedChoiceId'
        if Element['type'] == 'button':
            Action = Element.get('action', '')
            Element['event'] = {'advance': 'Advance', 'start': 'Start', 'resume': 'Resume',
                                'mainMenu': 'MainMenu', 'restart': 'restart', 'save': 'save', 'load': 'load'}.get(Action, '')
            if Action == 'advance':
                Element['enabledField'] = 'CanAdvance'
            if Element['id'] in GameEvents:
                Element['event'] = GameEvents[Element['id']]
        Element['role'] = 'none'
    return {'format': 'sakura.widget', 'version': 1, 'controller': Controller, 'widget': Widget}


def GenerateDefaults():
    Reference = json.loads((Repository / 'Root/Verification/WebTemplateOracle/novel.json').read_text(encoding='utf-8'))
    Widgets = {}
    for Source in Reference['widgets']:
        if Source['kind'] == 'hud':
            continue
        Kind = Source['kind']
        Name = {'dialogue': 'Dialogue', 'mainMenu': 'MainMenu', 'pauseMenu': 'PauseMenu'}[Kind]
        Document = Convert(Source, ControllerNames[Kind])
        Widgets[Kind] = WriteAsset(EngineContent / 'UI' / (Name + '.widget'), Document, 'Widget', 'engine/' + Name)
        if Kind == 'dialogue':
            (Repository / 'Root/Engine/Config/UI/Default.widget').write_text(json.dumps(Document, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    Actions = {}
    Bindings = []
    for Role, Name, Control in [('advance', 'Advance', 'Space'), ('nextChoice', 'NextChoice', 'ArrowDown'),
                                ('previousChoice', 'PreviousChoice', 'ArrowUp'), ('confirmChoice', 'ConfirmChoice', 'Enter'),
                                ('pause', 'Pause', 'Escape')]:
        Actions[Role] = WriteAsset(EngineContent / 'Input' / (Name + '.inputaction'),
                                  {'Version': 1, 'Action': {'Name': Name, 'ValueType': 0, 'Trigger': 0,
                                   'bExecuteWhenPaused': Role == 'pause'}}, 'InputAction', 'engine/input/' + Name)
        Bindings.append({'Identifier': Name, 'Action': Actions[Role], 'Device': 'Keyboard', 'Control': Control})
    Bindings += [{'Identifier': 'GamepadConfirm', 'Action': Actions['advance'], 'Device': 'Gamepad', 'Control': 'South'},
                 {'Identifier': 'GamepadPause', 'Action': Actions['pause'], 'Device': 'Gamepad', 'Control': 'Start'}]
    Context = WriteAsset(EngineContent / 'Input/Narrative.inputcontext',
                         {'Version': 1, 'Context': {'Name': 'Narrative', 'Bindings': Bindings}}, 'InputMappingContext', 'engine/input/Narrative')
    Settings = {'narrativeWidgets': Widgets, 'narrativeInputActions': Actions,
                'startupInputContexts': [{'Context': Context, 'Priority': 100}]}
    (Repository / 'Root/Engine/Config/UI/Defaults.json').write_text(json.dumps(Settings, indent=2) + '\n', encoding='utf-8')
    return Settings


def Generate(Scenario, Reference=None):
    Defaults = json.loads((Repository / 'Root/Engine/Config/UI/Defaults.json').read_text(encoding='utf-8'))
    if Reference is None:
        Reference = json.loads((Repository / 'Root/Verification/WebTemplateOracle' / (Scenario + '.json')).read_text(encoding='utf-8'))
    Destination = TemplateRoot / ('builtin-' + Scenario)
    WidgetIds = {}
    for Source in Reference['widgets']:
        Controller = ControllerNames[Source['kind']]
        if Source['kind'] == 'hud':
            Controller = 'game.ui.TemplateHud'
        WidgetIds[Source['id']] = WriteAsset(Destination / 'Content/UI' / (Source['id'] + '.widget'),
                                            Convert(Source, Controller), 'Widget', Scenario + '/' + Source['id'])
    Defaults['narrativeWidgets'] = {Kind: WidgetIds[Identifier] for Kind, Identifier in Reference['ui'].items()}
    GameplayContext = Destination / 'Content/Input/Gameplay.inputcontext.meta'
    if GameplayContext.exists():
        Defaults['startupInputContexts'].append({'Context': json.loads(GameplayContext.read_text())['guid'], 'Priority': 20})
    (Destination / 'WidgetSettings.json').write_text(json.dumps(Defaults, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    GenerateDefaults()
    for Scenario in ('quest', 'shooter', 'novel', 'horror'):
        Generate(Scenario)
