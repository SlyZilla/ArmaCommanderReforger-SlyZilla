// -acrIntelMarksTest 1 (server-capable, ~20 s): the rules behind the intel map markers. A foot
// team mostly carrying one weapon gets that weapon's NATO symbol, a squad with its usual gunner
// and launcher stays infantry; labels, size marks and ages read right; a contact gets a heading
// only once it has really moved, and loses it after standing still.
class ACR_TestIntelMarkers
{
 static const int CHECKS = 6;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  int infantry = EMilitarySymbolIcon.INFANTRY;
  int atTeam = EMilitarySymbolIcon.INFANTRY | EMilitarySymbolIcon.ANTITANK;
  int mgTeam = EMilitarySymbolIcon.INFANTRY | EMilitarySymbolIcon.MACHINEGUN;
  int lone = ACR_Intel.InfantryIcons(1, 1, 0, 0);
  int pair = ACR_Intel.InfantryIcons(2, 2, 0, 0);
  int squad = ACR_Intel.InfantryIcons(8, 1, 1, 1);
  int snipers = ACR_Intel.InfantryIcons(2, 0, 0, 1);
  int guns = ACR_Intel.InfantryIcons(3, 0, 2, 0);
  bool icons = lone == infantry && pair == atTeam && squad == infantry && snipers == EMilitarySymbolIcon.SNIPER && guns == mgTeam;
  ACR_TestRegress.Check("intel marks: a team mostly with one weapon is that team, a squad stays infantry", icons, string.Format("lone %1, AT pair %2, squad %3, snipers %4, guns %5", lone, pair, squad, snipers, guns));

  string tank = ACR_Intel.DescribeSymbol(EMilitarySymbolIcon.ARMOR, ACR_Intel.KIND_ARMOUR, 3);
  string apc = ACR_Intel.DescribeSymbol(EMilitarySymbolIcon.INFANTRY | EMilitarySymbolIcon.ARMOR, ACR_Intel.KIND_ARMOUR, 6);
  string truck = ACR_Intel.DescribeSymbol(EMilitarySymbolIcon.INFANTRY | EMilitarySymbolIcon.MOTORIZED, ACR_Intel.KIND_VEHICLE, 5);
  string nest = ACR_Intel.DescribeSymbol(mgTeam, ACR_Intel.KIND_ARMED, 1);
  string at = ACR_Intel.DescribeSymbol(atTeam, ACR_Intel.KIND_INFANTRY, 2);
  string men = ACR_Intel.DescribeSymbol(infantry, ACR_Intel.KIND_INFANTRY, 6);
  bool labels = tank == "tank" && apc == "APC/IFV" && truck == "vehicle with 5" && nest == "MG nest" && at == "AT team of 2" && men == "6 infantry";
  ACR_TestRegress.Check("intel marks: labels name what was seen", labels, string.Format("%1 | %2 | %3 | %4 | %5 | %6", tank, apc, truck, nest, at, men));

  int one = ACR_Intel.EchelonOf(ACR_Intel.KIND_INFANTRY, 1);
  int team = ACR_Intel.EchelonOf(ACR_Intel.KIND_INFANTRY, 3);
  int squadMark = ACR_Intel.EchelonOf(ACR_Intel.KIND_INFANTRY, 8);
  int platoon = ACR_Intel.EchelonOf(ACR_Intel.KIND_INFANTRY, 20);
  int vehicle = ACR_Intel.EchelonOf(ACR_Intel.KIND_VEHICLE, 8);
  bool echelons = one == EMilitarySymbolAmplifier.NONE && team == EMilitarySymbolAmplifier.TEAM && squadMark == EMilitarySymbolAmplifier.SQUAD && platoon == EMilitarySymbolAmplifier.PLATOON && vehicle == EMilitarySymbolAmplifier.NONE;
  ACR_TestRegress.Check("intel marks: size marks from the men seen, none on vehicles", echelons, string.Format("1: %1, 3: %2, 8: %3, 20: %4, vehicle: %5", one, team, squadMark, platoon, vehicle));

  string now = ACR_Intel.AgeText(2);
  string seconds = ACR_Intel.AgeText(40);
  string minutes = ACR_Intel.AgeText(185);
  ACR_TestRegress.Check("intel marks: ages read as now, seconds, minutes", now == "now" && seconds == "40s ago" && minutes == "3 min ago", string.Format("%1 | %2 | %3", now, seconds, minutes));

  vector origin = Vector(1000, 0, 1000);
  float north = ACR_Intel.Bearing(origin, origin + Vector(0, 0, 100));
  float east = ACR_Intel.Bearing(origin, origin + Vector(100, 0, 0));
  float south = ACR_Intel.Bearing(origin, origin + Vector(0, 0, -100));
  float west = ACR_Intel.Bearing(origin, origin + Vector(-100, 0, 0));
  bool bearings = Math.AbsFloat(north) < 0.01 && Math.AbsFloat(east - 90) < 0.01 && Math.AbsFloat(south - 180) < 0.01 && Math.AbsFloat(west - 270) < 0.01;
  ACR_TestRegress.Check("intel marks: bearings are compass degrees", bearings, string.Format("N %1, E %2, S %3, W %4", north, east, south, west));

  ACR_Intel intel = new ACR_Intel(null, "ACR_TEST");
  float first = intel.TestHeading("S1", origin, 0);
  float shuffle = intel.TestHeading("S1", origin + Vector(15, 0, 10), 5000);
  float moved = intel.TestHeading("S1", origin + Vector(0, 0, 50), 10000);
  float standing = intel.TestHeading("S1", origin + Vector(5, 0, 50), 60000);
  float stood = intel.TestHeading("S1", origin + Vector(5, 0, 52), 10000 + ACR_Intel.HEADING_KEEP_MS + 1000);
  bool heading = first == -1 && shuffle == -1 && Math.AbsFloat(moved) < 0.01 && Math.AbsFloat(standing) < 0.01 && stood == -1;
  ACR_TestRegress.Check("intel marks: a heading after a real move, gone after standing still", heading, string.Format("first %1, shuffle %2, moved %3, standing %4, stood %5", first, shuffle, moved, standing, stood));
  ACR_TestRegress.Complete("intelmarks", CHECKS);
 }
}

modded class ACR_Intel
{
 float TestHeading(string key, vector pos, float seen) { return Heading(key, pos, seen); }
}
