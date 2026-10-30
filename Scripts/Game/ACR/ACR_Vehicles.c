// Every vehicle in the world (placed, bought or spawned by Game Master) is
// registered when it is created, so the server can list a side's empty
// vehicles for its map without scanning the world.
class ACR_Vehicles
{
 protected static ref array<IEntity> s_All = {};
 protected static ref map<IEntity, string> s_Owner = new map<IEntity, string>();
 protected static ref map<IEntity, ResourceName> s_CustomCrew = new map<IEntity, ResourceName>();

 static void Register(IEntity vehicle)
 {
  if (!vehicle || !Vehicle.Cast(vehicle) || s_All.Contains(vehicle)) return;
  s_All.Insert(vehicle);
 }

 static void SetOwner(IEntity vehicle, string faction)
 {
  if (!vehicle || faction.IsEmpty()) return;
  s_Owner.Set(vehicle, faction);
  // Requisition can reuse a vanilla vehicle prefab for RHS. Set its actual
  // affiliation as well as our logistics owner so faction-aware systems such as
  // arsenals see the purchasing side's item catalog.
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey(faction);
  SCR_VehicleFactionAffiliationComponent vehicleFaction = SCR_VehicleFactionAffiliationComponent.Cast(affiliation);
  if (vehicleFaction) vehicleFaction.ACR_SetPurchasingFaction(faction);
 }

 static SCR_ArsenalComponent ArsenalOf(IEntity entity)
 {
  if (!entity) return null;
  SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity.FindComponent(SCR_ArsenalComponent));
  if (arsenal) return arsenal;
  IEntity child = entity.GetChildren();
  while (child)
  {
   arsenal = ArsenalOf(child);
   if (arsenal) return arsenal;
   child = child.GetSibling();
  }
  return null;
 }

 static void RefreshArsenals(IEntity entity)
 {
  if (!entity) return;
  SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity.FindComponent(SCR_ArsenalComponent));
  if (arsenal) arsenal.RefreshArsenal();
  IEntity child = entity.GetChildren();
  while (child)
  {
   RefreshArsenals(child);
   child = child.GetSibling();
  }
 }

 static void SetCrewPrefab(IEntity vehicle, ResourceName crewman)
 {
  if (vehicle && !crewman.IsEmpty()) s_CustomCrew.Set(vehicle, crewman);
 }

 static ResourceName CustomCrew(IEntity vehicle)
 {
  ResourceName crewman;
  if (vehicle) s_CustomCrew.Find(vehicle, crewman);
  return crewman;
 }

 // Living vehicles (deleted ones are dropped as they are found).
 static void All(notnull array<IEntity> result)
 {
  result.Clear();
  for (int i = s_All.Count() - 1; i >= 0; i--)
  {
   IEntity vehicle = s_All[i];
   if (!vehicle) { s_All.Remove(i); continue; }
   result.Insert(vehicle);
  }
  Prune();
 }

 // Living vehicles without running pruning passes (suitable for per-frame sweeps).
 static void Living(notnull array<IEntity> result)
 {
  result.Clear();
  for (int i = s_All.Count() - 1; i >= 0; i--)
  {
   IEntity vehicle = s_All[i];
   if (!vehicle) { s_All.Remove(i); continue; }
   result.Insert(vehicle);
  }
 }

 // Drop deleted vehicles from the side and crew records (R18).
 static void Prune()
 {
  for (int i = s_Owner.Count() - 1; i >= 0; i--)
   if (!s_Owner.GetKey(i)) s_Owner.RemoveElement(i);
  for (int j = s_CustomCrew.Count() - 1; j >= 0; j--)
   if (!s_CustomCrew.GetKey(j)) s_CustomCrew.RemoveElement(j);
 }

 static void Reset()
 {
  s_All.Clear();
  s_Owner.Clear();
  s_CustomCrew.Clear();
 }

 static int RecordCount() { return s_Owner.Count() + s_CustomCrew.Count(); }

 // The side that bought it through AC: Actual, "" for any other vehicle.
 static string BoughtBy(IEntity vehicle)
 {
  string owner;
  if (vehicle) s_Owner.Find(vehicle, owner);
  return owner;
 }

 // Who a vehicle belongs to: the side that bought it, else its default faction.
 static string FactionOf(IEntity vehicle)
 {
  string owner;
  if (s_Owner.Find(vehicle, owner)) return owner;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
  if (!affiliation) return "";
  Faction faction = affiliation.GetDefaultAffiliatedFaction();
  if (!faction) faction = affiliation.GetAffiliatedFaction();
  if (!faction) return "";
  return faction.GetFactionKey();
 }

 static bool IsEmpty(IEntity vehicle)
 {
  SCR_VehicleFactionAffiliationComponent affiliation = SCR_VehicleFactionAffiliationComponent.Cast(vehicle.FindComponent(SCR_VehicleFactionAffiliationComponent));
  if (affiliation) return !affiliation.IsVehicleOccupied();
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return true;
  array<IEntity> occupants = {};
  compartments.GetOccupants(occupants);
  return occupants.IsEmpty();
 }

 // Short readable name for a map label.
 static string NameOf(IEntity vehicle)
 {
  SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(vehicle.FindComponent(SCR_EditableEntityComponent));
  if (editable && editable.GetInfo() && !editable.GetInfo().GetName().IsEmpty()) return editable.GetInfo().GetName();
  string name = "Vehicle";
  if (vehicle.GetPrefabData()) name = vehicle.GetPrefabData().GetPrefabName();
  int slash = name.LastIndexOf("/");
  if (slash >= 0) name = name.Substring(slash + 1, name.Length() - slash - 1);
  name.Replace(".et", "");
  name.Replace("_", " ");
  return name;
 }
}

modded class SCR_VehicleFactionAffiliationComponent
{
 [RplProp(onRplName: "ACR_OnPurchasingFaction")]
 protected string m_sACRPurchasingFaction;

 string ACR_GetPurchasingFaction() { return m_sACRPurchasingFaction; }

 void ACR_SetPurchasingFaction(string faction)
 {
  if (m_sACRPurchasingFaction == faction) return;
  m_sACRPurchasingFaction = faction;
  Replication.BumpMe();
  ACR_OnPurchasingFaction();
 }

 protected void ACR_OnPurchasingFaction()
 {
  ACR_Vehicles.RefreshArsenals(GetOwner());
 }

 override event void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  ACR_Vehicles.Register(owner);
 }
}

modded class SCR_ArsenalComponent
{
 override SCR_Faction GetAssignedFaction()
 {
  IEntity entity = GetOwner();
  while (entity)
  {
   SCR_VehicleFactionAffiliationComponent affiliation = SCR_VehicleFactionAffiliationComponent.Cast(entity.FindComponent(SCR_VehicleFactionAffiliationComponent));
   if (affiliation && !affiliation.ACR_GetPurchasingFaction().IsEmpty())
   {
    SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(affiliation.ACR_GetPurchasingFaction()));
    if (faction) return faction;
   }
   entity = entity.GetParent();
  }
  return super.GetAssignedFaction();
 }
}

// Bought vehicles of a side with its own crewmen (RHS) seat those instead of the
// vehicle's default crew: RHS reuses vanilla vehicles, whose seats name vanilla 80s
// soldiers (Jacob, 8 Oct). Missing defaults stay empty except Typhoon crew seats and the crew
// seats a content pack fills (ACR_ContentSocket).
modded class BaseCompartmentSlot
{
 override ResourceName GetDefaultOccupantPrefab(bool checkIfValid = true)
 {
  ResourceName own = super.GetDefaultOccupantPrefab(checkIfValid);
  IEntity vehicle = GetVehicle();
  string owner = ACR_Vehicles.BoughtBy(vehicle);
  if (owner.IsEmpty()) return own;
  ResourceName custom = ACR_Vehicles.CustomCrew(vehicle);
  if (!custom.IsEmpty()) return custom;
  // A content pack's vehicles name the other side's soldiers or nobody (#108: Zagoria tanks seat
  // USSR riflemen whoever owns them, the M2 has no driver): the pack picks. Empty passenger
  // seats stay empty.
  if (!own.IsEmpty() || GetType() == ECompartmentType.PILOT || GetType() == ECompartmentType.TURRET)
  {
   ResourceName packed = ACR_ContentSocket.Get().Crewman(owner, vehicle, GetType());
   if (!packed.IsEmpty()) return packed;
  }
  if (own.IsEmpty())
  {
   // RHS/UA Typhoons omit default occupants, including the armed turret. Supply
   // only their working crew seats; empty passenger slots must stay empty.
   if (GetType() != ECompartmentType.PILOT && GetType() != ECompartmentType.TURRET) return own;
   if (!vehicle || !vehicle.GetPrefabData()) return own;
   string path = vehicle.GetPrefabData().GetPrefabName();
   if (!path.Contains("/Vehicles/Wheeled/K4386/")) return own;
  }
  ResourceName crewman = ACR_FactionCatalog.CrewmanFor(owner, vehicle);
  if (crewman.IsEmpty()) return own;
  return crewman;
 }
}

// LAV-style commander seats are CARGO slots wired to a commander sight turret
// (AutoConnectToTurret), so a crew spawned for PILOT + TURRET left the LAV without its
// commander (Jacob, 20 Oct). Bought vehicles also get those seats filled, in the same group.
modded class SCR_BaseCompartmentManagerComponent
{
 override protected bool InitiateSpawnDefaultOccupants(notnull array<ECompartmentType> compartmentTypes)
 {
  bool started = super.InitiateSpawnDefaultOccupants(compartmentTypes);
  if (!started || !m_aCompartmentsToSpawnDefaultOccupant || !compartmentTypes.Contains(ECompartmentType.TURRET)) return started;
  if (ACR_Vehicles.BoughtBy(GetOwner()).IsEmpty()) return started;
  array<BaseCompartmentSlot> cargo = {};
  GetFreeCompartmentsOfType(cargo, ECompartmentType.CARGO, true);
  foreach (BaseCompartmentSlot seat : cargo)
   if (ACR_Transport.CommanderSeat(seat) && !m_aCompartmentsToSpawnDefaultOccupant.Contains(seat)) m_aCompartmentsToSpawnDefaultOccupant.Insert(seat);
  return started;
 }
}
