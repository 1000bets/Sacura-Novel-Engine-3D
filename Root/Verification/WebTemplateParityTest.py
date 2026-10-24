import importlib.util
import json
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
Repository = Path(__file__).resolve().parents[2]
Templates = Repository / 'Root/Engine/Config/ProjectTemplates'
OracleDirectory = Path(__file__).parent / 'WebTemplateOracle'
Specification = importlib.util.spec_from_file_location('mechanics', Templates / 'Gameplay/mechanics.py')
Mechanics = importlib.util.module_from_spec(Specification)
Specification.loader.exec_module(Mechanics)


class WebTemplateParityTest(unittest.TestCase):
    def test_mesh_picking_matches_three_raycaster(Self):
        Geometries = json.loads((OracleDirectory/'geometry.json').read_text())
        Traces = json.loads((OracleDirectory/'picking.json').read_text())
        for Trace in Traces:
            with Self.subTest(Trace=Trace):
                Actual = Mechanics.RayMesh(Trace['origin'],Trace['direction'],Trace['center'],Trace['scale'],Trace['rotation'],Geometries[Trace['geometry']],10)
                if Trace['distance'] is None:
                    Self.assertIsNone(Actual)
                else:
                    Self.assertIsNotNone(Actual)
                    Self.assertAlmostEqual(Actual,Trace['distance'],places=6)

    def test_controller_collision_matches_web_axis_sliding(Self):
        Traces = json.loads((OracleDirectory/'traces.json').read_text(encoding='utf-8'))
        Colliders = [([1,0,-5],[2,3,5]),([-10,-1,-10],[10,0,10])]
        for Step in Traces['movement']:
            Self.assertEqual(Mechanics.MoveBox([0,0,0],Step['delta'],[0.45,1.6,0.45],[0,0.8,0],Colliders),Step['position'])
    def test_horror_matches_executed_web_component_trace(Self):
        Traces = json.loads((OracleDirectory/'traces.json').read_text(encoding='utf-8'))
        Game = Mechanics.HauntedRoom()
        Game.Door.OpenSeconds = 1
        Names = {'takeFuse':'fuse','listen':'record','escape':'exit'}
        Fields = {'fuse':'Fuse','power':'Power','record':'Record','mirrorSeen':'MirrorSeen','escaped':'Escaped','objective':'Objective'}
        for Step in Traces['horror']:
            with Self.subTest(Method=Step['method']):
                Result = Game.Interact(Names.get(Step['method'],Step['method']))
                if Step['method']=='escape' and Result:
                    Self.assertFalse(Game.Escaped)
                    Game.Tick(1)
                for Source,Target in Fields.items():
                    Self.assertEqual(getattr(Game,Target),Step['state'][Source])
                Self.assertEqual(Game.Message,Step['message'])
                Self.assertEqual(Game.Shadow,Step['shadow'])
                if Step['result'] is not None:
                    Self.assertEqual(Result,Step['result'])

    def test_enemy_chases_without_line_of_sight_but_cannot_attack(Self):
        Enemy = Mechanics.EnemyAgent([0,0,0])
        Target = Mechanics.Health()
        Enemy.Tick(0.1,0.1,[0,0,5],Target,False)
        Self.assertGreater(Enemy.Position[2],0)
        Enemy.Position = [0,0,4]
        for Step in range(20):
            Enemy.Tick(0.1,Step*0.1,[0,0,5],Target,False)
        Self.assertEqual(Target.Current,100)

    def test_horror_examine_range_and_visits(Self):
        Game = Mechanics.HauntedRoom()
        Self.assertFalse(Game.Examine('desk','examine','Desk',3.201))
        Self.assertEqual(Game.Visits,{})
        Self.assertTrue(Game.Examine('desk','examine','Desk',3.2))
        Self.assertEqual(Game.Visits,{'desk':1})
        Self.assertEqual(Game.Message,'Desk')

    def test_snapshot_restores_live_references_and_pending_reload(Self):
        Game = Mechanics.Shooter([[0,0,-2]])
        Game.Fire([0,0,0],[0,0,1])
        Game.Weapons[0].Reload(Game.Clock,Game.Health)
        Snapshot = json.loads(json.dumps(Mechanics.CaptureState(Game)))
        Game.Tick(2,[0,0,0])
        Game.Inventory.Remove('medkit')
        Mechanics.RestoreState(Game,Snapshot)
        Self.assertEqual(Game.Inventory.Count('medkit'),1)
        Self.assertEqual(Game.Weapons[0].Ammo,5)
        Game.Tick(1,[0,0,0])
        Self.assertEqual(Game.Weapons[0].Ammo,6)
        Game.Reward.Award()
        Self.assertEqual(Game.Score.Value,100)

    def test_characters_use_animated_skeletal_assets(Self):
        for Scenario in ('shooter','horror'):
            Scene = json.loads((Templates/('builtin-'+Scenario)/'Content/Scenes/Main.scene').read_text(encoding='utf-8'))
            Oracle = json.loads((OracleDirectory/(Scenario+'.json')).read_text(encoding='utf-8'))
            for Character in (Entry for Entry in Oracle['objects'] if Entry['type']=='Персонаж'):
                Root = next(Entry for Entry in Scene['objects'] if Entry['name']==Character['name'])
                Parts = [Entry for Entry in Scene['objects'] if Entry['parent']==Root['id']]
                Self.assertEqual(len(Parts),1)
                Component = Parts[0]['components'][0]
                Self.assertEqual(Component['type'],'engine.CharacterComponent')
                Self.assertEqual(Component['properties']['character']['assetId'],'832edba4-24e9-52cb-bf25-8e53c14497ef')
                Self.assertTrue(Component['properties']['automatic_walk'])
                Self.assertEqual(len(Component['properties']['material_slots']['entries']),2)
    def test_reward_and_starting_inventory_from_web_project(Self):
        Oracle = json.loads((OracleDirectory / 'shooter.json').read_text(encoding='utf-8'))
        Game = Mechanics.Shooter([[0,0,-2]])
        Reward = next(Entry for Entry in Oracle['gameplay']['mechanics'] if Entry['id'] == 'score-reward')
        Game.Enemies[0].Health.Damage(1000)
        Game.CheckOutcome()
        Self.assertEqual(Game.Score.Value, Reward['values']['base'])
        Self.assertEqual(Game.Inventory.Count('medkit'), Oracle['gameplay']['inventory']['starting'][0]['count'])

    def test_weapon_operates_independently_of_battle_owner(Self):
        Game = Mechanics.Shooter([[0,0,-2]])
        Self.assertTrue(Game.Fire([0,0,0],[0,0,-1]))
        Game.Stop()
        Game.Tick(0.31,[0,0,0])
        Self.assertTrue(Game.Fire([0,0,0],[0,0,-1]))

    def test_authored_objects_and_story_text_are_preserved(Self):
        for Scenario in ('quest','shooter','novel','horror'):
            with Self.subTest(Scenario=Scenario):
                Oracle = json.loads((OracleDirectory / (Scenario + '.json')).read_text(encoding='utf-8'))
                Native = Templates / ('builtin-' + Scenario) / 'Content'
                Scene = json.loads((Native / 'Scenes/Main.scene').read_text(encoding='utf-8'))
                Names = {Entry['name'] for Entry in Scene['objects']}
                Self.assertTrue({Entry['name'] for Entry in Oracle['objects']} <= Names)
                Story = json.loads((Native / 'Stories/Opening.story').read_text(encoding='utf-8'))
                Texts = {Entry.get('text',Entry.get('prompt','')) for Entry in Story['nodes']}
                for Beat in Oracle['chapters'][0]['beats']:
                    if Beat['text']:
                        Self.assertIn(Beat['text'],Texts)


if __name__ == '__main__':
    unittest.main()
