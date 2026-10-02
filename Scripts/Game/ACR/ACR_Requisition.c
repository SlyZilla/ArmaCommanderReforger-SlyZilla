// Buying units, reinforcements and the ammo restock at bases
// (split out of ACR_Manager, 2 Oct; the manager forwards its old entry points).
class ACR_Requisition : Managed
{
 protected ACR_Manager m_Manager;

 void ACR_Requisition(ACR_Manager manager) { m_Manager = manager; }

 static const int REINFORCE_COST_PER_SOLDIER = 3;

 ACR_Base FindSpawnBase(string factionKey, string label)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.GetLabel() == label && base.m_sOwner == factionKey) return base;
  return null;
 }

 ACR_Base NearestSpawnBase(string factionKey, vector position)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey || !base.m_bAllowSpawn || base.m_bContested) continue;
   float distance = vector.Distance(base.GetOrigin(), position);
   if (distance >= bestDistance) continue;
   bestDistance = distance;
   best = base;
  }
  return best;
 }

 vector ClearSpotNear(ACR_Base base, float minDistance, float maxDistance, float clearance) { return SpawnPoint(base, minDistance, maxDistance, clearance); }

 // Random point in a ring around the base, nudged to a spot clear of walls,
 // revetments and buildings (clearance = radius of the free cylinder needed).
 vector SpawnPoint(ACR_Base base, float minDistance, float maxDistance, float clearance = 1)
 {
  vector fallback;
  for (int attempt = 0; attempt < 8; attempt++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float distance = Math.RandomFloat(minDistance, maxDistance + attempt * 5);
   vector candidate = ACR_Manager.Ground(base.GetOrigin() + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
   if (attempt == 0) fallback = candidate;
   vector free;
   if (SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 8, clearance, 2.5))
    return ACR_Manager.Ground(free);
  }
  return fallback;
 }

 // A clear spot near the commander's click, pulled inside the base ring.
 vector SpotInBase(ACR_Base base, vector spot, float clearance)
 {
  vector centre = base.GetOrigin();
  float ring = Math.Max(base.m_fRadius, 30);
  vector flat = Vector(spot[0] - centre[0], 0, spot[2] - centre[2]);
  if (flat.Length() > ring) spot = centre + flat.Normalized() * ring;
  vector candidate = ACR_Manager.Ground(spot);
  vector free;
  if (SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 15, clearance, 2.5)) return ACR_Manager.Ground(free);
  return candidate;
 }

 string Purchase(SCR_PlayerController pc, int entryIndex, string baseLabel, vector spot = "0 0 0", bool hasSpot = false)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return "Only the commander can requisition units";
  string key = faction.GetFactionKey();
  int before = m_Manager.m_Bases.GetRP(key);
  ACR_Base spawnBase = FindSpawnBase(key, baseLabel);
  string result = PurchaseAt(key, entryIndex, spawnBase, spot, hasSpot);
  if (m_Manager.m_Bases.GetRP(key) < before && spawnBase)
  {
   string unit = m_Manager.m_Bases.GetFaction(key).m_aRequisitions[entryIndex].m_sName;
   m_Manager.LogSpend(pc, key, before, "requisitioned " + unit + " at " + spawnBase.m_sName);
  }
  return result;
 }

 // Shared by the player commander and the AI commander.
 // hasSpot: the commander clicked a point in the base ring; the unit appears there
 // (the AI commander and old callers get a random clear point in the ring).
 string PurchaseAt(string key, int entryIndex, ACR_Base base, vector spot = "0 0 0", bool hasSpot = false)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(key);
  if (entryIndex < 0 || entryIndex >= data.m_aRequisitions.Count()) return "Unknown unit";
  ACR_RequisitionEntry entry = data.m_aRequisitions[entryIndex];
  if (entry.m_iRemaining <= 0) return entry.m_sName + ": none left";
  if (!base || base.m_sOwner != key) return "Deploy at one of your own bases";
  if (!base.m_bAllowSpawn) return base.m_sName + " can't deploy units";
  if (base.m_bContested) return base.m_sName + " is under attack";
  if (m_Manager.m_Cleanup && m_Manager.m_Cleanup.SoldiersOf(key) >= m_Manager.m_iSoldierCap) return string.Format("Too many soldiers in the field (limit %1)", m_Manager.m_iSoldierCap);
  if (!m_Manager.m_Bases.Spend(key, entry.m_iCost)) return string.Format("Not enough RP (%1 needed)", entry.m_iCost);
  entry.m_iRemaining--;
  entry.m_iBought++;
  string name = string.Format("%1 %2", entry.m_sName, entry.m_iBought);

  m_Manager.m_sSpawningFaction = key;
  vector vehiclePos, groupPos;
  if (hasSpot)
  {
   vehiclePos = SpotInBase(base, spot, 4);
   // Infantry alone stands on the click; with a vehicle, beside it.
   if (entry.m_sVehicle.IsEmpty()) groupPos = SpotInBase(base, spot, 1.5);
   else groupPos = SpotInBase(base, vehiclePos + Vector(6, 0, 6), 1.5);
   PrintFormat("[ACR] PURCHASE spot %1 -> vehicle %2 group %3", spot, vehiclePos, groupPos);
  }
  else
  {
   vehiclePos = SpawnPoint(base, 10, 25, 4);
   groupPos = SpawnPoint(base, 5, 15, 1.5);
  }
  SpawnUnit(name, entry.m_sGroup, entry.m_sVehicle, entry.m_bCrewed, entry.m_sPassengers, vehiclePos, groupPos);
  m_Manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] PURCHASE %1 at %2 for %3 RP (%4 left)", name, base.m_sName, entry.m_iCost, entry.m_iRemaining);
  return string.Format("%1 deploying at %2", name, base.m_sName);
 }

 // Spawns a named unit: an infantry group, a vehicle (optionally crewed; the
 // crew squad takes the name), and/or passengers that board the vehicle.
 void SpawnUnit(string name, ResourceName group, ResourceName vehicleprefab, bool crewed, ResourceName passengers, vector vehiclePos, vector groupPos)
 {
  IEntity vehicle;
  if (!vehicleprefab.IsEmpty())
  {
   vehicle = ACR_Manager.SpawnPrefab(vehicleprefab, vehiclePos);
   if (vehicle && m_Manager.m_Cleanup) m_Manager.m_Cleanup.Watch(vehicle);
   if (vehicle) ACR_Vehicles.SetOwner(vehicle, m_Manager.m_sSpawningFaction);
   if (vehicle && crewed)
   {
    m_Manager.m_PendingCrewNames.Set(vehicle, name);
    SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
    array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
    if (compartments) compartments.SpawnDefaultOccupants(seats);
    // A construction truck's crew gets the building shovel (the default crew has none).
    if ((ACR_Logistics.ServiceOfName(vehicleprefab) & ACR_EService.CONSTRUCTION) != 0)
     GetGame().GetCallqueue().CallLater(GiveShovels, 1500, false, vehicle, m_Manager.m_sSpawningFaction);
   }
  }
  if (!group.IsEmpty())
  {
   SCR_AIGroup spawned = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(group, groupPos));
   if (spawned) spawned.SetCustomName(name, 0);
  }
  if (!passengers.IsEmpty())
  {
   SCR_AIGroup riders = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(passengers, groupPos));
   if (riders)
   {
    riders.SetCustomName(name + " infantry", 0);
    if (vehicle) m_Manager.m_PendingBoarding.Set(riders, vehicle);
   }
  }
 }

 static const ResourceName SHOVEL_WEST = "{6E35D94130954509}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE_FreeRoamBuilding_Gadget.et";
 static const ResourceName SHOVEL_EAST = "{062E2F1D7F6739D6}Prefabs/Items/Equipment/Accessories/ETool_MPL50/ETool_MPL50_FreeRoamBuilding_Gadget.et";
 // Returns how many crewmen got one.
 int GiveShovels(IEntity vehicle, string factionKey)
 {
  if (!vehicle) return 0;
  ResourceName shovel = SHOVEL_EAST;
  if (ACR_Supports.IsWestern(factionKey)) shovel = SHOVEL_WEST;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  int given;
  foreach (BaseCompartmentSlot slot : slots)
  {
   IEntity crewman = slot.GetOccupant();
   if (!crewman || HasShovel(crewman)) continue;
   FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(crewman.FindComponent(FactionAffiliationComponent));
   if (affiliation && affiliation.GetAffiliatedFaction())
   {
    shovel = SHOVEL_EAST;
    if (ACR_Supports.IsWestern(affiliation.GetAffiliatedFaction().GetFactionKey())) shovel = SHOVEL_WEST;
   }
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(crewman.FindComponent(SCR_InventoryStorageManagerComponent));
   if (!inventory) continue;
   bool ok = inventory.TrySpawnPrefabToStorage(shovel, null, -1, EStoragePurpose.PURPOSE_GADGET_PROXY);
   if (!ok) ok = inventory.TrySpawnPrefabToStorage(shovel);
   if (ok) given++;
  }
  PrintFormat("[ACR] Construction crew given shovels: %1", given);
  return given;
 }
 static string s_sLastShovel;
 static bool HasShovel(IEntity character)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item || !item.GetPrefabData()) continue;
   string prefab = item.GetPrefabData().GetPrefabName();
   // (the pouch, "ETool_..._carrier", is not the shovel)
   if (prefab.Contains("ETool") && !prefab.Contains("carrier")) { s_sLastShovel = prefab; return true; }
  }
  return false;
 }

 string Reinforce(SCR_PlayerController pc, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad || !squad.m_Group) return "Select a friendly squad first";
  string key = squad.GetFaction().GetFactionKey();
  if (SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId()) != squad.GetFaction()) return "That squad belongs to another faction";
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return "Only the commander can reinforce squads";
  array<ResourceName> missing = {};
  squad.GetMissing(missing);
  IEntity anchor = squad.GetAnchor();
  vector near = vector.Zero;
  if (anchor) near = anchor.GetOrigin();
  // Inside a base that can deploy units, the squad also gets its ammo back.
  ACR_Base depot = DeployBaseAt(key, near);
  if (missing.IsEmpty())
  {
   if (!depot) return squad.GetName() + " is at full strength (ammo only inside a base that can deploy units)";
   int restocked = RestockAmmo(squad, depot);
   PrintFormat("[ACR] REINFORCE %1 ammo only at %2: %3 soldiers restocked", squad.GetName(), depot.m_sName, restocked);
   if (!restocked) return squad.GetName() + " is at full strength and fully stocked";
   return string.Format("%1: %2 soldiers restocked with ammo at %3", squad.GetName(), restocked, depot.m_sName);
  }
  ACR_Base base = depot;
  if (!base) base = NearestSpawnBase(key, near);
  if (!base) return "No friendly base can send reinforcements";
  int cost = missing.Count() * REINFORCE_COST_PER_SOLDIER;
  if (!m_Manager.m_Bases.Spend(key, cost)) return string.Format("Not enough RP (%1 needed)", cost);
  m_Manager.LogSpend(pc, key, m_Manager.m_Bases.GetRP(key) + cost, "reinforced " + squad.GetName());
  foreach (ResourceName prefab : missing)
  {
   IEntity soldier = ACR_Manager.SpawnPrefab(prefab, SpawnPoint(base, 4, 12, 0.8));
   if (soldier) squad.m_Group.AddAIEntityToGroup(soldier);
  }
  int resupplied;
  if (depot) resupplied = RestockAmmo(squad, depot);
  PrintFormat("[ACR] REINFORCE %1 +%2 from %3 for %4 RP, %5 restocked", squad.GetName(), missing.Count(), base.m_sName, cost, resupplied);
  string result = string.Format("%1: %2 reinforcements from %3", squad.GetName(), missing.Count(), base.m_sName);
  if (resupplied) result += string.Format(", %1 restocked with ammo", resupplied);
  return result;
 }

 // A friendly, uncontested base that can deploy units, with the point inside its circle.
 ACR_Base DeployBaseAt(string factionKey, vector position)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey || !base.m_bAllowSpawn || base.m_bContested) continue;
   if (vector.DistanceXZ(base.GetOrigin(), position) <= base.m_fRadius) return base;
  }
  return null;
 }

 // Magazines back up to each soldier's normal load and a new launcher for one who
 // threw a spent one away, for members standing inside the base circle (free: the
 // base's stores, not a truck's). Returns how many soldiers got something.
 int RestockAmmo(ACR_Squad squad, ACR_Base base)
 {
  int served;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier)) continue;
   if (vector.DistanceXZ(soldier.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
   bool got;
   if (m_Manager.m_Launchers && m_Manager.m_Launchers.IsOwed(soldier) && m_Manager.m_Launchers.Rearm(soldier)) got = true;
   int target = squad.MagazineTarget(soldier);
   if (ACR_Logistics.SoldierMagazines(soldier) < target)
   {
    SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
    // Same count the ammo trucks hand out (the loaded magazine is the last one).
    int magazines = Math.Max(2, target - 1);
    if (inventory && inventory.IsResupplyMagazinesAvailable(magazines))
    {
     inventory.ResupplyMagazines(magazines);
     got = true;
    }
   }
   if (got) served++;
  }
  return served;
 }

}
