// -acrPadsTest 1 (server, Arland, ~1 min): survey for helipads. Lists helipad objects already
// in the world near each base and the level, open spots (room for a Mi-8) in and around them.
// Then each side buys both support helicopters for its HQ's pads: they must sit upright and unhurt.
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
  if (tick == 8) BuyAll();
  if (tick == 25) { CheckParked(); Print("[ACR] TEST COMPLETE"); }
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
 }
}
