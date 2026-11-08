// srv:tvz (Everon whole map, about 1.5 min): TvZ, one side against the zombies (ACR_TvZ, #107).
// Without Project Outbreak loaded: no TvZ, both sides keep their bases (nothing changes).
// With it (swap Outbreak into tools/testserver-args.txt for the run): the absent side owns no base,
// has no spawn point, no AI commander and no squads; players are put on the living side, which
// alone is offered; the absent side's HQ is the hive; zombies left alone inside a held base for a
// minute overrun it: it goes open and a nest settles in it.
class ACR_TestTvZ
{
 protected static ACR_Base s_Target;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10)
  {
   if (!ACR_Outbreak.Loaded()) { WithoutOutbreak(); return; }
   Sides();
  }
  if (!ACR_Outbreak.Loaded()) return;
  if (second == 12) ZombiesInside(manager);
  if (second == 30) NoAbsentSquads(manager);
  if (second == 85) Overrun();
 }

 protected static void WithoutOutbreak()
 {
  bool west, east;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == ACR_FactionSelection.West()) west = true;
   if (base.m_sOwner == ACR_FactionSelection.East()) east = true;
  }
  Check("tvz: without Outbreak, TvZ is off", !ACR_TvZ.On(), "");
  Check("tvz: without Outbreak, both sides hold bases", west && east, "");
  ACR_TestRegress.Complete("tvz-off", 2);
 }

 protected static void Sides()
 {
  string living = ACR_TvZ.Living();
  string absent = ACR_TvZ.Absent();
  int held, absentHeld, points;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == living) held++;
   if (base.m_sOwner == absent) absentHeld++;
  }
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints()) if (point && point.GetFactionKey() == absent) points++;
  Check("tvz: on, the living side holds bases, the absent side none", ACR_TvZ.On() && held > 0 && absentHeld == 0, string.Format("%1 held by %2, %3 by %4", held, living, absentHeld, absent));
  ACR_FactionSettings settings = ACR_FactionSettings.Find(absent);
  Check("tvz: the absent side has no spawn point and no AI commander", points == 0 && (!settings || !settings.m_bAICommander), string.Format("%1 spawn points", points));
  SCR_Faction absentFaction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(absent));
  Check("tvz: players join the living side, the absent side isn't offered", ACR_PlayerSide.Key() == living && absentFaction && !absentFaction.IsPlayable(), ACR_PlayerSide.Key());
  ACR_Base hive = ACR_TvZ.Hive();
  ACR_DefenderPost hiveNest;
  int atBases;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_Nest.m_sBase.IsEmpty()) continue;
   atBases++;
   if (hive && post.m_Nest.m_sBase == hive.m_sName) hiveNest = post;
  }
  string hiveText = "none";
  if (hiveNest) hiveText = string.Format("%1 (%2)", hiveNest.m_sName, hiveNest.m_iReserve);
  Check("tvz: the absent side's HQ is the hive, the biggest nest", hiveNest && hiveNest.m_iReserve >= 12, hiveText);
  Check("tvz: nests in at least half the open bases", atBases >= 3, string.Format("%1 base nests", atBases));
 }

 // An open base far from the living side's bases becomes the living side's, with no garrison,
 // and zombies are put inside it.
 protected static void ZombiesInside(ACR_Manager manager)
 {
  string living = ACR_TvZ.Living();
  float best = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_sOwner.IsEmpty() || base.m_bPocket || base.m_bBuilt || base == ACR_TvZ.Hive()) continue;
   float nearest = 100000;
   foreach (ACR_Base held : ACR_Base.s_All) if (held.m_sOwner == living) nearest = Math.Min(nearest, vector.DistanceXZ(held.GetOrigin(), base.GetOrigin()));
   if (nearest > best) { best = nearest; s_Target = base; }
  }
  if (!s_Target) return;
  s_Target.m_sOwner = living;
  s_Target.m_fScore = 100;
  s_Target.m_iGarrison = 0;
  s_Target.m_iGarrisonLeft = 0;
  SCR_AIGroup zombies = manager.m_Bases.SpawnGuards(ACR_Outbreak.FACTION, s_Target.GetOrigin(), 6, "TvZ test zombies", s_Target.m_fRadius, 10);
  PrintFormat("[ACR] TVZ test: %1 given to %2, zombies inside: %3", s_Target.m_sName, living, zombies != null);
 }

 protected static void NoAbsentSquads(ACR_Manager manager)
 {
  string absent = ACR_TvZ.Absent();
  int squads, living;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   if (squad.GetFaction().GetFactionKey() == absent) squads++;
   if (squad.GetFaction().GetFactionKey() == ACR_TvZ.Living()) living++;
  }
  Check("tvz: the absent side has no squads", squads == 0, string.Format("%1 absent, %2 living squads", squads, living));
 }

 protected static void Overrun()
 {
  if (!s_Target) { Check("tvz: zombies alone in a held base overrun it", false, "no base to test"); Finish(); return; }
  bool nest;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Nest && !post.m_bGone && post.m_Nest.m_sBase == s_Target.m_sName) nest = true;
  Check("tvz: zombies alone in a held base overrun it, a nest settles", s_Target.m_sOwner.IsEmpty() && nest, string.Format("%1 owner '%2', nest %3", s_Target.m_sName, s_Target.m_sOwner, nest));
  Finish();
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }

 protected static void Finish()
 {
  ACR_TestRegress.Complete("tvz", 7);
 }
}
