// Containment (#107, -acrOutbreak contain; setup row "Outbreak start"): the outbreak starts in one
// town and spreads, instead of nests everywhere from the start (ACR_Outbreak). Built to reward,
// not to press: no timer, no way to lose, no victory (none in any mode).
//  - Ground zero: one big nest in an open town as far from the starting bases as it gets (in TvZ
//    the hive). It refills fast (GZ_GROW_MS) and sends hordes often (GZ_HORDE_MS), so the dead
//    march out town by town; the towns they settle in grow and spread as ordinary nests. Outbreak's
//    zombie cap stops it. Both sides are told where it started and mark it as a zombie area.
//  - Survivors: a town a horde sets out for (not a raid, not one already nested) has people holed
//    up in it: a site on both sides' maps ("Survivors (n)"). Any soldier of a side within
//    RESCUE_RANGE with no zombie out within ZOMBIE_CLEAR rescues them: RP_EACH RP each for that
//    side (capped like income). The horde arriving with nobody there: they join it (within the cap).
//    A site nobody reaches goes quietly after SITE_MS (they made their own way out).
//  - AI commanders send one squad at a time to the best site they can reach (ManageRescues), day or
//    night; players race them for it.
//  - Bounties: clearing a nest pays the sides near it RP_PER_ZOMBIE per zombie it could hold, ground
//    zero RP_GROUND_ZERO (ACR_Outbreak.PackedUp). Both sides hear how far it has spread (Level).
class ACR_SurvivorSite : Managed
{
 vector m_vPos;
 string m_sTown;
 int m_iCount;
 float m_fSince;
}

class ACR_Containment
{
 static const float RESCUE_RANGE = 75;     // m: a soldier this close to the site rescues them ...
 static const float ZOMBIE_CLEAR = 100;    // ... with no zombie out this close
 static const int RP_EACH = 5;             // RP per survivor rescued
 static const int RP_PER_ZOMBIE = 2;       // a cleared nest pays this per zombie it could hold
 static const int RP_GROUND_ZERO = 60;     // ground zero cleared
 static const float SITE_MS = 2400000;     // a site nobody reaches goes after 40 min
 static const float OVERRUN_REACH = 250;   // a horde arriving this close to a site takes them
 static const float GUARDED = 300;         // ... unless a soldier is this close
 static const float GZ_GROW_MS = 75000;    // ground zero gets one back this often (night: a third)
 static const float GZ_HORDE_MS = 720000;  // and sends a horde about this often (night: a third)
 static const int GZ_HORDE_MIN = 16;       // once it has this many
 static const float FIRST_HORDE_MIN = 360000; // the first horde leaves 6-10 min in
 static const float FIRST_HORDE_MAX = 600000;
 static const int START_SITES = 2;         // towns round ground zero with survivors at the start
 static const float START_REACH = 3000;

 protected static bool s_bOn;
 protected static string s_sGroundZero;
 protected static ref array<ref ACR_SurvivorSite> s_aSites;
 protected static int s_iSeconds;
 protected static int s_iLevel;
 static int s_iRescued;   // survivors rescued this match, any side (tests)
 static int s_iLost;      // survivors the dead took (tests)
 static int s_iSitesMade; // sites made (tests)

 static void ACR_InitStatics()
 {
  s_aSites = {};
 }

 static void Reset()
 {
  s_bOn = false;
  s_sGroundZero = "";
  s_aSites.Clear();
  s_iSeconds = 0;
  s_iLevel = -1;
  s_iRescued = 0;
  s_iLost = 0;
  s_iSitesMade = 0;
 }

 static string Option()
 {
  string value = ACR_TestHooks.Get().Outbreak();
  if (value == "" && !ACR_Options.Get("acrOutbreak", value)) value = "";
  value.ToLower();
  if (value == "contain") return value;
  return "nests";
 }

 // Asked for (ACR_Outbreak.Apply decides whether it plays).
 static bool Asked() { return Option() == "contain"; }

 static bool On() { return s_bOn; }

 static string GroundZero() { return s_sGroundZero; }

 static bool IsGroundZero(ACR_DefenderPost post)
 {
  return post && post.m_Nest && post.m_Nest.m_bGroundZero;
 }

 // ---------------------------------------------------------------- start (ACR_Outbreak.Apply)

 // 'post' is ground zero, the nest in 'town'.
 static void Start(ACR_DefenderPost post, ACR_Base town)
 {
  s_bOn = true;
  s_sGroundZero = town.m_sName;
  float now = GetGame().GetWorld().GetWorldTime();
  post.m_Nest.m_bGroundZero = true;
  post.m_Nest.m_fHordeAt = now + Math.RandomFloat(FIRST_HORDE_MIN, FIRST_HORDE_MAX);
  post.m_sName = string.Format("Ground zero at %1", town.m_sName);
  PrintFormat("[ACR] CONTAIN ground zero at %1 (%2), %3 zombies", town.m_sName, ACR_Manager.Grid(town.GetOrigin()), post.m_iReserve);
  array<string> sides1 = Sides();
  foreach (string side : sides1)
  {
   ACR_Insurgency.MarkZombies(side, town.GetOrigin(), "");
   Feed(side, string.Format("Outbreak reported at %1. Contain it, and get the survivors out", town.m_sName), town.GetOrigin());
  }
  // The towns nearest ground zero have people in them from the start.
  array<ACR_Base> near = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == town || base.m_bRetired || base.m_bHidden || !base.m_sOwner.IsEmpty()) continue;
   if (vector.DistanceXZ(base.GetOrigin(), town.GetOrigin()) > START_REACH) continue;
   near.Insert(base);
  }
  for (int n = 0; n < START_SITES && !near.IsEmpty(); n++)
  {
   int best = -1;
   float bestDistance = START_REACH + 1;
   foreach (int i, ACR_Base candidate : near)
   {
    float d = vector.DistanceXZ(candidate.GetOrigin(), town.GetOrigin());
    if (d < bestDistance) { bestDistance = d; best = i; }
   }
   AddSurvivors(near[best].GetOrigin(), near[best].m_sName);
   near.Remove(best);
  }
 }

 // ---------------------------------------------------------------- survivors

 // People holed up in 'town' at 'pos' (a horde is on its way, or ground zero is near).
 static void AddSurvivors(vector pos, string town)
 {
  if (!s_bOn) return;
  foreach (ACR_SurvivorSite known : s_aSites) if (vector.DistanceXZ(known.m_vPos, pos) < OVERRUN_REACH) return;
  ACR_SurvivorSite site = new ACR_SurvivorSite();
  site.m_vPos = pos;
  site.m_sTown = town;
  site.m_iCount = Math.RandomIntInclusive(4, 12);
  site.m_fSince = GetGame().GetWorld().GetWorldTime();
  s_aSites.Insert(site);
  s_iSitesMade++;
  PrintFormat("[ACR] CONTAIN %1 survivors holed up at %2 (%3)", site.m_iCount, Where(site), ACR_Manager.Grid(pos));
  array<string> sides2 = Sides();
  foreach (string side : sides2) Feed(side, string.Format("Survivors holed up at %1 (%2). The dead are coming", Where(site), site.m_iCount), pos);
 }

 static bool Has(ACR_SurvivorSite site)
 {
  return site != null && s_aSites.Contains(site);
 }

 static int SiteCount() { return s_aSites.Count(); }

 static int LevelForTest() { return s_iLevel; }

 static ACR_SurvivorSite SiteAt(int i) { return s_aSites[i]; }

 protected static string Where(ACR_SurvivorSite site)
 {
  if (!site.m_sTown.IsEmpty()) return site.m_sTown;
  return ACR_Manager.Grid(site.m_vPos);
 }

 // A horde arrives where it was going (ACR_Outbreak.Arrive, nobody near it): survivors there with
 // nobody guarding them join it. Returns how many joined.
 static int HordeArrives(ACR_Manager manager, ACR_DefenderPost horde, int room)
 {
  if (!s_bOn || !manager) return 0;
  int joined;
  for (int i = s_aSites.Count() - 1; i >= 0; i--)
  {
   ACR_SurvivorSite site = s_aSites[i];
   if (vector.DistanceXZ(site.m_vPos, horde.m_vPos) > OVERRUN_REACH) continue;
   bool guarded;
   array<string> sides3 = Sides();
   foreach (string side : sides3) if (ACR_Outbreak.SideNear(manager, side, site.m_vPos, GUARDED)) guarded = true;
   if (guarded) continue;
   int add = Math.Min(site.m_iCount, Math.Max(0, room - joined));
   joined += add;
   s_iLost += site.m_iCount;
   PrintFormat("[ACR] CONTAIN %1 overran the survivors at %2: %3 lost, %4 join it", horde.m_sName, Where(site), site.m_iCount, add);
   array<string> sides4 = Sides();
   foreach (string feedSide : sides4) Feed(feedSide, string.Format("The dead overran %1. %2 survivors lost", Where(site), site.m_iCount), site.m_vPos);
   s_aSites.Remove(i);
  }
  return joined;
 }

 protected static void Rescue(ACR_Manager manager, ACR_SurvivorSite site, string side)
 {
  int rp = Give(manager, side, site.m_iCount * RP_EACH);
  s_iRescued += site.m_iCount;
  PrintFormat("[ACR] CONTAIN %1 rescued %2 survivors at %3 (+%4 RP)", side, site.m_iCount, Where(site), rp);
  Feed(side, string.Format("%1 survivors rescued at %2: +%3 RP", site.m_iCount, Where(site), rp), site.m_vPos);
  array<string> sides5 = Sides();
  foreach (string other : sides5)
   if (other != side) Feed(other, string.Format("The enemy got the survivors out of %1", Where(site)), site.m_vPos);
  s_aSites.RemoveItem(site);
 }

 // A zombie of ACA's nests out and alive within range of pos.
 protected static bool ZombiesNear(ACR_Manager manager, vector pos, float range)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone || !post.m_Group) continue;
   array<AIAgent> agents = {};
   post.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity zombie = agent.GetControlledEntity();
    if (zombie && ACR_Respawn.IsAlive(zombie) && vector.DistanceXZ(zombie.GetOrigin(), pos) <= range) return true;
   }
  }
  return false;
 }

 // ---------------------------------------------------------------- bounties

 // A nest of 'post' is cleared (ACR_Outbreak.PackedUp): RP for a side near it; 0 with none.
 static int Bounty(ACR_DefenderPost post)
 {
  if (!s_bOn || !post.m_Nest) return 0;
  if (post.m_Nest.m_bGroundZero) return RP_GROUND_ZERO;
  return Math.Max(post.m_Nest.m_iSize, 4) * RP_PER_ZOMBIE;
 }

 // Adds RP to a side up to the cap; what it actually got.
 static int Give(ACR_Manager manager, string side, int amount)
 {
  if (!manager || !manager.m_Bases || amount <= 0) return 0;
  ACR_FactionData data = manager.m_Bases.GetFaction(side);
  if (!data) return 0;
  int before = data.m_iRP;
  data.m_iRP = Math.Min(data.m_iRP + amount, ACR_BaseSystem.POINTS_CAP);
  return Math.Max(0, data.m_iRP - before);
 }

 // ---------------------------------------------------------------- every second (ACR_Manager.Tick)

 static void Tick()
 {
  if (!s_bOn || !Replication.IsServer()) return;
  s_iSeconds++;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (s_iSeconds % 5 == 0) CheckSites(manager);
  if (s_iSeconds % 60 == 0) Level(manager);
 }

 protected static void CheckSites(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aSites.Count() - 1; i >= 0; i--)
  {
   ACR_SurvivorSite site = s_aSites[i];
   if (now - site.m_fSince > SITE_MS)
   {
    PrintFormat("[ACR] CONTAIN the survivors at %1 made their own way out", Where(site));
    s_aSites.Remove(i);
    continue;
   }
   if (ZombiesNear(manager, site.m_vPos, ZOMBIE_CLEAR)) continue;
   array<string> sides6 = Sides();
   foreach (string side : sides6)
   {
    if (!ACR_Outbreak.SideNear(manager, side, site.m_vPos, RESCUE_RANGE)) continue;
    Rescue(manager, site, side);
    break;
   }
  }
 }

 // How far it has spread, told to both sides when that changes: 0 contained, 1 one town,
 // 2 spreading (2-4 nests or hordes), 3 out of control (5+).
 protected static void Level(ACR_Manager manager)
 {
  int nests;
  int hordes;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone) continue;
   if (post.m_Nest.m_bRoaming) hordes++;
   else nests++;
  }
  int level = 3;
  if (nests + hordes == 0) level = 0;
  else if (nests + hordes == 1) level = 1;
  else if (nests + hordes <= 4) level = 2;
  if (level == s_iLevel) return;
  bool first = s_iLevel < 0;
  s_iLevel = level;
  if (first && level == 1) return; // the start was already told
  string line = string.Format("Outbreak spreading: %1 nests, %2 hordes on the move, about %3 dead walking", nests, hordes, ACR_Outbreak.Total());
  if (level == 0) line = "The outbreak is contained: no nests left";
  else if (level == 1) line = "The outbreak is down to one nest";
  else if (level == 3) line = string.Format("Outbreak out of control: %1 nests, %2 hordes on the move, about %3 dead walking", nests, hordes, ACR_Outbreak.Total());
  PrintFormat("[ACR] CONTAIN level %1: %2 nests, %3 hordes, %4 zombies", level, nests, hordes, ACR_Outbreak.Total());
  array<string> sides7 = Sides();
  foreach (string side : sides7) Feed(side, line, vector.Zero);
 }

 // ---------------------------------------------------------------- shared

 protected static array<string> Sides()
 {
  array<string> sides = {};
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  if (!west.IsEmpty() && !ACR_TvZ.IsAbsent(west)) sides.Insert(west);
  if (!east.IsEmpty() && !ACR_TvZ.IsAbsent(east)) sides.Insert(east);
  return sides;
 }

 protected static void Feed(string side, string line, vector at)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), line, at);
 }

 // The side's command map: a ring per survivor site (ACR_MapFeed).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  if (!s_bOn || ACR_TvZ.IsAbsent(faction)) return;
  foreach (ACR_SurvivorSite site : s_aSites)
  {
   positions.Insert(site.m_vPos);
   radii.Insert(RESCUE_RANGE);
   labels.Insert(string.Format("Survivors (%1)", site.m_iCount));
  }
 }
}
