// -acrMapDeclutterTest 1 (server-capable, ~20 s): the calm command map's rules (ACR_MapDeclutter), as
// pure functions: INTEL merging (counts, RP totals, "+N older", "N new"), which bases keep their
// names at each zoom, squads at one base becoming one icon with "×N", the owner colours, the size
// pips, the corner marks, and the host option's default and setup row.
class ACR_TestMapDeclutter
{
 static const int CHECKS = 16;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  CheckIntel();
  CheckBases();
  CheckSquads();
  CheckPlacement();
  CheckOption();
  ACR_TestRegress.Complete("mapdeclutter", CHECKS);
 }

 static string Joined(array<string> texts)
 {
  string all = "";
  foreach (string text : texts) all += text + " | ";
  return all;
 }

 static void CheckIntel()
 {
  // The feed as the server sends it, newest first: four of Yung's purchases (the viewer's own), a
  // sighting twice, two of Popp's, a take of command five minutes back.
  array<string> lines = {
   "Yung requisitioned MG Team at Lamentin (-10 RP)",
   "Yung requisitioned Grenadier Team at Lamentin (-9 RP)",
   "Yung requisitioned MG Team at Lamentin (-10 RP)",
   "Yung requisitioned Grenadier Team at Lamentin (-10 RP)",
   "Alpha: enemy rifle squad near Le Moule",
   "Alpha: enemy rifle squad near Le Moule",
   "Popp requisitioned Transport Truck at Lamentin (-30 RP)",
   "Popp requisitioned Rifle Squad at Lamentin (-15 RP)",
   "Yung is in command"
  };
  array<int> ages = {5, 7, 8, 9, 30, 40, 50, 55, 300};
  array<string> texts = {};
  array<int> entryAges = {};
  ACR_MapDeclutter.MergeFeed(lines, ages, "Yung", texts, entryAges);
  array<string> expected = {"Bought MG Team ×2, Grenadier Team ×2 (-39 RP)", "Alpha: enemy rifle squad near Le Moule ×2", "Popp bought Transport Truck, Rifle Squad (-45 RP)", "Yung is in command"};
  bool same = texts.Count() == expected.Count();
  if (same) foreach (int i, string text : texts) if (text != expected[i]) same = false;
  bool agesOk = entryAges.Count() == 4 && entryAges[0] == 5 && entryAges[1] == 30 && entryAges[2] == 50 && entryAges[3] == 300;
  ACR_TestRegress.Check("mapdeclutter: INTEL merges a run of purchases (counts per unit, RP summed) and a repeated line", same && agesOk, Joined(texts));

  // Not merged: purchases minutes apart, or by different commanders; the tag-less form parses.
  array<string> apart = {"Yung requisitioned MG Team at Lamentin (-10 RP)", "Yung requisitioned MG Team at Lamentin (-10 RP)", "Popp requisitioned MG Team at Lamentin (-10 RP)"};
  array<int> apartAges = {5, 400, 410};
  array<string> apartTexts = {};
  array<int> apartEntryAges = {};
  ACR_MapDeclutter.MergeFeed(apart, apartAges, "Yung", apartTexts, apartEntryAges);
  string tag, unit;
  int rp;
  bool parsed = ACR_MapDeclutter.ParsePurchase("requisitioned Mortar Team at Le Moule (-24 RP)", tag, unit, rp) && tag.IsEmpty() && unit == "Mortar Team" && rp == 24;
  ACR_TestRegress.Check("mapdeclutter: purchases minutes apart or by another commander stay apart", apartTexts.Count() == 3 && apartTexts[0] == "Bought MG Team (-10 RP)" && apartTexts[2] == "Popp bought MG Team (-10 RP)" && parsed, Joined(apartTexts) + string.Format(" parsed %1 '%2' '%3' %4", parsed, tag, unit, rp));

  // The last 3 shown, "+N older"; "N new" since the last look; the bar's headline.
  bool older = ACR_MapDeclutter.Older(texts.Count()) == 1 && ACR_MapDeclutter.Older(3) == 0 && ACR_MapDeclutter.Older(0) == 0 && ACR_MapDeclutter.Older(9) == 6;
  bool fresh = ACR_MapDeclutter.NewCount(entryAges, 40) == 2 && ACR_MapDeclutter.NewCount(entryAges, 2) == 0;
  string attacked = ACR_MapDeclutter.Headline("Le Moule", texts);
  string newest = ACR_MapDeclutter.Headline("", texts);
  array<string> none = {};
  string empty = ACR_MapDeclutter.Headline("", none);
  bool headline = attacked == "Le Moule under attack" && newest.StartsWith("Bought MG Team") && newest.Length() <= ACR_MapDeclutter.HEADLINE_CHARS + 2 && newest.EndsWith("...") && empty == "No contacts reported";
  ACR_TestRegress.Check("mapdeclutter: INTEL shows 3 lines and +N older; the bar says what is under attack and how many are new", older && fresh && headline, string.Format("older %1, new %2/%3, '%4', '%5', '%6'", ACR_MapDeclutter.Older(texts.Count()), ACR_MapDeclutter.NewCount(entryAges, 40), ACR_MapDeclutter.NewCount(entryAges, 2), attacked, newest, empty));
 }

 static void CheckBases()
 {
  // Our two bases at 0 and 1000 m, a free base 500 m past the second (the front), one far out
  // (3000), an enemy base 800 m behind the first (the front too), the enemy HQ at 6000, an enemy
  // base at 4500 under attack.
  array<vector> pos = {Vector(0, 0, 0), Vector(1000, 0, 0), Vector(1500, 0, 0), Vector(3000, 0, 0), Vector(-800, 0, 0), Vector(6000, 0, 0), Vector(4500, 0, 0)};
  array<string> names = {"Lamentin", "Sawmill", "Le Moule", "Villeneuve", "Provins", "Harbor", "Richemont"};
  array<string> owners = {"US", "US", "", "", "USSR", "USSR", "USSR"};
  array<bool> hq = {false, false, false, false, false, true, false};
  array<bool> contested = {false, false, false, false, false, false, true};
  array<int> sides = {};
  foreach (string owner : owners) sides.Insert(ACR_MapDeclutter.SideOf(owner, "US"));
  array<bool> front = {};
  ACR_MapDeclutter.FrontBases(pos, sides, front);
  string farNamed = "";
  string nearNamed = "";
  foreach (int i, string name : names)
  {
   if (ACR_MapDeclutter.KeepsName(true, sides[i], hq[i], contested[i], front[i], false)) farNamed += name + " ";
   if (ACR_MapDeclutter.KeepsName(false, sides[i], hq[i], contested[i], front[i], false)) nearNamed += name + " ";
  }
  bool selectedKept = ACR_MapDeclutter.KeepsName(true, sides[3], false, false, false, true);
  // Far only near the map's widest zoom (the test range: 0.527 px/m; zoom 1 isn't far, pn12f).
  bool zoom = ACR_MapDeclutter.IsFar(0.527, 0.527) && ACR_MapDeclutter.IsFar(0.65, 0.527) && !ACR_MapDeclutter.IsFar(1, 0.527) && !ACR_MapDeclutter.IsFar(0.5, 0);
  ACR_TestRegress.Check("mapdeclutter: zoomed out only our bases, the front, the enemy HQ and bases under attack keep names; near, all", farNamed == "Lamentin Sawmill Le Moule Provins Harbor Richemont " && nearNamed == "Lamentin Sawmill Le Moule Villeneuve Provins Harbor Richemont " && selectedKept && zoom, string.Format("far: %1| near: %2| selected %3, zoom %4", farNamed, nearNamed, selectedKept, zoom));

  // Owner colours: blue ours, red enemy, yellow free (green FIA).
  Color own = ACR_MapDeclutter.SideColor(ACR_EBaseSide.OWN);
  Color enemy = ACR_MapDeclutter.SideColor(ACR_EBaseSide.ENEMY);
  Color free = ACR_MapDeclutter.SideColor(ACR_EBaseSide.FREE);
  bool sidesOk = ACR_MapDeclutter.SideOf("US", "US") == ACR_EBaseSide.OWN && ACR_MapDeclutter.SideOf("USSR", "US") == ACR_EBaseSide.ENEMY && ACR_MapDeclutter.SideOf("", "US") == ACR_EBaseSide.FREE && ACR_MapDeclutter.SideOf(ACR_Pockets.FACTION, "US") == ACR_EBaseSide.OTHER;
  bool colours = own.B() > own.R() && own.B() > own.G() && enemy.R() > enemy.G() && enemy.R() > enemy.B() && free.R() > free.B() && free.G() > free.B();
  ACR_TestRegress.Check("mapdeclutter: bases are coloured by owner: blue ours, red enemy, yellow free", sidesOk && colours, string.Format("sides %1, own %2, enemy %3, free %4", sidesOk, own, enemy, free));

  // Size pips instead of "[CP II]", from the label's [N] or the detail's +N RP; captions.
  bool pips = ACR_MapDeclutter.Pips("Le Moule [2]", "") == 2 && ACR_MapDeclutter.Pips("Lamentin [1]", "") == 1 && ACR_MapDeclutter.Pips("Harbor", "US | garrison 3 | +3 RP") == 3 && ACR_MapDeclutter.Pips("Farm", "Unclaimed | garrison 0 | no income") == 0 && ACR_MapDeclutter.Pips("Big [7]", "") == 3;
  string capHQ = ACR_MapDeclutter.Caption("Harbor [3]", "USSR | garrison 5 | +3 RP | HQ", ACR_EBaseSide.ENEMY, true);
  string capOwnHQ = ACR_MapDeclutter.Caption("Base [1]", "US | garrison 5 | +1 RP | HQ", ACR_EBaseSide.OWN, true);
  string capPlain = ACR_MapDeclutter.Caption("Le Moule [2]", "Unclaimed | +2 RP", ACR_EBaseSide.FREE, true);
  string capDot = ACR_MapDeclutter.Caption("Le Moule [2]", "Unclaimed | +2 RP", ACR_EBaseSide.FREE, false);
  bool hqDestroyed = !ACR_MapDeclutter.IsHQ("US | garrison 0 | HQ DESTROYED: FOB offline until rebuilt");
  ACR_TestRegress.Check("mapdeclutter: size pips come from the base size; the name drops [N]; HQs say so; a plain dot has no caption", pips && capHQ == "Harbor · enemy HQ" && capOwnHQ == "Base · HQ" && capPlain == "Le Moule" && capDot.IsEmpty() && hqDestroyed, string.Format("pips %1, '%2', '%3', '%4', '%5', destroyed-not-HQ %6", pips, capHQ, capOwnHQ, capPlain, capDot, hqDestroyed));

  // Filled shapes stay on the canvas and non-degenerate.
  array<float> onCanvas = {100, 100, 110, 100, 110, 110, 100, 110};
  array<float> offCanvas = {-500, -500, -490, -500, -490, -490};
  array<float> flat = {10, 10, 20, 20, 30, 30};
  ACR_TestRegress.Check("mapdeclutter: the calm map's filled shapes are on-canvas and non-degenerate", ACR_MapDeclutter.ShapeOk(onCanvas, 1920, 1080) && !ACR_MapDeclutter.ShapeOk(offCanvas, 1920, 1080) && !ACR_MapDeclutter.ShapeOk(flat, 1920, 1080), "");
 }

 static void CheckSquads()
 {
  // Three squads in Lamentin's ring (the second selected), one out in the field, a player group in
  // the ring (not clustered), one at the edge of Sawmill's 300 m ring.
  array<vector> groups = {Vector(10, 0, 0), Vector(-40, 0, 30), Vector(60, 0, -20), Vector(600, 0, 600), Vector(5, 0, 5), Vector(1280, 0, 0)};
  array<bool> clusterable = {true, true, true, true, false, true};
  array<bool> selected = {false, true, false, false, false, false};
  array<vector> bases = {Vector(0, 0, 0), Vector(1000, 0, 0)};
  array<float> radii = {80, 300};
  array<int> baseOf = {};
  array<int> count = {};
  array<bool> rep = {};
  ACR_MapDeclutter.Clusters(groups, clusterable, selected, bases, radii, baseOf, count, rep);
  string got = "";
  foreach (int g, int at : baseOf) got += string.Format("%1/%2/%3 ", at, count[g], rep[g]);
  bool ok = baseOf.Count() == 6 && baseOf[0] == 0 && baseOf[1] == 0 && baseOf[2] == 0 && baseOf[3] == -1 && baseOf[4] == -1 && baseOf[5] == 1;
  ok = ok && count[0] == 3 && count[1] == 3 && count[2] == 3 && count[3] == 1 && count[5] == 1;
  ok = ok && !rep[0] && rep[1] && !rep[2] && rep[3] && rep[4] && rep[5];
  ACR_TestRegress.Check("mapdeclutter: squads at one base become one icon with ×N (the selected one shown)", ok, got);

  bool marks = ACR_MapDeclutter.SquadMark(true, 300, -1) == ACR_ESquadMark.MOVING && ACR_MapDeclutter.SquadMark(true, 10, -1) == ACR_ESquadMark.HOLDING && ACR_MapDeclutter.SquadMark(false, -1, 150) == ACR_ESquadMark.FIGHTING && ACR_MapDeclutter.SquadMark(true, 300, 500) == ACR_ESquadMark.MOVING && ACR_MapDeclutter.SquadMark(false, -1, -1) == ACR_ESquadMark.HOLDING;
  ACR_TestRegress.Check("mapdeclutter: the corner mark says fighting, moving or holding", marks, "");
 }

 // Placement (pn12f shots: "Farm · HQ" under squad icons, order dots and "Crossroads"): names keep
 // their spot unless they'd cover another name or dot; icons keep clear of every name and dot;
 // order ends near a base end at it.
 static void CheckPlacement()
 {
  // Three bases: Farm, Crossroads 30 px to its right and 10 up, Outpost far off. Captions centred under
  // their dots, 120 x 20 px.
  array<float> dots = {500, 500, 530, 490, 900, 500};
  array<float> captions = {-60, 14, 60, 34, -60, 14, 60, 34, -60, 14, 60, 34};
  array<int> order = {0, 1, 2};
  array<float> shift = {};
  ACR_MapDeclutter.PlaceNames(dots, captions, order, 10, 4, shift);
  array<float> names = {};
  for (int b = 0; b < 3; b++)
  {
   names.Insert(dots[b * 2] + shift[b * 2] + captions[b * 4]);
   names.Insert(dots[b * 2 + 1] + shift[b * 2 + 1] + captions[b * 4 + 1]);
   names.Insert(dots[b * 2] + shift[b * 2] + captions[b * 4 + 2]);
   names.Insert(dots[b * 2 + 1] + shift[b * 2 + 1] + captions[b * 4 + 3]);
  }
  bool apart = true;
  for (int i = 0; i < 3; i++)
  {
   for (int j = 0; j < 3; j++)
   {
    if (i == j) continue;
    if (j > i && ACR_MapDeclutter.Overlaps(names[i * 4], names[i * 4 + 1], names[i * 4 + 2], names[i * 4 + 3], names[j * 4], names[j * 4 + 1], names[j * 4 + 2], names[j * 4 + 3])) apart = false;
    // ... nor over another base's dot.
    if (ACR_MapDeclutter.Overlaps(names[i * 4], names[i * 4 + 1], names[i * 4 + 2], names[i * 4 + 3], dots[j * 2] - 10, dots[j * 2 + 1] - 10, dots[j * 2] + 10, dots[j * 2 + 1] + 10)) apart = false;
   }
  }
  bool kept = shift[0] == 0 && shift[1] == 0 && shift[4] == 0 && shift[5] == 0 && (shift[2] != 0 || shift[3] != 0);
  ACR_TestRegress.Check("mapdeclutter: base names near each other separate (the first keeps its spot)", apart && kept, string.Format("shifts %1,%2 %3,%4 %5,%6", shift[0], shift[1], shift[2], shift[3], shift[4], shift[5]));

  // Squad icons (30 x 22): one wanted on Farm's name, one on Crossroads' dot, one in the open.
  array<float> obstacles = {};
  for (int d = 0; d < 3; d++)
  {
   obstacles.Insert(dots[d * 2] - 10);
   obstacles.Insert(dots[d * 2 + 1] - 10);
   obstacles.Insert(dots[d * 2] + 10);
   obstacles.Insert(dots[d * 2 + 1] + 10);
  }
  foreach (float edge : names) obstacles.Insert(edge);
  array<float> wanted = {500, 524, 530, 490, 700, 700};
  bool clear = true;
  string where = "";
  for (int k = 0; k < 3; k++)
  {
   vector at = ACR_MapDeclutter.Avoid(wanted[k * 2], wanted[k * 2 + 1], 30, 22, obstacles, 4);
   where += string.Format("(%1,%2) ", at[0], at[1]);
   if (ACR_MapDeclutter.FirstHit(at[0] - 15, at[1] - 11, at[0] + 15, at[1] + 11, obstacles) >= 0) clear = false;
   if (k == 2 && (at[0] != 700 || at[1] != 700)) clear = false;
  }
  ACR_TestRegress.Check("mapdeclutter: squad icons move off base names and dots (below or aside); one in the open stays", clear, where);

  // Order ends near a base end at it: two orders to Farm merge into one mark.
  // ... and one landing on a base's name (Crossroads', moved above its dot) ends at that base too.
  array<float> none = {};
  bool snaps = ACR_MapDeclutter.SnapToBase(505, 498, dots, 26, names) == 0 && ACR_MapDeclutter.SnapToBase(492, 512, dots, 26, names) == 0 && ACR_MapDeclutter.SnapToBase(533, 507, dots, 26, names) == 1 && ACR_MapDeclutter.SnapToBase(700, 700, dots, 26, names) == -1;
  bool onName = ACR_MapDeclutter.SnapToBase(580, 460, dots, 26, names) == 1 && ACR_MapDeclutter.SnapToBase(580, 460, dots, 26, none) == -1;
  ACR_TestRegress.Check("mapdeclutter: order ends near a base or on its name merge at its dot", snaps && onName, string.Format("near %1, on a name %2", snaps, onName));

  // Round markers (soldiers, the command unit, empty vehicles; 14 px): one on Farm's name or
  // touching Crossroads' dot counts as on it (hidden, or moved), one in the open doesn't; moved,
  // it's clear.
  bool onIt = ACR_MapDeclutter.OnAName(500, 524, 14, obstacles) && ACR_MapDeclutter.OnAName(552, 490, 14, obstacles) && !ACR_MapDeclutter.OnAName(700, 700, 14, obstacles);
  vector moved = ACR_MapDeclutter.Avoid(500, 524, 28, 28, obstacles, 4);
  bool movedClear = !ACR_MapDeclutter.OnAName(moved[0], moved[1], 14, obstacles);
  ACR_TestRegress.Check("mapdeclutter: round markers on a base's name or dot are hidden or moved clear", onIt && movedClear, string.Format("on %1, moved to (%2,%3)", onIt, moved[0], moved[1]));
 }

 static void CheckOption()
 {
  // On by default; -acrMapDeclutter 0 or the setup row turns it off.
  ACR_Options.Unset(ACR_MapDeclutter.OPTION);
  bool byDefault = ACR_MapDeclutter.OptionOn();
  string onRows = ACR_Setup.Rows();
  ACR_Options.Set(ACR_MapDeclutter.OPTION, "0");
  bool off = !ACR_MapDeclutter.OptionOn();
  string offRows = ACR_Setup.Rows();
  ACR_Options.Unset(ACR_MapDeclutter.OPTION);
  ACR_TestRegress.Check("mapdeclutter: on by default, -acrMapDeclutter 0 turns it off", byDefault && off, string.Format("default %1, with 0 off %2", byDefault, off));
  bool row = onRows.Contains("declutter\tCalm command map: on") && offRows.Contains("declutter\tCalm command map: off (the classic map)") && ACR_Setup.Option("declutter") == "acrMapDeclutter" && ACR_SetupDialog.Tab("declutter") == 1;
  ACR_TestRegress.Check("mapdeclutter: a setup row on the Command tab", row, string.Format("tab %1, option %2", ACR_SetupDialog.Tab("declutter"), ACR_Setup.Option("declutter")));
  // The player's own choices default to the sketch: the light map, compact.
  ACR_MapDeclutter.ResetForTest();
  ACR_TestRegress.Check("mapdeclutter: the dark map is off and the UI compact by default", !ACR_MapDeclutter.Dark() && ACR_MapDeclutter.Compact() && ACR_MapDeclutter.On(), "");
 }
}
