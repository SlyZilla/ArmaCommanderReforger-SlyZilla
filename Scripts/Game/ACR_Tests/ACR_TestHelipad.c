// -acrHelipadTest 1 (server-capable, ~3 min): support helicopters on helipads. A pad is placed
// at a US base; a gunship is stationed there, flies a rocket run from the pad, comes back,
// lands on the pad, is serviced, and is gone for good once destroyed.
class ACR_TestHelipad
{
 static ACR_Helipad s_Pad;
 static ref ACR_StationedHeli s_Heli; // ref: the station drops it when it is lost
 static ACR_Base s_Base;
 static vector s_Target;
 static int s_iCalledAt = -1;
 static int s_iLandedAt = -1;
 static int s_iReadyAt = -1;
 static int s_iRP;
 static float s_fTopHeight;
 static IEntity s_Ejected;
 static int s_iRecrewAt = -1;

 static int CrewSeats(IEntity vehicle)
 {
  int count;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  foreach (BaseCompartmentSlot slot : slots)
   if (slot.GetType() == ECompartmentType.PILOT || slot.GetType() == ECompartmentType.TURRET) count++;
  return count;
 }

 // Crew seated in the pilot and gun seats, alive.
 static int CrewAboard(IEntity vehicle)
 {
  int count;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetType() != ECompartmentType.PILOT && slot.GetType() != ECompartmentType.TURRET) continue;
   if (slot.GetOccupant() && ACR_Respawn.IsAlive(slot.GetOccupant())) count++;
  }
  return count;
 }

 static int Aboard(IEntity vehicle)
 {
  int count;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  foreach (BaseCompartmentSlot slot : slots) if (slot.GetOccupant()) count++;
  return count;
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
   ACR_HeliStation.SERVICE_SECONDS = 15;
  }
  // (Alpha only places things: the burning wreck at the end may kill it.)
  if (tick <= 10 && (!alpha || !alpha.GetAnchor())) return;
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  ACR_HeliStation station = manager.m_Supports.Station();
  if (tick == 3)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner != "US") continue;
    float d = vector.DistanceXZ(base.GetOrigin(), alpha.m_Home);
    if (d < best) { best = d; s_Base = base; }
   }
   vector spot = manager.ClearSpotNear(s_Base, 20, 35, 9);
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   parameters.Transform[3] = spot;
   s_Pad = ACR_Helipad.Cast(GetGame().SpawnEntity(ACR_Helipad, GetGame().GetWorld(), parameters));
   PrintFormat("[ACR] HELIPADTEST pad at %1 (%2 m from %3), pads in the world %4", spot, (int)vector.DistanceXZ(spot, s_Base.GetOrigin()), s_Base.m_sName, ACR_Helipad.s_All.Count());
  }
  if (tick == 5)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 500;
   ACR_Base other;
   foreach (ACR_Base b : ACR_Base.s_All) if (b != s_Base && b.m_sOwner == "US") other = b;
   if (other) ACR_TestRegress.Check("helipad: no buying at a base without a pad", station.CantBuy("US", other).Contains("no helipad"), station.CantBuy("US", other));
   ACR_TestRegress.Check("helipad: the pad belongs to its base", s_Pad && s_Pad.Base() == s_Base, "");
   ACR_Base named;
   foreach (ACR_Base nb : ACR_Base.s_All) if (nb != s_Base) named = nb;
   if (named && s_Pad)
   {
    s_Pad.m_sBase = named.m_sName;
    ACR_TestRegress.Check("helipad: a named base owns the pad", s_Pad.Base() == named, named.m_sName);
    s_Pad.m_sBase = "";
   }
   s_Heli = station.Station("US", true, s_Base, "Support gunship 1");
   ACR_TestRegress.Check("helipad: gunship stationed on the pad", s_Heli != null && s_Heli.m_Vehicle != null, "");
   ACR_TestRegress.Check("helipad: a second one finds the pad taken", station.CantBuy("US", s_Base).Contains("No free helipad"), station.CantBuy("US", s_Base));
   s_Target = ACR_Manager.Ground(alpha.m_Home + Vector(-200, 0, 700));
   s_iRP = manager.m_Bases.GetRP("US");
   string refused = manager.m_Supports.Call("US", h.SupportIndex(manager, ACR_ESupportType.HELI_DOOR_GUNS), s_Target, true);
   PrintFormat("[ACR] HELIPADTEST door gunners without one: %1", refused);
   ACR_TestRegress.Check("helipad: no door-gunner helicopter = refused, nothing spent", refused.Contains("buy one") && manager.m_Bases.GetRP("US") == s_iRP, refused);
  }
  if (!s_Heli) return;
  if (tick == 10)
  {
   PrintFormat("[ACR] HELIPADTEST parked: %1 m above the pad, status %2", s_Heli.m_Vehicle.GetOrigin()[1] - s_Pad.Spot()[1], station.StatusFor("US", true));
   string called = manager.m_Supports.Call("US", h.SupportIndex(manager, ACR_ESupportType.HELI_GUN_RUN), s_Target, true);
   PrintFormat("[ACR] HELIPADTEST rocket run: %1", called);
   s_iCalledAt = tick;
   ACR_TestRegress.Check("helipad: the call flies the stationed gunship", s_Heli.m_eState == ACR_EHeliState.FLYING && manager.m_Bases.GetRP("US") < s_iRP, called);
  }
  if (s_iCalledAt < 0) return;
  if (s_Heli.m_Vehicle)
  {
   float up = s_Heli.m_Vehicle.GetOrigin()[1] - GetGame().GetWorld().GetSurfaceY(s_Heli.m_Vehicle.GetOrigin()[0], s_Heli.m_Vehicle.GetOrigin()[2]);
   s_fTopHeight = Math.Max(s_fTopHeight, up);
   if (tick % 5 == 0 || (s_iLandedAt > 0 && s_iReadyAt < 0)) PrintFormat("[ACR] HELIPADTEST t=%1 state %2, %3 m up, %4 m from the pad, crew aboard %5, registered %6", tick, s_Heli.m_eState, (int)up, (int)vector.DistanceXZ(s_Heli.m_Vehicle.GetOrigin(), s_Pad.GetOrigin()), Aboard(s_Heli.m_Vehicle), manager.m_Supports.m_Vehicles.Contains(s_Heli.m_Vehicle));
  }
  if (s_iLandedAt < 0 && s_Heli.m_eState == ACR_EHeliState.SERVICING)
  {
   s_iLandedAt = tick;
   PrintFormat("[ACR] HELIPADTEST landed %1 s after the call", tick - s_iCalledAt);
   ACR_TestRegress.Check("helipad: it flew out and came back", s_fTopHeight > 40, string.Format("top %1 m", s_fTopHeight));
  }
  if (s_iLandedAt > 0 && tick == s_iLandedAt + 5)
  {
   float off = vector.DistanceXZ(s_Heli.m_Vehicle.GetOrigin(), s_Pad.GetOrigin());
   float height = s_Heli.m_Vehicle.GetOrigin()[1] - s_Pad.Spot()[1];
   ACR_TestRegress.Check("helipad: landed on its pad and stays there", off < 3 && Math.AbsFloat(height) < 1.5, string.Format("%1 m off, %2 m up", off, height));
   string busy = manager.m_Supports.Call("US", h.SupportIndex(manager, ACR_ESupportType.HELI_LOITER), s_Target, true);
   ACR_TestRegress.Check("helipad: no call while it is being serviced", busy.Contains("serviced"), busy);
  }
  if (s_iReadyAt < 0 && s_iLandedAt > 0 && s_Heli.m_eState == ACR_EHeliState.READY)
  {
   s_iReadyAt = tick;
   ACR_TestRegress.Check("helipad: ready again after servicing", tick - s_iLandedAt >= ACR_HeliStation.SERVICE_SECONDS - 1, string.Format("%1 s", tick - s_iLandedAt));
  }
  if (s_iReadyAt > 0 && tick == s_iReadyAt + 4)
  {
   int ourSquads;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.MarkVehicle() == s_Heli.m_Vehicle) ourSquads++;
   ACR_TestRegress.Check("helipad: the crew stays aboard and is never a squad", Aboard(s_Heli.m_Vehicle) >= 2 && ourSquads == 0, string.Format("aboard %1, squads %2", Aboard(s_Heli.m_Vehicle), ourSquads));
   // 4 Oct live: a helicopter shot up on its pad; its crew got out, was taken for a squad (the AI
   // commander ordered it about) and the repaired helicopter came back short of crew.
   // One crewman gets out, another dies.
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(s_Heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> slots = {};
   if (compartments) compartments.GetCompartments(slots);
   IEntity victim;
   foreach (BaseCompartmentSlot slot : slots)
   {
    IEntity man = slot.GetOccupant();
    if (!man) continue;
    if (!s_Ejected) { s_Ejected = man; continue; }
    if (!victim) victim = man;
   }
   ChimeraCharacter ejected = ChimeraCharacter.Cast(s_Ejected);
   if (ejected && ejected.GetCompartmentAccessComponent()) ejected.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   if (victim) h.Kill(victim);
   PrintFormat("[ACR] HELIPADTEST crewman out: %1, crewman killed: %2 (crew seats %3)", s_Ejected != null, victim != null, CrewSeats(s_Heli.m_Vehicle));
  }
  if (s_iReadyAt > 0 && tick == s_iReadyAt + 9)
  {
   bool gotOut = s_Ejected && !ACR_HeliStation.Aboard(s_Ejected, s_Heli.m_Vehicle);
   bool isSquad = s_Ejected && manager.FindSquadBySoldier(s_Ejected) != null;
   ACR_TestRegress.Check("helipad: a crewman who got out is never a squad", gotOut && !isSquad, string.Format("out %1, squad %2", gotOut, isSquad));
   // Shot up on the pad -> servicing (the real path: Tick services it when the time is up).
   s_Heli.m_eState = ACR_EHeliState.SERVICING;
   s_Heli.m_fReadyAt = GetGame().GetWorld().GetWorldTime() + 2000;
   s_iRecrewAt = tick;
  }
  if (s_iRecrewAt > 0 && tick == s_iRecrewAt + 5)
  {
   int seats = CrewSeats(s_Heli.m_Vehicle);
   int crew = CrewAboard(s_Heli.m_Vehicle);
   ACR_TestRegress.Check("helipad: serviced on the pad = fully crewed again", s_Heli.m_eState == ACR_EHeliState.READY && seats > 0 && crew == seats && (!s_Ejected || ACR_HeliStation.Aboard(s_Ejected, s_Heli.m_Vehicle)), string.Format("state %1, crew %2 of %3, straggler left %4", s_Heli.m_eState, crew, seats, s_Ejected != null));
   int crewSquads;
   foreach (ACR_Squad any : manager.m_Squads)
   {
    array<AIAgent> agents = {};
    any.GetAgents(agents);
    foreach (AIAgent agent : agents) if (agent.GetControlledEntity() && ACR_HeliStation.Aboard(agent.GetControlledEntity(), s_Heli.m_Vehicle)) crewSquads++;
   }
   ACR_TestRegress.Check("helipad: the new crew is never a squad", crewSquads == 0, string.Format("%1", crewSquads));
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(s_Heli.m_Vehicle.FindComponent(SCR_DamageManagerComponent));
   if (damage) damage.Kill(Instigator.CreateInstigator(null));
  }
  if (s_iRecrewAt > 0 && tick == s_iRecrewAt + 8)
  {
   ACR_TestRegress.Check("helipad: destroyed = gone, buy a new one", station.CountOf("US", true) == 0 && station.StatusFor("US", true).Contains("buy"), station.StatusFor("US", true));
   ACR_TestRegress.Complete("helipad");
  }
  if (tick == s_iCalledAt + 210)
  {
   PrintFormat("[ACR] HELIPADTEST timed out (landed at %1, ready at %2)", s_iLandedAt, s_iReadyAt);
   ACR_TestRegress.Abort("helipad", "timed out");
  }
 }
}
