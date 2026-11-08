// TvZ (#107): one side against the zombies, with no second side. The default zombie mode (TvTvZ)
// keeps both sides fighting each other while Project Outbreak's undead threaten both (ACR_Outbreak).
// In TvZ the players' side ("living") plays alone and the other side ("absent") never shows up:
//  - its bases are open towns from the start, its starting units and spawn points are dropped, it
//    has no AI commander and isn't offered on the deploy screen; every player joins the living side.
//  - its HQ becomes the hive: the biggest nest on the map, as far from the living side as it gets.
//  - zombies are the only enemy: more nests and a higher cap (ACR_Outbreak.Apply asks Share,
//    Country, Cap), hordes more often (HORDE_SCALE), and night raids hit HQs too.
//  - overrun: with no second economy to protect, zombies may take a base back. A held base (not
//    the home HQ, a FOB or a pocket) with zombies inside and none of its side for OVERRUN_MS falls
//    open and a nest settles in it (ACR_BaseSystem.UpdateCapture). A night raid on such a base
//    stays out in it until it falls (ACR_Outbreak.StayOut), then settles there itself.
// Option -acrZombieMode tvtvz|tvz (setup row "Zombie mode", shown only with Outbreak loaded). TvZ
// needs zombies: with -acrZombies off it plays as "few". Never on flashpoints (no zombies there).
class ACR_TvZ
{
 static const float OVERRUN_MS = 60000;   // zombies alone inside a held base this long: it falls
 static const int SETTLE_MIN = 4;         // a nest settling in an overrun base: at least this many
 static const int SETTLE_MAX = 16;        // ...and at most
 static const float HORDE_SCALE = 0.6;    // hordes leave nests this much sooner than in TvTvZ
 protected static bool s_bApplied;
 protected static string s_sLiving;
 protected static string s_sAbsent;
 protected static ACR_Base s_Home;         // the living side's HQ: never overrun
 protected static ACR_Base s_Hive;         // the absent side's HQ: the biggest nest

 static void Reset()
 {
  s_bApplied = false;
  s_sLiving = "";
  s_sAbsent = "";
  s_Home = null;
  s_Hive = null;
 }

 static string ModeOption()
 {
  string value = ACR_TestHooks.Get().ZombieMode();
  if (value == "" && !ACR_Options.Get("acrZombieMode", value)) value = "";
  value.ToLower();
  if (value == "tvz") return value;
  return "tvtvz";
 }

 // TvZ is being played: asked for, Outbreak loaded, not a flashpoint.
 static bool On()
 {
  if (ModeOption() != "tvz" || !ACR_Outbreak.Loaded()) return false;
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  return size != ACR_ESkirmishSize.FLASHPOINT;
 }

 // The players' side: -acrPlayers if it names one of the pair ("west"/"east" too), else West.
 static string Living()
 {
  if (!s_sLiving.IsEmpty()) return s_sLiving;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string value;
  if (!ACR_Options.Get("acrPlayers", value)) return west;
  value.TrimInPlace();
  value.ToLower();
  string eastLower = east;
  eastLower.ToLower();
  if (value == "east" || value == eastLower) return east;
  return west;
 }

 static string Absent()
 {
  if (!s_sAbsent.IsEmpty()) return s_sAbsent;
  if (Living() == ACR_FactionSelection.East()) return ACR_FactionSelection.West();
  return ACR_FactionSelection.East();
 }

 static bool IsAbsent(string factionKey)
 {
  return On() && factionKey == Absent();
 }

 static ACR_Base Hive() { return s_Hive; }

 // Server, after the layout and quick start, before FIA and zombies (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  if (s_bApplied || !On()) return;
  s_bApplied = true;
  s_sLiving = Living();
  s_sAbsent = Absent();
  s_Home = HQOf(s_sLiving);
  s_Hive = HQOf(s_sAbsent);
  // Starting units belong to the side whose base is nearest (as ACR_Mode spawns them): the absent
  // side's are dropped before its bases open up, or they'd join the living side.
  int units;
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   if (NearestOwner(unit.GetOrigin()) != s_sAbsent) continue;
   unit.m_bDropped = true;
   units++;
  }
  int bases;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner != s_sAbsent) continue;
   base.m_sOwner = "";
   base.m_fScore = 0;
   base.m_sCapturer = "";
   base.m_iGarrisonLeft = base.m_iGarrison;
   bases++;
  }
  int points;
  array<SCR_SpawnPoint> spawns = {};
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints()) if (point && point.GetFactionKey() == s_sAbsent) spawns.Insert(point);
  foreach (SCR_SpawnPoint gone : spawns) { SCR_EntityHelper.DeleteEntityAndChildren(gone); points++; }
  ACR_Skirmish.RecountPads();
  ACR_FactionSettings settings = ACR_FactionSettings.Find(s_sAbsent);
  if (settings) settings.m_bAICommander = false;
  ACR_PlayerSide.Force(s_sLiving);
  ACR_FactionSelection.SetPlayable(s_sLiving, s_sLiving);
  string home = "none";
  string hive = "none";
  if (s_Home) home = s_Home.m_sName;
  if (s_Hive) hive = s_Hive.m_sName;
  PrintFormat("[ACR] TVZ %1 against the zombies (%2 absent): %3 bases opened, %4 starting units and %5 spawn points dropped; home %6, hive %7", s_sLiving, s_sAbsent, bases, units, points, home, hive);
 }

 // ---------------------------------------------------------------- nest density (ACR_Outbreak.Apply)

 // Zombies are the only enemy: half the open bases on few, most of them on many.
 static float Share(string level)
 {
  if (level == "many") return 0.8;
  return 0.5;
 }

 static int Country(int country)
 {
  return Math.Ceil(country * 1.5);
 }

 // How many zombies may be on the map at once (nests, hordes and the risen dead).
 static int Cap(int cap)
 {
  return Math.Round(cap * 1.5);
 }

 // ---------------------------------------------------------------- overrun (ACR_BaseSystem.UpdateCapture)

 static bool CanFall(ACR_Base base)
 {
  if (!On() || base.m_sOwner != s_sLiving || base == s_Home) return false;
  return !base.m_bBuilt && !base.m_bPocket && !base.m_bRetired && base.m_eLogisticsRole != ACR_EBaseRole.HQ;
 }

 // Every base tick for a held base: zombies inside, its side not. True: it falls now.
 static bool Overrun(ACR_Base base, int zombies, int holders)
 {
  if (zombies <= 0 || holders > 0 || !CanFall(base))
  {
   base.m_fZombiesSince = 0;
   return false;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  if (base.m_fZombiesSince <= 0) base.m_fZombiesSince = now;
  return now - base.m_fZombiesSince >= OVERRUN_MS;
 }

 // The base has fallen open (ACR_BaseSystem.Overrun): the zombies that took it settle in. A raid
 // aimed at it settles there itself when it packs up; a nest already there grows.
 static void Fallen(ACR_Base base, int zombies)
 {
  base.m_fZombiesSince = 0;
  int size = Math.ClampInt(zombies, SETTLE_MIN, SETTLE_MAX);
  string how = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone) continue;
   if (post.m_Nest.m_Raid == base) { how = post.m_sName + " settles there"; break; }
   if (!post.m_Nest.m_bRoaming && post.m_Nest.m_sBase == base.m_sName)
   {
    post.m_Nest.m_iSize = Math.Min(ACR_Outbreak.NEST_MAX, Math.Max(post.m_Nest.m_iSize, size));
    if (!post.m_Group) post.m_iReserve = Math.Max(post.m_iReserve, size);
    how = post.m_sName + " grows";
    break;
   }
  }
  if (how.IsEmpty())
  {
   ACR_DefenderPost nest = ACR_Outbreak.AddNest(base.GetOrigin(), size, base.m_sName);
   how = string.Format("%1 (%2) settles", nest.m_sName, nest.m_iReserve);
  }
  PrintFormat("[ACR] TVZ %1 overrun by %2 zombies: open, %3", base.m_sName, zombies, how);
 }

 // ---------------------------------------------------------------- helpers

 // A side's HQ: its authored HQ-role base, else its owned base nearest its spawn point.
 protected static ACR_Base HQOf(string side)
 {
  foreach (ACR_Base hq : ACR_Base.s_All)
   if (!hq.m_bRetired && hq.m_sOwner == side && !hq.m_bPocket && hq.m_eLogisticsRole == ACR_EBaseRole.HQ) return hq;
  vector home;
  bool found;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == side) { home = point.GetOrigin(); found = true; break; }
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner != side || base.m_bPocket) continue;
   float distance = 0;
   if (found) distance = vector.DistanceXZ(base.GetOrigin(), home);
   if (distance < bestDistance) { bestDistance = distance; best = base; }
  }
  return best;
 }

 protected static string NearestOwner(vector position)
 {
  string owner = "";
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner == "" || base.m_bPocket) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), position);
   if (distance < bestDistance) { bestDistance = distance; owner = base.m_sOwner; }
  }
  return owner;
 }
}
