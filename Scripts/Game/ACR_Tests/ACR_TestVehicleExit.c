// -acrVehicleExitTest 1 (server, a scenario world, ~1 min): bought vehicles get spots they can drive
// out of (Jacob, 6 Nov: the AI's vehicles came out in courtyards they couldn't leave).
// Every base of the map: picks from the old finder (clear ground only) and from VehicleSpot, each
// judged by ACR_BaseSystem.DrivesOut, with a fan of 16 rays (metres to the first wall, 60 = open)
// printed for the old picks that are shut in, so a person can see the yard. Then the map's known
// cases (s_aBad, s_aGood) must read shut in / open, and a vehicle bought at their base must not go in
// the yard.
// Lines: [ACR] VEHEXIT ...
class ACR_TestVehicleExit
{
 static int s_iOldPicks;
 static int s_iOldShut;
 static int s_iNewPicks;
 static int s_iNewShut;
 static int s_iWorstMs;

 static void Run(int seconds)
 {
  if (seconds == 3)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) if (settings) settings.m_bAICommander = false;
  }
  if (seconds == 6) Survey();
  if (seconds == 8) Known();
 }

 static void Survey()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) { ACR_TestRegress.Abort("vehicle exit", "no manager"); return; }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   int oldShut, newShut, oldPicks, newPicks;
   for (int k = 0; k < 10; k++)
   {
    vector old = manager.m_Requisition.SpawnPoint(base, 10, 25, 4);
    oldPicks++;
    if (ACR_BaseSystem.DrivesOut(old)) continue;
    oldShut++;
    if (oldShut <= 2) PrintFormat("[ACR] VEHEXIT shut %1 at %2 %3 fan %4 | reached %5 steep %6 wet %7 walled %8", base.m_sName, (int)old[0], (int)old[2], Fan(old), ACR_BaseSystem.s_iExitReached, ACR_BaseSystem.s_iExitSteep, ACR_BaseSystem.s_iExitWet, ACR_BaseSystem.s_iExitWalled);
   }
   int sweeps = ACR_BaseSystem.s_iExitSweeps;
   int worst;
   for (int n = 0; n < 5; n++)
   {
    int started = System.GetTickCount();
    vector spot = manager.m_Requisition.VehicleSpot(base, 10, 25, 4);
    worst = (int)Math.Max(worst, System.GetTickCount() - started);
    newPicks++;
    if (ACR_BaseSystem.DrivesOut(spot)) continue;
    newShut++;
    PrintFormat("[ACR] VEHEXIT new pick shut %1 at %2 %3 fan %4 | reached %5 steep %6 wet %7 walled %8", base.m_sName, (int)spot[0], (int)spot[2], Fan(spot), ACR_BaseSystem.s_iExitReached, ACR_BaseSystem.s_iExitSteep, ACR_BaseSystem.s_iExitWet, ACR_BaseSystem.s_iExitWalled);
   }
   s_iWorstMs = (int)Math.Max(s_iWorstMs, worst);
   PrintFormat("[ACR] VEHEXIT base %1 at %2 %3: old finder shut in %4/%5, VehicleSpot %6/%7, %8 sweeps for 5 picks (incl. checks), slowest %9 ms", base.m_sName, (int)base.GetOrigin()[0], (int)base.GetOrigin()[2], oldShut, oldPicks, newShut, newPicks, ACR_BaseSystem.s_iExitSweeps - sweeps, worst);
   s_iOldPicks += oldPicks;
   s_iOldShut += oldShut;
   s_iNewPicks += newPicks;
   s_iNewShut += newShut;
  }
  PrintFormat("[ACR] VEHEXIT total: old finder shut in %1/%2, VehicleSpot %3/%4, slowest pick %5 ms", s_iOldShut, s_iOldPicks, s_iNewShut, s_iNewPicks, s_iWorstMs);
 }

 // Metres to the first wall in 16 directions, a vehicle-wide box at bonnet height (60 = open).
 static string Fan(vector spot)
 {
  TraceBox ray = new TraceBox();
  ray.Mins = Vector(-1, 0.5, -1);
  ray.Maxs = Vector(1, 2, 1);
  ray.Flags = TraceFlags.ENTS;
  ray.LayerMask = EPhysicsLayerDefs.Navmesh | EPhysicsLayerDefs.NavmeshVehicle;
  string fan;
  for (int i = 0; i < 16; i++)
  {
   float angle = i * Math.PI2 / 16;
   ray.Start = spot;
   ray.End = spot + Vector(Math.Sin(angle) * 60, 0, Math.Cos(angle) * 60);
   float hit = GetGame().GetWorld().TraceMove(ray, null);
   fan += string.Format(" %1", (int)(hit * 60));
  }
  return fan;
 }

 // srv:regress (the test range is on Arland): the farm yard at 2015 3030, walled on every side
 // (the old finder put Arland's vehicles there; vehexitarland-1106b), is shut in, and the spot
 // finder looking round it finds clear ground a vehicle can leave.
 static void Regress()
 {
  vector yard = ACR_Manager.Ground(Vector(2015, 0, 3030));
  ACR_TestRegress.Check("vehicle exit: the walled farm yard at 2015 3030 is shut in", !ACR_BaseSystem.DrivesOut(yard), string.Format("fan%1 | reached %2 walled %3", Fan(yard), ACR_BaseSystem.s_iExitReached, ACR_BaseSystem.s_iExitWalled));
  vector spot, clear;
  bool anyClear;
  set<int> shut = new set<int>();
  bool found = ACR_BaseSystem.DrivableGround(spot, anyClear, clear, yard, 35, 4, shut, ACR_BaseSystem.s_iExitSweeps + 400);
  ACR_TestRegress.Check("vehicle exit: beside the farm yard a vehicle gets clear ground it can leave", found && ACR_BaseSystem.DrivesOut(spot), string.Format("found %1 at %2 %3, %4 m from the yard, fan%5 (first clear spot %6 %7)", found, (int)spot[0], (int)spot[2], (int)vector.DistanceXZ(spot, yard), Fan(spot), (int)clear[0], (int)clear[2]));
 }

 // Known cases on this map: spots in a yard a vehicle can't leave (found by the survey, fans
 // printed), at a base where the old finder used them; VehicleSpot there must find a way out.
 static void Known()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  string world = FilePath.StripPath(GetGame().GetWorldFile());
  string baseName;
  array<vector> bad = {};
  if (world.Contains("Everon"))
  {
   // Saint-Philippe: the old finder put 10 of 10 vehicles in its yards (vehexit-1106a).
   baseName = "Saint-Philippe";
   bad.Insert(Vector(4490, 0, 10766));
   bad.Insert(Vector(4488, 0, 10769));
  }
  ACR_TestRegress.Check("vehicle exit: VehicleSpot picks have a way out at every base", s_iNewShut == 0, string.Format("%1/%2 shut in (old finder %3/%4), slowest pick %5 ms", s_iNewShut, s_iNewPicks, s_iOldShut, s_iOldPicks, s_iWorstMs));
  ACR_Base base;
  foreach (ACR_Base b : ACR_Base.s_All) if (b && b.m_sName == baseName) base = b;
  if (bad.IsEmpty() || !base)
  {
   ACR_TestRegress.Complete("vehicle exit", 1);
   return;
  }
  foreach (vector spot : bad)
  {
   vector at = ACR_Manager.Ground(spot);
   ACR_TestRegress.Check(string.Format("vehicle exit: the yard at %1 %2 (%3) is shut in", (int)at[0], (int)at[2], baseName), !ACR_BaseSystem.DrivesOut(at), "fan" + Fan(at));
  }
  int oldShut;
  for (int k = 0; k < 10; k++) if (!ACR_BaseSystem.DrivesOut(manager.m_Requisition.SpawnPoint(base, 10, 25, 4))) oldShut++;
  int open;
  string picks;
  for (int n = 0; n < 5; n++)
  {
   vector pick = manager.m_Requisition.VehicleSpot(base, 10, 25, 4);
   if (ACR_BaseSystem.DrivesOut(pick)) open++;
   picks += string.Format(" | %1 %2 fan%3", (int)pick[0], (int)pick[2], Fan(pick));
  }
  ACR_TestRegress.Check(string.Format("vehicle exit: vehicles bought at %1 get a way out", baseName), open == 5, string.Format("%1/5 open (old finder shut in %2/10)%3", open, oldShut, picks));
  ACR_TestRegress.Complete("vehicle exit", bad.Count() + 2);
 }
}
