// Disposable launchers (M72, RPG-22, RPG-75): an AI soldier who has fired his
// drops the empty tube, like a real one would. The soldier is owed one of the
// same kind; an ammo truck's resupply gives it back (the launcher from his
// original loadout). Players handle their own launchers. Server only.
class ACR_Launchers : Managed
{
 static const int CHECK_MS = 2000;
 static const float REARM_COST = 2;        // truck stock per launcher
 protected ACR_Manager m_Manager;
 protected float m_fNextCheck;
 protected ref map<IEntity, ResourceName> m_Owed = new map<IEntity, ResourceName>();
 protected ref map<IEntity, int> m_EmptyChecks = new map<IEntity, int>();
 int m_iDropped;
 int m_iRearmed;

 void ACR_Launchers(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 static bool IsDisposable(IEntity weapon)
 {
  if (!weapon) return false;
  BaseWeaponComponent component = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
  if (component)
  {
   array<BaseMuzzleComponent> muzzles = {};
   component.GetMuzzlesList(muzzles);
   foreach (BaseMuzzleComponent muzzle : muzzles)
    if (muzzle.IsDisposable()) return true;
  }
  if (!weapon.GetPrefabData()) return false;
  string name = weapon.GetPrefabData().GetPrefabName();
  name.ToUpper();
  return name.Contains("M72") || name.Contains("RPG22") || name.Contains("RPG75");
 }

 // Rounds left in the tube.
 static int Loaded(IEntity weaponEntity)
 {
  BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
  if (!weapon) return 0;
  int rounds;
  array<BaseMuzzleComponent> muzzles = {};
  weapon.GetMuzzlesList(muzzles);
  foreach (BaseMuzzleComponent muzzle : muzzles) rounds += muzzle.GetAmmoCount();
  BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
  if (magazine) rounds += magazine.GetAmmoCount();
  return rounds;
 }

 // The soldier's disposable launcher (or null).
 static IEntity DisposableOf(IEntity soldier)
 {
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return null;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weapon : list)
   if (IsDisposable(weapon)) return weapon;
  return null;
 }

 bool IsOwed(IEntity soldier) { return m_Owed.Contains(soldier); }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextCheck) return;
  m_fNextCheck = now + CHECK_MS;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!soldier || !ACR_Respawn.IsAlive(soldier) || ACR_Squad.FindController(soldier) != 0) continue;
    IEntity launcher = DisposableOf(soldier);
    if (!launcher) continue;
    if (Loaded(launcher) > 0) { m_EmptyChecks.Remove(launcher); continue; }
    // Empty on two checks in a row (not mid-reload), then drop it.
    int seen;
    m_EmptyChecks.Find(launcher, seen);
    seen++;
    m_EmptyChecks.Set(launcher, seen);
    if (seen < 2) continue;
    Drop(squad, soldier, launcher);
   }
  }
  // Forget soldiers who died.
  array<IEntity> gone = {};
  foreach (IEntity owed, ResourceName prefab : m_Owed)
   if (!owed || !ACR_Respawn.IsAlive(owed)) gone.Insert(owed);
  foreach (IEntity dead : gone) m_Owed.Remove(dead);
 }

 protected void Drop(ACR_Squad squad, IEntity soldier, IEntity launcher)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return;
  ResourceName prefab = launcher.GetPrefabData().GetPrefabName();
  m_EmptyChecks.Remove(launcher);
  if (!inventory.TryRemoveItemFromInventory(launcher)) return;
  m_Owed.Set(soldier, prefab);
  m_iDropped++;
  if (m_Manager.m_Cleanup) m_Manager.m_Cleanup.AddLitter(launcher);
  PrintFormat("[ACR] LAUNCHER %1: a soldier dropped his spent %2", squad.GetName(), ACR_Vehicles.NameOf(launcher));
 }

 // Hand the soldier a new launcher of the kind he started with. Returns true
 // if one was given (the caller pays for it).
 bool Rearm(IEntity soldier)
 {
  ResourceName prefab;
  if (!m_Owed.Find(soldier, prefab)) return false;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  bool ok = inventory.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY);
  if (!ok) ok = inventory.TrySpawnPrefabToStorage(prefab);
  if (!ok) return false;
  m_Owed.Remove(soldier);
  m_iRearmed++;
  return true;
 }
}
