// Every vehicle in the world (placed, bought or spawned by Game Master) is
// registered when it is created, so the server can list a side's empty
// vehicles for its map without scanning the world.
class ACR_Vehicles
{
 protected static ref array<IEntity> s_All = {};
 protected static ref map<IEntity, string> s_Owner = new map<IEntity, string>();

 static void Register(IEntity vehicle)
 {
  if (!vehicle || !Vehicle.Cast(vehicle) || s_All.Contains(vehicle)) return;
  s_All.Insert(vehicle);
 }

 static void SetOwner(IEntity vehicle, string faction)
 {
  if (vehicle && !faction.IsEmpty()) s_Owner.Set(vehicle, faction);
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
 }

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
 override event void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  ACR_Vehicles.Register(owner);
 }
}

// Bought vehicles of a side with its own crewmen (RHS) seat those instead of the
// vehicle's default crew: RHS reuses vanilla vehicles, whose seats name vanilla 80s
// soldiers (Jacob, 8 Oct). Seats without a default crewman stay empty.
modded class BaseCompartmentSlot
{
 override ResourceName GetDefaultOccupantPrefab(bool checkIfValid = true)
 {
  ResourceName own = super.GetDefaultOccupantPrefab(checkIfValid);
  if (own.IsEmpty()) return own;
  IEntity vehicle = GetVehicle();
  string owner = ACR_Vehicles.BoughtBy(vehicle);
  if (owner.IsEmpty()) return own;
  ResourceName crewman = ACR_FactionCatalog.CrewmanFor(owner, vehicle);
  if (crewman.IsEmpty()) return own;
  return crewman;
 }
}
