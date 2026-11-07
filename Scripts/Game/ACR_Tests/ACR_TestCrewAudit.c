// -acrCrewAuditTest 1 (server-capable, ~1.5 min; preset crewaudit): buys every crewed vehicle on the
// US, USSR and FIA buy lists (with a content pack loaded, its vehicles too) and reports each seat:
// type, name, who sits there, the default occupant and the turret's weapons. Checks that every
// crew seat (driver/pilot, every turret, a commander seat wired to a turret) of a bought vehicle
// is filled by the buyer's side, and that no rider (plain cargo) seat is. Seats are sampled every
// second for 15 s after the purchase (who sat there first): the bases on the test map are in sight
// of each other, so unarmed vehicles' drivers bail and gunners die soon after.
class ACR_TestCrewAudit
{
 static ref array<ref ACR_PendingPurchase> s_Bought;
 static ref array<string> s_Sides;
 static int s_iExpected;
 // "<purchase index>:<seat index>" -> who sat there first.
 static ref map<string, string> s_Seen;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Bought = {};
  s_Sides = {};
  s_Seen = new map<string, string>();
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings west = ACR_FactionSettings.Find("US");
   if (west) west.m_bAICommander = false;
   ACR_FactionSettings east = ACR_FactionSettings.Find("USSR");
   if (east) east.m_bAICommander = false;
  }
  if (tick == 6) BuyAll(manager);
  if (tick >= 7 && tick <= 22) Sample();
  if (tick == 23) Report();
  if (tick == 25) ACR_TestRegress.Complete("crewaudit", s_iExpected);
 }

 protected static ACR_Base HomeOf(string side)
 {
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == side) return candidate;
  return null;
 }

 protected static void BuyAll(ACR_Manager manager)
 {
  array<string> sides = {"US", "USSR", "FIA"};
  foreach (string side : sides)
  {
   ACR_Base home = HomeOf(side);
   float ring = 40;
   if (!home)
   {
    // FIA often holds no base: park its vehicles well out from the US one.
    home = HomeOf("US");
    ring = 260;
   }
   if (!home) { PrintFormat("[ACR] CREWAUDIT no base for %1", side); continue; }
   array<ref ACR_RequisitionEntry> entries = {};
   ACR_FactionCatalog.BuildRequisitions(side, ACR_FactionSettings.Find(side), entries);
   int slot;
   foreach (ACR_RequisitionEntry entry : entries)
   {
    if (entry.m_sVehicle.IsEmpty() || !entry.m_bCrewed || entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli > 0) continue;
    vector at = manager.ClearSpotNear(home, ring + 22 * slot, ring + 22 * slot + 40, 7);
    slot++;
    ACR_PendingPurchase purchase = new ACR_PendingPurchase();
    purchase.m_sName = entry.m_sName;
    purchase.m_sFaction = side;
    manager.m_sSpawningFaction = side;
    bool ok = manager.m_Requisition.SpawnParts(purchase, "", entry.m_sVehicle, true, "", at, at);
    manager.m_sSpawningFaction = "";
    PrintFormat("[ACR] CREWAUDIT bought %1 for %2: %3 %4 %5", entry.m_sName, side, ok, entry.m_sVehicle, purchase.m_sError);
    s_Bought.Insert(purchase);
    s_Sides.Insert(side);
   }
  }
 }

 protected static string Leaf(string path)
 {
  if (path.IsEmpty()) return "-";
  return FilePath.StripExtension(FilePath.StripPath(path));
 }

 protected static string TypeName(ECompartmentType type)
 {
  if (type == ECompartmentType.PILOT) return "PILOT";
  if (type == ECompartmentType.TURRET) return "TURRET";
  if (type == ECompartmentType.CARGO) return "CARGO";
  return "OTHER";
 }

 // The turret's weapons, by name ("" for a sight-only turret such as a periscope).
 protected static string TurretWeapons(BaseCompartmentSlot seat)
 {
  IEntity turret = seat.GetOwner();
  if (!turret) return "";
  TurretControllerComponent controller = TurretControllerComponent.Cast(turret.FindComponent(TurretControllerComponent));
  if (!controller || !controller.GetWeaponManager()) return "";
  array<BaseWeaponComponent> weapons = {};
  controller.GetWeaponManager().GetWeapons(weapons);
  string text;
  foreach (BaseWeaponComponent weapon : weapons)
  {
   if (!weapon) continue;
   IEntity owner = weapon.GetOwner();
   string name = "weapon";
   if (owner && owner.GetPrefabData()) name = Leaf(owner.GetPrefabData().GetPrefabName());
   text += name + "+";
  }
  return text;
 }

 // Seats in compartment managers anywhere in the vehicle's hierarchy that the hull's manager
 // does not list (a turret entity with its own manager).
 protected static void NestedSeats(IEntity entity, notnull array<BaseCompartmentSlot> listed, notnull array<BaseCompartmentSlot> result)
 {
  IEntity child = entity.GetChildren();
  while (child)
  {
   BaseCompartmentManagerComponent own = BaseCompartmentManagerComponent.Cast(child.FindComponent(BaseCompartmentManagerComponent));
   if (own)
   {
    array<BaseCompartmentSlot> seats = {};
    own.GetCompartments(seats);
    foreach (BaseCompartmentSlot seat : seats)
     if (!listed.Contains(seat) && !result.Contains(seat)) result.Insert(seat);
   }
   NestedSeats(child, listed, result);
   child = child.GetSibling();
  }
 }

 protected static void Seats(IEntity vehicle, notnull array<BaseCompartmentSlot> seats)
 {
  seats.Clear();
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments) compartments.GetCompartments(seats);
 }

 protected static void Sample()
 {
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   array<BaseCompartmentSlot> seats = {};
   Seats(purchase.m_Vehicle, seats);
   foreach (int j, BaseCompartmentSlot seat : seats)
   {
    IEntity man = seat.GetOccupant();
    string key = string.Format("%1:%2", i, j);
    if (!man || !man.GetPrefabData() || s_Seen.Contains(key)) continue;
    s_Seen.Set(key, Leaf(man.GetPrefabData().GetPrefabName()) + ":" + ACR_Intel.FactionKeyOf(man));
   }
  }
 }

 protected static void Report()
 {
  int vehicles;
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   string side = s_Sides[i];
   IEntity vehicle = purchase.m_Vehicle;
   if (!vehicle)
   {
    PrintFormat("[ACR] CREWAUDIT %1 (%2): no vehicle (%3)", purchase.m_sName, side, purchase.m_sError);
    ACR_TestRegress.Check("crew: " + side + " " + purchase.m_sName + " spawned", false, purchase.m_sError);
    s_iExpected++;
    continue;
   }
   vehicles++;
   array<BaseCompartmentSlot> seats = {};
   Seats(vehicle, seats);
   array<BaseCompartmentSlot> nested = {};
   NestedSeats(vehicle, seats, nested);
   int crewSeats;
   int crewFilled;
   int riders;
   int wrongSide;
   string missing;
   foreach (int j, BaseCompartmentSlot seat : seats)
   {
    bool crewSeat = seat.GetType() == ECompartmentType.PILOT || seat.GetType() == ECompartmentType.TURRET || ACR_Transport.CommanderSeat(seat);
    string who;
    bool man = s_Seen.Find(string.Format("%1:%2", i, j), who);
    if (man && !who.EndsWith(":" + side)) wrongSide++;
    if (!man) who = "empty";
    string name = seat.GetCompartmentName();
    PrintFormat("[ACR] CREWAUDIT seat %1 (%2) | %3 '%4' crew=%5 access=%6 | %7 | default %8 | turret=%9", purchase.m_sName, side, TypeName(seat.GetType()), name, crewSeat, seat.IsCompartmentAccessible(), who, Leaf(seat.GetDefaultOccupantPrefab(false)), seat.GetAutoConnectTurret() + " " + TurretWeapons(seat));
    if (crewSeat)
    {
     crewSeats++;
     if (man) crewFilled++;
     else missing += " " + TypeName(seat.GetType()) + ":'" + name + "'";
    }
    if (!crewSeat && man) riders++;
   }
   foreach (BaseCompartmentSlot hidden : nested)
    PrintFormat("[ACR] CREWAUDIT nested seat %1 (%2) | %3 '%4' | occupant %5", purchase.m_sName, side, TypeName(hidden.GetType()), hidden.GetCompartmentName(), hidden.GetOccupant() != null);
   string summary = string.Format("%1/%2 crew seats, %3 riders, %4 other side, %5 nested;%6", crewFilled, crewSeats, riders, wrongSide, nested.Count(), missing);
   PrintFormat("[ACR] CREWAUDIT vehicle %1 (%2) %3: %4", purchase.m_sName, side, Leaf(vehicle.GetPrefabData().GetPrefabName()), summary);
   ACR_TestRegress.Check("crew: " + side + " " + purchase.m_sName + " has every crew seat filled by its side and no riders", crewSeats > 0 && crewFilled == crewSeats && riders == 0 && wrongSide == 0, summary);
   s_iExpected++;
  }
  PrintFormat("[ACR] CREWAUDIT %1 vehicles bought", vehicles);
 }
}
