// -acrStructuresTest 1 (server-capable, ~2 min): player-built structures. Finished compositions
// are spawned on the test range (as if built by US players): a field hospital heals a wounded
// soldier, vehicle maintenance + fuel storage repair and refuel a parked Humvee, a helipad takes
// a bought helicopter, a headquarters away from the bases becomes a forward base where units can
// be bought; removing the structures undoes it.
class ACR_TestStructures
{
 static const ResourceName HOSPITAL = "{D0281DAF0B131743}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_US_01.et";
 static const ResourceName REPAIR = "{7AD06D9CF2672AC5}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et";
 static const ResourceName FUEL = "{6D2A35DA74224780}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_US_01.et";
 static const ResourceName AMMO = "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et";
 static const ResourceName HELIPAD = "{D3DDB76548C16BDB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_US_01.et";
 static const ResourceName HQ = "{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et";
 static const ResourceName HUMVEE = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 static const ResourceName CONSTRUCTION = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";

 static IEntity s_Hospital, s_Repair, s_Fuel, s_Ammo, s_Helipad, s_HQ, s_Humvee, s_Construction;
 static IEntity s_Wounded;
 static ACR_Base s_Base;
 static vector s_Yard;
 static bool s_bHealed, s_bRepaired, s_bRefuelled;

 static IEntity Place(ResourceName prefab, vector at)
 {
  IEntity entity = ACR_Manager.SpawnPrefab(prefab, at);
  PrintFormat("[ACR] STRUCTURETEST placed %1 at %2: %3", FilePath.StripPath(prefab), at, entity != null);
  return entity;
 }

 static int DistanceTo(IEntity a, IEntity b)
 {
  if (!a || !b) return -1;
  return vector.DistanceXZ(a.GetOrigin(), b.GetOrigin());
 }

 static bool InsideABase(vector at, float margin)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (vector.DistanceXZ(base.GetOrigin(), at) <= base.m_fRadius + margin) return true;
  return false;
 }

 static ACR_Base BaseNamed(string prefix)
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName.StartsWith(prefix)) return base;
  return null;
 }

 static int EntryIndex(ACR_Manager manager, bool aircraft)
 {
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iSupportHeli != 0 || entry.m_bMortarTeam) continue;
   if (aircraft && entry.m_bAircraft && !entry.m_sVehicle.IsEmpty()) return i;
   if (!aircraft && !entry.m_bAircraft && entry.m_sVehicle.IsEmpty() && !entry.m_sGroup.IsEmpty()) return i;
  }
  return -1;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (tick <= 10 && (!alpha || !alpha.GetAnchor())) return;
  if (!alpha) return;
  if (tick == 3)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner != "US") continue;
    float d = vector.DistanceXZ(base.GetOrigin(), alpha.m_Home);
    if (d < best) { best = d; s_Base = base; }
   }
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   vector home = alpha.GetAnchor().GetOrigin();
   // Field hospital beside Alpha; one of Alpha's soldiers is wounded.
   s_Hospital = Place(HOSPITAL, home + Vector(21, 0, 0));
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!soldier || soldier == alpha.GetAnchor()) continue;
    SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(soldier.FindComponent(SCR_CharacterDamageManagerComponent));
    if (!damage) continue;
    HitZone zone = damage.GetDefaultHitZone();
    if (zone) zone.SetHealthScaled(0.5);
    s_Wounded = soldier;
    break;
   }
   PrintFormat("[ACR] STRUCTURETEST wounded soldier health %1", ACR_Logistics.Health(s_Wounded));
   // A yard 120 m off: vehicle maintenance, fuel and ammo storage and a damaged, nearly dry Humvee.
   s_Yard = ACR_Manager.Ground(home + Vector(-120, 0, 40));
   s_Repair = Place(REPAIR, s_Yard + Vector(14, 0, 0));
   s_Fuel = Place(FUEL, s_Yard + Vector(-12, 0, 0));
   s_Construction = Place(CONSTRUCTION, s_Yard + Vector(35, 0, 0));
   if (s_Construction) ACR_Vehicles.SetOwner(s_Construction, "US");
   // (No ammo storage: its vanilla resupply action logs a script error on a dedicated server,
   // which ends the run; it is registered the same way as fuel storage.)
   s_Humvee = Place(HUMVEE, s_Yard);
   if (s_Humvee)
   {
    ACR_Vehicles.SetOwner(s_Humvee, "US");
    SCR_DamageManagerComponent vdamage = SCR_DamageManagerComponent.Cast(s_Humvee.FindComponent(SCR_DamageManagerComponent));
    if (vdamage) vdamage.SetHealthScaled(0.4);
    SCR_FuelManagerComponent tanks = SCR_FuelManagerComponent.Cast(s_Humvee.FindComponent(SCR_FuelManagerComponent));
    if (tanks) tanks.SetTotalFuelPercentage(0.2);
    PrintFormat("[ACR] STRUCTURETEST humvee health %1 fuel %2", ACR_Logistics.Health(s_Humvee), ACR_Logistics.Fuel(s_Humvee));
   }
   // Helipad at the base.
   vector padSpot = manager.ClearSpotNear(s_Base, 30, 70, 14);
   s_Helipad = Place(HELIPAD, padSpot);
   // Headquarters away from every base.
   vector hqSpot;
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450));
    if (!InsideABase(candidate, 60) && !BBB_Terrain.IsWaterAt(candidate)) { hqSpot = candidate; break; }
   }
   s_HQ = Place(HQ, hqSpot);
  }
  if (tick == 8)
  {
   ACR_TestRegress.Check("structures: helipad registered as a US pad", ACR_Helipad.s_All.Count() > 0 && ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1].Faction() == "US", string.Format("pads %1", ACR_Helipad.s_All.Count()));
   array<IEntity> stations = {};
   manager.m_Logistics.Stations("US", ACR_EService.MEDICAL, stations);
   ACR_TestRegress.Check("structures: field hospital is a medical point", stations.Contains(s_Hospital), string.Format("%1 medical stations", stations.Count()));
   manager.m_Logistics.Stations("US", ACR_EService.FUEL, stations);
   ACR_TestRegress.Check("structures: fuel storage is a fuel point", stations.Contains(s_Fuel), string.Format("%1 fuel stations", stations.Count()));
   SCR_CampaignBuildingCompositionComponent hospital = SCR_CampaignBuildingCompositionComponent.Cast(s_Hospital.FindComponent(SCR_CampaignBuildingCompositionComponent));
   float constructionStock = manager.m_Logistics.StockOf(s_Construction);
   if (hospital) hospital.ACR_SetProviderForTest(s_Construction);
   if (hospital) hospital.ACR_OnCompositionSpawned(true);
   if (hospital) hospital.ACR_OnCompositionSpawned(true);
   float afterConstructionStock = manager.m_Logistics.StockOf(s_Construction);
   ACR_TestRegress.Check("structures: completed build charges its truck once", hospital && afterConstructionStock < constructionStock && Math.AbsFloat((constructionStock - afterConstructionStock) - 10) < 0.01, string.Format("stock %1 -> %2 loads", constructionStock, afterConstructionStock));
   ACR_TestRegress.Check("structures: an ammo storage counts as ammo", ACR_Structures.ServiceOf(ACR_Structures.KindOfName(AMMO)) == ACR_EService.AMMO, "");
   ACR_Base fob = BaseNamed("FOB");
   ACR_TestRegress.Check("structures: headquarters away from bases = a forward base", fob && fob.m_sOwner == "US" && fob.m_bAllowSpawn, "");
   if (fob)
   {
    string bought = manager.PurchaseAt("US", EntryIndex(manager, false), fob);
    ACR_TestRegress.Check("structures: units can be bought at the forward base", bought.Contains("deploying at " + fob.m_sName), bought);
   }
   int heli = EntryIndex(manager, true);
   if (heli >= 0)
   {
    ACR_Helipad pad = ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1];
    string boughtHeli = manager.PurchaseAt("US", heli, pad.Base());
    PrintFormat("[ACR] STRUCTURETEST bought helicopter at %1: %2", pad.Base().m_sName, boughtHeli);
    GetGame().GetCallqueue().CallLater(CheckHeliOnPad, 3000, false, pad);
   }
   else Print("[ACR] STRUCTURETEST no helicopter in the US buy list");
  }
  if (tick > 8 && tick % 5 == 0)
   PrintFormat("[ACR] STRUCTURETEST t=%1 wounded %2 (%5 m from the hospital), humvee health %3 fuel %4", tick, ACR_Logistics.Health(s_Wounded), ACR_Logistics.Health(s_Humvee), ACR_Logistics.Fuel(s_Humvee), DistanceTo(s_Wounded, s_Hospital));
  if (!s_bHealed && s_Wounded && ACR_Logistics.Health(s_Wounded) > 0.95 && tick > 8)
  {
   s_bHealed = true;
   ACR_TestRegress.Check("structures: the field hospital healed the wounded soldier after a stay", tick >= 22, string.Format("t=%1", tick));
  }
  if (!s_bRepaired && s_Humvee && ACR_Logistics.Health(s_Humvee) > 0.95 && tick > 8)
  {
   s_bRepaired = true;
   ACR_TestRegress.Check("structures: vehicle maintenance repaired the Humvee slowly", tick >= 30, string.Format("t=%1", tick));
  }
  if (!s_bRefuelled && s_Humvee && ACR_Logistics.Fuel(s_Humvee) > 0.9 && tick > 8)
  {
   s_bRefuelled = true;
   ACR_TestRegress.Check("structures: fuel storage refuelled the Humvee", true, string.Format("t=%1", tick));
  }
  if (tick == 75)
  {
   ACR_TestRegress.Check("structures: healed by t=75", s_bHealed, string.Format("%1", ACR_Logistics.Health(s_Wounded)));
   ACR_TestRegress.Check("structures: repaired by t=75", s_bRepaired, string.Format("%1", ACR_Logistics.Health(s_Humvee)));
   ACR_TestRegress.Check("structures: refuelled by t=75", s_bRefuelled, string.Format("%1", ACR_Logistics.Fuel(s_Humvee)));
   if (s_HQ) SCR_EntityHelper.DeleteEntityAndChildren(s_HQ);
   if (s_Hospital) SCR_EntityHelper.DeleteEntityAndChildren(s_Hospital);
   if (s_Helipad) SCR_EntityHelper.DeleteEntityAndChildren(s_Helipad);
  }
  if (tick == 80)
  {
   ACR_TestRegress.Check("structures: forward base gone with its headquarters", BaseNamed("FOB") == null, "");
   array<IEntity> medical = {};
   manager.m_Logistics.Stations("US", ACR_EService.MEDICAL, medical);
   ACR_TestRegress.Check("structures: no medical point once the hospital is gone", medical.IsEmpty(), string.Format("%1", medical.Count()));
   ACR_Helipad last = ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1];
   ACR_TestRegress.Check("structures: pad gone with its helipad", last.Faction() == "", last.Faction());
   ACR_TestRegress.Complete("structures");
  }
 }

 static void CheckHeliOnPad(ACR_Helipad pad)
 {
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  float best = float.MAX;
  foreach (IEntity vehicle : vehicles)
  {
   if (!ACR_Logistics.ServiceOf(vehicle) && vehicle.GetPrefabData() && vehicle.GetPrefabData().GetPrefabName().Contains("Helicopter"))
    best = Math.Min(best, vector.DistanceXZ(vehicle.GetOrigin(), pad.GetOrigin()));
  }
  ACR_TestRegress.Check("structures: the bought helicopter appeared on the built helipad", best < 4, string.Format("%1 m", best));
 }
}
