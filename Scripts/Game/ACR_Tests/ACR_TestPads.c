// -acrPadsTest 1 (server, Arland, ~1 min): survey for helipads. Lists helipad objects already
// in the world near each base and the level, open spots (room for a Mi-8) in and around them.
// Every placed pad must have nothing standing within 12 m (2 m grid of downward traces; the
// helipad models themselves ignored).
// Then each side buys both support helicopters for its HQ's pads: they must sit upright and unhurt.
// Then each side calls its door-gunner helicopter on Military Hill: it must lift off its pad
// (above 25 m) unhurt, which catches pads hemmed in by roofs, wires or trees (~75 s in all).
class ACR_TestPads
{
 static ref array<IEntity> s_Found = {};
 static bool Collect(IEntity e)
 {
  if (!e || !e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  if (name.Contains("elipad") && !s_Found.Contains(e)) s_Found.Insert(e);
  return true;
 }

 // Level within 1 m over a 9 m radius, nothing within 11 m up to 8 m high, not on an object, dry.
 static bool PadSpot(vector at, out float spread)
 {
  at = ACR_Manager.Ground(at);
  if (BBB_Terrain.IsWaterAt(at)) return false;
  BaseWorld world = GetGame().GetWorld();
  float low = float.MAX, high = -float.MAX;
  for (int r = 0; r <= 9; r += 3)
   for (int a = 0; a < 12; a++)
   {
    float y = world.GetSurfaceY(at[0] + Math.Cos(a * 0.5236) * r, at[2] + Math.Sin(a * 0.5236) * r);
    low = Math.Min(low, y);
    high = Math.Max(high, y);
   }
  spread = high - low;
  if (spread > 1.0) return false;
  vector free;
  if (!SCR_WorldTools.FindEmptyTerrainPosition(free, at, 0.3, 11, 8)) return false;
  if (vector.DistanceXZ(free, at) > 1) return false;
  return !ACR_Mortars.OnObject(at);
 }

 static void Survey(ACR_Base base, float reach, int maxPrint)
 {
  vector c = base.GetOrigin();
  int printed;
  for (float d = 0; d <= reach && printed < maxPrint; d += 6)
  {
   int steps = Math.Max(1, (int)(d / 6) * 6);
   for (int s = 0; s < steps && printed < maxPrint; s++)
   {
    float ang = s * Math.PI2 / steps;
    vector p = c + Vector(Math.Cos(ang) * d, 0, Math.Sin(ang) * d);
    float spread;
    if (!PadSpot(p, spread)) continue;
    // skip spots within 25 m of one already printed
    bool near;
    foreach (vector q : s_Spots) if (vector.DistanceXZ(q, p) < 25) near = true;
    if (near) continue;
    s_Spots.Insert(p);
    printed++;
    PrintFormat("[ACR] PADS %1: open spot %2 m from the centre at <%3, %4> (height spread %5 m)", base.m_sName, (int)d, (int)p[0], (int)p[2], spread);
   }
  }
  if (printed == 0) PrintFormat("[ACR] PADS %1: no open spot within %2 m", base.m_sName, reach);
 }
 static ref array<vector> s_Spots = {};

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 6) CheckClear();
  if (tick == 8) BuyAll();
  if (tick == 25) { CheckParked(); CallBoth(); }
  if (tick > 25 && tick < 65) WatchLift();
  if (tick == 65) { CheckLift(); Print("[ACR] TEST COMPLETE"); }
  if (tick != 5) return;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   s_Found.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(base.GetOrigin(), base.m_fRadius + 150, Collect, null, EQueryEntitiesFlags.STATIC);
   foreach (IEntity pad : s_Found)
    PrintFormat("[ACR] PADS %1 (owner %2, radius %3): helipad object %4 at %5, %6 m from the centre", base.m_sName, base.m_sOwner, base.m_fRadius, FilePath.StripPath(pad.GetPrefabData().GetPrefabName()), pad.GetOrigin(), (int)vector.DistanceXZ(pad.GetOrigin(), base.GetOrigin()));
   float reach = base.m_fRadius + 30;
   if (base.m_sName == "Heide" || base.m_sName == "Airfield") reach = 500;
   int count = 3;
   if (base.m_sName == "Heide") count = 10;
   s_Spots.Clear();
   Survey(base, reach, count);
  }
 }

 static bool NotPad(notnull IEntity e)
 {
  if (!e.GetPrefabData()) return true;
  return !e.GetPrefabData().GetPrefabName().Contains("elipad");
 }

 // 2 m grid of downward traces within 12 m: cells where anything but a helipad stands
 // 0.4 m or more over the ground (buildings, walls, trees, wires).
 static int BlockedCells(vector c, out string what)
 {
  BaseWorld world = GetGame().GetWorld();
  int blocked;
  what = "";
  for (float dx = -12; dx <= 12; dx += 2)
   for (float dz = -12; dz <= 12; dz += 2)
   {
    if (dx * dx + dz * dz > 144) continue;
    float x = c[0] + dx, z = c[2] + dz;
    float ty = world.GetSurfaceY(x, z);
    TraceParam trace = new TraceParam();
    trace.Start = Vector(x, ty + 40, z);
    trace.End = Vector(x, ty - 0.5, z);
    trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
    float fraction = world.TraceMove(trace, NotPad);
    if (fraction >= 1 || 40 - 40.5 * fraction < 0.4) continue;
    blocked++;
    if (trace.TraceEnt && trace.TraceEnt.GetPrefabData() && what.Length() < 200) what += " " + FilePath.StripPath(trace.TraceEnt.GetPrefabData().GetPrefabName());
   }
  return blocked;
 }

 // Everon skirmish HQs without pads (5 Oct): open, level spots clear of everything within 12 m,
 // nearest the centre first, 30 m apart (-acrEveronPadsTest, from ACR_Mode).
 static void SurveyEveron(int tick)
 {
  if (tick != 5) return;
  array<string> names = {"Chotain Base", "Power Plant", "Gravette Base", "Central Industrial", "Regina Sawmill"};
  foreach (string name : names)
  {
   ACR_Base base = ACR_Skirmish.FindBase(name);
   if (!base) { PrintFormat("[ACR] PADS %1: no such base", name); continue; }
   vector c = base.GetOrigin();
   array<vector> found = {};
   for (float d = 0; d <= base.m_fRadius + 150 && found.Count() < 6; d += 5)
   {
    int steps = Math.Max(1, (int)(d / 5) * 6);
    for (int s = 0; s < steps && found.Count() < 6; s++)
    {
     float ang = s * Math.PI2 / steps;
     vector p = ACR_Manager.Ground(c + Vector(Math.Cos(ang) * d, 0, Math.Sin(ang) * d));
     bool near;
     foreach (vector q : found) if (vector.DistanceXZ(q, p) < 30) near = true;
     if (near) continue;
     float spread;
     if (!PadSpot(p, spread)) continue;
     string what;
     if (BlockedCells(p, what) > 0) continue;
     found.Insert(p);
     PrintFormat("[ACR] PADSPOT %1 | %2 %3 %4 | %5 m from centre, spread %6", name, (int)p[0], p[1], (int)p[2], (int)d, spread);
    }
   }
   if (found.IsEmpty()) PrintFormat("[ACR] PADSPOT %1 | none within %2 m", name, base.m_fRadius + 150);
  }
  Print("[ACR] TEST COMPLETE");
 }

 static void CheckClear()
 {
  foreach (ACR_Helipad pad : ACR_Helipad.s_All)
  {
   if (pad.m_bGone) continue;
   vector c = pad.GetOrigin();
   string what;
   int blocked = BlockedCells(c, what);
   string baseName = "none";
   if (pad.Base()) baseName = pad.Base().m_sName;
   PrintFormat("[ACR] PADS pad at %1 yaw %2 base %3: %4 blocked cells%5", c, pad.Yaw(), baseName, blocked, what);
   ACR_TestRegress.Check("pads: pad at " + c.ToString() + " (" + baseName + ") clear within 12 m", blocked == 0, what);
  }
 }

 static void BuyAll()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  array<string> sides = {"US", "USSR"};
  foreach (string side : sides)
  {
   ACR_FactionSettings settings = ACR_FactionSettings.Find(side);
   if (settings) settings.m_bAICommander = false;
   ACR_FactionData data = manager.m_Bases.GetFaction(side);
   data.m_iRP = 1000;
   foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
   {
    if (entry.m_iSupportHeli == 0) continue;
    ACR_Helipad pad = manager.m_Supports.Station().FreePad(side, null);
    ACR_Base base;
    if (pad) base = pad.Base();
    PrintFormat("[ACR] PADS %1 buys %2: %3", side, entry.m_sName, manager.PurchaseAt(side, i, base));
   }
  }
 }

 static ref map<string, float> s_Lift = new map<string, float>();
 static ref map<string, float> s_Health = new map<string, float>();

 static void CallBoth()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  vector target;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Military Hill") target = base.GetOrigin();
  // Skirmish runs (Everon): halfway between the two HQs.
  if (target == vector.Zero)
  {
   int hqCount;
   foreach (ACR_Base hq : ACR_Base.s_All) if (hq.m_sOwner != "") { target = target + hq.GetOrigin(); hqCount++; }
   if (hqCount > 0) target = target * (1.0 / hqCount);
  }
  array<string> sides = {"US", "USSR"};
  foreach (string side : sides)
  {
   int index = -1;
   foreach (int i, ACR_SupportEntry entry : manager.m_Bases.GetFaction(side).m_aSupports)
    if (entry.m_eType == ACR_ESupportType.HELI_DOOR_GUNS) index = i;
   manager.m_Bases.GetFaction(side).m_iRP = 1000;
   string refused = manager.m_Supports.Call(side, index, ACR_Manager.Ground(target), true);
   PrintFormat("[ACR] PADS %1 calls door gunners on Military Hill: %2", side, refused);
   s_Lift.Set(side, 0);
   s_Health.Set(side, 1);
  }
 }

 static void WatchLift()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  foreach (ACR_StationedHeli heli : manager.m_Supports.Station().m_Helis)
  {
   if (!heli.m_sName.Contains("door") || !heli.m_Vehicle) continue;
   float height = heli.m_Vehicle.GetOrigin()[1] - heli.m_Pad.Spot()[1];
   s_Lift.Set(heli.m_sFaction, Math.Max(s_Lift.Get(heli.m_sFaction), height));
   s_Health.Set(heli.m_sFaction, Math.Min(s_Health.Get(heli.m_sFaction), ACR_Logistics.Health(heli.m_Vehicle)));
  }
 }

 static void CheckLift()
 {
  foreach (string side, float lift : s_Lift)
  {
   PrintFormat("[ACR] PADS %1 door gunners: rose %2 m over the pad, lowest health %3", side, lift, s_Health.Get(side));
   ACR_TestRegress.Check("pads: " + side + " door gunners lift off the pad unhurt", lift > 25 && s_Health.Get(side) > 0.9, string.Format("%1 m, health %2", lift, s_Health.Get(side)));
  }
 }

 static void CheckParked()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  int count;
  foreach (ACR_StationedHeli heli : manager.m_Supports.Station().m_Helis)
  {
   count++;
   vector angles = heli.m_Vehicle.GetYawPitchRoll();
   float health = ACR_Logistics.Health(heli.m_Vehicle);
   float height = heli.m_Vehicle.GetOrigin()[1] - heli.m_Pad.Spot()[1];
   float off = vector.DistanceXZ(heli.m_Vehicle.GetOrigin(), heli.m_Pad.GetOrigin());
   PrintFormat("[ACR] PADS %1 %2 at %3: pitch %4 roll %5, health %6, %7 m above the pad, %8 m off it, state %9", heli.m_sFaction, heli.m_sName, heli.m_Pad.Base().m_sName, angles[1], angles[2], health, height, off, heli.m_eState);
   ACR_TestRegress.Check("pads: " + heli.m_sName + " " + heli.m_sFaction + " parked upright and unhurt", Math.AbsFloat(angles[1]) < 8 && Math.AbsFloat(angles[2]) < 8 && health > 0.95 && off < 2 && heli.m_eState == ACR_EHeliState.READY, "");
  }
  ACR_TestRegress.Check("pads: four helicopters on the HQ pads", count == 4, string.Format("%1", count));
  int models, inPlay;
  foreach (ACR_Helipad pad : ACR_Helipad.s_All)
  {
   if (pad.m_bGone) continue;
   inPlay++;
   if (pad.HasModel()) models++;
  }
  ACR_TestRegress.Check("pads: every helipad in play has its model", models == inPlay, string.Format("%1 of %2", models, inPlay));
 }
}
