// The calm command map (declutter part B, from the sketch agreed in #14): ACA's own map UI made
// quieter. The normal look; the host option -acrMapDeclutter 0 (setup row "Calm command map")
// brings back the classic map exactly as it was. Here: the option, the player's own choices (dark
// map, UI size) and the rules, as pure functions the srv test checks (ACR_TestMapDeclutter). What
// draws it is ACR_MapDeclutterView.c.
//  - INTEL merges repeats ("Bought Grenadier Team ×2, MG Team ×2 (-39 RP)"), shows the last 3 lines
//    and "+N older"; zoomed out it is one line ("INTEL  Le Moule under attack · 2 new").
//  - Bases: a dot in the owner's colour (blue ours, red enemy, yellow free), size pips after the name
//    instead of "[CP II]", a red ring while contested. Zoomed out only our bases, the front, the enemy
//    HQ and bases under attack keep their names; the rest are plain dots.
//  - Squads at a base sit under its marker as one icon with "×N"; a corner mark says moving,
//    fighting or holding (legend bottom right). Zoomed out, order lines only for the selected squads.
enum ACR_ESquadMark
{
 HOLDING,
 MOVING,
 FIGHTING
}

enum ACR_EBaseSide
{
 FREE,
 OWN,
 ENEMY,
 OTHER // a third party (FIA pockets)
}

class ACR_MapDeclutter
{
 static const string OPTION = "acrMapDeclutter";
 static const string PREFS = "$profile:ACA_MapStyle.txt";
 static const int INTEL_SHOWN = 3;          // INTEL lines shown; the rest are "+N older"
 static const float MERGE_SECONDS = 120;    // feed lines this close together may merge
 static const float FAR_ZOOM = 1.25;        // zoomed out: at most this times the map's widest zoom
 static const float CLUSTER_MIN_RADIUS = 100; // m: a squad this close to a base (or in its ring) is "at" it
 static const float ARRIVED = 40;           // m from its order's target: holding, not moving
 static const float FIGHT_RANGE = 200;      // m: a fresh contact this close: fighting
 static const int FIGHT_AGE = 30;           // s: contacts older than this don't count
 static const int HEADLINE_CHARS = 46;
 static const float ROUND_MARKER = 14;      // reference px: the radius of a soldier's or vehicle's round marker

 // ---------------------------------------------------------------- the option and the player's choices
 // Server: the host's choice (on unless set to 0).
 static bool OptionOn()
 {
  return ACR_LogisticsRules.Choice(OPTION, "1") != "0";
 }

 // Client: what the server last said (ACR_MapFeed sends it with the map picture). The setup
 // screen's layout preview stays classic: the host edits every base there, by name.
 protected static bool s_bOn = true;
 static bool On() { return s_bOn && !ACR_SetupClient.s_bActive; }
 static void SetOn(bool on) { s_bOn = on; }

 // The player's own: the dark map (off by default) and the UI size (compact by default), kept in
 // his profile between games.
 protected static bool s_bDark;
 protected static bool s_bCompact = true;
 protected static bool s_bPrefsRead;

 static bool Dark()
 {
  ReadPrefs();
  return s_bDark;
 }

 static bool Compact()
 {
  ReadPrefs();
  return s_bCompact;
 }

 static void SetDark(bool dark)
 {
  ReadPrefs();
  s_bDark = dark;
  SavePrefs();
 }

 static void SetCompact(bool compact)
 {
  ReadPrefs();
  s_bCompact = compact;
  SavePrefs();
 }

 protected static void ReadPrefs()
 {
  if (s_bPrefsRead) return;
  s_bPrefsRead = true;
  if (!FileIO.FileExists(PREFS)) return;
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.READ);
  if (!file) return;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   line = line.Trim();
   if (line == "dark=1") s_bDark = true;
   if (line == "dark=0") s_bDark = false;
   if (line == "size=normal") s_bCompact = false;
   if (line == "size=compact") s_bCompact = true;
  }
  file.Close();
 }

 protected static void SavePrefs()
 {
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.WRITE);
  if (!file) return;
  if (s_bDark) file.WriteLine("dark=1");
  else file.WriteLine("dark=0");
  if (s_bCompact) file.WriteLine("size=compact");
  else file.WriteLine("size=normal");
  file.Close();
 }

 // Tests: the defaults again, without the profile file.
 static void ResetForTest()
 {
  s_bOn = true;
  s_bDark = false;
  s_bCompact = true;
  s_bPrefsRead = true;
 }

 // ---------------------------------------------------------------- colours
 // An sRGB colour (the sketch's hex) as widgets and the map canvas want it: linear.
 static Color Rgb(int r, int g, int b, float a = 1)
 {
  return new Color(Linear(r), Linear(g), Linear(b), a);
 }

 static float Linear(int channel)
 {
  float c = channel / 255.0;
  return Math.Pow(c, 2.2);
 }

 static Color PanelColor() { return Rgb(24, 27, 30, 0.96); }
 static Color AccentBlue() { return Rgb(90, 166, 240); }   // #5AA6F0: headers
 static Color TextColor() { return Rgb(226, 228, 230); }
 static Color GreyText() { return Rgb(150, 156, 162); }
 static Color ButtonColor() { return Rgb(36, 50, 66); }
 static Color ButtonHover() { return Rgb(52, 72, 96); }
 static Color ButtonBorder() { return Rgb(70, 86, 104); }
 // The dark map's tint: a near-black blue over the map (linear colour; tweak the strength here).
 static const float DARK_TINT_ALPHA = 0.82;
 static Color DarkTint() { return Rgb(8, 12, 24, DARK_TINT_ALPHA); }

 // ---------------------------------------------------------------- bases
 // Whose a base is, as this player sees it.
 static int SideOf(string owner, string mine)
 {
  if (owner.IsEmpty()) return ACR_EBaseSide.FREE;
  if (!mine.IsEmpty() && owner == mine) return ACR_EBaseSide.OWN;
  if (owner == ACR_Pockets.FACTION) return ACR_EBaseSide.OTHER;
  return ACR_EBaseSide.ENEMY;
 }

 // Blue ours, red enemy, yellow free (green: FIA, a third party).
 static Color SideColor(int side, float alpha = 1)
 {
  if (side == ACR_EBaseSide.OWN) return Rgb(47, 128, 237, alpha);
  if (side == ACR_EBaseSide.ENEMY) return Rgb(229, 62, 62, alpha);
  if (side == ACR_EBaseSide.OTHER) return Rgb(72, 168, 96, alpha);
  return Rgb(242, 201, 76, alpha);
 }

 // The base's size as pips (1-3, its income), from the label's " [N]" or, failing that, the
 // detail's "+N RP"; 0 for none.
 static int Pips(string label, string detail)
 {
  int open = label.LastIndexOf(" [");
  if (open >= 0 && label.EndsWith("]"))
  {
   string value = label.Substring(open + 2, label.Length() - open - 3);
   int n = value.ToInt();
   if (n > 0) return Math.Min(n, 3);
  }
  if (detail.Contains("+3 RP")) return 3;
  if (detail.Contains("+2 RP")) return 2;
  if (detail.Contains("+1 RP")) return 1;
  return 0;
 }

 // An HQ: the server adds an " | HQ" part to its detail while the calm map is on.
 static bool IsHQ(string detail)
 {
  // Its own " | HQ" part, not " | HQ DESTROYED: ..." (a FOB's lost HQ).
  return detail == "HQ" || detail.EndsWith(" | HQ") || detail.Contains(" | HQ | ");
 }

 static bool IsContested(string detail)
 {
  return detail.Contains("CONTESTED");
 }

 // The name alone: "Le Moule [2]" -> "Le Moule".
 static string ShortName(string label)
 {
  int open = label.LastIndexOf(" [");
  if (open > 0 && label.EndsWith("]")) return label.Substring(0, open);
  return label;
 }

 // The caption under a base's dot: its name, "· HQ" / "· enemy HQ" for HQs, "" for a plain dot.
 static string Caption(string label, string detail, int side, bool keepName)
 {
  if (!keepName) return "";
  string name = ShortName(label);
  if (IsHQ(detail))
  {
   if (side == ACR_EBaseSide.ENEMY) return name + " · enemy HQ";
   return name + " · HQ";
  }
  return name;
 }

 // Zoomed out: really far out, the map at most FAR_ZOOM times its widest zoom (minZoom, its
 // pixels per metre when the whole map fits). The one rule for the calm map's far view.
 static bool IsFar(float zoom, float minZoom)
 {
  return minZoom > 0 && zoom > 0 && zoom <= minZoom * FAR_ZOOM;
 }

 // The front: for each of our bases, the nearest base that isn't ours.
 static void FrontBases(array<vector> positions, array<int> sides, notnull array<bool> front)
 {
  front.Clear();
  foreach (int f, vector unused : positions) front.Insert(false);
  foreach (int i, vector own : positions)
  {
   if (i >= sides.Count() || sides[i] != ACR_EBaseSide.OWN) continue;
   int best = -1;
   float bestDistance;
   foreach (int j, vector other : positions)
   {
    if (j >= sides.Count() || sides[j] == ACR_EBaseSide.OWN) continue;
    float d = vector.DistanceXZ(own, other);
    if (best >= 0 && d >= bestDistance) continue;
    best = j;
    bestDistance = d;
   }
   if (best >= 0) front[best] = true;
  }
 }

 // Whether a base keeps its name: always when near; zoomed out only ours, the front, the enemy HQ,
 // a base under attack and the selected one.
 static bool KeepsName(bool far, int side, bool hq, bool contested, bool front, bool selected)
 {
  if (!far || selected || contested || front) return true;
  if (side == ACR_EBaseSide.OWN) return true;
  return hq && side == ACR_EBaseSide.ENEMY;
 }

 // ---------------------------------------------------------------- squads
 // Squads at a base become one icon: baseOf[g] the base (-1 none), count[g] the squads at it
 // (on the icon shown), rep[g] whether g is the one shown (the selected one if any, else the first).
 static void Clusters(array<vector> groupPos, array<bool> clusterable, array<bool> selected, array<vector> basePos, array<float> baseRadii, notnull array<int> baseOf, notnull array<int> count, notnull array<bool> rep)
 {
  baseOf.Clear();
  count.Clear();
  rep.Clear();
  foreach (int g, vector pos : groupPos)
  {
   int at = -1;
   float atDistance;
   if (g < clusterable.Count() && clusterable[g])
   {
    foreach (int b, vector base : basePos)
    {
     float radius = CLUSTER_MIN_RADIUS;
     if (b < baseRadii.Count()) radius = Math.Max(radius, baseRadii[b]);
     float d = vector.DistanceXZ(pos, base);
     if (d > radius || (at >= 0 && d >= atDistance)) continue;
     at = b;
     atDistance = d;
    }
   }
   baseOf.Insert(at);
   count.Insert(1);
   rep.Insert(true);
  }
  foreach (int b, vector unusedBase : basePos)
  {
   array<int> members = {};
   int shown = -1;
   foreach (int g, int at : baseOf)
   {
    if (at != b) continue;
    members.Insert(g);
    if (shown < 0 && g < selected.Count() && selected[g]) shown = g;
   }
   if (members.IsEmpty()) continue;
   if (shown < 0) shown = members[0];
   foreach (int member : members)
   {
    count[member] = members.Count();
    rep[member] = member == shown;
   }
  }
 }

 // The corner mark: fighting (a fresh contact close by), moving (an order not reached yet) or
 // holding. toTarget and nearestContact < 0: none.
 static int SquadMark(bool hasOrder, float toTarget, float nearestContact)
 {
  if (nearestContact >= 0 && nearestContact <= FIGHT_RANGE) return ACR_ESquadMark.FIGHTING;
  if (hasOrder && toTarget > ARRIVED) return ACR_ESquadMark.MOVING;
  return ACR_ESquadMark.HOLDING;
 }

 static string MarkName(int mark)
 {
  if (mark == ACR_ESquadMark.MOVING) return "moving";
  if (mark == ACR_ESquadMark.FIGHTING) return "fighting";
  return "holding";
 }

 // ---------------------------------------------------------------- INTEL
 // A purchase line ("Yung requisitioned MG Team at Lamentin (-20 RP)", or without the tag): who,
 // what, and the RP.
 static bool ParsePurchase(string line, out string tag, out string unit, out int rp)
 {
  tag = "";
  unit = "";
  rp = 0;
  int cost = line.LastIndexOf(" (-");
  if (cost < 0 || !line.EndsWith(" RP)")) return false;
  string number = line.Substring(cost + 3, line.Length() - cost - 7);
  rp = number.ToInt();
  string body = line.Substring(0, cost);
  int verb = body.IndexOf("requisitioned ");
  if (verb < 0) return false;
  if (verb > 0) tag = body.Substring(0, verb).Trim();
  string rest = body.Substring(verb + 14, body.Length() - verb - 14);
  int at = rest.LastIndexOf(" at ");
  if (at > 0) rest = rest.Substring(0, at);
  unit = rest.Trim();
  return !unit.IsEmpty();
 }

 // The feed (newest first) as shown: a run of one commander's purchases becomes one line ("Bought
 // Grenadier Team ×2, MG Team ×2 (-39 RP)"; "Popp bought ..." for another commander's), a line
 // repeated straight after itself becomes "line ×N"; only lines within MERGE_SECONDS of the run's
 // newest merge. texts and entryAges: the shown lines, newest first, and their newest line's age.
 static void MergeFeed(array<string> lines, array<int> ages, string myTag, notnull array<string> texts, notnull array<int> entryAges)
 {
  texts.Clear();
  entryAges.Clear();
  // The open run: a purchase run (tag, units and counts, RP) or a repeated line (count).
  bool buying;
  string runTag;
  string runLine;
  int runAge;
  int runCount;
  int runRp;
  array<string> units = {};
  array<int> unitCounts = {};
  foreach (int i, string line : lines)
  {
   int age = 0;
   if (i < ages.Count()) age = ages[i];
   string tag, unit;
   int rp;
   bool purchase = ParsePurchase(line, tag, unit, rp);
   bool close = runCount > 0 && age - runAge <= MERGE_SECONDS;
   if (close && purchase && buying && tag == runTag)
   {
    int known = units.Find(unit);
    if (known >= 0) unitCounts[known] = unitCounts[known] + 1;
    else
    {
     units.Insert(unit);
     unitCounts.Insert(1);
    }
    runRp += rp;
    runCount++;
    continue;
   }
   if (close && !purchase && !buying && line == runLine)
   {
    runCount++;
    continue;
   }
   if (runCount > 0) Close(texts, entryAges, buying, runTag, myTag, runLine, runAge, runCount, runRp, units, unitCounts);
   buying = purchase;
   runTag = tag;
   runLine = line;
   runAge = age;
   runCount = 1;
   runRp = rp;
   units.Clear();
   unitCounts.Clear();
   if (purchase)
   {
    units.Insert(unit);
    unitCounts.Insert(1);
   }
  }
  if (runCount > 0) Close(texts, entryAges, buying, runTag, myTag, runLine, runAge, runCount, runRp, units, unitCounts);
 }

 protected static void Close(array<string> texts, array<int> entryAges, bool buying, string tag, string myTag, string line, int age, int count, int rp, array<string> units, array<int> unitCounts)
 {
  string text = line;
  if (buying)
  {
   text = "Bought ";
   if (!tag.IsEmpty() && tag != myTag) text = tag + " bought ";
   foreach (int u, string unit : units)
   {
    if (u > 0) text += ", ";
    text += unit;
    if (unitCounts[u] > 1) text += " ×" + unitCounts[u].ToString();
   }
   text += string.Format(" (-%1 RP)", rp);
  }
  else if (count > 1) text = line + " ×" + count.ToString();
  texts.Insert(text);
  entryAges.Insert(age);
 }

 // "+N older": the lines past the INTEL_SHOWN shown.
 static int Older(int entries)
 {
  return Math.Max(0, entries - INTEL_SHOWN);
 }

 // Lines newer than the last look at the full INTEL (seenAgo s ago).
 static int NewCount(array<int> entryAges, int seenAgo)
 {
  int count;
  foreach (int age : entryAges) if (age < seenAgo) count++;
  return count;
 }

 // The one-line INTEL bar's text: our base under attack first, else the newest line, cut short.
 static string Headline(string attacked, array<string> texts)
 {
  string text = "";
  if (!attacked.IsEmpty()) text = attacked + " under attack";
  else if (texts && !texts.IsEmpty()) text = texts[0];
  else return "No contacts reported";
  if (text.Length() <= HEADLINE_CHARS) return text;
  // Cut at a space (never inside a multi-byte letter such as "×").
  string cut = text.Substring(0, HEADLINE_CHARS - 3);
  int space = cut.LastIndexOf(" ");
  if (space > 10) cut = cut.Substring(0, space);
  return cut + "...";
 }

 // ---------------------------------------------------------------- placement
 // Screen rectangles are 4 floats: x0, y0, x1, y1 (px). A list of them is a flat array.
 static bool Overlaps(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1)
 {
  return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
 }

 // The first rectangle of the list the box hits, or -1.
 static int FirstHit(float x0, float y0, float x1, float y1, array<float> rects)
 {
  for (int r = 0; r + 3 < rects.Count(); r += 4)
   if (Overlaps(x0, y0, x1, y1, rects[r], rects[r + 1], rects[r + 2], rects[r + 3])) return r / 4;
  return -1;
 }

 // Base names keep their spot unless they would cover another base's name or dot: then the name
 // goes above its dot, to its right, to its left, or further down, whichever is clear first (the
 // most important bases, first in order, choose first). dots: x, y per base; captions: each base's
 // caption rectangle relative to its dot (x1 <= x0: no caption); shift: dx, dy per base.
 static void PlaceNames(array<float> dots, array<float> captions, array<int> order, float dotRadius, float gap, notnull array<float> shift)
 {
  shift.Clear();
  int count = dots.Count() / 2;
  for (int i = 0; i < count; i++)
  {
   shift.Insert(0);
   shift.Insert(0);
  }
  // Every dot is an obstacle; placed names are added as they are placed.
  array<float> taken = {};
  for (int d = 0; d < count; d++)
  {
   taken.Insert(dots[d * 2] - dotRadius);
   taken.Insert(dots[d * 2 + 1] - dotRadius);
   taken.Insert(dots[d * 2] + dotRadius);
   taken.Insert(dots[d * 2 + 1] + dotRadius);
  }
  foreach (int b : order)
  {
   if (b < 0 || b >= count || b * 4 + 3 >= captions.Count()) continue;
   float x0 = captions[b * 4];
   float y0 = captions[b * 4 + 1];
   float x1 = captions[b * 4 + 2];
   float y1 = captions[b * 4 + 3];
   if (x1 <= x0 || y1 <= y0) continue;
   float h = y1 - y0;
   float mid = -(y0 + y1) * 0.5;
   array<float> moves = {0, 0, 0, -(y0 + y1), dotRadius + gap - x0, mid, -(dotRadius + gap) - x1, mid, 0, h + gap, 0, 2 * (h + gap)};
   float cx = dots[b * 2];
   float cy = dots[b * 2 + 1];
   int pick = 0;
   for (int m = 0; m + 1 < moves.Count(); m += 2)
   {
    // Its own dot doesn't count against it.
    float ox0 = cx + x0 + moves[m];
    float oy0 = cy + y0 + moves[m + 1];
    float ox1 = cx + x1 + moves[m];
    float oy1 = cy + y1 + moves[m + 1];
    bool clear = true;
    for (int r = 0; r + 3 < taken.Count(); r += 4)
    {
     if (r / 4 == b) continue;
     if (Overlaps(ox0, oy0, ox1, oy1, taken[r], taken[r + 1], taken[r + 2], taken[r + 3])) { clear = false; break; }
    }
    if (!clear) continue;
    pick = m;
    break;
   }
   shift[b * 2] = moves[pick];
   shift[b * 2 + 1] = moves[pick + 1];
   taken.Insert(cx + x0 + moves[pick]);
   taken.Insert(cy + y0 + moves[pick + 1]);
   taken.Insert(cx + x1 + moves[pick]);
   taken.Insert(cy + y1 + moves[pick + 1]);
  }
 }

 // A squad icon (w x h, centred at x, y) clear of the names and dots: below what it hits, else to
 // its right, its left or above it, a few times over. The new centre.
 static vector Avoid(float x, float y, float w, float h, array<float> obstacles, float gap)
 {
  float ox = x;
  float oy = y;
  for (int pass = 0; pass < 6; pass++)
  {
   int hit = FirstHit(ox - w * 0.5, oy - h * 0.5, ox + w * 0.5, oy + h * 0.5, obstacles);
   if (hit < 0) break;
   float r0 = obstacles[hit * 4];
   float r1 = obstacles[hit * 4 + 1];
   float r2 = obstacles[hit * 4 + 2];
   float r3 = obstacles[hit * 4 + 3];
   array<float> tries = {ox, r3 + gap + h * 0.5, r2 + gap + w * 0.5, oy, r0 - gap - w * 0.5, oy, ox, r1 - gap - h * 0.5};
   bool moved;
   for (int t = 0; t + 1 < tries.Count(); t += 2)
   {
    if (FirstHit(tries[t] - w * 0.5, tries[t + 1] - h * 0.5, tries[t] + w * 0.5, tries[t + 1] + h * 0.5, obstacles) >= 0) continue;
    ox = tries[t];
    oy = tries[t + 1];
    moved = true;
    break;
   }
   if (moved) break;
   // Boxed in: go below this one and look again.
   oy = r3 + gap + h * 0.5;
  }
  return Vector(ox, oy, 0);
 }

 // A round marker (radius r px, centred at x, y) touching a base's name or dot.
 static bool OnAName(float x, float y, float r, array<float> obstacles)
 {
  return FirstHit(x - r, y - r, x + r, y + r, obstacles) >= 0;
 }

 // An order's end near a base (within reach px of its dot, or on its name) ends at that base: one
 // mark for all the orders going there, none on the name. names: each base's name rectangle
 // (x1 <= x0: none), or empty. The base, or -1.
 static int SnapToBase(float x, float y, array<float> dots, float reach, array<float> names)
 {
  if (names)
  {
   for (int n = 0; n + 3 < names.Count(); n += 4)
    if (names[n + 2] > names[n] && x >= names[n] && x <= names[n + 2] && y >= names[n + 1] && y <= names[n + 3]) return n / 4;
  }
  int best = -1;
  float bestDistance = reach;
  for (int b = 0; b + 1 < dots.Count(); b += 2)
  {
   float dx = dots[b] - x;
   float dy = dots[b + 1] - y;
   float d = Math.Sqrt(dx * dx + dy * dy);
   if (d > bestDistance) continue;
   bestDistance = d;
   best = b / 2;
  }
  return best;
 }

 // ---------------------------------------------------------------- canvas shapes
 // A filled shape the game can triangulate and that shows on a width x height canvas (0: unknown):
 // three points or more, some area, finite, not wholly off it. The calm map's dots and marks go
 // through this; once map-polygon-guard (#62) is in, ACR_OrderDisplay.AddPolygon does the same.
 static bool ShapeOk(array<float> vertices, float width, float height)
 {
  if (!vertices || vertices.Count() < 6 || vertices.Count() % 2 != 0) return false;
  float minX = vertices[0];
  float maxX = minX;
  float minY = vertices[1];
  float maxY = minY;
  float twiceArea = 0;
  int points = vertices.Count() / 2;
  for (int p = 0; p < points; p++)
  {
   float x = vertices[p * 2];
   float y = vertices[p * 2 + 1];
   if (!(Math.AbsFloat(x) < 1000000) || !(Math.AbsFloat(y) < 1000000)) return false;
   minX = Math.Min(minX, x);
   maxX = Math.Max(maxX, x);
   minY = Math.Min(minY, y);
   maxY = Math.Max(maxY, y);
   int q = (p + 1) % points;
   twiceArea += (x - vertices[0]) * (vertices[q * 2 + 1] - vertices[1]) - (vertices[q * 2] - vertices[0]) * (y - vertices[1]);
  }
  if (Math.AbsFloat(twiceArea) < 0.1) return false;
  if (width <= 0 || height <= 0) return true;
  return maxX >= -2 && maxY >= -2 && minX <= width + 2 && minY <= height + 2;
 }
}
