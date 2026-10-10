// -acrMortarTest 1 (server-capable, about 5 min): the mortar team. Ground check on steep
// ground, set-up, a fire mission in range (real rounds), a refusal out of range, packing up
// for a move and setting up again, a creeping barrage, and calls refused once it is dead.
class ACR_TestMortar
{
 static ACR_MortarTeam s_Team;
 static ACR_MortarTeam s_East;
 static IEntity s_Truck;
 static int s_iPhase;
 static int s_iSince;
 static vector s_Moved;

 static int Index(ACR_Manager manager, ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 static string Call(ACR_Manager manager, ACR_ESupportType type, vector target)
 {
  ACR_SupportEntry entry = manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_aSupports[Index(manager, type)];
  entry.m_fReadyAt = 0;
  return manager.m_Supports.Call(ACR_FactionSelection.West(), Index(manager, type), target);
 }

 static string State()
 {
  if (!s_Team) return "no team";
  string order = "-";
  if (s_Team.m_Squad) order = s_Team.m_Squad.m_Order;
  return string.Format("%1 (%2) order '%3'", typename.EnumToString(ACR_EMortarState, s_Team.m_eState), s_Team.m_sStatus, order);
 }

 // Steep ground near the range: a spot there is refused, a level one found nearby.
 static void GroundCheck(vector around)
 {
  vector steep;
  float worst;
  for (int x = -20; x <= 20; x++)
  {
   for (int z = -20; z <= 20; z++)
   {
    vector p = ACR_Manager.Ground(around + Vector(x * 15, 0, z * 15));
    if (BBB_Terrain.IsWaterAt(p)) continue;
    float slope = ACR_Mortars.SlopeAt(p);
    if (slope > worst) { worst = slope; steep = p; }
   }
  }
  vector spot;
  bool there = ACR_Mortars.CheckSpot(steep, spot);
  vector near;
  bool found = ACR_Mortars.FindSpot(steep, near);
  PrintFormat("[ACR] MORTAR steepest ground %1 deg at %2: set up there %3; level spot found %4, %5 m away, slope %6 deg", worst, steep, there, found, vector.DistanceXZ(near, steep), ACR_Mortars.SlopeAt(near));
  ACR_TestRegress.Check("mortar: steep ground refused", worst <= ACR_Mortars.MAX_SLOPE_DEG || !there, string.Format("%1 deg accepted", worst));
  ACR_TestRegress.Check("mortar: a level spot is found near steep ground", !found || ACR_Mortars.SlopeAt(near) <= ACR_Mortars.MAX_SLOPE_DEG + 0.5, string.Format("slope %1", ACR_Mortars.SlopeAt(near)));
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find(ACR_FactionSelection.East());
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find(ACR_FactionSelection.West());
   if (us) us.m_bAICommander = false;
   ACR_Mortars.DEPLOY_SECONDS = 10;
   ACR_Mortars.PACK_SECONDS = 6;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  vector home = alpha.m_Home;
  if (tick == 3)
  {
   PrintFormat("[ACR] MORTAR reach: M252 %1 m, 2B14 %2 m", ACR_Mortars.MaxRange(ACR_Mortars.TUBE_WEST), ACR_Mortars.MaxRange(ACR_Mortars.TUBE_EAST));
   bool listed;
   foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(ACR_FactionSelection.East()).m_aRequisitions) if (entry.m_bMortarTeam) listed = true;
   ACR_TestRegress.Check("mortar: team in the requisition list", listed, "");
   GroundCheck(home);
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_iRP = 900;
   PrintFormat("[ACR] MORTAR call with no team: %1", Call(manager, ACR_ESupportType.MORTAR_HE_SMALL, ACR_Manager.Ground(home + Vector(0, 0, 600))));
  }
  if (tick == 3) s_Truck = ACR_Manager.SpawnPrefab("{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", ACR_Manager.Ground(home + Vector(60, 0, -40)));
  if (tick == 6 && s_Truck)
  {
   vector spot;
   bool onTruck = ACR_Mortars.CheckSpot(s_Truck.GetOrigin(), spot);
   ACR_TestRegress.Check("mortar: no setting up on top of an object", !onTruck, string.Format("accepted at %1", spot));
   SCR_EntityHelper.DeleteEntityAndChildren(s_Truck);
  }
  if (tick == 9 && s_Team && s_Team.m_Squad)
  {
   int carriers;
   array<AIAgent> crew = {};
   s_Team.m_Squad.GetAgents(crew);
   string backs = "";
   foreach (AIAgent man : crew)
   {
    string back = ACR_Mortars.BackOf(man.GetControlledEntity());
    backs += back + " ";
    if (back.Contains("Part_")) carriers++;
   }
   ACR_TestRegress.Check("mortar: packed, two crewmen carry the baseplate and barrel", carriers == 2, backs);
  }
  if (tick == 5)
  {
   manager.Mortars().SpawnTeam(ACR_FactionSelection.West(), "Mortar team 1", ACR_Manager.Ground(home + Vector(25, 0, -10)));
   s_Team = manager.Mortars().m_Teams[manager.Mortars().m_Teams.Count() - 1];
   // A Soviet team far off: its base group must not bring its own six riflemen.
   manager.Mortars().SpawnTeam(ACR_FactionSelection.East(), "Mortar team 1", ACR_Manager.Ground(home + Vector(-900, 0, 300)));
   s_East = manager.Mortars().m_Teams[manager.Mortars().m_Teams.Count() - 1];
   s_iPhase = 1;
   s_iSince = tick;
  }
  if (tick > 5 && tick % 3 == 0 && s_iPhase != 6 && s_iPhase != 7) PrintFormat("[ACR] MORTAR t%1 phase %2 %3", tick, s_iPhase, State());
  int waited = tick - s_iSince;
  // 1: sets up by itself, then a refusal out of range and a mission in range
  if (s_iPhase == 1 && s_Team && (s_Team.m_eState == ACR_EMortarState.DEPLOYED || waited > 30))
  {
   ACR_TestRegress.Check("mortar: sets up by itself", s_Team.m_eState == ACR_EMortarState.DEPLOYED && s_Team.m_Tube != null, State());
   string backsUp = "";
   bool partsGone = true;
   array<AIAgent> setUp = {};
   s_Team.m_Squad.GetAgents(setUp);
   foreach (AIAgent member : setUp)
   {
    string worn = ACR_Mortars.BackOf(member.GetControlledEntity());
    backsUp += worn + " ";
    if (worn.Contains("Part_")) partsGone = false;
   }
   ACR_TestRegress.Check("mortar: set up, the parts are off their backs", partsGone, backsUp);
   int east = -1;
   if (s_East && s_East.m_Squad) east = s_East.m_Squad.CountAlive();
   ACR_TestRegress.Check("mortar: Soviet team is just its 3 crew", east == 3, string.Format("%1 soldiers", east));
   if (s_Team.m_Tube) PrintFormat("[ACR] MORTAR tube at %1, slope %2 deg, height above ground %3", s_Team.m_Tube.GetOrigin(), ACR_Mortars.SlopeAt(s_Team.m_Tube.GetOrigin()), s_Team.m_Tube.GetOrigin()[1] - GetGame().GetWorld().GetSurfaceY(s_Team.m_Tube.GetOrigin()[0], s_Team.m_Tube.GetOrigin()[2]));
   string far = Call(manager, ACR_ESupportType.MORTAR_HE_SMALL, ACR_Manager.Ground(home + Vector(0, 0, 4200)));
   ACR_TestRegress.Check("mortar: out of range refused", far.Contains("out of range"), far);
   ACR_Mortars.s_iLastFired = -1;
   string near = Call(manager, ACR_ESupportType.MORTAR_HE_SMALL, ACR_Manager.Ground(home + Vector(0, 0, 700)));
   ACR_TestRegress.Check("mortar: fire mission accepted", near.Contains("firing"), near);
   s_iPhase = 2;
   s_iSince = tick;
  }
  // 2: confirm a real smoke mission and all four smoke rounds before moving.
  if (s_iPhase == 2 && s_Team && (ACR_Mortars.s_iLastFired >= 0 || waited > 50))
  {
   ACR_TestRegress.Check("mortar: all 4 rounds fired", ACR_Mortars.s_iLastFired == 4, string.Format("fired %1, %2", ACR_Mortars.s_iLastFired, State()));
   ACR_Mortars.s_iLastFired = -1;
   string smoke = Call(manager, ACR_ESupportType.MORTAR_SMOKE, ACR_Manager.Ground(home + Vector(0, 0, 700)));
   ACR_TestRegress.Check("mortar: smoke screen call accepted", smoke.Contains("firing"), smoke);
   s_iPhase = 20;
   s_iSince = tick;
  }
  if (s_iPhase == 20 && s_Team && (ACR_Mortars.s_iLastFired >= 0 || waited > 50))
  {
   bool shellValid = !s_Team.m_sShell.IsEmpty() && Resource.Load(s_Team.m_sShell).IsValid();
   ACR_TestRegress.Check("mortar: smoke shell is valid and all 4 rounds fired", shellValid && ACR_Mortars.s_iLastFired == 4, string.Format("shell %1, fired %2", s_Team.m_sShell, ACR_Mortars.s_iLastFired));
   ACR_Mortars.s_iLastFired = -1;
   string large = Call(manager, ACR_ESupportType.MORTAR_HE_LARGE, ACR_Manager.Ground(home + Vector(0, 0, 700)));
   ACR_TestRegress.Check("mortar: eight-round HE mission accepted", large.Contains("firing"), large);
   s_iPhase = 21;
   s_iSince = tick;
  }
  if (s_iPhase == 21 && s_Team && (ACR_Mortars.s_iLastFired >= 0 || waited > 80))
  {
   ACR_TestRegress.Check("mortar: all eight HE rounds fired across ammo batches", ACR_Mortars.s_iLastFired == 8, string.Format("fired %1, %2", ACR_Mortars.s_iLastFired, State()));
   s_Moved = ACR_Manager.Ground(s_Team.m_Spot + Vector(-30, 0, 8));
   PrintFormat("[ACR] MORTAR move order: %1", s_Team.m_Squad.MoveTo(s_Moved));
   s_iPhase = 3;
   s_iSince = tick;
  }
  if (s_iPhase == 3 && waited == 3)
  {
   ACR_TestRegress.Check("mortar: packs up before moving", s_Team.m_eState == ACR_EMortarState.PACKING && s_Team.m_Squad.m_Order.Contains("Packing"), State());
   s_iPhase = 4;
  }
  // 4: walks there, sets up again, creeping barrage
  if (s_iPhase == 4 && s_Team && ((s_Team.m_eState == ACR_EMortarState.DEPLOYED && vector.DistanceXZ(s_Team.m_Spot, s_Moved) < 30) || waited > 80))
  {
   ACR_TestRegress.Check("mortar: set up again after the move", s_Team.m_eState == ACR_EMortarState.DEPLOYED && vector.DistanceXZ(s_Team.m_Spot, s_Moved) < 30, string.Format("%1, %2 m from the new spot, %3 s", State(), vector.DistanceXZ(s_Team.m_Spot, s_Moved), waited));
   ACR_Mortars.s_iLastFired = -1;
   PrintFormat("[ACR] MORTAR creeping: %1", Call(manager, ACR_ESupportType.MORTAR_CREEPING, ACR_Manager.Ground(home + Vector(300, 0, 800))));
   s_iPhase = 5;
   s_iSince = tick;
  }
  // 5: nine rounds out, then the crew dies
  if (s_iPhase == 5 && s_Team && (ACR_Mortars.s_iLastFired >= 0 || waited > 70))
  {
   ACR_TestRegress.Check("mortar: creeping barrage fired (9 rounds)", ACR_Mortars.s_iLastFired == 9, string.Format("fired %1, %2", ACR_Mortars.s_iLastFired, State()));
   // The mortar is destroyed: a Reinforce brings a new one and they set up again.
   if (s_Team.m_Tube) SCR_EntityHelper.DeleteEntityAndChildren(s_Team.m_Tube);
   s_iPhase = 51;
   s_iSince = tick;
  }
  if (s_iPhase == 51 && waited == 2)
  {
   ACR_TestRegress.Check("mortar: a destroyed mortar is noticed", s_Team.m_bLost, State());
   manager.Mortars().Rearmed(s_Team);
   s_iPhase = 52;
   s_iSince = tick;
  }
  if (s_iPhase == 52 && (s_Team.m_eState == ACR_EMortarState.DEPLOYED || waited > 30))
  {
   ACR_TestRegress.Check("mortar: sets up again with the new mortar", s_Team.m_eState == ACR_EMortarState.DEPLOYED && !s_Team.m_bLost, State());
   array<AIAgent> agents = {};
   s_Team.m_Squad.GetAgents(agents);
   foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
   s_iPhase = 6;
   s_iSince = tick;
  }
  if (s_iPhase == 6 && waited == 3)
  {
   string dead = Call(manager, ACR_ESupportType.MORTAR_HE_SMALL, ACR_Manager.Ground(home + Vector(0, 0, 600)));
   ACR_TestRegress.Check("mortar: no calls once the team is dead", dead.Contains("no mortar team"), dead);
   ACR_TestRegress.Complete("mortar", 21);
   s_iPhase = 7;
  }
 }
}
