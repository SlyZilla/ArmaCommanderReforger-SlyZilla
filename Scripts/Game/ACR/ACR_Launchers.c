// Disposable launchers: dropping the spent tube and remembering who is owed one is
// BBB_Launchers; giving it back (an ammo truck's resupply, a base restock) is here.
class ACR_Launchers : Managed
{
 static const float REARM_COST = 2;        // truck stock per launcher
 protected ACR_Manager m_Manager;
 int m_iRearmed;

 void ACR_Launchers(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 bool IsOwed(IEntity soldier) { return BBB.s_Launchers && BBB.s_Launchers.IsOwed(soldier); }

 // Hand the soldier a new launcher of the kind he started with. Returns true
 // if one was given (the caller pays for it).
 bool Rearm(IEntity soldier)
 {
  ResourceName prefab;
  if (!BBB.s_Launchers || !BBB.s_Launchers.OwedPrefab(soldier, prefab)) return false;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  bool ok = inventory.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY);
  if (!ok) ok = inventory.TrySpawnPrefabToStorage(prefab);
  if (!ok) return false;
  BBB.s_Launchers.Settle(soldier);
  m_iRearmed++;
  return true;
 }
}
