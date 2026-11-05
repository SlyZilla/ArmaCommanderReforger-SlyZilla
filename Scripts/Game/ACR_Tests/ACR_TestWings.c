// -acrWingsTest 1 (server-capable, ~2 min; any map): the wings of Fortify positions. Positions dug in round the
// map's bases: most have a wing, the shapes vary, no piece stands on a road or in a building, each wing starts at
// its post and hangs together. Upgrades still go in and a gun's bags make way. The wings go when the position
// does, the off switch works and the same seed gives the same wing. Checkpoints on the roads between bases:
// wings stay out of the lane and off the road, and go when it's dismantled.
// A position's reserve goes behind them, unless told not to.
// -acrWingsOffTest 1: the same with wings off, for the upgrade figures.
// -acrWingShotsTest 1 (a game client): photographs of some of them, each shape. Shots: $profile:acr-shots/wing_*.
class ACR_TestWings
{
 static const int CHECKS = 20;
 static const int MOST_SITES = 40;
 static const int UPGRADES = 14;
 static const float STEP_S = 4;
 protected static ref array<ACR_Fortification> s_aForts;
 protected static ref array<ACR_Roadblock> s_aBlocks;
 protected static int s_iFailedAt;
 protected static int s_iStage;
 protected static ref ACR_Fortification s_Gone;
 protected static ref array<IEntity> s_aGoneWings;
 protected static ref array<vector> s_aEyes;
 protected static ref array<vector> s_aAngles;
 protected static ref array<string> s_aNames;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aForts = {};
  s_aBlocks = {};
  s_aGoneWings = {};
  s_aEyes = {};
  s_aAngles = {};
  s_aNames = {};
 }
 protected static CameraBase s_Camera;
 protected static int s_iNext;
 protected static int s_iAt;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  bool shots = System.IsCLIParam("acrWingShotsTest");
  if (System.IsCLIParam("acrWingsOffTest")) ACR_Options.Set("acrWings", "0");
  if (second == 8) Dig();
  if (second == 10) Blocks();
  if (shots)
  {
   if (second == 12) ShotList();
   if (second >= 12 && !s_aNames.IsEmpty()) Shots(second);
   return;
  }
  if (second == 11) Look();
  if (second == 12) Upgrades();
  if (second == 13) LookAtBlocks();
  if (second == 14) StartGone();
  if (second == 15 && !System.IsCLIParam("acrWingsOffTest")) WingMen();
  if (second == 26) Finish();
 }

 // A reserve sent out for a position with the longest wings.
 protected static void WingMen()
 {
  ACR_Fortification best;
  int most = 0;
  foreach (ACR_Fortification fort : s_aForts)
  {
   if (!fort || fort.m_aWings.Count() <= most) continue;
   most = fort.m_aWings.Count();
   best = fort;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  bool manned = false;
  bool off = false;
  if (best)
  {
   ACR_DefenderPost post = ACR_Defenders.Add(best.m_sFaction, best.m_vPos, "wings test", 30);
   post.m_Fort = best;
   post.m_Group = manager.m_Bases.SpawnGuards(best.m_sFaction, best.m_vPos, 4, "wings test", 20);
   manned = ACR_Wings.ManWings(post, 4);
   ACR_Options.Set("acrWingMen", "0");
   off = ACR_Wings.ManWings(post, 4);
   ACR_Options.Unset("acrWingMen");
   ACR_Defenders.Remove(post);
  }
  ACR_TestRegress.Check("wings: a position's reserve goes behind them", manned, string.Format("%1 pieces", most));
  ACR_TestRegress.Check("wings: -acrWingMen 0 leaves them round the position", !off, "");
 }

 protected static void Dig()
 {
  string side = ACR_FactionSelection.West();
  array<float> rings = {150, 200, 110, 70};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (s_aForts.Count() >= MOST_SITES) break;
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt) continue;
   RandomGenerator rng = ACR_Variety.For("wings test " + base.m_sName);
   float turn = rng.RandFloat01() * 360;
   int made;
   foreach (float ring : rings)
   {
    for (int b = 0; b < 3 && made < 2; b++)
    {
     float bearing = turn + b * 120;
     vector point = base.GetOrigin() + ACR_BaseSurvey.Dir(bearing) * ring;
     point[1] = ACR_BaseLiner.Ground(point);
     if (ACR_Fortify.NearestOwn(side, point, 55) || BBB_Terrain.IsWaterAt(point)) continue;
     string why;
     if (!ACR_Fortify.CheckTier(point, bearing, 1, null, why)) continue;
     ACR_FortifyPlan plan = new ACR_FortifyPlan();
     plan.m_sFaction = side;
     plan.m_vSite = point;
     plan.m_fYaw = bearing;
     plan.m_iTier = 1;
     ACR_Fortification fort = ACR_Fortify.Build(plan, why);
     if (!fort) continue;
     s_aForts.Insert(fort);
     made++;
    }
   }
  }
  ACR_TestRegress.Check("wings: positions dug in", s_aForts.Count() >= 15, string.Format("%1", s_aForts.Count()));
 }

 // Checkpoints a side would put on the roads between its bases.
 protected static void Blocks()
 {
  string side = ACR_FactionSelection.West();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (s_aBlocks.Count() >= 12) break;
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt) continue;
   ACR_Base other;
   float least = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!candidate || candidate == base || candidate.m_bRetired || candidate.m_bHidden) continue;
    float apart = vector.DistanceXZ(candidate.GetOrigin(), base.GetOrigin());
    if (apart >= least) continue;
    least = apart;
    other = candidate;
   }
   if (!other) continue;
   int considered;
   string none;
   ACR_RoadSite site = ACR_RoadblockSiting.Best(base.GetOrigin(), other.GetOrigin(), side, false, considered, none);
   if (!site) continue;
   ACR_Roadblock block = ACR_Roadblocks.SpawnRoadblock(side, site.m_vPos, site.m_fYaw);
   if (!block) continue;
   ACR_Wings.ForRoadblock(block);
   s_aBlocks.Insert(block);
  }
 }

 protected static void LookAtBlocks()
 {
  if (!ACR_Wings.On()) return;
  int withWing, pieces, inLane, onRoad;
  foreach (ACR_Roadblock block : s_aBlocks)
  {
   if (!block.m_aWingPlans.IsEmpty()) withWing++;
   ACR_BaseSurvey survey = ACR_BaseSurvey.TakeAround(block.m_vPos, ACR_Wings.RADIUS);
   vector root[4];
   block.m_Entity.GetWorldTransform(root);
   foreach (ACR_WingPlan wing : block.m_aWingPlans)
   {
    foreach (ACR_LinePiece piece : wing.m_aPieces)
    {
     pieces++;
     vector at = piece.m_vPos - root[3];
     float x = vector.Dot(at, root[0]);
     float z = vector.Dot(at, root[2]);
     if (Math.AbsFloat(z) < 12 && Math.AbsFloat(x - ACR_RoadblockLane.LaneX(z)) < ACR_RoadblockLane.LANE_HALF_WIDTH + 1) inLane++;
     if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, piece.m_vPos) < 0.2) onRoad++;
    }
   }
  }
  PrintFormat("[ACR] TEST wings checkpoints: %1, %2 with a wing, %3 pieces", s_aBlocks.Count(), withWing, pieces);
  ACR_TestRegress.Check("wings: checkpoints built", s_aBlocks.Count() >= 3, string.Format("%1", s_aBlocks.Count()));
  ACR_TestRegress.Check("wings: most checkpoints have one", withWing * 2 >= s_aBlocks.Count(), string.Format("%1 of %2", withWing, s_aBlocks.Count()));
  ACR_TestRegress.Check("wings: the lane stays clear", inLane == 0, string.Format("%1 of %2", inLane, pieces));
  ACR_TestRegress.Check("wings: none on a checkpoint's road", onRoad == 0, string.Format("%1 of %2", onRoad, pieces));
  // Dismantled, it takes its wings along.
  bool gone = true;
  foreach (ACR_Roadblock withWings : s_aBlocks)
  {
   if (withWings.m_aWings.IsEmpty()) continue;
   ACR_Roadblocks.DismantleRoadblock(withWings);
   gone = withWings.m_aWings.IsEmpty();
   break;
  }
  ACR_TestRegress.Check("wings: they go when it's dismantled", gone, "");
 }

 protected static void Look()
 {
  int withWing, whole, ties, refused, runs, pieces, onRoad, inHouse, loose, farStart, worstMs;
  array<int> lengths = {};
  foreach (ACR_Fortification fort : s_aForts)
  {
   // Only those whose end posts both went in (the first places may be by a road, or in a yard).
   if (fort.m_aPieces[0] && fort.m_aPieces[1])
   {
    whole++;
    if (!fort.m_aWingPlans.IsEmpty()) withWing++;
   }
   ACR_BaseSurvey survey = ACR_BaseSurvey.TakeAround(fort.m_vPos, ACR_Wings.RADIUS);
   foreach (ACR_WingPlan wing : fort.m_aWingPlans)
   {
    if (wing.m_iShape == ACR_Wings.TIE) ties++;
    if (wing.m_iShape == ACR_Wings.REFUSE) refused++;
    if (wing.m_iShape == ACR_Wings.RUN_ON) runs++;
    int count = wing.m_aPieces.Count();
    if (!lengths.Contains(count)) lengths.Insert(count);
    foreach (int i, ACR_LinePiece piece : wing.m_aPieces)
    {
     pieces++;
     if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, piece.m_vPos) < 0.2)
     {
      onRoad++;
      PrintFormat("[ACR] TEST wings at %1: piece at %2 on a road", fort.m_vPos, piece.m_vPos);
     }
     if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, piece.m_vPos, 1) < 0.2)
     {
      inHouse++;
      PrintFormat("[ACR] TEST wings at %1: piece at %2 in a building", fort.m_vPos, piece.m_vPos);
     }
     // Joined to the piece before it, but for the gap for men on foot.
     if (i > 0)
     {
      float gap = vector.DistanceXZ(wing.m_aPieces[i - 1].m_vTo, piece.m_vFrom);
      if (gap > ACR_BaseLines.LONG_STEP + 0.5)
      {
       loose++;
       PrintFormat("[ACR] TEST wings at %1: piece %2 of %3, %4 m after the one before", fort.m_vPos, i, count, gap);
      }
     }
    }
    // Hard by its post.
    if (count > 0)
    {
     float best = 1000;
     for (int spot = 0; spot < 3; spot++)
     {
      if (!fort.m_aPieces[spot]) continue;
      for (int way = -1; way <= 1; way += 2)
      {
       vector tip, heading;
       if (ACR_Wings.EndOf(fort.m_aPieces[spot], way, tip, heading)) best = Math.Min(best, vector.DistanceXZ(tip, wing.m_aPieces[0].m_vFrom));
      }
     }
     if (best > 6.5) farStart++;
    }
   }
  }
  PrintFormat("[ACR] TEST wings: %1 positions, %2 with both end posts, %3 of those with a wing; %4 tie in, %5 refused, %6 run on; %7 pieces; lengths %8", s_aForts.Count(), whole, withWing, ties, refused, runs, pieces, lengths.Count());
  if (!ACR_Wings.On())
  {
   ACR_TestRegress.Check("wings: none with the switch off", withWing == 0 && pieces == 0, string.Format("%1", pieces));
   return;
  }
  ACR_TestRegress.Check("wings: most positions have a wing", withWing * 10 >= whole * 7, string.Format("%1 of %2", withWing, whole));
  ACR_TestRegress.Check("wings: the shapes vary", ties > 0 && refused > 0 && runs > 0 && lengths.Count() >= 3, string.Format("%1 tie in, %2 refused, %3 run on, %4 lengths", ties, refused, runs, lengths.Count()));
  ACR_TestRegress.Check("wings: none on a road", onRoad == 0, string.Format("%1 of %2", onRoad, pieces));
  ACR_TestRegress.Check("wings: none in a building", inHouse == 0, string.Format("%1 of %2", inHouse, pieces));
  ACR_TestRegress.Check("wings: they hang together", loose == 0, string.Format("%1", loose));
  ACR_TestRegress.Check("wings: each starts at its post", farStart == 0, string.Format("%1", farStart));
  // The same seed and the same place: the same wing.
  ACR_Fortification first = s_aForts[0];
  vector tipAgain, headingAgain;
  ACR_BaseSurvey again = ACR_BaseSurvey.TakeAround(first.m_vPos, ACR_Wings.RADIUS);
  bool same = true;
  if (first.m_aPieces[1] && ACR_Wings.EndOf(first.m_aPieces[1], 1, tipAgain, headingAgain))
  {
   ACR_WingPlan one = ACR_Wings.Plan(again, first.m_sFaction, tipAgain + Vector(0, 0, 200), headingAgain, 1, "wings test same");
   ACR_WingPlan two = ACR_Wings.Plan(again, first.m_sFaction, tipAgain + Vector(0, 0, 200), headingAgain, 1, "wings test same");
   same = one.m_iShape == two.m_iShape && one.m_aPieces.Count() == two.m_aPieces.Count();
  }
  ACR_TestRegress.Check("wings: the same seed gives the same wing", same, "");
 }

 protected static void Upgrades()
 {
  int tried, refused, cutGuns, close, overBags;
  string reasons;
  foreach (int f, ACR_Fortification fort : s_aForts)
  {
   if (f >= UPGRADES) break;
   for (int tier = 2; tier <= 3; tier++)
   {
    ACR_FortifyPlan plan = new ACR_FortifyPlan();
    plan.m_sFaction = fort.m_sFaction;
    plan.m_vSite = fort.m_vPos;
    plan.m_fYaw = fort.m_fYaw;
    plan.m_iTier = tier;
    plan.m_Target = fort;
    tried++;
    string why;
    int before = fort.m_aWings.Count();
    if (!ACR_Fortify.Build(plan, why))
    {
     refused++;
     reasons += why + "; ";
     if (why.Contains("Sandbag")) overBags++;
     break;
    }
    if (fort.m_aWings.Count() < before) cutGuns++;
   }
   IEntity gun = fort.m_aPieces[3];
   if (!gun) continue;
   foreach (IEntity piece : fort.m_aWings)
    if (piece && vector.DistanceXZ(piece.GetOrigin(), gun.GetOrigin()) < 1.5) close++;
  }
  PrintFormat("[ACR] TEST wings upgrades: %1 tried, %2 refused (%3), %4 made bags go", tried, refused, reasons, cutGuns);
  if (!ACR_Wings.On()) return;
  // (These positions stand wherever a ring round a base falls: about a third are refused with wings off too.)
  ACR_TestRegress.Check("wings: no upgrade refused over its bags", overBags == 0 && refused * 2 <= tried, string.Format("%1 of %2 refused, %3 over sandbags", refused, tried, overBags));
  ACR_TestRegress.Check("wings: a gun's bags make way", close == 0, string.Format("%1 pieces stand on a gun", close));
 }

 protected static void StartGone()
 {
  foreach (ACR_Fortification fort : s_aForts)
  {
   if (fort.m_aWings.IsEmpty()) continue;
   s_Gone = fort;
   break;
  }
  if (!s_Gone) return;
  s_aGoneWings.Copy(s_Gone.m_aWings);
  foreach (IEntity piece : s_Gone.m_aPieces) if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
 }

 protected static void Finish()
 {
  if (!ACR_Wings.On())
  {
   ACR_TestRegress.Complete("wings", 2);
   return;
  }
  int left;
  foreach (IEntity piece : s_aGoneWings) if (piece && !piece.IsDeleted()) left++;
  ACR_TestRegress.Check("wings: they go with the position", s_Gone && left == 0 && !ACR_Fortify.s_Positions.Contains(s_Gone), string.Format("%1 of %2 left", left, s_aGoneWings.Count()));
  // A position dug in with the switch off has none.
  ACR_Options.Set("acrWings", "0");
  bool none = true;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden) continue;
   vector point = base.GetOrigin() + Vector(0, 0, 150);
   point[1] = ACR_BaseLiner.Ground(point);
   string why;
   if (ACR_Fortify.NearestOwn(ACR_FactionSelection.West(), point, 55) || !ACR_Fortify.CheckTier(point, 0, 1, null, why)) continue;
   ACR_FortifyPlan plan = new ACR_FortifyPlan();
   plan.m_sFaction = ACR_FactionSelection.West();
   plan.m_vSite = point;
   plan.m_iTier = 1;
   ACR_Fortification fort = ACR_Fortify.Build(plan, why);
   if (fort && !fort.m_aWings.IsEmpty()) none = false;
   break;
  }
  ACR_Options.Unset("acrWings");
  ACR_TestRegress.Check("wings: the switch turns them off", none, "");
  ACR_TestRegress.Check("wings: planned quickly", ACR_Wings.s_iWorstMs < 400, string.Format("worst %1 ms", ACR_Wings.s_iWorstMs));
  ACR_TestRegress.Complete("wings", CHECKS);
 }

 // ---------------------------------------------------------------- photographs

 protected static void ShotList()
 {
  array<int> taken = {0, 0, 0};
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  TimeAndWeatherManagerEntity time;
  if (world) time = world.GetTimeAndWeatherManager();
  if (time) time.SetHoursMinutesSeconds(11, 0, 0, true);
  foreach (int f, ACR_Fortification fort : s_aForts)
  {
   string words;
   bool wanted;
   foreach (ACR_WingPlan wing : fort.m_aWingPlans)
   {
    words += string.Format("%1 %2 %3, ", ACR_Wings.ShapeWord(wing.m_iShape), wing.m_aPieces.Count(), wing.m_sEnd);
    if (wing.m_iShape >= 0 && taken[wing.m_iShape] < 4) wanted = true;
   }
   if (!wanted) continue;
   foreach (ACR_WingPlan counted : fort.m_aWingPlans) if (counted.m_iShape >= 0) taken[counted.m_iShape] = taken[counted.m_iShape] + 1;
   string name = string.Format("wing_%1", f);
   vector forward = ACR_BaseSurvey.Dir(fort.m_fYaw);
   vector eye = fort.m_vPos + forward * 34 + Vector(0, 15, 0);
   AddShot(eye, Aim(eye, fort.m_vPos), name + "_front", words);
   vector over = fort.m_vPos + Vector(0, 45, 0);
   AddShot(over, Vector(fort.m_fYaw, -88, 0), name + "_above", words);
  }
  foreach (int r, ACR_Roadblock block : s_aBlocks)
  {
   if (r >= 6) break;
   string blockWords;
   foreach (ACR_WingPlan blockWing : block.m_aWingPlans) blockWords += string.Format("%1 %2 %3, ", ACR_Wings.ShapeWord(blockWing.m_iShape), blockWing.m_aPieces.Count(), blockWing.m_sEnd);
   vector ahead = ACR_BaseSurvey.Dir(block.m_fYaw);
   vector blockEye = block.m_vPos + ahead * 30 + Vector(0, 14, 0);
   AddShot(blockEye, Aim(blockEye, block.m_vPos), string.Format("wing_block%1_front", r), blockWords);
   AddShot(block.m_vPos + Vector(0, 45, 0), Vector(block.m_fYaw, -88, 0), string.Format("wing_block%1_above", r), blockWords);
  }
  ACR_TestRegress.Check("wings: shots to take", !s_aNames.IsEmpty(), string.Format("%1", s_aNames.Count()));
  Camera();
 }

 protected static vector Aim(vector eye, vector target)
 {
  vector to = target + Vector(0, 1, 0) - eye;
  float flat = Math.Sqrt(to[0] * to[0] + to[2] * to[2]);
  return Vector(ACR_BaseDefences.Bearing(to), Math.Atan2(to[1], Math.Max(flat, 0.1)) * Math.RAD2DEG, 0);
 }

 protected static void AddShot(vector eye, vector angles, string name, string words)
 {
  float ground = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]);
  if (eye[1] < ground + 2) eye[1] = ground + 2;
  s_aEyes.Insert(eye);
  s_aAngles.Insert(angles);
  s_aNames.Insert(name);
  PrintFormat("[ACR] WINGSHOTS plan %1 from %2: %3", name, eye, words);
 }

 protected static void Camera()
 {
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixIdentity4(params.Transform);
  params.Transform[3] = s_aEyes[0];
  Resource prefab = Resource.Load("{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et");
  if (prefab && prefab.IsValid()) s_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), params));
  if (!s_Camera) s_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), params));
  if (!s_Camera)
  {
   ACR_TestRegress.Abort("wingshots", "no camera");
   return;
  }
  SCR_ManualCamera manual = SCR_ManualCamera.Cast(s_Camera);
  if (manual)
  {
   manual.SetInputEnabled(false);
   manual.ClearEventMask(EntityEvent.POSTFRAME);
  }
  s_Camera.SetVerticalFOV(70);
  s_Camera.SetNearPlane(0.3);
  s_Camera.SetFarPlane(6000);
  GetGame().GetCameraManager().SetCamera(s_Camera);
  s_iNext = 0;
  s_iAt = 0;
  AimAt(0);
 }

 protected static void AimAt(int index)
 {
  s_Camera.SetOrigin(s_aEyes[index]);
  s_Camera.SetYawPitchRoll(s_aAngles[index]);
  s_Camera.Update();
  GetGame().GetCameraManager().SetCamera(s_Camera);
 }

 protected static void Shots(int second)
 {
  if (!s_Camera) return;
  MenuManager menus = GetGame().GetMenuManager();
  if (menus) menus.CloseAllMenus();
  int step = (second - 12) / STEP_S;
  if (step == s_iAt || s_iNext >= s_aNames.Count()) return;
  s_iAt = step;
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/" + s_aNames[s_iNext]);
  s_iNext++;
  if (s_iNext < s_aNames.Count())
  {
   AimAt(s_iNext);
   return;
  }
  ACR_TestRegress.Complete("wingshots", 2);
 }
}
