// Disposable launchers (M72, RPG-22, RPG-75): an AI soldier who has fired his
// drops the empty tube, like a real one would. The soldier is then owed one of the
// same kind; the host decides how he gets it back (in AC: Actual an ammo truck or
// a base restock). Players handle their own launchers. Server only.
class BBB_Launchers : Managed
{
 static const int CHECK_MS = 2000;
 protected float m_fNextCheck;
 protected ref map<IEntity, ResourceName> m_Owed = new map<IEntity, ResourceName>();
 protected ref map<IEntity, int> m_EmptyChecks = new map<IEntity, int>();
 int m_iDropped;

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

 // The launcher prefab a soldier is owed (false if none).
 bool OwedPrefab(IEntity soldier, out ResourceName prefab) { return m_Owed.Find(soldier, prefab); }
 void Settle(IEntity soldier) { m_Owed.Remove(soldier); }

 // Short readable name for log lines (the editor name, else the prefab's).
 static string WeaponName(IEntity weapon)
 {
  SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(weapon.FindComponent(SCR_EditableEntityComponent));
  if (editable && editable.GetInfo() && !editable.GetInfo().GetName().IsEmpty()) return editable.GetInfo().GetName();
  string name = "Vehicle";
  if (weapon.GetPrefabData()) name = weapon.GetPrefabData().GetPrefabName();
  int slash = name.LastIndexOf("/");
  if (slash >= 0) name = name.Substring(slash + 1, name.Length() - slash - 1);
  name.Replace(".et", "");
  name.Replace("_", " ");
  return name;
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextCheck) return;
  m_fNextCheck = now + CHECK_MS;
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  foreach (SCR_AIGroup group : groups)
  {
   array<AIAgent> agents = {};
   BBB.s_Hooks.GetMembers(group, agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!soldier || !BBB.IsAlive(soldier) || BBB.PlayerOf(soldier) != 0) continue;
    IEntity launcher = DisposableOf(soldier);
    if (!launcher) continue;
    if (Loaded(launcher) > 0) { m_EmptyChecks.Remove(launcher); continue; }
    // Empty on two checks in a row (not mid-reload), then drop it.
    int seen;
    m_EmptyChecks.Find(launcher, seen);
    seen++;
    m_EmptyChecks.Set(launcher, seen);
    if (seen < 2) continue;
    Drop(group, soldier, launcher);
   }
  }
  // Forget soldiers who died.
  array<IEntity> gone = {};
  foreach (IEntity owed, ResourceName prefab : m_Owed)
   if (!owed || !BBB.IsAlive(owed)) gone.Insert(owed);
  foreach (IEntity dead : gone) m_Owed.Remove(dead);
 }

 protected void Drop(SCR_AIGroup group, IEntity soldier, IEntity launcher)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return;
  ResourceName prefab = launcher.GetPrefabData().GetPrefabName();
  m_EmptyChecks.Remove(launcher);
  if (!inventory.TryRemoveItemFromInventory(launcher)) return;
  m_Owed.Set(soldier, prefab);
  m_iDropped++;
  BBB.s_Hooks.OnLitter(launcher);
  PrintFormat("[ACR] LAUNCHER %1: a soldier dropped his spent %2", BBB.s_Hooks.NameOf(group), WeaponName(launcher));
 }

}
