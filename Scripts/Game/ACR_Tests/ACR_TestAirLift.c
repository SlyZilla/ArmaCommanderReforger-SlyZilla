// -acrAirLiftTest 1 (srv:airlift, test range, ~4.5 min): the air lift support (aca-f22).
// Refusals (no squad, too close), the price and the AI commander's half price, then Alpha is
// flown about 1 km: helicopters land by it, it boards, flies, lands, gets out and holds the
// landing zone; the helicopters leave and are deleted; Alpha stays a squad throughout.
// -acrAirLiftTwoTest 1 (srv:airlifttwo): the same with 2 seats per helicopter, so two fly.
class ACR_TestAirLift
{
 static const int CHECKS = 10;
 static vector s_Target;
 static float s_fMaxDamage;
 static int s_iStage = -1;
 static int s_iMaxAboard;
 static int s_iAliveAtTakeoff;
 static int s_iAliveAtCall;
 static bool s_bDropped;          // Alpha lost its squad status during the flight
 static int s_iEndTick = -1;
 static ref array<IEntity> s_Helis = {};
 static ref array<IEntity> s_Crew = {};

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  // Settings and groups can finish registering after the first harness tick.
  // Keep this a transport drill throughout, including newly spawned garrisons.
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  foreach (ACR_Squad peaceful : manager.m_Squads)
  {
   if (peaceful.m_Group && peaceful.m_Group.GetGroupUtilityComponent()) peaceful.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  }
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick > 5) ACR_TestRegress.Abort("airlift", "transport squad lost");
   return;
  }
  if (tick == 5) Call(h, manager, alpha);
  if (tick > 5 && s_iEndTick < 0) Watch(manager, alpha, tick);
  if (s_iEndTick >= 0 && tick == s_iEndTick) Finish(manager, alpha);
  if (tick == 420 && s_iEndTick < 0) ACR_TestRegress.Abort("airlift", string.Format("lift never finished (stage %1)", s_iStage));
 }

 static void Call(ACR_TestHarness h, ACR_Manager manager, ACR_Squad alpha)
 {
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  data.m_iRP = 500;
  int index = h.SupportIndex(manager, ACR_ESupportType.AIR_LIFT);
  if (index < 0) { ACR_TestRegress.Abort("airlift", "no air lift in the US support list"); return; }
  ACR_SupportEntry entry = data.m_aSupports[index];
  vector from = alpha.GetAnchor().GetOrigin();
  s_iAliveAtCall = ACR_AirLift.Alive(alpha);
  string noSquad = manager.m_Supports.Call("US", index, from + Vector(0, 0, 900), false, -1, null);
  string tooClose = manager.m_Supports.Call("US", index, from + Vector(120, 0, 0), false, -1, alpha);
  ACR_TestRegress.Check("airlift: refused without a squad and when too close", noSquad.Contains("Select the squad") && tooClose.Contains("walk it") && data.m_iRP == 500, noSquad + " / " + tooClose);
  // A drop-off about 1 km away with room to land.
  array<vector> spots = {};
  int wantedSpots = 1;
  if (ACR_LiftHeli.s_iTestSeats > 0) wantedSpots = 2;
  vector targetCenter;
  for (int i = 0; i < 256 && spots.IsEmpty(); i++)
  {
   int bearing = i % 64;
   int ring = i / 64;
   float angle = Math.PI2 * bearing / 64.0;
   float distance = 600 + ring * 300;
   vector candidate = from + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
   if (ACR_Manager.OnMap(candidate) && !BBB_Terrain.IsWater(candidate[0], candidate[2])) ACR_LandingZone.Find(candidate, 100, wantedSpots, spots);
   if (spots.Count() < wantedSpots) spots.Clear();
   else targetCenter = candidate;
  }
  if (spots.IsEmpty()) { ACR_TestRegress.Abort("airlift", "no open ground 1 km from Alpha"); return; }
  s_Target = spots[0];
  if (wantedSpots == 2) s_Target = targetCenter;
  string result = manager.m_Supports.Call("US", index, s_Target, false, -1, alpha);
  PrintFormat("[ACR] TEST airlift call: %1", result);
  ACR_AirLift called = ACR_AirLift.For(alpha);
  if (called) PrintFormat("[ACR] TEST airlift landing room: %1 pickup, %2 drop spots", called.m_Pickups.Count(), called.m_Drops.Count());
  array<ACR_HeliRun> traffic = {};
  manager.m_Supports.Helicopters(traffic);
  int registered;
  if (called)
   foreach (ACR_LiftHeli carrier : called.m_Helis)
    if (traffic.Contains(carrier)) registered++;
  ACR_TestRegress.Check("airlift: every carrier participates in helicopter avoidance", called != null && registered == called.m_Helis.Count() && registered > 0, string.Format("%1 carriers registered", registered));
  if (called && System.IsCLIParam("acrAirLiftContactTest"))
  {
   foreach (ACR_LiftHeli quick : called.m_Helis)
   {
    quick.m_Pos = quick.m_Spot;
    quick.m_bDown = true;
   }
   ACR_AirLift.NEAR_BOARD_MS = 0;
   ACR_AirLift.FORCE_BOARD_MS = 0;
  }
  int spent = 500 - data.m_iRP;
  ACR_TestRegress.Check("airlift: called, full price for a player, half for the AI commander", ACR_AirLift.Busy(alpha) && spent == entry.m_iCost && entry.CostFor(true) == entry.m_iCost / 2 && entry.m_iCost > 0, string.Format("%1; spent %2 of %3, AI %4", result, spent, entry.m_iCost, entry.CostFor(true)));
 }

 static void Watch(ACR_Manager manager, ACR_Squad alpha, int tick)
 {
  ACR_AirLift lift = ACR_AirLift.For(alpha);
  if (!lift)
  {
   if (s_iStage >= ACR_AirLift.INBOUND) s_iEndTick = tick + 2;
   return;
  }
  if (lift.m_iStage != s_iStage)
  {
   PrintFormat("[ACR] TEST airlift stage %1 -> %2 at tick %3", s_iStage, lift.m_iStage, tick);
   if (lift.m_iStage == ACR_AirLift.TRANSIT) s_iAliveAtTakeoff = ACR_AirLift.Alive(alpha);
   s_iStage = lift.m_iStage;
  }
  foreach (ACR_LiftHeli heli : lift.m_Helis)
  {
   if (heli.m_Vehicle)
   {
    float damage = heli.MovementDamage();
    if (damage > s_fMaxDamage) PrintFormat("[ACR] AIRLIFTTEST damage %1 at %2 (stage %3)", damage, heli.m_Pos, lift.m_iStage);
    s_fMaxDamage = Math.Max(s_fMaxDamage, damage);
   }
   if (!heli.m_Vehicle || s_Helis.Contains(heli.m_Vehicle)) continue;
   s_Helis.Insert(heli.m_Vehicle);
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> slots = {};
   if (compartments) compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots) if (slot.GetType() != ECompartmentType.CARGO && slot.GetOccupant()) s_Crew.Insert(slot.GetOccupant());
  }
  int aboard;
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (soldier && s_Helis.Contains(ACR_Transport.VehicleOf(soldier))) aboard++;
  }
  s_iMaxAboard = Math.Max(s_iMaxAboard, aboard);
  if (aboard > 0 && (!manager.m_Squads.Contains(alpha) || manager.m_Supports.IsSupportGroup(alpha.m_Group))) s_bDropped = true;
  if (lift.m_iStage == ACR_AirLift.LEAVING && s_iEndTick < 0 && tick % 10 == 0) PrintFormat("[ACR] TEST airlift leaving, %1 helicopters still there", s_Helis.Count());
 }

 static void Finish(ACR_Manager manager, ACR_Squad alpha)
 {
  int wanted = 1;
  if (ACR_LiftHeli.s_iTestSeats > 0) wanted = 2;
  ACR_TestRegress.Check("airlift: enough helicopters for the squad", s_Helis.Count() == wanted, string.Format("%1 helicopter(s), %2 wanted", s_Helis.Count(), wanted));
  ACR_TestRegress.Check("airlift: helicopters landed by the squad", s_iStage >= ACR_AirLift.BOARDING, string.Format("last stage %1", s_iStage));
  ACR_TestRegress.Check("airlift: the whole squad boarded and took off", s_iMaxAboard >= s_iAliveAtCall && s_iAliveAtTakeoff == s_iAliveAtCall && s_iAliveAtCall > 0, string.Format("%1 aboard of %2 called, %3 alive at takeoff", s_iMaxAboard, s_iAliveAtCall, s_iAliveAtTakeoff));
  ACR_TestRegress.Check("airlift: still a squad in the air (not taken for helicopter crew)", !s_bDropped && manager.m_Squads.Contains(alpha), "");
  int near, outside;
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier)) continue;
   if (ACR_Transport.VehicleOf(soldier)) continue;
   outside++;
   if (vector.DistanceXZ(soldier.GetOrigin(), s_Target) < 120) near++;
  }
  ACR_TestRegress.Check("airlift: landed at the drop-off, everyone alive and out, holding it", near == outside && outside == s_iAliveAtCall && outside > 0 && alpha.OrderState() == ACR_EOrderState.DEFEND, string.Format("%1 of %2 within 120 m, %3 called, order %4", near, outside, s_iAliveAtCall, alpha.m_Order));
  int left;
  foreach (IEntity heli : s_Helis) if (heli) left++;
  foreach (IEntity crewman : s_Crew) if (crewman) left++;
  ACR_TestRegress.Check("airlift: helicopters and crews gone afterwards", left == 0 && s_Helis.Count() > 0, string.Format("%1 of %2 helicopters/crew left", left, s_Helis.Count() + s_Crew.Count()));
  ACR_TestRegress.Check("airlift: rotors and drivetrain undamaged throughout", s_fMaxDamage < 0.01, s_fMaxDamage.ToString());
  int expected = CHECKS;
  if (System.IsCLIParam("acrAirLiftTwoTest")) expected += 3;
  ACR_TestRegress.Complete("airlift", expected);
 }
}

// Reproduce a first search that found only one real landing spot at each end.
// The production fallback must find both missing spots and then fly the squad.
modded class ACR_AirLift
{
 override bool Begin(ACR_Supports supports, float now)
 {
  if (!System.IsCLIParam("acrAirLiftTwoTest")) return super.Begin(supports, now);
  while (m_Pickups.Count() > 1) m_Pickups.RemoveOrdered(1);
  while (m_Drops.Count() > 1) m_Drops.RemoveOrdered(1);
  ACR_TestRegress.Check("airlift: fallback starts with one spot at both ends", m_Pickups.Count() == 1 && m_Drops.Count() == 1, "native first spots retained");
  array<vector> confined = {m_Pickups[0]};
  bool added = ACR_LandingZone.Another(m_Pickups[0], 0, confined);
  ACR_TestRegress.Check("airlift: fallback refuses overlapping spots", !added && confined.Count() == 1, "zero-radius search cannot fit another rotor disk");
  bool begun = super.Begin(supports, now);
  bool safe = begun && m_Pickups.Count() == 2 && m_Drops.Count() == 2 && m_Helis.Count() == 2;
  if (safe)
  {
   float pickupGap = vector.DistanceXZ(m_Pickups[0], m_Pickups[1]);
   float dropGap = vector.DistanceXZ(m_Drops[0], m_Drops[1]);
   safe = pickupGap >= ACR_LandingZone.SPACING && pickupGap <= SECOND_SPOT_SEARCH && dropGap >= ACR_LandingZone.SPACING && dropGap <= SECOND_SPOT_SEARCH;
   safe = safe && ACR_LandingZone.Clear(m_Pickups[1]) && ACR_LandingZone.Clear(m_Drops[1]);
  }
  ACR_TestRegress.Check("airlift: fallback supplies two safe spaced landing pairs", safe, string.Format("pickups=%1 drops=%2 carriers=%3", m_Pickups.Count(), m_Drops.Count(), m_Helis.Count()));
  return begun;
 }
}
