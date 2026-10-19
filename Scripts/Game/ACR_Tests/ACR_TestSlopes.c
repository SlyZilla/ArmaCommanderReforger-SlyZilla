// srv:slopes (generated Kolguyev, about 2.5 min): driving on hills (Jacob 19 Oct).
// Finds the steepest straight road stretches and a road cut along a hillside, then
//  G: a gunner-driven Humvee climbs the steepest stretch forwards, then in reverse
//     (Jacob: forwards fails where reverse makes it, so a gear problem);
//  A: an AI-driven M923 climbs the second stretch;
//  K: an AI-driven M923 on the hillside road is sent 150 m back along it, so it must
//     turn round (Jacob: K-turns back off the road down slopes it can't climb).
// Logs SLOPE lines once a second; verdict at the end (gunner-drive checks; the AI
// drivers and steep forwards are printed as SLOPE DIAG measurements).
class ACR_TestSlopes
{
 static const float WINDOW = 60;   // m of road per grade sample
 static const float STEP = 5;
 static const ResourceName HUMVEE = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
 static const ResourceName TRUCK = "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et";

 static ref array<vector> s_Bottom = {};
 static ref array<vector> s_Top = {};
 static ref array<float> s_Grade = {};
 static vector s_KPos, s_KAhead, s_KDest;
 static float s_fKCross;
 static bool s_bK;
 static IEntity s_G, s_A, s_K;
 static ACR_GunnerDrive s_Session;
 static float s_fFwdBest, s_fRevBest, s_fABest;
 static int s_iKReverseSeconds, s_iABackSeconds;
 static float s_fKOffRoad, s_fKBestDest = 10000, s_fKDropMax;
 static int s_iKArrived = -1, s_iAArrived = -1;
 static vector s_SFoot, s_STop;
 static float s_fSGrade;
 static bool s_bS;
 static int s_iShiftStart;
 static ref array<float> s_aGBest = {0, 0, 0, 0};
 static ref array<float> s_aGEnd = {0, 0, 0, 0};
 static ref array<int> s_aGShifts = {0, 0, 0, 0};

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  foreach (ACR_Squad peaceful : manager.m_Squads)
   if (peaceful.m_Group && peaceful.m_Group.GetGroupUtilityComponent()) peaceful.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  if (second == 4) { FindSites(); FindSteep(); }
  if (second == 5 && s_Bottom.Count() >= 2 && s_bS)
  {
   manager.m_sSpawningFaction = "US";
   manager.SpawnUnit("Slope Humvee", "", HUMVEE, true, "", s_Bottom[0] - Dir(0) * 15, s_Bottom[0] - Dir(0) * 15 + Vector(6, 0, 0));
   manager.SpawnUnit("Slope Truck", "", TRUCK, true, "", s_Bottom[1] - Dir(1) * 15, s_Bottom[1] - Dir(1) * 15 + Vector(6, 0, 0));
   if (s_bK) manager.SpawnUnit("Turn Truck", "", TRUCK, true, "", s_KPos, s_KPos + Vector(6, 0, 0));
  }
  if (second == 12)
  {
   s_G = SquadVehicle(manager, "Slope Humvee");
   s_A = SquadVehicle(manager, "Slope Truck");
   s_K = SquadVehicle(manager, "Turn Truck");
   PrintFormat("[ACR] SLOPE vehicles G=%1 A=%2 K=%3", s_G != null, s_A != null, s_K != null);
   if (s_G) Pose(s_G, s_Bottom[0], Dir(0));
   if (s_A) Pose(s_A, s_Bottom[1], Dir(1));
   if (s_K) Pose(s_K, s_KPos, s_KAhead - s_KPos);
  }
  if (second == 15)
  {
   ACR_Squad g = Squad(manager, "Slope Humvee");
   if (s_G && g) s_Session = manager.TestDrive(s_G, ACR_Squad.DriverOf(s_G), g);
   ACR_Squad a = Squad(manager, "Slope Truck");
   if (a) PrintFormat("[ACR] SLOPE A order: %1", a.MoveTo(ACR_Manager.Ground(s_Top[1] + Dir(1) * 30)));
   ACR_Squad k = Squad(manager, "Turn Truck");
   if (k) PrintFormat("[ACR] SLOPE K order: %1 (dest %2 m behind)", k.MoveTo(s_KDest), (int)vector.DistanceXZ(s_KPos, s_KDest));
  }
  // G runs (20 s each, placed at the foot first): 0 = road stretch, old gearbox;
  // 1 = road stretch, climbing fixes; 2 = steep off-road slope forwards; 3 = it in reverse.
  if (s_Session && s_G && s_bS)
  {
   s_Session.m_LastInput = GetGame().GetWorld().GetWorldTime();
   s_Session.m_Steer = 0;
   int run = -1;
   int at;
   for (int r = 0; r < 4; r++) if (second >= 15 + r * 23 && second <= 35 + r * 23) { run = r; at = second - (15 + r * 23); }
   ACR_GunnerDrive.s_bHillStart = run != 0;
   s_Session.m_Throttle = 0;
   if (run >= 0 && at < 20) { s_Session.m_Throttle = 1; if (run == 3) s_Session.m_Throttle = -1; }
   if (second == 36) Pose(s_G, s_Bottom[0], Dir(0));
   if (second == 59) Pose(s_G, s_SFoot, SDir());
   if (second == 82) Pose(s_G, s_SFoot - SDir() * 4, -SDir());
   if (run >= 0 && at == 0) s_iShiftStart = ACR_GunnerDrive.s_iShifts;
   if (run >= 0)
   {
    float p;
    float len;
    if (run < 2) { p = Progress(s_G, 0); len = Length(0); }
    else { p = SProgress(s_G); len = SLength(); }
    s_aGBest[run] = Math.Max(s_aGBest[run], p);
    s_aGShifts[run] = ACR_GunnerDrive.s_iShifts - s_iShiftStart;
    if (at == 20) s_aGEnd[run] = p;
    PrintFormat("[ACR] SLOPE G run=%1 t=%2 thr=%3 progress=%4/%5 m %6", run, second, s_Session.m_Throttle, (int)p, (int)len, State(s_G));
   }
  }
  if (s_A && second >= 15 && second <= 104)
  {
   float pa = Progress(s_A, 1);
   s_fABest = Math.Max(s_fABest, pa);
   if (s_iAArrived < 0 && pa >= Length(1) * 0.9) s_iAArrived = second - 15;
   if (ForwardSpeed(s_A) < -0.5) s_iABackSeconds++;
   if (second % 2 == 0) PrintFormat("[ACR] SLOPE A t=%1 progress=%2/%3 m %4", second, (int)pa, (int)Length(1), State(s_A));
  }
  if (s_K && second >= 15 && second <= 104)
  {
   float off = RoadGap(s_K.GetOrigin());
   s_fKOffRoad = Math.Max(s_fKOffRoad, off);
   float toDest = vector.DistanceXZ(s_K.GetOrigin(), s_KDest);
   s_fKBestDest = Math.Min(s_fKBestDest, toDest);
   if (s_iKArrived < 0 && toDest < 25) s_iKArrived = second - 15;
   if (ForwardSpeed(s_K) < -0.5) s_iKReverseSeconds++;
   s_fKDropMax = Math.Max(s_fKDropMax, s_KPos[1] - s_K.GetOrigin()[1]);
   PrintFormat("[ACR] SLOPE K t=%1 toDest=%2 offRoad=%3 drop=%4 %5", second, (int)toDest, off.ToString(4, 1), (s_KPos[1] - s_K.GetOrigin()[1]).ToString(4, 1), State(s_K));
  }
  if (second == 106 && !s_bS) ACR_TestRegress.Abort("slopes", "no steep off-road slope found");
  if (second == 106)
  {
   PrintFormat("[ACR] SLOPE summary G road %1%% (%2 m): old gearbox %3 m in 20 s (%4 shifts), climbing fixes %5 m (%6 shifts)", (int)(s_Grade[0] * 100), (int)Length(0), (int)s_aGEnd[0], s_aGShifts[0], (int)s_aGEnd[1], s_aGShifts[1]);
   PrintFormat("[ACR] SLOPE summary G off-road %1%% (%2 m): forwards %3 m, reverse %4 m", (int)(s_fSGrade * 100), (int)SLength(), (int)s_aGBest[2], (int)s_aGBest[3]);
   PrintFormat("[ACR] SLOPE summary: road grade %4%%; A %5 of %6 m (arrived %7 s, reversing %8 s); K best %9 m from dest", 0, 0, 0, (int)(s_Grade[0] * 100), (int)s_fABest, (int)Length(1), s_iAArrived, s_iABackSeconds, (int)s_fKBestDest);
   PrintFormat("[ACR] SLOPE summary K: arrived %1 s, reversing %2 s, max off road %3 m, max drop %4 m", s_iKArrived, s_iKReverseSeconds, s_fKOffRoad.ToString(4, 1), s_fKDropMax.ToString(4, 1));
   ACR_TestRegress.Check("slopes: steep road stretches and a hillside road found", s_Bottom.Count() >= 2 && s_Grade[0] >= 0.12 && s_bK, string.Format("%1 stretches, steepest %2", s_Bottom.Count(), s_Grade.Get(0)));
   ACR_TestRegress.Check("slopes: a steep clear off-road slope found", s_bS, string.Format("grade %1", s_fSGrade));
   ACR_TestRegress.Check("slopes: gunner drive climbs the road stretch with the climbing fixes", s_aGBest[1] >= Length(0) * 0.85, string.Format("%1 of %2 m", (int)s_aGBest[1], (int)Length(0)));
   ACR_TestRegress.Check("slopes: climbing fixes no slower than the old gearbox on the road stretch", s_aGEnd[1] >= s_aGEnd[0] - 5, string.Format("old %1 m, fixed %2 m in 20 s", (int)s_aGEnd[0], (int)s_aGEnd[1]));
   ACR_TestRegress.Check("slopes: gunner drive climbs the steep slope in reverse (site check)", s_aGBest[3] >= SLength() * 0.85, string.Format("%1 of %2 m", (int)s_aGBest[3], (int)SLength()));
   // Measurements, not pass/fail yet (19 Oct): forwards on the steep slope is a gearing
   // limit (first gear has about 60 % of reverse's pull); A and K are the game's own driver.
   PrintFormat("[ACR] SLOPE DIAG steep forwards %1 of %2 m; AI truck arrived=%3 (%4 of %5 m, %6 s reversing); turning truck arrived=%7 (%8 s reversing, %9 m off road max)", (int)s_aGBest[2], (int)SLength(), s_iAArrived >= 0, (int)s_fABest, (int)Length(1), s_iABackSeconds, s_iKArrived >= 0, s_iKReverseSeconds, s_fKOffRoad.ToString(4, 1));
   ACR_TestRegress.Complete("slopes", 5);
  }
 }

 static vector Dir(int i)
 {
  vector d = s_Top[i] - s_Bottom[i];
  d[1] = 0;
  return d.Normalized();
 }
 static float Length(int i)
 {
  if (i >= s_Top.Count()) return 1;
  return vector.DistanceXZ(s_Top[i], s_Bottom[i]);
 }
 static float Progress(IEntity v, int i)
 {
  vector rel = v.GetOrigin() - s_Bottom[i];
  rel[1] = 0;
  return vector.Dot(rel, Dir(i));
 }
 static float ForwardSpeed(IEntity v)
 {
  Physics p = v.GetPhysics();
  if (!p) return 0;
  return vector.Dot(p.GetVelocity(), v.GetTransformAxis(2));
 }
 static string State(IEntity v)
 {
  string s = string.Format("speed=%1 pitch=%2", ForwardSpeed(v).ToString(5, 1), (int)v.GetYawPitchRoll()[1]);
  CarControllerComponent car = CarControllerComponent.Cast(v.FindComponent(CarControllerComponent));
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation sim = car.GetSimulation();
   int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
   s += string.Format(" gear=%1 rpm=%2 driven=%3 thr=%4 brk=%5 clutch=%6", sim.GetGear() - neutral, (int)sim.EngineGetRPM(), (int)sim.EngineGetRPMFeedback(), sim.GetThrottle().ToString(4, 2), sim.GetBrake().ToString(4, 2), sim.GetClutch().ToString(4, 2));
  }
  ChimeraCharacter driver = ACR_Squad.DriverOf(v);
  s += string.Format(" driver=%1", driver != null);
  return s;
 }
 static float RoadGap(vector pos)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return -1;
  BaseRoad road;
  float distance = -1;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, distance);
  return distance;
 }
 static ACR_Squad Squad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }
 static IEntity SquadVehicle(ACR_Manager manager, string name)
 {
  ACR_Squad squad = Squad(manager, name);
  if (!squad) return null;
  return ACR_Transport.CrewedVehicle(squad);
 }
 static void Pose(IEntity v, vector pos, vector dir)
 {
  vector mat[4];
  Math3D.AnglesToMatrix(Vector(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 0, 0), mat);
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]) + 1.2;
  mat[3] = pos;
  v.SetWorldTransform(mat);
  Physics p = v.GetPhysics();
  if (p) { p.SetVelocity(vector.Zero); p.SetAngularVelocity(vector.Zero); p.SetActive(ActiveState.ACTIVE); }
  PrintFormat("[ACR] SLOPE placed at <%1 %2> facing %3 (forward axis dot %4)", (int)pos[0], (int)pos[2], (int)(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG), vector.Dot(v.GetTransformAxis(2), dir.Normalized()).ToString(5, 2));
 }

 static vector SDir()
 {
  vector d = s_STop - s_SFoot;
  d[1] = 0;
  return d.Normalized();
 }
 static float SLength() { return Math.Max(1, vector.DistanceXZ(s_STop, s_SFoot)); }
 static float SProgress(IEntity v)
 {
  vector rel = v.GetOrigin() - s_SFoot;
  rel[1] = 0;
  return vector.Dot(rel, SDir());
 }

 // A steady, clear off-road slope near the steepest road stretch: 50 m rising evenly
 // at 35-55 %, no trees over 4 m, no water. The one nearest 42 % is used.
 static void FindSteep()
 {
  if (s_Bottom.IsEmpty()) return;
  int started = System.GetTickCount();
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  float best = 1;
  int found;
  for (float x = mins[0] + 200; x <= maxs[0] - 200; x += 70)
  {
   for (float z = mins[2] + 200; z <= maxs[2] - 200; z += 70)
   {
    vector a = Vector(x, 0, z);
    if (!ACR_Manager.OnMap(a) || BBB_Terrain.IsWater(a[0], a[2])) continue;
    float ha = H(a);
    for (int k = 0; k < 8; k++)
    {
     float ang = k * 45 * Math.DEG2RAD;
     vector d = Vector(Math.Sin(ang), 0, Math.Cos(ang));
     bool even = true;
     float prev = ha;
     for (int m = 1; m <= 5 && even; m++)
     {
      float hm = H(a + d * (m * 10));
      float g = (hm - prev) / 10;
      if (g < 0.22 || g > 0.7) even = false;
      prev = hm;
     }
     if (!even) continue;
     float grade = (prev - ha) / 50;
     if (grade < 0.33 || grade > 0.47) continue;
     vector b = a + d * 50;
     if (BBB_Terrain.IsWater(b[0], b[2])) continue;
     found++;
     if (Math.AbsFloat(grade - 0.40) < Math.AbsFloat(best - 0.40))
     {
      // Nothing solid on the way (the 19 Oct run stopped dead 5 m up against a rock).
      int blocked = 0;
      for (int m2 = -1; m2 <= 6 && blocked == 0; m2++) blocked += Obstacles(a + d * (m2 * 9), 5);
      if (blocked > 0) { s_iBlocked++; continue; }
      best = grade;
      s_SFoot = BBB.Ground(a);
      s_STop = BBB.Ground(b);
      s_fSGrade = grade;
      s_bS = true;
     }
    }
   }
  }
  PrintFormat("[ACR] SLOPE steep off-road: %1 candidates (" + s_iBlocked + " blocked), using <%2 %3> -> <%4 %5> grade %6%% (%7 ms)", found, (int)s_SFoot[0], (int)s_SFoot[2], (int)s_STop[0], (int)s_STop[2], (int)(s_fSGrade * 100), System.GetTickCount() - started);
 }

 protected static int s_iObstacles;
 static int s_iBlocked;
 static int Obstacles(vector centre, float radius)
 {
  s_iObstacles = 0;
  centre[1] = H(centre);
  GetGame().GetWorld().QueryEntitiesBySphere(centre, radius, CountObstacle, null, EQueryEntitiesFlags.STATIC);
  return s_iObstacles;
 }
 protected static bool CountObstacle(IEntity e)
 {
  if (!e || GenericTerrainEntity.Cast(e) || !e.GetPhysics()) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[1] - mins[1] > 0.3) s_iObstacles++;
  return true;
 }

 static float H(vector p) { return GetGame().GetWorld().GetSurfaceY(p[0], p[2]); }

 // Steep straight-ish road stretches (distinct, steepest first) and the road point with
 // the steepest drop to one side (a road cut along a hillside).
 static void FindSites()
 {
  int started = System.GetTickCount();
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) { ACR_TestRegress.Abort("slopes", "no road network"); return; }
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(mins, maxs, roads);
  array<vector> cb = {}, ct = {};
  array<float> cg = {};
  float bestCross = 0;
  int windows, over10, over15, over20;
  int n = Math.Round(WINDOW / STEP);
  foreach (BaseRoad road : roads)
  {
   array<vector> raw = {};
   road.GetPoints(raw);
   array<vector> pts = {};
   foreach (int i, vector point : raw)
   {
    if (i > 0)
    {
     vector from = raw[i - 1];
     int steps = Math.Max(1, Math.Floor(vector.DistanceXZ(from, point) / STEP));
     for (int f = 1; f < steps; f++) pts.Insert(from + (point - from) * (f / (float)steps));
    }
    pts.Insert(point);
   }
   for (int s = 0; s + n < pts.Count(); s += 2)
   {
    vector a = pts[s];
    vector b = pts[s + n];
    float along = 0;
    for (int q = s; q < s + n; q++) along += vector.DistanceXZ(pts[q], pts[q + 1]);
    float straight = vector.DistanceXZ(a, b);
    if (along < WINDOW * 0.7 || straight < along * 0.9) continue;
    if (BBB_Terrain.IsWater(a[0], a[2]) || BBB_Terrain.IsWater(b[0], b[2])) continue;
    float grade = Math.AbsFloat(H(b) - H(a)) / straight;
    windows++;
    if (grade > 0.10) over10++;
    if (grade > 0.15) over15++;
    if (grade > 0.20) over20++;
    if (grade > 0.45) continue; // likely a bridge/tunnel artefact
    vector lo = a, hi = b;
    if (H(b) < H(a)) { lo = b; hi = a; }
    bool near = false;
    foreach (int c, vector other : cb)
    {
     if (vector.DistanceXZ(other, lo) < 300)
     {
      near = true;
      if (grade > cg[c]) { cg[c] = grade; cb[c] = lo; ct[c] = hi; }
      break;
     }
    }
    if (!near) { cb.Insert(lo); ct.Insert(hi); cg.Insert(grade); }
    // Hillside: flat along the road, steep to one side.
    if (grade < 0.06 && s + n + 30 < pts.Count() && s >= 32)
    {
     vector mid = pts[s + n / 2];
     vector dir = b - a;
     dir[1] = 0;
     dir.Normalize();
     vector side = Vector(dir[2], 0, -dir[0]);
     float drop = Math.Max(H(mid) - H(mid + side * 10), H(mid) - H(mid - side * 10));
     if (drop > bestCross && drop < 6 && !BBB_Terrain.IsWater(mid[0], mid[2]))
     {
      bestCross = drop;
      s_KPos = mid;
      s_KAhead = mid + dir * 20;
      s_KDest = BBB.Ground(pts[s + n / 2 - 30]);
      s_bK = true;
     }
    }
   }
  }
  // Steepest first (selection sort; a few hundred entries at most).
  for (int i = 0; i < cg.Count(); i++)
  {
   int best = i;
   for (int j = i + 1; j < cg.Count(); j++) if (cg[j] > cg[best]) best = j;
   if (best != i) { cg.SwapItems(i, best); cb.SwapItems(i, best); ct.SwapItems(i, best); }
  }
  for (int i = 0; i < Math.Min(8, cg.Count()); i++)
  {
   s_Bottom.Insert(BBB.Ground(cb[i]));
   s_Top.Insert(BBB.Ground(ct[i]));
   s_Grade.Insert(cg[i]);
   PrintFormat("[ACR] SLOPE stretch %1: <%2 %3> -> <%4 %5> grade %6%% rise %7 m", i, (int)cb[i][0], (int)cb[i][2], (int)ct[i][0], (int)ct[i][2], (int)(cg[i] * 100), (int)(H(ct[i]) - H(cb[i])));
  }
  PrintFormat("[ACR] SLOPE roads %1, windows %2: over 10%% %3, over 15%% %4, over 20%% %5; hillside drop %6 m over 10 m at <%7 %8> (%9 ms)", roads.Count(), windows, over10, over15, over20, bestCross.ToString(4, 1), (int)s_KPos[0], (int)s_KPos[2], System.GetTickCount() - started);
 }
}
