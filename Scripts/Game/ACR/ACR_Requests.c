// Player requests to the server: possession, orders, rides, landing zones, unloading
// (split out of ACR_Manager, 2 Oct; the manager forwards its old entry points).
class ACR_Requests : Managed
{
 protected ACR_Manager m_Manager;

 void ACR_Requests(ACR_Manager manager) { m_Manager = manager; }

 string HandleRequest(SCR_PlayerController pc, int command, RplId targetId, vector point)
 {
  if (!Replication.IsServer() || !pc) return "Server unavailable";
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  switch (command)
  {
   case ACR_ERequest.RETURN: return ReturnToCommandUnit(pc, body);
   case ACR_ERequest.POSSESS: return Possess(pc, body, targetId);
   case ACR_ERequest.SWITCH_TO_GROUP: return SwitchToGroup(pc, body, targetId);
   case ACR_ERequest.TAKE_GUNNER: return TakeGunner(pc, body, targetId);
   case ACR_ERequest.TAKE_COMMAND: return m_Manager.TakeCommand(pc);
   case ACR_ERequest.REINFORCE: return m_Manager.m_Requisition.Reinforce(pc, targetId);
   case ACR_ERequest.LEAVE_COMMAND: return m_Manager.LeaveCommand(pc);
   case ACR_ERequest.UNLOAD_HERE: return UnloadHere(pc, body);
  }
  return Order(pc, command, targetId, point);
 }

 string ReturnToCommandUnit(SCR_PlayerController pc, ChimeraCharacter body)
 {
  if (!pc.IsPossessing()) return "You are already in your command unit";
  if (m_Manager.m_Respawn)
  {
   string waiting = m_Manager.m_Respawn.OnReturnRequested(pc);
   if (!waiting.IsEmpty()) return waiting;
  }
  ACR_MapHandoff.Release(pc, body);
  m_Manager.ReleaseAuthority(pc);
  pc.SetPossessedEntity(null);
  ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
  PrintFormat("[ACR] RETURN player=%1", pc.GetPlayerId());
  return "Returned to your command unit";
 }

 string Possess(SCR_PlayerController pc, ChimeraCharacter body, RplId targetId)
 {
  ChimeraCharacter target = ChimeraCharacter.Cast(SCR_EntityHelper.RplIdToEntity(targetId));
  if (!target) return "Aim directly at a friendly soldier, or select its map marker";
  if (target == body) return "You already control this soldier";
  ACR_Squad squad = m_Manager.FindSquadBySoldier(target);
  if (!squad) return "That soldier is not in a commandable squad";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  if (target.GetCharacterController().IsDead()) return "That soldier is dead";
  if (ACR_Squad.FindController(target)) return "Another player controls that soldier";
  SCR_PossessingManagerComponent possessing = SCR_PossessingManagerComponent.GetInstance();
  if (possessing && possessing.GetIdFromMainEntity(target) != 0) return "That soldier is a player's command unit";

  // The engine supports chained possession and preserves the command unit.
  ACR_MapHandoff.Release(pc, body);
  m_Manager.ReleaseAuthority(pc);
  pc.SetPossessedEntity(target);
  squad.Refresh();
  ACR_MapHandoff.ClearCharacterInput(target);
  m_Manager.m_LastSquad.Set(pc.GetPlayerId(), squad);
  PrintFormat("[ACR] POSSESS player=%1 squad=%2 index=%3", pc.GetPlayerId(), squad.GetName(), squad.IndexOf(target));
  if (pc.GetControlledEntity() != target) return "Control transfer failed; try again";
  if (squad.IsLeader(pc))
   return string.Format("Now leading %1. Native squad commands are available. F7 swaps again; F8 returns.", squad.GetName());
  return string.Format("Now controlling %1-%2. F7 swaps again; F8 returns to your command unit.", squad.GetName(), squad.NumberOf(target));
 }

 // Arma Commander's Switch button: the leader if free, otherwise any free soldier.
 // Straight into the turret of the squad's armed vehicle. Plain possession: the
 // squad keeps its leader and order; drive from the turret (W/A/S/D) or give
 // the squad map orders as usual.
 string TakeGunner(SCR_PlayerController pc, ChimeraCharacter body, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) return "Select a friendly squad first";
  ChimeraCharacter gunner = ACR_GunnerDrive.SquadGunner(squad);
  if (!gunner) return "That squad has no manned turret";
  if (gunner == body) return "You are already the gunner";
  return Possess(pc, body, SCR_EntityHelper.EntityToRplId(gunner));
 }

 string SwitchToGroup(SCR_PlayerController pc, ChimeraCharacter body, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) return "Select a friendly squad first";
  array<IEntity> candidates = {};
  IEntity leader = squad.GetLeader();
  if (leader) candidates.Insert(leader);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() && !candidates.Contains(agent.GetControlledEntity())) candidates.Insert(agent.GetControlledEntity());
  foreach (IEntity candidate : candidates)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(candidate);
   if (!soldier || soldier == body || soldier.GetCharacterController().IsDead() || ACR_Squad.FindController(soldier)) continue;
   return Possess(pc, body, SCR_EntityHelper.EntityToRplId(soldier));
  }
  return string.Format("%1 has no free soldier to take over", squad.GetName());
 }

 // A player's order, credited to him (shared command: who ordered what).
 string Order(SCR_PlayerController pc, int command, RplId groupId, vector point)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) squad = m_Manager.GetSelectedSquad(pc);
  int serial = -1;
  int previous;
  float previousAt;
  if (squad)
  {
   serial = squad.m_iOrderSerial;
   previous = squad.m_iOrderedBy;
   previousAt = squad.m_fOrderedAt;
  }
  string result = GiveOrder(pc, command, groupId, point);
  if (!squad || !m_Manager.m_Squads.Contains(squad) || !squad.GetFaction()) return result;
  bool queued = (command == ACR_ERequest.QUEUE_MOVE || command == ACR_ERequest.QUEUE_DEFEND || command == ACR_ERequest.QUEUE_GARRISON) && !result.Contains("full");
  bool ride = command == ACR_ERequest.ORDER_RIDE && result.Contains("on its way");
  if (squad.m_iOrderSerial == serial && !queued && !ride) return result;
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return result;
  int id = pc.GetPlayerId();
  float now = GetGame().GetWorld().GetWorldTime();
  squad.m_iOrderedBy = id;
  squad.m_fOrderedAt = now;
  string key = squad.GetFaction().GetFactionKey();
  string what = OrderWords(command);
  string place = "";
  ACR_Intel intel = m_Manager.IntelFor(key);
  if (intel && command != ACR_ERequest.ORDER_HALT && command != ACR_ERequest.ORDER_GET_OUT) place = " " + intel.Place(point);
  m_Manager.Command().Note(id, string.Format("%1 %2%3", squad.GetName(), what, place));
  // Someone else's fresh order replaced: tell the side (the client asked first).
  if (previous != 0 && previous != id && now - previousAt < ACR_Command.RECENT_MS)
   m_Manager.Command().Feed(key, string.Format("%1 re-tasked %2 (was %3's order)", m_Manager.Command().TagOf(id), squad.GetName(), m_Manager.Command().TagOf(previous)));
  return result;
 }

 static string OrderWords(int command)
 {
  switch (command)
  {
   case ACR_ERequest.ORDER_DEFEND: return "defending";
   case ACR_ERequest.QUEUE_DEFEND: return "then defending";
   case ACR_ERequest.ORDER_GARRISON: return "garrisoning";
   case ACR_ERequest.QUEUE_GARRISON: return "then garrisoning";
   case ACR_ERequest.QUEUE_MOVE: return "then moving";
   case ACR_ERequest.ORDER_GET_IN: return "getting in";
   case ACR_ERequest.ORDER_GET_OUT: return "getting out";
   case ACR_ERequest.ORDER_RIDE: return "riding";
   case ACR_ERequest.ORDER_HUNT: return "hunting";
   case ACR_ERequest.ORDER_SET_LZ: return "unloading";
   case ACR_ERequest.ORDER_HALT: return "halted";
  }
  return "moving";
 }

 string GiveOrder(SCR_PlayerController pc, int command, RplId groupId, vector point)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) squad = m_Manager.GetSelectedSquad(pc);
  if (!squad) return "No squad selected";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return "Only the commander can give map orders. Take command first.";
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return "That squad is not ready";
  bool usesPoint = command == ACR_ERequest.MOVE_TO || command == ACR_ERequest.ORDER_DEFEND || command == ACR_ERequest.ORDER_GET_IN || command == ACR_ERequest.ORDER_SET_LZ || command == ACR_ERequest.ORDER_RIDE || command == ACR_ERequest.QUEUE_MOVE || command == ACR_ERequest.QUEUE_DEFEND || command == ACR_ERequest.ORDER_HUNT || command == ACR_ERequest.ORDER_GARRISON || command == ACR_ERequest.QUEUE_GARRISON;
  // Reject nonsense coordinates from a client.
  if (usesPoint && (vector.Distance(point, anchor.GetOrigin()) > 20000 || !ACR_Manager.OnMap(point))) return "Order is too far away";
  // Map clicks made inside Game Master / build mode are not orders (the client checks too).
  if (usesPoint && ACR_Manager.EditorOpenFor(pc.GetPlayerId())) return "Close the editor to give map orders";
  if (usesPoint) point = ACR_Manager.Ground(point);
  m_Manager.m_LastSquad.Set(pc.GetPlayerId(), squad);
  string name = squad.GetName();
  // Helicopter crews only hold or get out (AI can't fly; any move makes them get out and walk).
  bool crewSafe = command == ACR_ERequest.ORDER_FIRE_AT_WILL || command == ACR_ERequest.ORDER_HALT || command == ACR_ERequest.ORDER_GET_OUT || command == ACR_ERequest.ORDER_UNLOAD || command == ACR_ERequest.MODE_COMBAT || command == ACR_ERequest.MODE_FAST || command == ACR_ERequest.MODE_SILENT;
  if (!crewSafe && squad.Aircraft()) return name + ": AI can't fly helicopters. Take the pilot's seat to fly it, or order Get out";
  switch (command)
  {
   case ACR_ERequest.ADVANCE:
    if (squad.MoveTo(ACR_Manager.Ground(squad.m_Home + Vector(35, 0, 0)))) return name + ": moving";
    break;
   case ACR_ERequest.RALLY:
    if (squad.MoveTo(ACR_Manager.Ground(squad.m_Home))) return name + ": rallying";
    break;
   case ACR_ERequest.MOVE_TO:
    if (squad.MoveTo(point))
    {
     PrintFormat("[ACR] MOVE player=%1 squad=%2 target=%3", pc.GetPlayerId(), name, point);
     return name + ": moving";
    }
    break;
   case ACR_ERequest.ORDER_DEFEND:
    if (squad.Defend(point)) return name + ": defending";
    break;
   case ACR_ERequest.ORDER_GARRISON:
    if (squad.Garrison(point)) return name + ": garrisoning";
    if (ACR_Transport.CrewedVehicle(squad)) return name + ": vehicle crews can't garrison, defending there";
    return name + ": no building with room there, defending";
   case ACR_ERequest.ORDER_GET_IN:
   {
    bool boarding = squad.GetIn(point);
    PrintFormat("[ACR] GET IN player=%1 squad=%2 target=%3 -> %4", pc.GetPlayerId(), name, point, boarding);
    if (boarding) return name + ": getting in";
    return name + ": no vehicle found";
   }
   case ACR_ERequest.ORDER_GET_OUT:
   {
    bool leaving = squad.GetOut();
    PrintFormat("[ACR] GET OUT player=%1 squad=%2 -> %3", pc.GetPlayerId(), name, leaving);
    if (leaving) return name + ": getting out";
    return name + " is not in a vehicle";
   }
   case ACR_ERequest.ORDER_UNLOAD:
    return Unload(squad);
   case ACR_ERequest.ORDER_SET_LZ:
    return SetLZ(squad, point);
   case ACR_ERequest.ORDER_RIDE:
    return Ride(squad, point);
   case ACR_ERequest.ORDER_HUNT:
    return m_Manager.Hunt(squad, point);
   case ACR_ERequest.ORDER_HALT:
    squad.Halt("Halted");
    return name + ": halted";
   case ACR_ERequest.QUEUE_MOVE:
    if (squad.Queue(point, false)) return name + ": waypoint added";
    return name + ": waypoint list is full";
   case ACR_ERequest.QUEUE_DEFEND:
    if (squad.Queue(point, true)) return name + ": will defend there after its current order";
    return name + ": waypoint list is full";
   case ACR_ERequest.QUEUE_GARRISON:
    if (squad.Queue(point, false, true)) return name + ": will garrison there after its current order";
    return name + ": waypoint list is full";
   case ACR_ERequest.MODE_COMBAT:
    squad.SetMode(ACR_EMoveMode.COMBAT);
    return name + ": combat";
   case ACR_ERequest.MODE_FAST:
    squad.SetMode(ACR_EMoveMode.FAST);
    return name + ": fast";
   case ACR_ERequest.ORDER_FIRE_AT_WILL:
   {
    bool mortar = m_Manager.Mortars().TeamOf(squad) != null;
    if (!mortar && !ACR_FireMissions.IsObserver(squad)) return name + ": needs a radio operator or a mortar to fire at will";
    squad.m_bFireAtWill = !squad.m_bFireAtWill;
    PrintFormat("[ACR] FIRE player=%1 %2 fire at will %3", pc.GetPlayerId(), name, squad.m_bFireAtWill);
    if (!squad.m_bFireAtWill) return name + ": fire at will off";
    if (mortar) return name + ": fire at will on (fires a few rounds of its own at spotted enemies in reach, no RP)";
    return name + ": fire at will on (calls mortar fire on enemies it sees; usual RP and cooldown)";
   }
   case ACR_ERequest.MODE_SILENT:
    squad.SetMode(ACR_EMoveMode.SILENT);
    return name + ": silent";
   default:
    return "Unknown command";
  }
  return name + ": could not create that order";
 }

 // A free transport nearby (never fetched from far away) picks the squad up,
 // drops it at the point and it takes positions there (walking the rest if the ride fails).
 string Ride(ACR_Squad squad, vector point)
 {
  if (!m_Manager.m_Rides) m_Manager.m_Rides = new ACR_Rides(m_Manager);
  string why;
  if (!m_Manager.m_Rides.Request(squad, point, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why))
  {
   // Walking beats the road there: just go.
   if (why.Contains("walking is faster") && squad.MoveTo(point)) return squad.GetName() + ": " + why + ", on foot";
   return squad.GetName() + ": no ride, " + why;
  }
  ACR_Ride ride = m_Manager.m_Rides.RideOf(squad);
  string taxi = "a transport";
  if (ride) taxi = ride.m_Taxi.GetName();
  return string.Format("%1: %2 is on its way to pick them up", squad.GetName(), taxi);
 }

 // Queue an unload: the passengers get out by themselves when their vehicle
 // sets down near the point. Selecting the transport (its crew) applies it to
 // everyone riding in the back.
 string SetLZ(ACR_Squad squad, vector point)
 {
  array<ACR_Squad> targets = {};
  IEntity crewed = ACR_Transport.CrewedVehicle(squad);
  if (crewed) ACR_Transport.PassengerSquads(m_Manager, crewed, targets);
  if (targets.IsEmpty()) targets.Insert(squad);
  string names = "";
  foreach (ACR_Squad passenger : targets)
  {
   passenger.m_LZ = point;
   passenger.m_bHasLZ = true;
   passenger.m_fLZSetAt = GetGame().GetWorld().GetWorldTime();
   if (!names.IsEmpty()) names += ", ";
   names += passenger.GetName();
  }
  PrintFormat("[ACR] LZ set for %1 at %2", names, point);
  return string.Format("%1: will unload at the LZ (%2)", names, ACR_Manager.Grid(point));
 }

 // F6 while in a vehicle. Not settled: warn, and a second press within 3 s drops them anyway.
 protected ref map<int, float> m_UnloadConfirm = new map<int, float>();
 string UnloadHere(SCR_PlayerController pc, ChimeraCharacter body)
 {
  IEntity vehicle = ACR_Transport.VehicleOf(body);
  if (!vehicle) return "You are not in a vehicle";
  array<ACR_Squad> passengers = {};
  ACR_Transport.PassengerSquads(m_Manager, vehicle, passengers);
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  for (int i = passengers.Count() - 1; i >= 0; i--)
   if (passengers[i].GetFaction() != faction || passengers[i].HasPlayer()) passengers.Remove(i);
  if (passengers.IsEmpty()) return "No passenger squads aboard";
  float height, speed;
  bool settled = ACR_Transport.Settled(vehicle, height, speed);
  float now = GetGame().GetWorld().GetWorldTime();
  float asked;
  bool confirmed = m_UnloadConfirm.Find(pc.GetPlayerId(), asked) && now - asked < 3000;
  if (!settled && !confirmed)
  {
   m_UnloadConfirm.Set(pc.GetPlayerId(), now);
   return string.Format("%1 m up at %2 km/h. Press F6 again to drop them anyway.", (int)height, (int)(speed * 3.6));
  }
  m_UnloadConfirm.Remove(pc.GetPlayerId());
  int soldiers;
  foreach (ACR_Squad squad : passengers) soldiers += squad.DismountNow();
  PrintFormat("[ACR] UNLOAD HERE player=%1 squads=%2 soldiers=%3 height=%4 speed=%5", pc.GetPlayerId(), passengers.Count(), soldiers, height, speed);
  return string.Format("Unloading %1 squad(s)", passengers.Count());
 }

 // Queued unloads (server, several times a second).
 protected float m_fNextLZCheck;
 void CheckLandingZones()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextLZCheck) return;
  m_fNextLZCheck = now + 250;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.m_bHasLZ) continue;
   if (now - squad.m_fLZSetAt > 900000) { squad.m_bHasLZ = false; continue; }
   array<IEntity> vehicles = {};
   squad.GetVehicles(vehicles);
   if (vehicles.IsEmpty()) continue; // not aboard yet
   IEntity vehicle = vehicles[0];
   if (vector.DistanceXZ(vehicle.GetOrigin(), squad.m_LZ) > ACR_Transport.LZ_RADIUS) continue;
   float height, speed;
   if (!ACR_Transport.Settled(vehicle, height, speed)) continue;
   int soldiers = squad.DismountNow();
   PrintFormat("[ACR] LZ UNLOAD %1: %2 soldiers out (height %3, speed %4)", squad.GetName(), soldiers, height, speed);
   Faction faction = squad.GetFaction();
   if (faction) m_Manager.NotifyFaction(faction.GetFactionKey(), squad.GetName() + ": at the LZ, getting out");
  }
 }

 // Every other squad riding in this squad's vehicles dismounts.
 string Unload(ACR_Squad transport)
 {
  array<IEntity> vehicles = {};
  transport.GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return transport.GetName() + " is not in a vehicle";
  int unloaded;
  array<IEntity> passengerVehicles = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad == transport) continue;
   squad.GetVehicles(passengerVehicles);
   foreach (IEntity vehicle : passengerVehicles)
   {
    if (!vehicles.Contains(vehicle)) continue;
    if (squad.GetOut()) unloaded++;
    break;
   }
  }
  if (!unloaded) return transport.GetName() + " has no passengers";
  return string.Format("%1: unloading %2 squad(s)", transport.GetName(), unloaded);
 }

}
