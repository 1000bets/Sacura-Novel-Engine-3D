import importlib.util
import sys
import unittest
from pathlib import Path


Source = Path(__file__).resolve().parents[1] / 'Engine/Config/ProjectTemplates/Gameplay/mechanics.py'
sys.dont_write_bytecode = True
Specification = importlib.util.spec_from_file_location('mechanics', Source)
Mechanics = importlib.util.module_from_spec(Specification)
Specification.loader.exec_module(Mechanics)


class TemplateMechanicsTest(unittest.TestCase):
    def test_health_armor_death_and_healing(Self):
        Health = Mechanics.Health(100, 5)
        Health.Damage(20)
        Self.assertEqual(Health.Current, 85)
        Health.Heal(50)
        Self.assertEqual(Health.Current, 100)
        Health.Damage(200)
        Self.assertTrue(Health.Dead)
        Self.assertFalse(Health.Heal(50))
        Self.assertFalse(Health.Damage(1))

    def test_doors_have_independent_locks_and_opening(Self):
        Inventory = Mechanics.Inventory()
        Door = Mechanics.LockedDoor(True)
        Warehouse = Mechanics.LockedDoor()
        Self.assertFalse(Door.Interact(Inventory))
        Self.assertTrue(Inventory.Pickup('key-object', 'key'))
        Self.assertFalse(Inventory.Pickup('key-object', 'key'))
        Self.assertTrue(Door.Interact(Inventory))
        Door.Tick(0.25)
        Self.assertEqual(Door.Progress, 0.5)
        Self.assertTrue(Warehouse.Locked)
        Warehouse.SetLocked(False)
        Warehouse.Tick(10)
        Self.assertEqual(Warehouse.Progress, 0)
        Warehouse.Open()
        Warehouse.Tick(0.5)
        Self.assertEqual(Warehouse.Progress, 1)
        Self.assertFalse(Warehouse.Opening)
        Warehouse.Open()
        Warehouse.Tick(0.25)
        Self.assertEqual(Warehouse.Progress, 1)
        Self.assertTrue(Warehouse.Opening)
        Warehouse.Tick(0.25)
        Self.assertFalse(Warehouse.Opening)

    def test_weapon_reload_cooldown_and_separate_loadout(Self):
        Health = Mechanics.Health()
        Pistol = Mechanics.Weapon()
        Launcher = Mechanics.Weapon(60, 2, 8, 0.8, 1.5, True)
        Self.assertTrue(Pistol.Shoot(0, Health))
        Self.assertFalse(Pistol.Shoot(0.1, Health))
        Self.assertTrue(Pistol.Reload(0.2, Health))
        Self.assertFalse(Pistol.Shoot(1.0, Health))
        Pistol.Tick(1.2)
        Self.assertEqual((Pistol.Ammo, Pistol.Reserve), (6, 39))
        Self.assertEqual((Launcher.Ammo, Launcher.Reserve), (2, 8))

    def test_projectile_has_travel_time_and_awards_once(Self):
        Game = Mechanics.Shooter([[0,1,-10]])
        Game.Start()
        Game.Selected = 1
        Self.assertTrue(Game.Fire([0,1,0], [0,0,-1]))
        Self.assertEqual(Game.Enemies[0].Health.Current, 75)
        Game.Tick(0.1, [0,1,0])
        Self.assertEqual(Game.Enemies[0].Health.Current, 75)
        Game.Tick(0.5, [0,1,0])
        Self.assertEqual(Game.Enemies[0].Health.Current, 15)
        Game.Selected = 0
        Self.assertTrue(Game.Fire([0,1,0], [0,0,-1]))
        Self.assertEqual(Game.Outcome, 'Победа')
        Self.assertEqual(Game.Score.Value, 100)
        Game.CheckOutcome()
        Self.assertEqual(Game.Score.Value, 100)

    def test_enemy_windup_rechecks_distance_and_death(Self):
        Enemy = Mechanics.EnemyAgent([0,1,0])
        Health = Mechanics.Health()
        Enemy.Tick(0.1, 0, [0,1,1], Health)
        Self.assertEqual(Health.Current, 100)
        Enemy.Tick(0.1, 0.4, [0,1,5], Health)
        Self.assertEqual(Health.Current, 100)
        Enemy.Tick(0.1, 1, [0,1,1], Health)
        Enemy.Tick(0.1, 1.4, [0,1,1], Health)
        Self.assertEqual(Health.Current, 91)
        Enemy.Health.Damage(100)
        Enemy.Tick(10, 10, [0,1,1], Health)
        Self.assertEqual(Health.Current, 91)

    def test_medkit_consumption_and_game_over(Self):
        Game = Mechanics.Shooter([[0,1,0]])
        Self.assertTrue(Game.UseMedkit())
        Self.assertFalse(Game.UseMedkit())
        Game.Inventory.Pickup('box','medkit')
        Game.Health.Damage(50)
        Self.assertTrue(Game.UseMedkit())
        Self.assertEqual(Game.Health.Current, 85)
        Self.assertEqual(Game.Inventory.Count('medkit'), 0)
        Game.Start()
        Game.Health.Damage(1000)
        Game.CheckOutcome()
        Self.assertEqual(Game.Outcome, 'Game Over')
        Self.assertFalse(Game.Running)

    def test_relationship_snapshot_does_not_follow_second_reward(Self):
        Novel = Mechanics.Novel()
        Self.assertTrue(Novel.Choose('support'))
        Self.assertEqual(Novel.Snapshot, 1)
        Self.assertTrue(Novel.Choose('support'))
        Self.assertEqual(Novel.Relationship.Value, 2)
        Self.assertEqual(Novel.Snapshot, 1)
        Cold = Mechanics.Novel()
        Cold.Choose('cold')
        Self.assertFalse(Cold.Choose('support'))
        Self.assertTrue(Cold.Choose('goodbye'))

    def test_horror_puzzle_guards_and_terminal_state(Self):
        Room = Mechanics.HauntedRoom()
        Self.assertFalse(Room.Interact('exit'))
        Self.assertFalse(Room.Interact('record'))
        Self.assertFalse(Room.Interact('power'))
        Self.assertTrue(Room.Interact('fuse'))
        Self.assertFalse(Room.Interact('fuse'))
        Self.assertTrue(Room.Interact('power'))
        Self.assertTrue(Room.Shadow)
        Self.assertFalse(Room.Interact('exit'))
        Self.assertTrue(Room.Interact('record'))
        Self.assertFalse(Room.Shadow)
        Self.assertFalse(Room.Door.Locked)
        Self.assertEqual(Room.Door.Progress, 0)
        Room.Interact('mirror')
        Self.assertTrue(Room.Shadow)
        Room.Interact('mirror')
        Self.assertFalse(Room.Shadow)
        Self.assertTrue(Room.Interact('exit'))
        Self.assertFalse(Room.Escaped)
        Room.Tick(Room.Door.OpenSeconds)
        Self.assertTrue(Room.Escaped)
        Self.assertFalse(Room.Interact('stay'))


if __name__ == '__main__':
    unittest.main()
