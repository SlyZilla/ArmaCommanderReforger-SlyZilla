// -acrRoadblockLaneTest 1 (server-capable, ~3 min): the friendly lane through a roadblock (pn12d
// playtest, 27 Oct: the AI US side's medium checkpoint jammed its own M923, truck and Humvee and
// ran over its own men). On a straight road near Alpha every roadblock (US and USSR S / M / L
// checkpoints and barricades, FIA logs) spawns with no solid piece in its lane (the vanilla layout
// for comparison), the M and L guns beside the lane and covering it, the holders' post off the lane
// with the gun inside its Defend. Then Alpha builds a medium checkpoint and holds it, and one of our
// M923s is sent along the road through it: it drives the lane, slowed, with no "stuck", and none of
// the checkpoint's men is hurt. As it comes within 30 m one of them is put in the middle of the lane:
// he steps clear, and every man on foot stays at least HULL_GAP from its hull (sampled every 0.1 s).
class ACR_TestRoadblockLane
{
 static const int CHECKS = 8;
 static const float HULL_GAP = 1.0; // m between a man on foot and the truck's hull, at least
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const float COVER = 8;   // the gun's distance from the lane's line, at most
 static int s_iStep;
 static int s_iStepAt;
 static vector s_vSite;
 static float s_fYaw;
 static ACR_Squad s_Truck;
 static ACR_Roadblock s_Rb;
 static vector s_vGoal;
 static float s_fHealth;
 static int s_iAlive;
 static float s_fEnterZ = float.MAX;
 static float s_fExitZ = -float.MAX;
 static float s_fMaxX;
 static float s_fMaxKmh;
 static int s_iOnLane;    // tick samples with one of the holders on foot in the lane
 static int s_iSamples;
 static float s_fNearest = float.MAX; // the truck's closest pass to a holder on foot
 static ref array<IEntity> s_aHolders = {};
 static ref array<float> s_aHealthAt = {}; // each holder's health when the truck set off
 static ref array<float> s_aClosest = {};  // and the truck's closest pass to him
 static ref array<vector> s_aPieces = {};  // where the checkpoint's pieces stood
 static ref array<float> s_aGap = {};      // each holder's closest to the truck's hull
 static IEntity s_Planted;                 // the holder put in the lane in front of it
 static float s_fPlantedClear = float.MAX; // his clearance of the lane with the truck abeam of him
 static int s_iStepsAt;

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
   if (tick == 60) ACR_TestRegress.Abort("rblane", "no Alpha squad");
   return;
  }
  if (tick > 380) ACR_TestRegress.Abort("rblane", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   if (!FindStraightSite(alpha.GetAnchor().GetOrigin(), s_vSite, s_fYaw))
   {
    ACR_TestRegress.Abort("rblane", "no straight road site near Alpha");
    return;
   }
   PrintFormat("[ACR] RBLANE test: site %1 yaw %2, road %3 m wide, %4 m from Alpha", s_vSite, s_fYaw, ACR_Roadblocks.RoadWidthAt(s_vSite), vector.DistanceXZ(s_vSite, alpha.GetAnchor().GetOrigin()));
   CheckLayouts();
   // Alpha at the checkpoint's stage beside the road, then the order (built in seconds).
   vector stage = ACR_Roadblocks.Stage(s_vSite, s_fYaw, ACR_ERoadblockType.MEDIUM, alpha.GetAnchor().GetOrigin());
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (int i, AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    int column = i % 3;
    int row = i / 3;
    if (body) body.SetOrigin(ACR_Manager.Ground(stage + Vector(column, 0, row)));
   }
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (waited < 3) return;
   ACR_Roadblocks.s_fWorkScaleForTest = 0.1;
   manager.m_Bases.GetFaction("US").m_iRP = 500;
   if (!alpha.EstablishRoadblock(s_vSite, s_fYaw, ACR_ERoadblockType.MEDIUM, false))
   {
    ACR_TestRegress.Abort("rblane", "Alpha's roadblock order failed");
    return;
   }
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (!alpha.m_ActiveRoadblock)
   {
    if (waited > 120) ACR_TestRegress.Abort("rblane", "Alpha did not build the checkpoint: " + alpha.m_Order);
    return;
   }
   ACR_Roadblocks.s_fWorkScaleForTest = 1;
   s_Rb = alpha.m_ActiveRoadblock;
   vector along = Vector(Math.Sin(s_fYaw * Math.DEG2RAD), 0, Math.Cos(s_fYaw * Math.DEG2RAD));
   vector start, goal;
   if (!BBB_Terrain.RoadPointNear(s_vSite - along * 70, 30, start) || !BBB_Terrain.RoadPointNear(s_vSite + along * 90, 30, goal))
   {
    ACR_TestRegress.Abort("rblane", "no road either side of the checkpoint");
    return;
   }
   s_vGoal = goal;
   vector side = Vector(along[2], 0, -along[0]);
   manager.SpawnUnit("Lane Truck 1", "", TRUCK, true, "", ACR_Manager.Ground(start), ACR_Manager.Ground(start + side * 8));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (!s_Truck)
   {
    foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Lane Truck 1") s_Truck = squad;
   }
   IEntity crewed = null;
   if (s_Truck) crewed = ACR_Transport.CrewedVehicle(s_Truck);
   if (!crewed || waited < 20)
   {
    if (waited > 60) ACR_TestRegress.Abort("rblane", "the truck has no crew");
    return;
   }
   // The holders have had 20 s to take their posts.
   HoldersHealth(alpha, s_fHealth, s_iAlive);
   array<AIAgent> holders = {};
   alpha.GetAgents(holders);
   foreach (AIAgent holder : holders)
   {
    IEntity body = holder.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body)) continue;
    s_aHolders.Insert(body);
    s_aHealthAt.Insert(BBB.Health(body));
    s_aClosest.Insert(float.MAX);
    s_aGap.Insert(float.MAX);
   }
   Pieces(s_aPieces);
   s_iStepsAt = ACR_RoadblockLane.s_iStepsForTest;
   GetGame().GetCallqueue().CallLater(Sample, 100, true);
   s_Truck.m_iStuckCount = 0;
   s_Truck.MoveTo(s_vGoal);
   PrintFormat("[ACR] RBLANE test: truck at %1 sent to %2 through the checkpoint; holders %3 alive, least health %4, post %5", crewed.GetOrigin(), s_vGoal, s_iAlive, s_fHealth, s_Rb.m_vPost);
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(s_Truck);
   if (vehicle && s_Rb.m_Entity)
   {
    vector local = ACR_RoadblockLane.Local(s_Rb.m_Entity, vehicle.GetOrigin());
    float z = local[2];
    // Within 30 m: one of the holders on foot stands in the middle of the lane, in its way.
    if (!s_Planted && z >= -ACR_Roadblocks.LANE_SLOW_RANGE && z < 0)
    {
     foreach (IEntity candidate : s_aHolders)
     {
      if (!candidate || !ACR_Respawn.IsAlive(candidate) || ACR_Transport.VehicleOf(candidate)) continue;
      s_Planted = candidate;
      candidate.SetOrigin(ACR_RoadblockLane.World(s_Rb.m_Entity, 0, 0));
      PrintFormat("[ACR] RBLANE test: a holder put in the middle of the lane, the truck %1 m off along the road", Math.Round(z));
      break;
     }
    }
    s_fEnterZ = Math.Min(s_fEnterZ, z);
    s_fExitZ = Math.Max(s_fExitZ, z);
    if (Math.AbsFloat(z) < 8)
    {
     s_fMaxX = Math.Max(s_fMaxX, Math.AbsFloat(local[0]));
     Physics phys = vehicle.GetPhysics();
     if (phys) s_fMaxKmh = Math.Max(s_fMaxKmh, phys.GetVelocity().Length() * 3.6);
     s_iSamples++;
     if (HoldersOnLane(alpha) > 0) s_iOnLane++;
    }
    foreach (int hi, IEntity man : s_aHolders)
    {
     if (!man || ACR_Transport.VehicleOf(man)) continue;
     float d = vector.Distance(man.GetOrigin(), vehicle.GetOrigin());
     s_aClosest[hi] = Math.Min(s_aClosest[hi], d);
     s_fNearest = Math.Min(s_fNearest, d);
    }
   }
   bool past = s_fExitZ > 15;
   if (!past && waited < 150 && s_Truck.m_iStuckCount < 3) return;
   GetGame().GetCallqueue().Remove(Sample);

   float health;
   int alive;
   HoldersHealth(alpha, health, alive);
   bool through = past && s_fEnterZ < -15 && s_fMaxX < 5;
   bool slowed = s_fMaxKmh <= ACR_Roadblocks.LANE_SPEED_KMH + 8;
   ACR_TestRegress.Check("rblane: our truck drives through our own medium checkpoint's lane, slowed, never stuck", through && slowed && s_Truck.m_iStuckCount == 0, string.Format("local Z %1 -> %2 in %3 s, at most %4 m off the centre line in the checkpoint, top speed there %5 km/h, stuck %6 times, order '%7'", s_fEnterZ, s_fExitZ, waited, s_fMaxX, s_fMaxKmh, s_Truck.m_iStuckCount, s_Truck.m_Order));
   // Hurt by the truck: dead, or down in health after coming within HULL_GAP of its hull (sampled
   // every 0.1 s). A loss with the hull farther off is something else (the checkpoint's wire, a
   // wound from before): 27 Oct, a man 4.6 m off the hull lost 2%.
   int hurt = 0;
   string hurtText = "";
   foreach (int k, IEntity held : s_aHolders)
   {
    float now = BBB.Health(held);
    hurtText += string.Format(" %1->%2 (truck %3 m, hull %4 m)", Math.Round(s_aHealthAt[k] * 100) / 100, Math.Round(now * 100) / 100, Math.Round(s_aClosest[k] * 10) / 10, Math.Round(s_aGap[k] * 10) / 10);
    if (!ACR_Respawn.IsAlive(held) || (now < s_aHealthAt[k] - 0.01 && s_aGap[k] < HULL_GAP)) hurt++;
   }
   array<vector> after = {};
   Pieces(after);
   int knocked = 0;
   for (int q = 0; q < Math.Min(after.Count(), s_aPieces.Count()); q++) if (vector.Distance(after[q], s_aPieces[q]) > 0.3) knocked++;
   ACR_TestRegress.Check("rblane: none of the checkpoint's men is hurt as it passes", alive == s_iAlive && hurt == 0, string.Format("alive %1 -> %2, hurt by the truck %3; health%4; truck's closest pass to a man on foot %5 m; one on foot in the lane in %6 of %7 samples; %8 of %9 pieces knocked", s_iAlive, alive, hurt, hurtText, Math.Round(s_fNearest * 10) / 10, s_iOnLane, s_iSamples, knocked, after.Count()));
   float least = float.MAX;
   string gaps = "";
   foreach (float gap : s_aGap)
   {
    least = Math.Min(least, gap);
    gaps += string.Format(" %1", Math.Round(gap * 10) / 10);
   }
   int steps = ACR_RoadblockLane.s_iStepsForTest - s_iStepsAt;
   // Out of the lane with the truck abeam (the hull gap below is the safety margin).
   bool stepped = s_Planted && s_fPlantedClear >= 0;
   ACR_TestRegress.Check(string.Format("rblane: a man in the lane steps clear as our truck comes, every man on foot stays %1 m off its hull", HULL_GAP), stepped && least >= HULL_GAP && steps > 0, string.Format("put in the lane %1, %2 m clear of it with the truck abeam; %3 step-clear moves; closest to the hull per man (m):%4", s_Planted != null, Math.Round(s_fPlantedClear * 10) / 10, steps, gaps));
   s_Truck.Halt("Holding");
   ACR_TestRegress.Complete("rblane", CHECKS);
   s_iStep = 9;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // Every roadblock on the site: no solid piece in the lane, the M and L guns beside it and
 // covering it, the post off it with the gun inside its Defend; the vanilla layout for comparison.
 protected static void CheckLayouts()
 {
  ACR_RoadblockLane.s_bOff = true;
  ACR_Roadblock before = ACR_Roadblocks.SpawnRoadblock("US", s_vSite, s_fYaw, "", ACR_ERoadblockType.MEDIUM);
  string beforeWorst;
  float beforeClear = 0;
  if (before) beforeClear = ACR_RoadblockLane.WorstClearance(before.m_Entity, beforeWorst);
  // Playtest 29 Oct: the nest's loose end sandbag stuck out in front of it, and cuttings and the
  // stop sign stood in the middle of the road. As vanilla built it, for comparison:
  vector endBefore;
  string decorBeforeWhat;
  float decorBefore = -1;
  if (before)
  {
   endBefore = ACR_RoadblockLane.FromNest(before.m_Entity, "Sandbag_01_end");
   decorBefore = ACR_RoadblockLane.DecorInnermost(before.m_Entity, decorBeforeWhat);
  }
  Remove(before);
  ACR_RoadblockLane.s_bOff = false;
  bool decorOff = true;
  bool nestWhole = true;
  string decorText = string.Format("vanilla M US: %1 %2 m from the centre line; now:", decorBeforeWhat, Math.Round(decorBefore * 10) / 10);
  string nestText = string.Format("vanilla M US end sandbag from the nest %1;", endBefore);

  bool clear = true;
  bool guns = true;
  bool posts = true;
  string clearText = string.Format("vanilla M US: %1 m (%2); now:", beforeClear, beforeWorst);
  string gunText = "";
  string postText = "";
  array<string> factions = {"US", "USSR", "FIA"};
  array<int> types = {ACR_ERoadblockType.SMALL, ACR_ERoadblockType.MEDIUM, ACR_ERoadblockType.LARGE, ACR_ERoadblockType.BARRICADE};
  foreach (string faction : factions)
  {
   foreach (int type : types)
   {
    if (faction == "FIA" && type != ACR_ERoadblockType.BARRICADE) continue;
    ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock(faction, s_vSite, s_fYaw, "", type);
    if (!rb || !rb.m_Entity)
    {
     clear = false;
     clearText += string.Format(" %1 %2 did not spawn", faction, ACR_Roadblocks.Name(type));
     continue;
    }
    string worst;
    float least = ACR_RoadblockLane.WorstClearance(rb.m_Entity, worst);
    clear = clear && least >= 0;
    clearText += string.Format(" %1 %2 %3 m (%4, %5 moved)", faction, ACR_Roadblocks.Name(type), Math.Round(least * 100) / 100, worst, rb.m_iLaneMoved);
    string decorWhat;
    float decorIn = ACR_RoadblockLane.DecorInnermost(rb.m_Entity, decorWhat);
    decorOff = decorOff && decorIn >= ACR_RoadblockLane.ROAD_ASPHALT;
    if (decorIn < float.MAX) decorText += string.Format(" %1 %2 %3 m (%4);", faction, ACR_Roadblocks.Name(type), Math.Round(decorIn * 10) / 10, decorWhat);
    if (faction == "US" && type == ACR_ERoadblockType.MEDIUM)
    {
     vector endNow = ACR_RoadblockLane.FromNest(rb.m_Entity, "Sandbag_01_end");
     nestWhole = endBefore != vector.Zero && vector.DistanceXZ(endNow, endBefore) < 0.05;
     nestText += string.Format(" now %1 (%2 loose pieces moved with the nest)", endNow, ACR_RoadblockLane.s_iNestFollowers);
    }
    // The post: its Defend at least 2.5 m off the lane, the gun inside it.
    vector post = ACR_RoadblockLane.Local(rb.m_Entity, rb.m_vPost);
    float postClear = ACR_RoadblockLane.Clearance(post[0], post[0], post[2], post[2]);
    bool postOk = postClear >= ACR_RoadblockLane.POST_RADIUS + 2.5;
    if (rb.m_Turret)
    {
     vector root[4];
     rb.m_Entity.GetWorldTransform(root);
     float x0, x1, z0, z1;
     ACR_RoadblockLane.Footprint(rb.m_Turret, root, x0, x1, z0, z1);
     vector gun = ACR_RoadblockLane.Local(rb.m_Entity, rb.m_Turret.GetOrigin());
     float off = Math.AbsFloat(gun[0] - ACR_RoadblockLane.LaneX(gun[2]));
     bool beside = ACR_RoadblockLane.Clearance(x0, x1, z0, z1) >= 0 && off <= COVER;
     guns = guns && beside;
     gunText += string.Format(" %1 %2: %3 m off the line (clear %4 m)", faction, ACR_Roadblocks.Name(type), Math.Round(off * 10) / 10, Math.Round(ACR_RoadblockLane.Clearance(x0, x1, z0, z1) * 100) / 100);
     float reach = vector.DistanceXZ(rb.m_Turret.GetOrigin(), rb.m_vPost);
     postOk = postOk && reach <= ACR_RoadblockLane.POST_RADIUS;
     postText += string.Format(" %1 %2: %3 m off the lane, gun %4 m;", faction, ACR_Roadblocks.Name(type), Math.Round(postClear * 10) / 10, Math.Round(reach * 10) / 10);
    }
    else if (type != ACR_ERoadblockType.BARRICADE) postText += string.Format(" %1 %2: %3 m off the lane;", faction, ACR_Roadblocks.Name(type), Math.Round(postClear * 10) / 10);
    if (type != ACR_ERoadblockType.BARRICADE) posts = posts && postOk;
    if ((type == ACR_ERoadblockType.MEDIUM || type == ACR_ERoadblockType.LARGE) && faction != "FIA" && !rb.m_Turret) guns = false;
    Remove(rb);
   }
  }
  ACR_TestRegress.Check(string.Format("rblane: every roadblock leaves a %1 m chicane lane with no solid piece in it", ACR_RoadblockLane.LANE_HALF_WIDTH * 2), clear && beforeClear < 0, clearText);
  ACR_TestRegress.Check(string.Format("rblane: the M and L checkpoint guns sit beside the lane, within %1 m of its line (an enemy driving it passes under the gun)", COVER), guns && !gunText.IsEmpty(), gunText);
  ACR_TestRegress.Check("rblane: the holders post off the lane, the gun inside their Defend", posts, postText);
  ACR_TestRegress.Check(string.Format("rblane: no cutting or checkpoint sign stands on the road (all %1 m or more from its centre line)", ACR_RoadblockLane.ROAD_ASPHALT), decorOff && decorBefore >= 0 && decorBefore < ACR_RoadblockLane.ROAD_ASPHALT, decorText);
  ACR_TestRegress.Check("rblane: the gun nest's loose end sandbag moves with the nest (M US, as vanilla placed it beside it)", nestWhole, nestText);
 }

 protected static void Remove(ACR_Roadblock rb)
 {
  if (!rb) return;
  if (rb.m_Entity) ACR_Navmesh.DeleteStructure(rb.m_Entity);
  ACR_Defenders.Remove(rb.m_Defenders);
  ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
 }

 // Every 0.1 s while the truck drives: each man on foot's gap to its hull, and the man put in the
 // lane's clearance of it while the truck is abeam of him. A man within 1.5 m of the hull is logged
 // where he stands (the composition's frame). Only while the truck moves.
 protected static void Sample()
 {
  if (!s_Truck || !s_Rb || !s_Rb.m_Entity) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(s_Truck);
  if (!vehicle) return;
  vector truck = ACR_RoadblockLane.Local(s_Rb.m_Entity, vehicle.GetOrigin());
  // (A truck stopped for a man walks past nobody: only a moving one is judged.)
  Physics phys = vehicle.GetPhysics();
  if (!phys || phys.GetVelocity().Length() < 0.5) return;
  foreach (int i, IEntity man : s_aHolders)
  {
   if (!man || !ACR_Respawn.IsAlive(man) || ACR_Transport.VehicleOf(man)) continue;
   float gap = HullGap(vehicle, man.GetOrigin());
   vector l = ACR_RoadblockLane.Local(s_Rb.m_Entity, man.GetOrigin());
   if (gap < 1.5 && gap < s_aGap[i]) PrintFormat("[ACR] RBLANE test: a holder %1 m off the truck's hull, at local X %2 Z %3 (truck X %4 Z %5)", Math.Round(gap * 100) / 100, l[0], l[2], truck[0], truck[2]);
   s_aGap[i] = Math.Min(s_aGap[i], gap);
   if (man == s_Planted && Math.AbsFloat(truck[2] - l[2]) < 5) s_fPlantedClear = Math.Min(s_fPlantedClear, ACR_RoadblockLane.Clearance(l[0] - 0.3, l[0] + 0.3, l[2], l[2]));
  }
 }

 // How far pos is from the vehicle's hull (its bounds, in its own frame; 0 inside them).
 protected static float HullGap(IEntity vehicle, vector pos)
 {
  vector mins, maxs;
  vehicle.GetBounds(mins, maxs);
  vector l = vehicle.CoordToLocal(pos);
  float dx = Math.Max(Math.Max(mins[0] - l[0], l[0] - maxs[0]), 0);
  float dz = Math.Max(Math.Max(mins[2] - l[2], l[2] - maxs[2]), 0);
  return Math.Sqrt(dx * dx + dz * dz);
 }

 // Where the checkpoint's solid pieces stand now.
 protected static void Pieces(notnull array<vector> into)
 {
  into.Clear();
  if (!s_Rb || !s_Rb.m_Entity) return;
  array<IEntity> units = {};
  ACR_RoadblockLane.Units(s_Rb.m_Entity, units);
  foreach (IEntity unit : units) if (unit && !ACR_RoadblockLane.IsDecal(unit)) into.Insert(unit.GetOrigin());
 }

 protected static void HoldersHealth(ACR_Squad alpha, out float least, out int alive)
 {
  least = 1;
  alive = 0;
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   alive++;
   least = Math.Min(least, BBB.Health(body));
  }
 }

 // Holders on foot inside the lane.
 protected static int HoldersOnLane(ACR_Squad alpha)
 {
  if (!s_Rb || !s_Rb.m_Entity) return 0;
  int count = 0;
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body)) continue;
   vector local = ACR_RoadblockLane.Local(s_Rb.m_Entity, body.GetOrigin());
   if (Math.AbsFloat(local[2]) > 12 || ACR_RoadblockLane.Clearance(local[0] - 0.3, local[0] + 0.3, local[2], local[2]) >= 0) continue;
   count++;
   vector post = ACR_RoadblockLane.Local(s_Rb.m_Entity, s_Rb.m_vPost);
   string doing = "";
   SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
   SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
   if (chimera && chimera.m_UtilityComponent && chimera.m_UtilityComponent.GetCurrentBehavior()) doing = chimera.m_UtilityComponent.GetCurrentBehavior().Type().ToString();
   PrintFormat("[ACR] RBLANE test: a holder on foot in the lane at local X %1 Z %2 (post X %3 Z %4), doing %5", local[0], local[2], post[0], post[2], doing);
  }
  return count;
 }

 // How far pos is from the nearest road piece's line.
 protected static float RoadOff(RoadNetworkManager network, vector pos)
 {
  array<BaseRoad> near = {};
  network.GetRoadsInAABB(pos - Vector(8, 1000, 8), pos + Vector(8, 1000, 8), near);
  float best = float.MAX;
  foreach (BaseRoad piece : near)
  {
   if (!piece) continue;
   array<vector> points = {};
   piece.GetPoints(points);
   int segment;
   vector point;
   bool inside;
   best = Math.Min(best, ACR_Roadblocks.NearestOnPiece(points, pos, segment, point, inside));
  }
  return best;
 }

 // The nearest site 60-500 m from 'from' on a road straight for 90 m either way (a truck's run in
 // and out), where a roadblock may go.
 protected static bool FindStraightSite(vector from, out vector site, out float yaw)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  RoadNetworkManager network = aiWorld.GetRoadNetworkManager();
  array<BaseRoad> roads = {};
  network.GetRoadsInAABB(from - Vector(500, 500, 500), from + Vector(500, 500, 500), roads);
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
    if (d < 60 || d >= best) continue;
    vector snapped;
    float heading;
    string why;
    if (!ACR_Roadblocks.ValidateSite(centre, snapped, heading, why, "US")) continue;
    vector dir = Vector(Math.Sin(heading * Math.DEG2RAD), 0, Math.Cos(heading * Math.DEG2RAD));
    bool straight = true;
    for (int k = -3; k <= 3 && straight; k++)
    {
     if (k == 0) continue;
     straight = RoadOff(network, snapped + dir * (k * 30)) <= 3;
    }
    if (!straight) continue;
    best = d;
    site = snapped;
    yaw = heading;
   }
  }
  return best < float.MAX;
 }
}
