// Field engineering orders (server), modelled on ACA's Establish FOB order and FOB upgrades:
// the squad is paid for up front (truck supplies or RP), travels to the site (a ride when
// far, its own vehicle when it has one), works there while any member is within the work
// circle (paused while contested, engineers twice as fast, given up after 3 minutes without
// progress), then the vanilla composition or the mines appear. A new order to the squad
// cancels the job and refunds it, like a cancelled FOB build.
enum ACAFE_EPhase
{
 TRAVEL,
 WORK
}

class ACAFE_Job : Managed
{
 int m_iId;
 int m_eOrder;                 // ACAFE_EOrder
 ACR_Squad m_Squad;
 string m_sFaction;
 int m_iPlayerId;              // who ordered it (0: the AI commander)
 bool m_bAI;
 int m_ePhase;                 // ACAFE_EPhase
 vector m_vSite;               // structure / minefield centre / sweep centre
 vector m_vForward;            // road heading (structures, fields on a road) or the approach
 float m_fYaw;
 vector m_vStage;              // where the squad waits and works from
 float m_fFootprint;
 float m_fRoadWidth;
 float m_fWorkRadius;
 float m_fDuration;
 float m_fProgress;
 float m_fStartedAt;
 float m_fTravelBudgetMs;
 float m_fLastTick;
 float m_fLastProgress;
 int m_iSerial;                // the squad's order serial while the job owns it
 bool m_bResumeCheck;          // came out of a ride or a dismount: check it still heads for us
 string m_sStatus;
 int m_iPaidRP;
 IEntity m_PaidTruck;
 float m_fPaidLoads;
 ref array<ResourceName> m_aPrefabs = {};
 ref array<vector> m_aOffsets = {};
 ref array<vector> m_aPlan = {};
 ref array<bool> m_aPlanAT = {};
 int m_iLaid;
 int m_iKitAP;                 // kit mines: planned mines still to come out of the soldiers' kit
 int m_iKitAT;
 int m_iTruckMineSupplies;     // kit mines: supplies the truck pays for the rest
 ACAFE_Minefield m_Field;
 int m_iCleared;
 float m_fClearCredit;

 string SquadName()
 {
  if (m_Squad) return m_Squad.GetName();
  return "A squad";
 }

 string PaidText()
 {
  string text = "";
  if (m_iKitAP + m_iKitAT > 0) text = string.Format("%1 mines from kit", m_iKitAP + m_iKitAT);
  string paid = "";
  if (m_fPaidLoads > 0) paid = string.Format("%1 supplies", (int)Math.Round(m_fPaidLoads * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION));
  else if (m_iPaidRP > 0) paid = string.Format("-%1 RP", m_iPaidRP);
  if (!text.IsEmpty() && !paid.IsEmpty()) return text + ", " + paid;
  if (!text.IsEmpty()) return text;
  if (!paid.IsEmpty()) return paid;
  return "free";
 }
}

// Something we built: the pieces of one roadblock or checkpoint.
class ACAFE_Structure : Managed
{
 int m_eOrder;
 string m_sFaction;
 bool m_bAI;                   // built for the AI commander
 vector m_vPos;
 float m_fRadius;
 ref array<IEntity> m_aPieces = {};

 bool Standing()
 {
  foreach (IEntity piece : m_aPieces)
  {
   if (piece) return true;
  }
  return false;
 }
}

class ACAFE_Engineering : Managed
{
 protected static ref ACAFE_Engineering s_Instance;
 protected ACR_Manager m_Manager;
 ref array<ref ACAFE_Job> m_aJobs = {};
 ref array<ref ACAFE_Structure> m_aStructures = {};
 protected int m_iNextJob;
 protected int m_iTicks;

 // One per ACA session: a new mission (a new ACR_Manager) starts clean.
 static ACAFE_Engineering Get()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!s_Instance || s_Instance.m_Manager != manager)
  {
   s_Instance = new ACAFE_Engineering();
   s_Instance.m_Manager = manager;
   ACAFE_Mines.Reset();
   if (manager) Print("[ACAFE] Field engineering " + ACAFE_Config.VERSION + " ready: " + ACAFE_Settings.Describe());
  }
  return s_Instance;
 }

 // ---------------------------------------------------------------- player orders

 // A commander's order from the map (ACAFE_OrderServer). Returns the message for him.
 static string Order(SCR_PlayerController pc, int order, RplId groupId, vector point)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (!ACR_Setup.Started()) return "Wait for the match to start";
  if (order <= ACAFE_EOrder.NONE || order >= ACAFE_Orders.COUNT) return "Unknown engineering order";
  int playerId = pc.GetPlayerId();
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  if (!manager.IsCommander(playerId)) return "Only the commander can give engineering orders. Take command first.";
  if (ACR_Manager.EditorOpenFor(playerId)) return "Close the editor to give map orders";
  ACR_Squad squad = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad || !manager.m_Squads.Contains(squad)) return "That squad is gone (select it again)";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(playerId)) return "That squad belongs to another faction";
  if (!ACR_Manager.OnMap(point)) return "That point is off the map";
  string refusal;
  ACAFE_Engineering engineering = Get();
  ACAFE_Job job = engineering.Start(squad, order, point, playerId, false, refusal);
  if (!job)
  {
   PrintFormat("[ACAFE] ORDER refused player=%1 squad=%2 %3: %4", playerId, squad.GetName(), ACAFE_Orders.Name(order), refusal);
   return squad.GetName() + ": " + refusal;
  }
  squad.m_iOrderedBy = playerId;
  squad.m_fOrderedAt = GetGame().GetWorld().GetWorldTime();
  manager.m_LastSquad.Set(playerId, squad);
  string place = engineering.PlaceOf(job);
  manager.Command().Feed(job.m_sFaction, string.Format("%1: %2 %3%4 (%5)", manager.Command().TagOf(playerId), squad.GetName(), Doing(order), place, job.PaidText()));
  manager.Command().Note(playerId, string.Format("%1 %2%3", squad.GetName(), Doing(order), place));
  return string.Format("%1: %2 (%3, %4 s of work)", squad.GetName(), Doing(order), job.PaidText(), (int)job.m_fDuration);
 }

 // "building a medium checkpoint"
 static string Doing(int order)
 {
  if (ACAFE_Orders.IsStructure(order)) return "building " + ACAFE_Orders.AName(order);
  if (ACAFE_Orders.IsMinefield(order)) return "laying " + ACAFE_Orders.AName(order);
  return "clearing mines";
 }

 // The AI commander's own orders (ACAFE_AI): paid from its RP, no player to tell.
 ACAFE_Job StartAI(ACR_Squad squad, int order, vector point, out string refusal)
 {
  ACAFE_Job job = Start(squad, order, point, 0, true, refusal);
  if (!job) return null;
  if (m_Manager) m_Manager.Command().Feed(job.m_sFaction, string.Format("AI: %1 %2%3 (%4)", squad.GetName(), Doing(order), PlaceOf(job), job.PaidText()));
  return job;
 }

 ACAFE_Job Start(ACR_Squad squad, int order, vector point, int playerId, bool ai, out string refusal)
 {
  refusal = "";
  if (!ACAFE_Settings.Enabled())
  {
   refusal = "field engineering is off in this match's settings";
   return null;
  }
  if (!squad || squad.CountAlive() <= 0)
  {
   refusal = "the squad is eliminated";
   return null;
  }
  if (!squad.GetFaction())
  {
   refusal = "the squad has no side";
   return null;
  }
  if (squad.Aircraft())
  {
   refusal = "AI can't fly helicopters: order Get out first";
   return null;
  }
  if (ACR_AirLift.Busy(squad))
  {
   refusal = "it is on an air lift: orders again once it has landed";
   return null;
  }
  if (squad.m_bMortar)
  {
   refusal = "mortar teams stay on their tubes";
   return null;
  }
  if (squad.OrderState() == ACR_EOrderState.BUILD_FOB)
  {
   refusal = "it is establishing a FOB";
   return null;
  }
  if (ACAFE_Settings.EngineersOnly() && !HasEngineers(squad))
  {
   refusal = "only engineers or sappers, or a squad with a construction truck within 50 m, can do that in this match";
   return null;
  }
  IEntity anchor = squad.GetAnchor();
  if (!anchor)
  {
   refusal = "the squad is not ready";
   return null;
  }
  vector from = anchor.GetOrigin();
  if (vector.DistanceXZ(from, point) > ACAFE_Config.MAX_ORDER_DISTANCE)
  {
   refusal = string.Format("that is too far away (%1 km at most)", (int)(ACAFE_Config.MAX_ORDER_DISTANCE / 1000));
   return null;
  }
  ACAFE_Job job = new ACAFE_Job();
  job.m_eOrder = order;
  job.m_Squad = squad;
  job.m_sFaction = squad.GetFaction().GetFactionKey();
  job.m_iPlayerId = playerId;
  job.m_bAI = ai;
  job.m_fDuration = ACAFE_Orders.Duration(order);
  job.m_fWorkRadius = ACAFE_Config.WORK_RADIUS;
  bool planned;
  if (ACAFE_Orders.IsStructure(order)) planned = PlanStructure(job, point, from, refusal);
  else if (ACAFE_Orders.IsMinefield(order)) planned = ACAFE_Mines.Plan(job, point, from, refusal);
  else planned = PlanClearing(job, point, from);
  if (!planned) return null;
  job.m_fDuration = job.m_fDuration * ACAFE_Settings.TimeScale();
  if (!Pay(job, refusal)) return null;
  // One engineering job per squad: a new one replaces the old (refunded).
  ACAFE_Job old = JobOf(squad);
  if (old)
  {
   End(old, "replaced by a new engineering order", true, false);
   m_aJobs.RemoveItem(old);
  }
  if (!SendSquad(job, true))
  {
   Refund(job);
   refusal = "could not move there";
   return null;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  job.m_fStartedAt = now;
  job.m_fLastTick = now;
  job.m_fLastProgress = now;
  job.m_fTravelBudgetMs = (vector.DistanceXZ(from, job.m_vStage) / ACAFE_Config.TRAVEL_SPEED + 180) * 1000;
  m_iNextJob++;
  job.m_iId = m_iNextJob;
  m_aJobs.Insert(job);
  if (ai) squad.m_iAITask = ACAFE_Orders.AI_TASK;
  PrintFormat("[ACAFE] JOB %1 %2 %3: %4 at %5 (stage %6, %7, %8 s)", job.m_iId, job.m_sFaction, squad.GetName(), ACAFE_Orders.Name(order), job.m_vSite, job.m_vStage, job.PaidText(), (int)job.m_fDuration);
  return job;
 }

 // ---------------------------------------------------------------- planning

 protected bool PlanStructure(ACAFE_Job job, vector click, vector from, out string refusal)
 {
  vector roadPoint;
  vector roadDir;
  float width;
  if (!ACAFE_Roads.Snap(click, ACAFE_Config.ROAD_SNAP_DISTANCE, roadPoint, roadDir, width))
  {
   refusal = string.Format("no road within %1 m of that point", (int)ACAFE_Config.ROAD_SNAP_DISTANCE);
   return false;
  }
  if (BBB_Terrain.IsWaterAt(roadPoint))
  {
   refusal = "that road is under water";
   return false;
  }
  // The compositions' +Z end (stop sign, gun) faces away from where the squad comes from.
  vector toSite = roadPoint - from;
  toSite[1] = 0;
  if (vector.Dot(toSite, roadDir) < 0) roadDir = roadDir * (-1.0);
  job.m_vSite = roadPoint;
  job.m_vForward = roadDir;
  job.m_fRoadWidth = width;
  job.m_fYaw = ACAFE_Roads.Yaw(roadDir) + ACAFE_Config.ROAD_YAW_OFFSET;
  job.m_fFootprint = ACAFE_Orders.Pieces(job.m_eOrder, job.m_sFaction, width, job.m_aPrefabs, job.m_aOffsets);
  string why;
  if (!SiteClear(job, why))
  {
   refusal = why;
   return false;
  }
  // The builders wait beside the road on their own side of it, short of the site.
  vector side = Vector(roadDir[2], 0, -roadDir[0]);
  if (vector.Dot(from - roadPoint, side) < 0) side = side * (-1.0);
  vector stage = roadPoint - roadDir * (job.m_fFootprint + 4) + side * (width * 0.5 + ACAFE_Config.STAGE_OFFSET);
  if (BBB_Terrain.IsWaterAt(stage)) stage = roadPoint - roadDir * (job.m_fFootprint + 8);
  job.m_vStage = ACR_Manager.Ground(stage);
  return true;
 }

 // Nothing of ours, and none of ACA's compositions, where the pieces go.
 bool SiteClear(ACAFE_Job job, out string why)
 {
  why = "";
  foreach (ACAFE_Structure built : m_aStructures)
  {
   if (!built.Standing()) continue;
   if (vector.DistanceXZ(built.m_vPos, job.m_vSite) < built.m_fRadius + job.m_fFootprint)
   {
    why = string.Format("a %1 already stands there", ACAFE_Orders.Name(built.m_eOrder));
    return false;
   }
  }
  foreach (ACAFE_Job other : m_aJobs)
  {
   if (other == job || other.m_Squad == job.m_Squad || !ACAFE_Orders.IsStructure(other.m_eOrder)) continue;
   if (vector.DistanceXZ(other.m_vSite, job.m_vSite) < other.m_fFootprint + job.m_fFootprint)
   {
    why = other.SquadName() + " is already building there";
    return false;
   }
  }
  IEntity composition = ACR_Structures.CompositionOverlapping(job.m_vSite, job.m_fFootprint);
  if (composition)
  {
   why = string.Format("a %1 is in the way", ACR_Structures.KindName(ACR_Structures.KindOf(composition)));
   return false;
  }
  return true;
 }

 protected bool PlanClearing(ACAFE_Job job, vector click, vector from)
 {
  click[1] = GetGame().GetWorld().GetSurfaceY(click[0], click[2]);
  job.m_vSite = click;
  vector approach = click - from;
  approach[1] = 0;
  if (approach.Length() < 1) approach = "0 0 1";
  approach.Normalize();
  job.m_vForward = approach;
  job.m_vStage = ACR_Manager.Ground(click - approach * (ACAFE_Config.CLEAR_RADIUS * 0.6));
  if (BBB_Terrain.IsWaterAt(job.m_vStage)) job.m_vStage = ACR_Manager.Ground(click);
  job.m_fWorkRadius = ACAFE_Config.CLEAR_RADIUS + 15;
  job.m_fDuration = ACAFE_Config.CLEAR_MIN_SECONDS;
  return true;
 }

 // ---------------------------------------------------------------- payment

 // A construction truck with the squad pays in supplies (like vanilla building); otherwise RP.
 // Match settings: costs scaled; "truck materials" makes structures truck-only; "kit mines"
 // pays minefields with the mines carried (planned in ACAFE_Mines.Plan) plus the truck's
 // supplies for the rest, never RP.
 protected bool Pay(ACAFE_Job job, out string refusal)
 {
  float scale = ACAFE_Settings.CostScale();
  int rp = (int)Math.Round(ACAFE_Orders.CostRP(job.m_eOrder) * scale);
  int supplies = (int)Math.Round(ACAFE_Orders.CostSupplies(job.m_eOrder) * scale);
  bool kit = ACAFE_Orders.IsMinefield(job.m_eOrder) && ACAFE_Settings.KitMines();
  bool truckOnly = kit || (ACAFE_Orders.IsStructure(job.m_eOrder) && ACAFE_Settings.TruckMaterials());
  if (kit)
  {
   rp = 0;
   supplies = job.m_iTruckMineSupplies;
  }
  if (supplies > 0 && (truckOnly || ACAFE_Config.TRUCK_SUPPLIES_FIRST))
  {
   IEntity truck = job.m_Squad.ConstructionTruck();
   if (truck)
   {
    if (!m_Manager.m_Logistics) m_Manager.m_Logistics = new ACR_Logistics(m_Manager);
    float loads = supplies / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
    if (m_Manager.m_Logistics.StockOf(truck) >= loads && m_Manager.m_Logistics.Use(truck, loads))
    {
     job.m_PaidTruck = truck;
     job.m_fPaidLoads = loads;
     return true;
    }
   }
   if (truckOnly)
   {
    refusal = string.Format("needs a construction truck with %1 supplies within 50 m of the squad", supplies);
    return false;
   }
  }
  if (truckOnly) return true;
  if (rp <= 0) return true;
  int have = m_Manager.m_Bases.GetRP(job.m_sFaction);
  if (!m_Manager.m_Bases.Spend(job.m_sFaction, rp))
  {
   refusal = string.Format("needs %1 RP (your side has %2)", rp, have);
   if (ACAFE_Config.TRUCK_SUPPLIES_FIRST && supplies > 0) refusal += string.Format(", or a construction truck with %1 supplies", supplies);
   return false;
  }
  job.m_iPaidRP = rp;
  return true;
 }

 // Money back for work that never happened (a partly laid minefield keeps what it cost).
 protected void Refund(ACAFE_Job job)
 {
  if (ACAFE_Orders.IsMinefield(job.m_eOrder) && job.m_iLaid > 0) return;
  if (job.m_iPaidRP > 0)
  {
   ACR_FactionData data = m_Manager.m_Bases.GetFaction(job.m_sFaction);
   data.m_iRP = (int)Math.Min(data.m_iRP + job.m_iPaidRP, ACR_BaseSystem.POINTS_CAP);
   PrintFormat("[ACAFE] JOB %1: refunded %2 RP", job.m_iId, job.m_iPaidRP);
  }
  if (job.m_fPaidLoads > 0 && job.m_PaidTruck && ACR_Logistics.Health(job.m_PaidTruck) > 0 && m_Manager.m_Logistics)
  {
   m_Manager.m_Logistics.AddStock(job.m_PaidTruck, job.m_fPaidLoads);
   PrintFormat("[ACAFE] JOB %1: refunded %2 loads to the construction truck", job.m_iId, job.m_fPaidLoads);
  }
  job.m_iPaidRP = 0;
  job.m_fPaidLoads = 0;
 }

 // ---------------------------------------------------------------- moving

 // On foot far away: a ride (ACA's Ride order); mounted crews drive (roads on the way);
 // otherwise walk round any water. They take positions at the stage on arrival.
 protected bool SendSquad(ACAFE_Job job, bool allowRide)
 {
  ACR_Squad squad = job.m_Squad;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  bool sent;
  if (allowRide && ACAFE_Config.USE_RIDES && !squad.IsMounted() && vector.DistanceXZ(anchor.GetOrigin(), job.m_vStage) >= ACAFE_Config.RIDE_MIN_DISTANCE)
  {
   if (!m_Manager.m_Rides) m_Manager.m_Rides = new ACR_Rides(m_Manager);
   string why;
   if (m_Manager.m_Rides.Request(squad, job.m_vStage, 0, ACR_Squad.WP_DEFEND, 15, "Defending", ACR_EOrderKind.DEFEND, why))
   {
    sent = true;
    PrintFormat("[ACAFE] %1: riding to the %2 site", squad.GetName(), ACAFE_Orders.Name(job.m_eOrder));
   }
   else PrintFormat("[ACAFE] %1: no ride (%2), on foot", squad.GetName(), why);
  }
  if (!sent) sent = squad.RouteTo(job.m_vStage, ACR_Squad.WP_DEFEND, 15, "Defending", ACR_EOrderKind.DEFEND, squad.IsMounted());
  if (!sent) return false;
  job.m_iSerial = squad.m_iOrderSerial;
  squad.m_OrderKind = ACR_EOrderKind.DEFEND;
  Relabel(job);
  return true;
 }

 // ACA bumps the order serial for its own transit steps too: a ride, the dismount at its end
 // or under fire. Those carry our order on; anything else is a new order.
 protected bool InTransit(ACAFE_Job job)
 {
  ACR_Squad squad = job.m_Squad;
  if (squad.OrderState() == ACR_EOrderState.DISMOUNT) return true;
  if (!m_Manager.m_Rides) return false;
  ACR_Ride ride = m_Manager.m_Rides.RideOf(squad);
  return ride && ride.m_Rider == squad && ride.m_iRiderSerial == squad.m_iOrderSerial;
 }

 // Our label on the map and in the panel, unless ACA's own steps show theirs (riding...).
 protected void Relabel(ACAFE_Job job)
 {
  ACR_Squad squad = job.m_Squad;
  ACR_EOrderState state = squad.OrderState();
  if (state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.NONE) return;
  string label = Label(job);
  if (squad.m_Order != label) squad.SetOrder(label, ACR_EOrderState.DEFEND);
 }

 string Label(ACAFE_Job job)
 {
  string what = "Building " + ACAFE_Orders.Name(job.m_eOrder);
  if (ACAFE_Orders.IsMinefield(job.m_eOrder)) what = "Laying " + ACAFE_Orders.Name(job.m_eOrder);
  if (job.m_eOrder == ACAFE_EOrder.CLEAR_MINES) what = "Clearing mines";
  if (job.m_ePhase == ACAFE_EPhase.TRAVEL) return what + " (on the way)";
  string text;
  if (ACAFE_Orders.IsMinefield(job.m_eOrder)) text = string.Format("%1 (%2/%3)", what, job.m_iLaid, job.m_aPlan.Count());
  else if (job.m_eOrder == ACAFE_EOrder.CLEAR_MINES) text = string.Format("%1 (%2 lifted)", what, job.m_iCleared);
  else
  {
   int pct = Math.ClampInt((int)(job.m_fProgress / job.m_fDuration * 100), 0, 99);
   text = string.Format("%1 (%2%%)", what, pct);
  }
  if (!job.m_sStatus.IsEmpty()) text += " - " + job.m_sStatus;
  return text;
 }

 string PlaceOf(ACAFE_Job job)
 {
  if (!m_Manager) return "";
  ACR_Intel intel = m_Manager.IntelFor(job.m_sFaction);
  if (!intel) return "";
  return " " + intel.Place(job.m_vSite);
 }

 // ---------------------------------------------------------------- tick (every second)

 void Tick()
 {
  if (!m_Manager) return;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aJobs.Count() - 1; i >= 0; i--)
  {
   if (i >= m_aJobs.Count()) continue;
   ACAFE_Job job = m_aJobs[i];
   if (!TickJob(job, now)) m_aJobs.RemoveOrdered(i);
  }
  ACAFE_Mines.Tick();
  m_iTicks++;
  if (m_iTicks % 10 != 0) return;
  for (int s = m_aStructures.Count() - 1; s >= 0; s--)
  {
   if (m_aStructures[s].Standing()) continue;
   PrintFormat("[ACAFE] %1 %2 at %3 is gone", m_aStructures[s].m_sFaction, ACAFE_Orders.Name(m_aStructures[s].m_eOrder), m_aStructures[s].m_vPos);
   m_aStructures.RemoveOrdered(s);
  }
 }

 // false: the job is over (finished, cancelled or abandoned).
 protected bool TickJob(ACAFE_Job job, float now)
 {
  ACR_Squad squad = job.m_Squad;
  if (!squad || !m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0)
  {
   End(job, "the squad was lost", true, false);
   return false;
  }
  if (squad.m_iOrderSerial != job.m_iSerial)
  {
   if (!InTransit(job))
   {
    End(job, "it was given new orders", true, false);
    return false;
   }
   job.m_iSerial = squad.m_iOrderSerial;
   job.m_bResumeCheck = true;
  }
  else if (job.m_bResumeCheck && !InTransit(job) && squad.OrderState() != ACR_EOrderState.RIDE)
  {
   // Out of the ride / dismount: still heading for our stage? (F6 unloads hold where they are.)
   job.m_bResumeCheck = false;
   if (job.m_ePhase == ACAFE_EPhase.TRAVEL && vector.DistanceXZ(squad.m_MoveTarget, job.m_vStage) > 25 && !MemberNear(squad, job.m_vSite, job.m_fWorkRadius))
   {
    PrintFormat("[ACAFE] JOB %1: %2 resumes the trip to the site", job.m_iId, squad.GetName());
    SendSquad(job, false);
   }
  }
  float dt = (now - job.m_fLastTick) / 1000.0;
  job.m_fLastTick = now;
  if (dt > 5.0) dt = 1.0;
  if (dt < 0) dt = 0;
  bool present = MemberNear(squad, job.m_vSite, job.m_fWorkRadius);

  if (job.m_ePhase == ACAFE_EPhase.TRAVEL)
  {
   if (!present)
   {
    if (now - job.m_fStartedAt > job.m_fTravelBudgetMs)
    {
     End(job, "it could not reach the site", true, true);
     return false;
    }
    Relabel(job);
    return true;
   }
   job.m_ePhase = ACAFE_EPhase.WORK;
   job.m_fLastProgress = now;
   PrintFormat("[ACAFE] JOB %1: %2 at the site, work starts", job.m_iId, squad.GetName());
  }

  // Three minutes without progress (fighting, chased off, a vehicle on the spot): give up.
  if (now - job.m_fLastProgress >= ACAFE_Config.GIVE_UP_SECONDS * 1000)
  {
   End(job, "no progress for 3 minutes", true, true);
   return false;
  }
  bool contested = ACR_FobBuild.IsContested(job.m_vSite, job.m_sFaction) || squad.KnownEnemies(60, true) > 0;
  if (contested)
  {
   job.m_sStatus = "contested";
   Relabel(job);
   return true;
  }
  if (!present)
  {
   job.m_sStatus = "waiting for the squad";
   if (squad.m_Group && !squad.m_Group.GetCurrentWaypoint() && !squad.HasPlayer()) squad.IssueWaypoint(ACR_Squad.WP_DEFEND, job.m_vStage, 15);
   Relabel(job);
   return true;
  }
  job.m_sStatus = "";
  float speed = 1.0;
  if (HasEngineers(squad)) speed = ACAFE_Config.ENGINEER_SPEED;
  float work = dt * speed;

  if (job.m_eOrder == ACAFE_EOrder.CLEAR_MINES)
  {
   job.m_fLastProgress = now;
   job.m_fProgress += work;
   job.m_fClearCredit += work;
   float perMine = ACAFE_Config.CLEAR_SECONDS_PER_MINE * ACAFE_Settings.TimeScale();
   IEntity mine = ACAFE_Mines.NearestMine(job.m_vSite, ACAFE_Config.CLEAR_RADIUS, job.m_vStage);
   if (mine && job.m_fClearCredit >= perMine)
   {
    job.m_fClearCredit -= perMine;
    ACAFE_Mines.Lift(mine);
    job.m_iCleared++;
    mine = ACAFE_Mines.NearestMine(job.m_vSite, ACAFE_Config.CLEAR_RADIUS, job.m_vStage);
   }
   if (!mine) job.m_fClearCredit = Math.Min(job.m_fClearCredit, perMine);
   if (!mine && job.m_fProgress >= job.m_fDuration)
   {
    Complete(job);
    return false;
   }
   Relabel(job);
   return true;
  }

  if (job.m_fProgress < job.m_fDuration)
  {
   job.m_fProgress = Math.Min(job.m_fProgress + work, job.m_fDuration);
   job.m_fLastProgress = now;
  }
  if (ACAFE_Orders.IsMinefield(job.m_eOrder)) ACAFE_Mines.Lay(job, job.m_fProgress / job.m_fDuration);
  if (job.m_fProgress < job.m_fDuration)
  {
   Relabel(job);
   return true;
  }
  if (ACAFE_Orders.IsStructure(job.m_eOrder) && !StructureSpotReady(job))
  {
   Relabel(job);
   return true;
  }
  Complete(job);
  return false;
 }

 static bool MemberNear(ACR_Squad squad, vector point, float radius)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && vector.DistanceXZ(body.GetOrigin(), point) <= radius) return true;
  }
  return false;
 }

 // Sappers / engineers, or a construction truck with them, work twice as fast (FOB rule).
 static bool HasEngineers(ACR_Squad squad)
 {
  if (ACR_SapperDemo.IsSapperSquad(squad)) return true;
  return squad.ConstructionTruck() != null;
 }

 // Finished work waits for vehicles to leave the road there; something built on the spot
 // meanwhile ends the job (refunded).
 protected bool StructureSpotReady(ACAFE_Job job)
 {
  if (ACR_FobSlots.VehicleOnSpot(job.m_vSite, job.m_fFootprint))
  {
   job.m_sStatus = "move vehicles off the road";
   return false;
  }
  job.m_sStatus = "";
  return true;
 }

 // ---------------------------------------------------------------- finishing

 protected void Complete(ACAFE_Job job)
 {
  ACR_Squad squad = job.m_Squad;
  string place = PlaceOf(job);
  string done;
  if (ACAFE_Orders.IsStructure(job.m_eOrder))
  {
   string why;
   if (!SiteClear(job, why))
   {
    End(job, why, true, true);
    return;
   }
   ACAFE_Structure built = Build(job);
   if (!built)
   {
    End(job, "the composition failed to spawn", true, true);
    return;
   }
   done = string.Format("%1 built %2%3", job.SquadName(), ACAFE_Orders.AName(job.m_eOrder), place);
  }
  else if (ACAFE_Orders.IsMinefield(job.m_eOrder))
  {
   ACAFE_Mines.Lay(job, 1);
   int laid = 0;
   if (job.m_Field) laid = job.m_Field.m_aMines.Count();
   ACAFE_Mines.Finish(job);
   done = string.Format("%1 laid %2 of %3 mines%4", job.SquadName(), ACAFE_Orders.AName(job.m_eOrder), laid, place);
  }
  else
  {
   if (job.m_iCleared > 0) done = string.Format("%1 lifted %2 mines%3", job.SquadName(), job.m_iCleared, place);
   else done = string.Format("%1 swept%2: no mines found", job.SquadName(), place);
  }
  PrintFormat("[ACAFE] JOB %1 done: %2", job.m_iId, done);
  m_Manager.Command().Feed(job.m_sFaction, done);
  ACR_Report.Event(job.m_sFaction, done);
  if (job.m_iPlayerId > 0) m_Manager.Notify(job.m_iPlayerId, done);
  if (!squad) return;
  // Player squads hold what they built (the Defend at the stage stays); the AI commander's
  // go back to its own plans.
  string label = "Holding position";
  if (!job.m_bAI && ACAFE_Config.HOLD_AFTER_BUILD && job.m_eOrder != ACAFE_EOrder.CLEAR_MINES) label = "Holding " + ACAFE_Orders.Name(job.m_eOrder);
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.DEFEND || state == ACR_EOrderState.NONE) squad.SetOrder(label, ACR_EOrderState.DEFEND);
  if (job.m_bAI && squad.m_iAITask == ACAFE_Orders.AI_TASK) squad.m_iAITask = ACR_EAITask.FREE;
 }

 // The pieces, each a vanilla composition owned by the builders' side (like ACA's FOB
 // upgrades: already paid for, navmesh rebuilt round it).
 protected ACAFE_Structure Build(ACAFE_Job job)
 {
  ACAFE_Structure built = new ACAFE_Structure();
  built.m_eOrder = job.m_eOrder;
  built.m_sFaction = job.m_sFaction;
  built.m_bAI = job.m_bAI;
  built.m_vPos = job.m_vSite;
  built.m_fRadius = job.m_fFootprint;
  foreach (int i, ResourceName prefab : job.m_aPrefabs)
  {
   vector pos = ACAFE_Roads.ToWorld(job.m_vSite, job.m_vForward, job.m_aOffsets[i]);
   ACR_FobBuild.FellTrees(pos, 4);
   IEntity piece = SpawnPiece(prefab, pos, job.m_fYaw, job.m_sFaction);
   if (piece) built.m_aPieces.Insert(piece);
  }
  if (built.m_aPieces.IsEmpty()) return null;
  m_aStructures.Insert(built);
  return built;
 }

 static IEntity SpawnPiece(ResourceName prefab, vector pos, float yaw, string faction)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid())
  {
   PrintFormat("[ACAFE] prefab missing: %1", prefab);
   return null;
  }
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), spawnParams.Transform);
  spawnParams.Transform[3] = pos;
  IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
  if (!entity) return null;
  entity.SetYawPitchRoll(Vector(yaw, 0, 0));
  SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (composition)
  {
   composition.ACR_SetFaction(faction);
   composition.ACR_MarkBuildCharged();
   composition.ACR_SetCompositionSpawned(true);
  }
  ACR_Navmesh.RebuildAfterSpawn(entity);
  return entity;
 }

 // reason: for the player and the side's feed. touchSquad: the job still owns the squad's
 // order (not replaced by a new one), so it goes back to plain holding.
 protected void End(ACAFE_Job job, string reason, bool refund, bool touchSquad)
 {
  if (refund) Refund(job);
  if (ACAFE_Orders.IsMinefield(job.m_eOrder)) ACAFE_Mines.Finish(job);
  ACR_Squad squad = job.m_Squad;
  string line = string.Format("%1 stopped %2: %3", job.SquadName(), Doing(job.m_eOrder), reason);
  PrintFormat("[ACAFE] JOB %1 ended: %2", job.m_iId, line);
  if (m_Manager)
  {
   m_Manager.Command().Feed(job.m_sFaction, line);
   if (job.m_iPlayerId > 0) m_Manager.Notify(job.m_iPlayerId, line);
  }
  if (!squad) return;
  if (job.m_bAI && squad.m_iAITask == ACAFE_Orders.AI_TASK) squad.m_iAITask = ACR_EAITask.FREE;
  if (!touchSquad) return;
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.DEFEND || state == ACR_EOrderState.NONE) squad.SetOrder("Holding position", ACR_EOrderState.DEFEND);
 }

 // ---------------------------------------------------------------- queries

 ACAFE_Job JobOf(ACR_Squad squad)
 {
  foreach (ACAFE_Job job : m_aJobs)
  {
   if (job.m_Squad == squad) return job;
  }
  return null;
 }

 int ActiveJobs(string faction, bool aiOnly)
 {
  int count;
  foreach (ACAFE_Job job : m_aJobs)
  {
   if (job.m_sFaction == faction && (!aiOnly || job.m_bAI)) count++;
  }
  return count;
 }

 int CountStructures(string faction)
 {
  int count;
  foreach (ACAFE_Structure built : m_aStructures)
  {
   if (built.m_sFaction == faction && built.Standing()) count++;
  }
  return count;
 }

 // A structure (built or being built) of any side this close.
 bool StructureNear(vector pos, float radius)
 {
  foreach (ACAFE_Structure built : m_aStructures)
  {
   if (built.Standing() && vector.DistanceXZ(built.m_vPos, pos) <= radius) return true;
  }
  foreach (ACAFE_Job job : m_aJobs)
  {
   if (ACAFE_Orders.IsStructure(job.m_eOrder) && vector.DistanceXZ(job.m_vSite, pos) <= radius) return true;
  }
  return false;
 }

 // The owner's command map (ACR_Mortars.Ranges hook): minefields as red danger rings,
 // work sites and what we built as pale rings ("~"). -acafeAITest also shows every AI
 // commander's work to everyone ("AI ..."), so a test can watch it.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  if (!s_Instance || s_Instance.m_Manager != ACR_Manager.GetInstance()) return;
  bool test = ACAFE_Config.AITest();
  ACAFE_Mines.Zones(faction, positions, radii, labels, test);
  foreach (ACAFE_Job job : s_Instance.m_aJobs)
  {
   string owner = "";
   if (job.m_sFaction != faction)
   {
    if (!test || !job.m_bAI) continue;
    owner = "AI " + job.m_sFaction + " ";
   }
   positions.Insert(job.m_vSite);
   float radius = 10;
   if (ACAFE_Orders.IsMinefield(job.m_eOrder)) radius = ACAFE_Config.MINEFIELD_SIZE * 0.6;
   if (job.m_eOrder == ACAFE_EOrder.CLEAR_MINES) radius = ACAFE_Config.CLEAR_RADIUS;
   radii.Insert(radius);
   string state = "planned";
   if (job.m_ePhase == ACAFE_EPhase.WORK) state = string.Format("%1%%", Math.ClampInt((int)(job.m_fProgress / job.m_fDuration * 100), 0, 99));
   labels.Insert(string.Format("~%1%2: %3 %4", owner, job.SquadName(), ACAFE_Orders.ShortName(job.m_eOrder), state));
  }
  if (!ACAFE_Config.SHOW_STRUCTURES_ON_MAP) return;
  foreach (ACAFE_Structure built : s_Instance.m_aStructures)
  {
   if (!built.Standing()) continue;
   string builtBy = "";
   if (built.m_sFaction != faction)
   {
    if (!test || !built.m_bAI) continue;
    builtBy = "AI " + built.m_sFaction + " ";
   }
   positions.Insert(built.m_vPos);
   radii.Insert(Math.Max(built.m_fRadius, 8));
   labels.Insert("~" + builtBy + ACAFE_Orders.ShortName(built.m_eOrder));
  }
 }
}
