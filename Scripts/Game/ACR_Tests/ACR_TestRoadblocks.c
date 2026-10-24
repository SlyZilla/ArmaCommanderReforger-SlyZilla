// -acrRoadblocksTest 1 (server-capable, ~1 min): roadblock sizes and payment (the ACA Field
// Engineering port). Prefabs, prices and request mapping per size and side (RHS AFRF builds the
// USSR set, FIA log barricades); every size spawned for US, USSR and FIA on a road near Alpha,
// guns on the medium and large checkpoints only; RP and construction truck payment, each refunded
// where it came from (Halt, a new order, a walk that times out), a truck reserved for another
// squad's FOB left alone, the AI commander paying RP, a regroup pausing a paid build without a
// refund; then Alpha builds a barricade that waits for a truck to leave the road, faces a known
// enemy and is held until a new order.
class ACR_TestRoadblocks
{
 static const int CHECKS = 18;
 static const ResourceName TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";
 static int s_iStep;
 static int s_iStepAt;
 static vector s_vSite;
 static float s_fYaw;
 static IEntity s_Truck;
 static vector s_vThreat;
 static bool s_bThreatKnown;
 static vector s_vBuild;
 static float s_fStock;
 static int s_iVehicleWaits;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("roadblocks", "no Alpha squad");
   return;
  }
  if (tick > 180) ACR_TestRegress.Abort("roadblocks", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
  ACR_Logistics logistics = manager.m_Logistics;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   CheckStatic();
   ACR_Roadblocks.s_fWorkScaleForTest = 0.2;
   us.m_iRP = 500;
   if (!FindRoadSite(alpha.GetAnchor().GetOrigin(), s_vSite, s_fYaw))
   {
    ACR_TestRegress.Abort("roadblocks", "no road site near Alpha");
    return;
   }
   PrintFormat("[ACR] ROADBLOCKS test: site %1 yaw %2, road %3 m wide, %4 m from Alpha", s_vSite, s_fYaw, ACR_Roadblocks.RoadWidthAt(s_vSite), vector.DistanceXZ(s_vSite, alpha.GetAnchor().GetOrigin()));
   CheckSpawns("US");
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   CheckSpawns("USSR");
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   CheckSpawns("FIA");
   s_Truck = ACR_Manager.SpawnPrefab(TRUCK, ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(250, 0, 0)));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (!s_Truck)
   {
    ACR_TestRegress.Abort("roadblocks", "no construction truck");
    return;
   }
   CheckPayment(manager, alpha, us, h);
   // A regroup sets the order aside (state REGROUP) and restores it later: the build stays paid.
   s_fStock = logistics.StockOf(s_Truck);
   if (!alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.SMALL))
   {
    ACR_TestRegress.Abort("roadblocks", "the regroup fixture's order failed");
    return;
   }
   alpha.SetOrder("Regrouping", ACR_EOrderState.REGROUP);
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   if (waited < 2) return;
   float paused = logistics.StockOf(s_Truck);
   bool pending = alpha.RoadblockPending() && alpha.OrderState() == ACR_EOrderState.REGROUP;
   alpha.Halt("Holding");
   float back = logistics.StockOf(s_Truck);
   ACR_TestRegress.Check("roadblocks: a regroup pauses a paid build without refunding it; Halt then refunds it once", pending && Math.AbsFloat(s_fStock - paused - 3) < 0.01 && Math.AbsFloat(back - s_fStock) < 0.01 && !alpha.RoadblockPending(), string.Format("stock %1 -> %2 while regrouping, %3 after Halt", s_fStock, paused, back));
   // Alpha next to the road, the truck beside it (off the road, within 50 m).
   vector along = Along(s_fYaw);
   vector side = Vector(along[2], 0, -along[0]);
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (int i, AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body) body.SetOrigin(ACR_Manager.Ground(s_vSite + side * 10 + along * (i * 2 - 3)));
   }
   s_Truck.SetOrigin(ACR_Manager.Ground(s_vSite + side * 25));
   s_Truck.Update();
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   if (waited < 2) return;
   // A known enemy 100 m along the road, behind the checkpoint's road-order facing: the order
   // turns it round (ACR_Intel.ThreatPoint, as ACR_Requests does).
   s_vThreat = ACR_Manager.Ground(s_vSite - Along(s_fYaw) * 100);
   ACR_Intel intel = manager.IntelFor("US");
   ACR_Contact contact = new ACR_Contact();
   contact.m_Pos = s_vThreat;
   contact.m_fSeen = GetGame().GetWorld().GetWorldTime();
   if (intel) intel.m_Contacts.Insert(contact);
   vector threat = ACR_Intel.ThreatPoint("US", s_vSite);
   s_bThreatKnown = vector.DistanceXZ(threat, s_vThreat) < 0.5;
   string why;
   float yaw;
   bool valid = ACR_Roadblocks.ValidateSite(s_vSite, s_vBuild, yaw, why, "US", threat);
   if (intel) intel.m_Contacts.RemoveItem(contact);
   s_fStock = logistics.StockOf(s_Truck);
   if (!valid || !alpha.EstablishRoadblock(s_vBuild, yaw, ACR_ERoadblockType.BARRICADE))
   {
    ACR_TestRegress.Abort("roadblocks", "the barricade order failed: " + why);
    return;
   }
   PrintFormat("[ACR] ROADBLOCKS test: barricade ordered at %1 yaw %2, threat %3 (known %4), paid by truck %5", s_vBuild, yaw, threat, s_bThreatKnown, alpha.m_RoadblockTruck == s_Truck);
   // The truck parks on the spot before the work is done.
   s_Truck.SetOrigin(s_vBuild + Vector(0, 1, 0));
   s_Truck.Update();
   NextStep(tick);
   return;
  }

  if (s_iStep == 6)
  {
   if (alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && alpha.m_Order.Contains("vehicles")) s_iVehicleWaits++;
   if (s_iVehicleWaits < 3 && alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && waited < 60) return;
   ACR_TestRegress.Check("roadblocks: finished work waits for a vehicle to leave the road", s_iVehicleWaits >= 3 && alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && !alpha.m_ActiveRoadblock, string.Format("waited %1 s while the truck stood there, order '%2'", s_iVehicleWaits, alpha.m_Order));
   vector away = Along(s_fYaw);
   s_Truck.SetOrigin(ACR_Manager.Ground(s_vBuild + Vector(away[2], 0, -away[0]) * 25));
   s_Truck.Update();
   NextStep(tick);
   return;
  }

  if (s_iStep == 7)
  {
   if (!alpha.m_ActiveRoadblock && alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && waited < 30) return;
   ACR_Roadblock rb = alpha.m_ActiveRoadblock;
   string prefab;
   float facing;
   if (rb && rb.m_Entity) prefab = rb.m_Entity.GetPrefabData().GetPrefabName();
   if (rb)
   {
    vector to = s_vThreat - rb.m_vPos;
    to[1] = 0;
    to.Normalize();
    vector forward = Along(rb.m_fYaw);
    facing = forward[0] * to[0] + forward[2] * to[2];
   }
   float stock = logistics.StockOf(s_Truck);
   bool built = rb && rb.m_iType == ACR_ERoadblockType.BARRICADE && prefab == ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "US", ACR_Roadblocks.RoadWidthAt(rb.m_vPos)) && !rb.m_Turret;
   bool held = alpha.OrderState() == ACR_EOrderState.DEFEND && alpha.m_Order == "Holding roadblock" && !alpha.RoadblockPending();
   bool paid = Math.AbsFloat(s_fStock - stock - 1.6) < 0.01;
   ACR_TestRegress.Check("roadblocks: the barricade goes in paid by the truck, faces the known enemy, and is held", built && held && paid && s_bThreatKnown && facing > 0.9, string.Format("'%1', order '%2', stock %3 -> %4, facing dot %5 (threat known %6)", FilePath.StripPath(prefab), alpha.m_Order, s_fStock, stock, facing, s_bThreatKnown));
   alpha.MoveTo(ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(5, 0, 0)));
   ACR_TestRegress.Check("roadblocks: a new order releases a barricade's squad; the barricade stays", rb && !alpha.m_ActiveRoadblock && ACR_Roadblocks.s_aRoadblocks.Contains(rb) && rb.m_Entity);
   if (rb)
   {
    if (rb.m_Entity) ACR_Navmesh.DeleteStructure(rb.m_Entity);
    ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
   }
   if (s_Truck) SCR_EntityHelper.DeleteEntityAndChildren(s_Truck);
   ACR_Roadblocks.s_fWorkScaleForTest = 1;
   ACR_TestRegress.Complete("roadblocks", CHECKS);
   s_iStep = 8;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // The checkpoint's +Z for a yaw.
 protected static vector Along(float yaw)
 {
  return Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
 }

 // Prefabs, prices and request mapping (nothing spawned or charged).
 protected static void CheckStatic()
 {
  bool checkpoints = ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.SMALL, "US", 6) == ACR_Roadblocks.CHECKPOINT_S_US && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.MEDIUM, "US", 6) == ACR_Roadblocks.PREFAB_US && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.LARGE, "US", 6) == ACR_Roadblocks.CHECKPOINT_L_US;
  bool east = ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.SMALL, "RHS_AFRF", 6) == ACR_Roadblocks.CHECKPOINT_S_USSR && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.MEDIUM, "RHS_AFRF", 6) == ACR_Roadblocks.PREFAB_USSR && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.LARGE, "USSR", 6) == ACR_Roadblocks.CHECKPOINT_L_USSR && ACR_Roadblocks.PrefabFor("USSR") == ACR_Roadblocks.PREFAB_USSR && ACR_Roadblocks.TypeFor(ACR_ERoadblockType.LARGE, "RHS_AFRF") == ACR_ERoadblockType.LARGE;
  bool barricades = ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "US", 5) == ACR_Roadblocks.BARRICADE_S_US && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "USSR", 8) == ACR_Roadblocks.BARRICADE_M_USSR && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "RHS_AFRF", 12) == ACR_Roadblocks.BARRICADE_L_USSR;
  bool logs = ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.MEDIUM, "FIA", 4) == ACR_Roadblocks.LOGS_1 && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "FIA", 7) == ACR_Roadblocks.LOGS_2 && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.SMALL, "FIA", 10) == ACR_Roadblocks.LOGS_3 && ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.LARGE, "FIA", 4) == ACR_Roadblocks.LOGS_3 && ACR_Roadblocks.TypeFor(ACR_ERoadblockType.LARGE, "FIA") == ACR_ERoadblockType.BARRICADE;
  ACR_TestRegress.Check("roadblocks: prefabs per size and side (RHS AFRF builds the USSR set, FIA log barricades)", checkpoints && east && barricades && logs, string.Format("checkpoints %1, east %2, barricades %3, logs %4", checkpoints, east, barricades, logs));

  ACR_Roadblocks.s_fWorkScaleForTest = 1;
  bool rp = ACR_Roadblocks.Cost(ACR_ERoadblockType.SMALL) == 12 && ACR_Roadblocks.Cost(ACR_ERoadblockType.MEDIUM) == 20 && ACR_Roadblocks.Cost(ACR_ERoadblockType.LARGE) == 30 && ACR_Roadblocks.Cost(ACR_ERoadblockType.BARRICADE) == 8;
  bool supplies = ACR_Roadblocks.Supplies(ACR_ERoadblockType.SMALL) == 75 && ACR_Roadblocks.Supplies(ACR_ERoadblockType.MEDIUM) == 225 && ACR_Roadblocks.Supplies(ACR_ERoadblockType.LARGE) == 350 && ACR_Roadblocks.Supplies(ACR_ERoadblockType.BARRICADE) == 40;
  bool work = ACR_Roadblocks.Duration(ACR_ERoadblockType.SMALL) == 35 && ACR_Roadblocks.Duration(ACR_ERoadblockType.MEDIUM) == 45 && ACR_Roadblocks.Duration(ACR_ERoadblockType.LARGE) == 70 && ACR_Roadblocks.Duration(ACR_ERoadblockType.BARRICADE) == 25;
  ACR_TestRegress.Check("roadblocks: prices and work times (the medium as before: 20 RP, 45 s)", rp && supplies && work, string.Format("rp %1, supplies %2, work %3", rp, supplies, work));

  bool requests = ACR_Roadblocks.TypeOfRequest(ACR_ERequest.ORDER_ROADBLOCK) == ACR_ERoadblockType.MEDIUM && ACR_Roadblocks.TypeOfRequest(ACR_ERequest.ORDER_ROADBLOCK_S) == ACR_ERoadblockType.SMALL && ACR_Roadblocks.TypeOfRequest(ACR_ERequest.ORDER_ROADBLOCK_L) == ACR_ERoadblockType.LARGE && ACR_Roadblocks.TypeOfRequest(ACR_ERequest.ORDER_BARRICADE) == ACR_ERoadblockType.BARRICADE && ACR_Roadblocks.TypeOfRequest(ACR_ERequest.MOVE_TO) == -1;
  bool buttons = ACR_CommandPanel.RoadblockOfMode(ACR_ECursorMode.ROADBLOCK) == ACR_ERoadblockType.MEDIUM && ACR_CommandPanel.RoadblockOfMode(ACR_ECursorMode.ROADBLOCK_S) == ACR_ERoadblockType.SMALL && ACR_CommandPanel.RoadblockOfMode(ACR_ECursorMode.ROADBLOCK_L) == ACR_ERoadblockType.LARGE && ACR_CommandPanel.RoadblockOfMode(ACR_ECursorMode.BARRICADE) == ACR_ERoadblockType.BARRICADE && ACR_CommandPanel.RoadblockOfMode(ACR_ECursorMode.MOVE) == -1;
  ACR_TestRegress.Check("roadblocks: map orders and panel buttons map to the sizes", requests && buttons, string.Format("requests %1, buttons %2", requests, buttons));
 }

 // Each size and the barricade on the site for one side: the side's prefab, a gun on the medium
 // and large checkpoints only (FIA: log barricades whatever the order).
 protected static void CheckSpawns(string faction)
 {
  float width = ACR_Roadblocks.RoadWidthAt(s_vSite);
  bool fia = ACR_Roadblocks.IsFia(faction);
  bool ok = true;
  string detail = string.Format("road %1 m:", width);
  array<int> types = {ACR_ERoadblockType.SMALL, ACR_ERoadblockType.MEDIUM, ACR_ERoadblockType.LARGE, ACR_ERoadblockType.BARRICADE};
  foreach (int type : types)
  {
   ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock(faction, s_vSite, s_fYaw, "", type);
   string prefab;
   bool gun;
   if (rb && rb.m_Entity) prefab = rb.m_Entity.GetPrefabData().GetPrefabName();
   if (rb) gun = rb.m_Turret != null;
   bool wantGun = !fia && (type == ACR_ERoadblockType.MEDIUM || type == ACR_ERoadblockType.LARGE);
   bool right = rb && prefab == ACR_Roadblocks.PrefabFor(type, faction, width) && gun == wantGun && rb.m_iType == ACR_Roadblocks.TypeFor(type, faction);
   ok = ok && right;
   string gunText = "";
   if (gun) gunText = " +gun";
   detail += string.Format(" %1=%2%3", ACR_Roadblocks.Name(type), FilePath.StripPath(prefab), gunText);
   if (!rb) continue;
   if (rb.m_Entity) ACR_Navmesh.DeleteStructure(rb.m_Entity);
   ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
  }
  if (fia) ACR_TestRegress.Check("roadblocks: FIA builds log barricades, no gun", ok, detail);
  else ACR_TestRegress.Check(string.Format("roadblocks: %1 checkpoints S / M / L and barricade spawn their prefabs, guns on M and L only", faction), ok, detail);
 }

 // Paying and refunds, all in one tick: RP, the truck, and where each goes back to.
 protected static void CheckPayment(ACR_Manager manager, ACR_Squad alpha, ACR_FactionData us, ACR_TestHarness h)
 {
  ACR_Logistics logistics = manager.m_Logistics;
  vector home = alpha.GetAnchor().GetOrigin();

  // No construction truck near: RP pays, and Halt gives it back (it used not to: Halt sets its
  // own state before NewOrder).
  us.m_iRP = 500;
  bool ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.SMALL);
  int charged = us.m_iRP;
  string label = alpha.m_Order;
  alpha.Halt("Holding");
  ACR_TestRegress.Check("roadblocks: RP pays with no construction truck near; Halt refunds it", ordered && charged == 488 && label.StartsWith("Building Roadblock S") && us.m_iRP == 500 && !alpha.RoadblockPending() && alpha.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK, string.Format("rp 500 -> %1 -> %2, '%3'", charged, us.m_iRP, label));

  // A stocked construction truck with the squad pays in supplies; a new order puts them back.
  s_Truck.SetOrigin(ACR_Manager.Ground(home + Vector(15, 0, 8)));
  s_Truck.Update();
  Restock(logistics, 20);
  ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.LARGE);
  float stock = logistics.StockOf(s_Truck);
  label = alpha.m_Order;
  ACR_TestRegress.Check("roadblocks: a construction truck with the squad pays in supplies (350 = 14 loads)", ordered && Math.AbsFloat(stock - 6) < 0.01 && us.m_iRP == 500 && alpha.m_RoadblockTruck == s_Truck && label.StartsWith("Building Roadblock L"), string.Format("stock 20 -> %1, rp %2, '%3'", stock, us.m_iRP, label));
  alpha.MoveTo(ACR_Manager.Ground(home + Vector(3, 0, 0)));
  stock = logistics.StockOf(s_Truck);
  ACR_TestRegress.Check("roadblocks: a new order refunds the supplies to the truck", Math.AbsFloat(stock - 20) < 0.01 && !alpha.RoadblockPending() && us.m_iRP == 500 && alpha.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK, string.Format("stock %1, rp %2", stock, us.m_iRP));

  // Short of supplies: RP.
  Restock(logistics, 2);
  ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.MEDIUM);
  charged = us.m_iRP;
  stock = logistics.StockOf(s_Truck);
  alpha.Halt("Holding");
  ACR_TestRegress.Check("roadblocks: a truck short of supplies leaves it to RP", ordered && charged == 480 && Math.AbsFloat(stock - 2) < 0.01 && us.m_iRP == 500, string.Format("rp 500 -> %1 -> %2, stock %3", charged, us.m_iRP, stock));

  // A truck another squad has reserved for its FOB is left alone.
  Restock(logistics, 20);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!bravo) ACR_TestRegress.Skip("roadblocks: a truck reserved for another squad's FOB is left alone", "no Bravo squad");
  else
  {
   bravo.SetFobReservedTruckForTest(s_Truck);
   ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.SMALL);
   charged = us.m_iRP;
   stock = logistics.StockOf(s_Truck);
   alpha.Halt("Holding");
   bravo.SetFobReservedTruckForTest(null);
   ACR_TestRegress.Check("roadblocks: a truck reserved for another squad's FOB is left alone", ordered && charged == 488 && Math.AbsFloat(stock - 20) < 0.01 && us.m_iRP == 500, string.Format("rp 500 -> %1, stock %2", charged, stock));
  }

  // The AI commander's roadblocks pay RP, as before (its FOBs need the truck).
  ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.MEDIUM, false);
  charged = us.m_iRP;
  stock = logistics.StockOf(s_Truck);
  alpha.Halt("Holding");
  ACR_TestRegress.Check("roadblocks: the AI commander's roadblocks pay RP", ordered && charged == 480 && Math.AbsFloat(stock - 20) < 0.01 && us.m_iRP == 500, string.Format("rp 500 -> %1, stock %2", charged, stock));

  // Short of both: refused, nothing charged.
  Restock(logistics, 0);
  us.m_iRP = 5;
  string why;
  bool refused = !ACR_Roadblocks.CanOrder(alpha, why, ACR_ERoadblockType.BARRICADE) && !alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.BARRICADE);
  ACR_TestRegress.Check("roadblocks: refused short of RP and supplies, nothing charged", refused && us.m_iRP == 5 && logistics.StockOf(s_Truck) < 0.01 && why.Contains("RP") && alpha.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK, why);
  us.m_iRP = 500;
  Restock(logistics, 20);

  // The give-up timer runs from arrival: a long walk is not "no progress"; the walk has its own
  // limit, and giving up refunds (here to the truck).
  if (vector.DistanceXZ(home, s_vSite) < 40)
  {
   ACR_TestRegress.Skip("roadblocks: the give-up timer starts on arrival; the walk has its own limit", "the road is too close to Alpha");
   return;
  }
  ordered = alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.SMALL);
  float now = GetGame().GetWorld().GetWorldTime();
  alpha.m_fRoadblockLastProgress = now - ACR_Squad.FOB_GIVE_UP_MS - 1000;
  alpha.TickEstablishRoadblock(now + 1000);
  bool walking = alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && alpha.m_Order.Contains("moving to road");
  string walkLabel = alpha.m_Order;
  alpha.m_fRoadblockDeadline = now;
  alpha.TickEstablishRoadblock(now + 2000);
  stock = logistics.StockOf(s_Truck);
  ACR_TestRegress.Check("roadblocks: the give-up timer starts on arrival; the walk has its own limit", ordered && walking && alpha.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK && !alpha.RoadblockPending() && Math.AbsFloat(stock - 20) < 0.01, string.Format("'%1', then '%2', stock %3", walkLabel, alpha.m_Order, stock));
 }

 protected static void Restock(ACR_Logistics logistics, float loads)
 {
  logistics.Use(s_Truck, logistics.StockOf(s_Truck));
  if (loads > 0) logistics.AddStock(s_Truck, loads);
 }

 // The nearest road spot 45-600 m from 'from' where a roadblock may go (on the road the spot is
 // nearest to, clear of bases, roadblocks and vehicles).
 protected static bool FindRoadSite(vector from, out vector site, out float yaw)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(from - Vector(600, 500, 600), from + Vector(600, 500, 600), roads);
  float best = float.MAX;
  foreach (BaseRoad road : roads)
  {
   array<vector> nodes = {};
   road.GetPoints(nodes);
   for (int i = 1; i < nodes.Count(); i++)
   {
    vector along = nodes[i] - nodes[i - 1];
    along[1] = 0;
    if (along.Length() < 12) continue;
    vector centre = (nodes[i] + nodes[i - 1]) * 0.5;
    float d = vector.DistanceXZ(centre, from);
    if (d < 45 || d >= best) continue;
    BaseRoad closest;
    float distance;
    aiWorld.GetRoadNetworkManager().GetClosestRoad(centre, closest, distance);
    if (closest != road) continue;
    vector snapped;
    float heading;
    string why;
    if (!ACR_Roadblocks.ValidateSite(centre, snapped, heading, why, "US")) continue;
    best = d;
    site = snapped;
    yaw = heading;
   }
  }
  return best < float.MAX;
 }
}
