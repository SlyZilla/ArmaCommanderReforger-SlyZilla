// Scenario generator (board aca-f07, 14 Oct). Builds bases, HQs and starting units from the map's
// own named places, by the rules of Jacob's hand-built Everon layout:
//   city r90 / town r75 / village r60 / settlement r45: value 1
//   military site r60: value 3; industry (harbour, factory, power plant, sawmill, quarry, mine) r50: value 2
//   airport r80: value 3; a few landmarks r40: value 1, hidden
//   HQs: value 2, radius >= 70, garrison 6, owned; other bases start neutral with no garrison.
// Every base sits on its map place (moved onto dry ground when the place is in water).
// Runs instead of a hand-built layout when the host picks "Generated" (acrLayout generated) or
// the world has no bases (e.g. ACR_Kolguyev). Same seed + size + world = same layout.
// Sizes: full map; half (the sites on one side of the map's long axis); flashpoint (about seven
// sites within 1.8 km of one town, HQs inside it at opposite ends).
class ACR_GenSite
{
 string m_sName;
 string m_sKind;     // city town village settlement military airport industry landmark
 vector m_vPos;
 float m_fRadius;
 int m_iValue;
 int m_iRank;        // which of two overlapping sites stays
 string m_sKey;      // the map's location key, e.g. Military, Harbour, Cain_Kamensk
 string m_sPrefab;
}

class ACR_Generator
{
 protected static bool s_bBuilt;
 static int s_iSeed;
 static ref array<ref ACR_GenSite> s_Sites = {};   // the layout in play (after Build)
 protected static ref array<IEntity> s_Found = {};

 static void Reset()
 {
  s_bBuilt = false;
  s_iSeed = 0;
  s_Sites.Clear();
 }

 static bool Built() { return s_bBuilt; }

 // The host asked for a generated layout, or the world has no bases of its own.
 static bool Wanted()
 {
  string layout;
  if (ACR_Options.Get("acrLayout", layout))
  {
   layout.ToLower();
   if (layout == "generated") return true;
  }
  return !HasOwnBases();
 }

 // The world was laid out by hand (Arland, Everon); else only generated layouts exist.
 static bool HasOwnBases()
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bBuilt) return true;
  return false;
 }

 // The seed in use: the host's (acrSeed), else a random one, remembered so the log and the
 // setup screen can show it.
 static int Seed()
 {
  string text;
  int seed = 0;
  if (ACR_Options.Get("acrSeed", text)) seed = text.ToInt();
  if (seed <= 0)
  {
   seed = Math.RandomInt(1, 100000);
   if (seed < 0) seed = -seed;
   if (seed == 0) seed = 1;
   ACR_Options.Set("acrSeed", seed.ToString());
  }
  return seed;
 }

 // ---------------------------------------------------------------- reading the map
 protected static int s_iHouses;

 protected static bool CountHouse(IEntity e)
 {
  if (!e) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e)) { s_iHouses++; return true; }
  EntityPrefabData data = e.GetPrefabData();
  if (data && data.GetPrefabName().Contains("/Buildings/")) s_iHouses++;
  return true;
 }

 // Buildings within a radius (a named place with none is a field or a pass, not a settlement).
 static int Houses(vector at, float radius)
 {
  s_iHouses = 0;
  GetGame().GetWorld().QueryEntitiesBySphere(at, radius, CountHouse);
  return s_iHouses;
 }

 protected static bool Collect(IEntity e)
 {
  if (e && e.FindComponent(MapDescriptorComponent)) s_Found.Insert(e);
  return true;
 }

 // Every usable named place of the world, classified and named.
 static void Survey(notnull array<ref ACR_GenSite> sites)
 {
  sites.Clear();
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  world.GetBoundBox(mins, maxs);
  s_Found.Clear();
  world.QueryEntitiesByAABB(Vector(mins[0], -500, mins[2]), Vector(maxs[0], 3000, maxs[2]), Collect);
  array<ref ACR_GenSite> pending = {};
  foreach (IEntity e : s_Found)
  {
   MapDescriptorComponent d = MapDescriptorComponent.Cast(e.FindComponent(MapDescriptorComponent));
   if (!d) continue;
   int type = d.GetBaseType();
   MapItem item = d.Item();
   if (!item) continue;
   string raw = item.GetDisplayName();
   ACR_GenSite site = new ACR_GenSite();
   site.m_vPos = e.GetOrigin();
   site.m_sKey = raw;
   site.m_sKey.Replace("#AR-MapLocation_", "");
   site.m_sKey.Replace("#AR-Campaign_MapLocation_", "");
   if (e.GetPrefabData())
   {
    string extension;
    site.m_sPrefab = FilePath.StripExtension(FilePath.StripPath(e.GetPrefabData().GetPrefabName()), extension);
   }
   string shown = raw;
   if (raw.StartsWith("#")) shown = WidgetManager.Translate(raw);
   if (!Classify(site, type, shown)) continue;
   pending.Insert(site);
  }
  // A stable order, so the same seed always picks the same sites.
  while (!pending.IsEmpty())
  {
   int best = 0;
   for (int i = 1; i < pending.Count(); i++)
   {
    if (pending[i].m_vPos[0] < pending[best].m_vPos[0]) best = i;
    else if (pending[i].m_vPos[0] == pending[best].m_vPos[0] && pending[i].m_vPos[2] < pending[best].m_vPos[2]) best = i;
   }
   sites.Insert(pending[best]);
   pending.RemoveOrdered(best);
  }
  NameIndustry(sites);
 }

 // Kind, radius, value and (for settlements and landmarks) the name. False = not a base.
 protected static bool Classify(ACR_GenSite site, int type, string shown)
 {
  string key = site.m_sKey;
  if (type == EMapDescriptorType.MDT_NAME_CITY) return Set(site, "city", 90, 1, 3, shown);
  if (type == EMapDescriptorType.MDT_NAME_TOWN) return Set(site, "town", 75, 1, 3, shown);
  if (type == EMapDescriptorType.MDT_NAME_VILLAGE) return Set(site, "village", 60, 1, 2, shown);
  if (type == EMapDescriptorType.MDT_NAME_SETTLEMENT) return Set(site, "settlement", 45, 1, 1, shown);
  // Plain labels ("farm", "harbor" as a local name) are not places of their own.
  if (shown != "" && !shown.StartsWith("#"))
  {
   int first = shown.Get(0).ToAscii();
   if (first >= 97 && first <= 122 && type != EMapDescriptorType.MDT_NAME_GENERIC) return false;
  }
  if (type == EMapDescriptorType.MDT_NAME_LOCAL)
  {
   if (key.Contains("Mine")) return Set(site, "industry", 50, 2, 4, shown);
   if (shown == "" || shown.StartsWith("#")) return false;
   return Set(site, "landmark", 40, 1, 0, shown);
  }
  if (type != EMapDescriptorType.MDT_NAME_GENERIC) return false;
  if (key == "Military") return Set(site, "military", 60, 3, 5, "");
  if (key == "Airport") return Set(site, "airport", 80, 3, 4, "");
  if (key == "Harbour" || key == "Factory" || key == "Industrial" || key == "PowerPlant" || key == "Sawmill" || key.Contains("Quarry")) return Set(site, "industry", 50, 2, 4, "");
  if (key == "Landfill") return Set(site, "landmark", 40, 1, 0, shown);
  if (key == "Farm") return false;
  // Any other named place (Cain_Langonnet, Cain_Villa...) is a settlement when houses stand
  // there; fields, passes and heaths (Everon's Ramtop Meadows, Saint-Pierre's Pass) are not.
  if (shown == "" || shown.StartsWith("#")) return false;
  if (Houses(site.m_vPos, 120) < 6) return false;
  return Set(site, "settlement", 45, 1, 1, shown);
 }

 protected static bool Set(ACR_GenSite site, string kind, float radius, int value, int rank, string name)
 {
  site.m_sKind = kind;
  site.m_fRadius = radius;
  site.m_iValue = value;
  site.m_iRank = rank;
  site.m_sName = name;
  return true;
 }

 // Industry and military sites are named after the place they belong to: the prefab's own
 // place name when it has one (Langonnet_Military_01 -> "Langonnet Base"), else the nearest
 // settlement ("Chotain Base").
 protected static void NameIndustry(array<ref ACR_GenSite> sites)
 {
  array<string> used = {};
  foreach (ACR_GenSite named : sites) if (named.m_sName != "") used.Insert(named.m_sName);
  foreach (ACR_GenSite site : sites)
  {
   if (site.m_sName != "") continue;
   string suffix = Suffix(site);
   string place = PrefabPlace(site.m_sPrefab);
   if (place == "" && site.m_sKind != "airport") place = NearestSettlement(sites, site.m_vPos);
   string name = (place + " " + suffix).Trim();
   int n = 2;
   string tryName = name;
   while (used.Contains(tryName)) { tryName = string.Format("%1 %2", name, n); n++; }
   site.m_sName = tryName;
   used.Insert(tryName);
  }
 }

 protected static string Suffix(ACR_GenSite site)
 {
  string key = site.m_sKey;
  if (site.m_sKind == "military") return "Base";
  if (site.m_sKind == "airport") return "Airport";
  if (key == "Harbour") return "Harbor";
  if (key == "PowerPlant") return "Power Plant";
  if (key.Contains("Quarry")) return "Quarry";
  return key;
 }

 // "BareRidge_Military_01" -> "Bare Ridge"; "" for generic prefabs (LocationUrbanGeneric_...).
 protected static string PrefabPlace(string prefab)
 {
  if (prefab == "" || prefab.StartsWith("Location")) return "";
  array<string> parts = {};
  prefab.Split("_", parts, true);
  if (parts.Count() < 2) return "";
  string word = parts[0];
  string spaced;
  for (int i = 0; i < word.Length(); i++)
  {
   string c = word.Get(i);
   int code = c.ToAscii();
   if (i > 0 && code >= 65 && code <= 90)
   {
    int before = word.Get(i - 1).ToAscii();
    if (before >= 97 && before <= 122) spaced += " ";
   }
   spaced += c;
  }
  return spaced;
 }

 protected static string NearestSettlement(array<ref ACR_GenSite> sites, vector at)
 {
  string best = "";
  float bestDistance = float.MAX;
  foreach (ACR_GenSite site : sites)
  {
   if (site.m_sKind != "city" && site.m_sKind != "town" && site.m_sKind != "village" && site.m_sKind != "settlement") continue;
   float distance = vector.DistanceXZ(site.m_vPos, at);
   if (distance < bestDistance) { bestDistance = distance; best = site.m_sName; }
  }
  return best;
 }

 // ---------------------------------------------------------------- choosing the layout
 // The layout for a seed and size, without touching the world (tests call this to compare).
 // size: -1 full, else ACR_ESkirmishSize. hqs[0] = west HQ, hqs[1] = east HQ.
 static bool Plan(int seed, int size, notnull array<ref ACR_GenSite> layout, notnull array<ACR_GenSite> hqs)
 {
  layout.Clear();
  hqs.Clear();
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed(seed);
  array<ref ACR_GenSite> sites = {};
  Survey(sites);
  // Dry ground, and no two capture circles too close: the higher-ranked site stays.
  array<ref ACR_GenSite> kept = {};
  foreach (ACR_GenSite site : sites)
  {
   if (!Dry(site)) continue;
   kept.Insert(site);
  }
  array<ref ACR_GenSite> spaced = {};
  for (int rank = 5; rank >= 0; rank--)
  {
   foreach (ACR_GenSite candidate : kept)
   {
    if (candidate.m_iRank != rank || candidate.m_sKind == "landmark") continue;
    if (Fit(spaced, candidate)) spaced.Insert(candidate);
   }
  }
  // A few hidden landmarks, spread out: about one for every ten other bases.
  array<ACR_GenSite> marks = {};
  foreach (ACR_GenSite mark : kept) if (mark.m_sKind == "landmark") marks.Insert(mark);
  float others = spaced.Count();
  int wantMarks = Math.Round(others / 10);
  if (wantMarks < 2) wantMarks = 2;
  int tries = 0;
  int added = 0;
  while (added < wantMarks && !marks.IsEmpty() && tries < 200)
  {
   tries++;
   int pick = Math.Floor(rng.RandFloat01() * marks.Count());
   if (pick >= marks.Count()) pick = marks.Count() - 1;
   ACR_GenSite chosen = marks[pick];
   marks.Remove(pick);
   if (!Fit(spaced, chosen)) continue;
   bool farFromMarks = true;
   foreach (ACR_GenSite other : spaced) if (other.m_sKind == "landmark" && vector.DistanceXZ(other.m_vPos, chosen.m_vPos) < 600) farFromMarks = false;
   if (!farFromMarks) continue;
   spaced.Insert(chosen);
   added++;
  }
  // Size: the sites in play.
  array<ACR_GenSite> pool = {};
  foreach (ACR_GenSite s : spaced) pool.Insert(s);
  if (size == ACR_ESkirmishSize.HALF) pool = Half(pool, rng);
  else if (size == ACR_ESkirmishSize.FLASHPOINT) pool = Flashpoint(pool, rng);
  if (pool.Count() < 3)
  {
   PrintFormat("[ACR] GENERATOR only %1 sites for this size: nothing generated", pool.Count());
   return false;
  }
  // HQs: two military sites (or the airport) far apart; a flashpoint uses its two ends.
  ACR_GenSite west, east;
  PickHQs(pool, size == ACR_ESkirmishSize.FLASHPOINT, rng, west, east);
  if (!west || !east) return false;
  if (rng.RandFloat01() < 0.5) { ACR_GenSite swap = west; west = east; east = swap; }
  hqs.Insert(west);
  hqs.Insert(east);
  foreach (ACR_GenSite p : pool) layout.Insert(p);
  // HQs get a 70 m circle where there is room for it.
  foreach (ACR_GenSite hq : hqs)
  {
   float allowed = 70;
   foreach (ACR_GenSite near : layout)
   {
    if (near == hq) continue;
    float room = vector.DistanceXZ(near.m_vPos, hq.m_vPos) - near.m_fRadius - 10;
    if (room < allowed) allowed = room;
   }
   if (allowed > hq.m_fRadius) hq.m_fRadius = allowed;
  }
  return true;
 }

 // The place, or dry ground within 100 m of it.
 protected static bool Dry(ACR_GenSite site)
 {
  if (!BBB_Terrain.IsWater(site.m_vPos[0], site.m_vPos[2])) return true;
  for (int ring = 20; ring <= 100; ring += 20)
  {
   for (int step = 0; step < 12; step++)
   {
    float angle = Math.PI2 * step / 12.0;
    float x = site.m_vPos[0] + Math.Cos(angle) * ring;
    float z = site.m_vPos[2] + Math.Sin(angle) * ring;
    if (BBB_Terrain.IsWater(x, z)) continue;
    site.m_vPos = Vector(x, site.m_vPos[1], z);
    return true;
   }
  }
  return false;
 }

 // Fits a site among those placed: its capture circle shrinks (to 35 m at least) to stay 10 m
 // clear of every placed circle, like Saint-Philippe and its harbour on Everon. False = no room.
 protected static bool Fit(array<ref ACR_GenSite> placed, ACR_GenSite site)
 {
  float allowed = site.m_fRadius;
  foreach (ACR_GenSite other : placed)
  {
   float room = vector.DistanceXZ(other.m_vPos, site.m_vPos) - other.m_fRadius - 10;
   if (room < allowed) allowed = room;
  }
  if (allowed < 35) return false;
  site.m_fRadius = allowed;
  return true;
 }

 // The map's long axis = the two sites farthest apart; a half = the sites on one side of the
 // middle across it.
 protected static array<ACR_GenSite> Half(array<ACR_GenSite> pool, RandomGenerator rng)
 {
  ACR_GenSite a, b;
  Farthest(pool, a, b);
  array<ACR_GenSite> result = {};
  if (!a) return result;
  vector axis = b.m_vPos - a.m_vPos;
  axis[1] = 0;
  float length = axis.Length();
  if (length < 1) return result;
  axis = axis / length;
  array<float> along = {};
  foreach (ACR_GenSite site : pool) along.Insert(vector.Dot(site.m_vPos - a.m_vPos, axis));
  array<float> sorted = {};
  sorted.Copy(along);
  sorted.Sort();
  float middle = sorted[sorted.Count() / 2];
  bool first = rng.RandFloat01() < 0.5;
  foreach (int i, ACR_GenSite s : pool)
  {
   bool low = along[i] < middle;
   if (low == first) result.Insert(s);
  }
  return result;
 }

 // About seven sites round a town or village (within 1.8 km), at least five.
 protected static array<ACR_GenSite> Flashpoint(array<ACR_GenSite> pool, RandomGenerator rng)
 {
  array<ACR_GenSite> centres = {};
  foreach (ACR_GenSite site : pool) if (site.m_sKind == "city" || site.m_sKind == "town" || site.m_sKind == "village") centres.Insert(site);
  if (centres.IsEmpty()) centres.Copy(pool);
  array<ACR_GenSite> result = {};
  for (int attempt = 0; attempt < 20 && !centres.IsEmpty(); attempt++)
  {
   int pick = Math.Floor(rng.RandFloat01() * centres.Count());
   if (pick >= centres.Count()) pick = centres.Count() - 1;
   ACR_GenSite centre = centres[pick];
   centres.Remove(pick);
   result.Clear();
   array<ACR_GenSite> near = {};
   foreach (ACR_GenSite s : pool) if (vector.DistanceXZ(s.m_vPos, centre.m_vPos) <= 1800) near.Insert(s);
   while (result.Count() < 7 && !near.IsEmpty())
   {
    int best = 0;
    for (int i = 1; i < near.Count(); i++)
     if (vector.DistanceXZ(near[i].m_vPos, centre.m_vPos) < vector.DistanceXZ(near[best].m_vPos, centre.m_vPos)) best = i;
    result.Insert(near[best]);
    near.Remove(best);
   }
   if (result.Count() >= 5) return result;
  }
  return result;
 }

 protected static void Farthest(array<ACR_GenSite> pool, out ACR_GenSite a, out ACR_GenSite b)
 {
  float best = -1;
  for (int i = 0; i < pool.Count(); i++)
   for (int j = i + 1; j < pool.Count(); j++)
   {
    float d = vector.DistanceXZ(pool[i].m_vPos, pool[j].m_vPos);
    if (d > best) { best = d; a = pool[i]; b = pool[j]; }
   }
 }

 // HQ pair: among military sites and airports (else every site), one of the three pairs
 // farthest apart (by seed). A flashpoint uses its two farthest sites.
 protected static void PickHQs(array<ACR_GenSite> pool, bool flashpoint, RandomGenerator rng, out ACR_GenSite west, out ACR_GenSite east)
 {
  array<ACR_GenSite> candidates = {};
  if (!flashpoint)
   foreach (ACR_GenSite site : pool) if (site.m_sKind == "military" || site.m_sKind == "airport") candidates.Insert(site);
  // Never a hidden landmark.
  if (candidates.Count() < 2)
  {
   candidates.Clear();
   foreach (ACR_GenSite open : pool) if (open.m_sKind != "landmark") candidates.Insert(open);
  }
  if (candidates.Count() < 2) candidates.Copy(pool);
  if (flashpoint) { Farthest(candidates, west, east); return; }
  // The three farthest pairs.
  array<float> dist = {0, 0, 0};
  array<int> ia = {-1, -1, -1};
  array<int> ib = {-1, -1, -1};
  for (int i = 0; i < candidates.Count(); i++)
   for (int j = i + 1; j < candidates.Count(); j++)
   {
    float d = vector.DistanceXZ(candidates[i].m_vPos, candidates[j].m_vPos);
    for (int k = 0; k < 3; k++)
    {
     if (d <= dist[k]) continue;
     for (int m = 2; m > k; m--) { dist[m] = dist[m - 1]; ia[m] = ia[m - 1]; ib[m] = ib[m - 1]; }
     dist[k] = d; ia[k] = i; ib[k] = j;
     break;
    }
   }
  int count = 0;
  for (int c = 0; c < 3; c++) if (ia[c] >= 0) count++;
  if (count == 0) return;
  int choice = Math.Floor(rng.RandFloat01() * count);
  if (choice >= count) choice = count - 1;
  west = candidates[ia[choice]];
  east = candidates[ib[choice]];
 }

 // ---------------------------------------------------------------- building it
 // Server, from ACR_Skirmish.ApplyOnce (after the faction pair is applied, before any tick).
 static void Build()
 {
  if (s_bBuilt) return;
  s_bBuilt = true;
  s_iSeed = Seed();
  int size = ACR_Skirmish.SizeOption();
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  if (!Plan(s_iSeed, size, layout, hqs))
  {
   Print("[ACR] GENERATOR failed: playing the world as placed");
   return;
  }
  string westSide = ACR_FactionSelection.West();
  string eastSide = ACR_FactionSelection.East();
  // Hand-built bases and pads go; their starting units follow their side to the new HQ.
  map<ACR_StartingUnit, string> unitSides = new map<ACR_StartingUnit, string>();
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All) unitSides.Set(unit, NearestOwner(unit.GetOrigin()));
  array<ACR_Base> old = {};
  old.Copy(ACR_Base.s_All);
  foreach (ACR_Base gone : old) if (!gone.m_bBuilt) gone.Retire("generated layout");
  foreach (ACR_Helipad pad : ACR_Helipad.s_All) pad.Drop();
  ACR_Skirmish.RecountPads();
  // The new bases.
  ACR_Base westHQ, eastHQ;
  foreach (ACR_GenSite site : layout)
  {
   string owner = "";
   if (site == hqs[0]) owner = westSide;
   else if (site == hqs[1]) owner = eastSide;
   ACR_Base base = MakeBase(site, owner);
   if (site == hqs[0]) westHQ = base;
   if (site == hqs[1]) eastHQ = base;
   s_Sites.Insert(site);
  }
  // Starting units: the world's own, else a default force per side, at each side's HQ.
  if (ACR_StartingUnit.s_All.IsEmpty())
  {
   DefaultForce(westHQ, true);
   DefaultForce(eastHQ, false);
  }
  else
  {
   foreach (ACR_StartingUnit u, string side : unitSides)
   {
    if (side == eastSide) u.m_MovedTo = eastHQ;
    else u.m_MovedTo = westHQ;
    if (u.m_MovedTo) u.SetOrigin(u.m_MovedTo.GetOrigin());
   }
  }
  // Player spawn points go to their side's HQ.
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   string key = point.GetFactionKey();
   if (key == "US") key = westSide;
   else if (key == "USSR") key = eastSide;
   ACR_Base target = westHQ;
   if (key == eastSide) target = eastHQ;
   if (!target) continue;
   point.SetFactionKey(key);
   vector at = ACR_Manager.Ground(target.GetOrigin() + Vector(6, 0, 6));
   vector free;
   if (SCR_WorldTools.FindEmptyTerrainPosition(free, at, 20, 1.5, 2.5)) at = ACR_Manager.Ground(free);
   point.SetOrigin(at);
  }
  string sizeName = "full";
  if (size >= 0) sizeName = ACR_Skirmish.SizeName(size);
  PrintFormat("[ACR] GENERATED seed %1 size %2: %3 bases; HQs %4=%5 %6=%7, %8 m apart", s_iSeed, sizeName, layout.Count(), westSide, hqs[0].m_sName, eastSide, hqs[1].m_sName, (int)vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos));
  foreach (ACR_GenSite s : layout) PrintFormat("[ACR] GEN BASE %1 | %2 | %3 %4 | v%5 r%6", s.m_sName, s.m_sKind, (int)s.m_vPos[0], (int)s.m_vPos[2], s.m_iValue, (int)s.m_fRadius);
 }

 protected static ACR_Base MakeBase(ACR_GenSite site, string owner)
 {
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = ACR_Manager.Ground(site.m_vPos);
  ACR_Base base = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), parameters));
  base.m_sName = site.m_sName;
  base.m_iValue = site.m_iValue;
  base.m_fRadius = site.m_fRadius;
  base.m_bIncome = true;
  base.m_bHidden = site.m_sKind == "landmark";
  base.m_bAllowSpawn = true;
  base.m_iGarrison = 0;
  base.m_sOwner = owner;
  base.m_sCapturer = "";
  base.m_fScore = 0;
  if (owner != "")
  {
   base.m_iValue = 2;
   base.m_iGarrison = 6;
   base.m_bHidden = false;
   base.m_fScore = 100;
  }
  base.m_iGarrisonLeft = base.m_iGarrison;
  return base;
 }

 // Two rifle squads, a weapons team, a truck and a car (as on Everon), in the side's colours
 // through the faction swaps.
 protected static void DefaultForce(ACR_Base hq, bool west)
 {
  if (!hq) return;
  if (west)
  {
   Unit(hq, "1st Squad", "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", "");
   Unit(hq, "2nd Squad", "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", "");
   Unit(hq, "Weapons Team", "{958039B857396B7B}Prefabs/Groups/BLUFOR/Group_US_MachineGunTeam.et", "");
   Unit(hq, "Truck 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et");
   Unit(hq, "Humvee 1", "", "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et");
   return;
  }
  Unit(hq, "Soviet 1st Squad", "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", "");
  Unit(hq, "Soviet 2nd Squad", "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", "");
  Unit(hq, "Soviet Weapons Team", "{A2F75E45C66B1C0A}Prefabs/Groups/OPFOR/Group_USSR_MachineGunTeam.et", "");
  Unit(hq, "Ural 1", "", "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et");
  Unit(hq, "UAZ 1", "", "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et");
 }

 protected static void Unit(ACR_Base hq, string name, ResourceName group, ResourceName vehicle)
 {
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = hq.GetOrigin();
  ACR_StartingUnit unit = ACR_StartingUnit.Cast(GetGame().SpawnEntity(ACR_StartingUnit, GetGame().GetWorld(), parameters));
  unit.m_sName = name;
  unit.m_sGroup = group;
  unit.m_sVehicle = vehicle;
  unit.m_bCrewed = true;
  unit.m_MovedTo = hq;
 }

 protected static string NearestOwner(vector position)
 {
  string owner = "";
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == "") continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), position);
   if (distance < bestDistance) { bestDistance = distance; owner = base.m_sOwner; }
  }
  return owner;
 }
}
