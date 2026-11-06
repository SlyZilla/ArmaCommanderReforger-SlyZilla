// -acrPocketsTest 1 (server, Everon whole map, ~40 s): FIA pockets of resistance (ACR_Pockets).
// Checks: a few neutral bases start held by FIA with garrisons by kind of site, away from both
// starting bases and from each other; both sides see them as unclaimed; a US soldier 300 m from
// one finds it (US now sees FIA, USSR still doesn't) and its garrison deploys; a capture ends it
// and its garrison's survivors fall back, then join another pocket's reserve (none vanish).
class ACR_TestPockets
{
 static const ResourceName US_RIFLEMAN = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 protected static ACR_Base s_Target;
 protected static IEntity s_Scout;
 protected static int s_iSurvivors;
 protected static int s_iOtherMen;   // FIA men at the other pockets (reserve + patrols) before the capture
 static const int CHECKS = 14;

 static void Run(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (tick == 10) Placement(manager);
  if (tick == 25) Found(manager);
  if (tick == 40) Rejoined(manager);
 }

 protected static int OtherPocketMen(ACR_Manager manager)
 {
  int men;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == s_Target || base.m_bRetired || base.m_sOwner != ACR_Pockets.FACTION) continue;
   men += base.m_iGarrisonLeft + manager.m_Bases.GuardsAlive(base.m_PatrolGroup) + manager.m_Bases.GuardsAlive(base.m_GarrisonGroup);
  }
  return men;
 }

 protected static void Placement(ACR_Manager manager)
 {
  array<ACR_Base> pockets = {};
  array<ACR_Base> starts = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bPocket) pockets.Insert(base);
   else if (!base.m_sOwner.IsEmpty()) starts.Insert(base);
  }
  ACR_TestRegress.Check("pockets: a few FIA pockets on the whole map", pockets.Count() >= 5 && pockets.Count() <= 7, string.Format("%1 pockets", pockets.Count()));
  bool owners = true;
  bool garrisons = true;
  bool clear = true;
  bool spaced = true;
  bool hidden = true;
  string detail = "";
  float nearestToUS = float.MAX;
  foreach (ACR_Base pocket : pockets)
  {
   if (pocket.m_sOwner != ACR_Pockets.FACTION) owners = false;
   int low = 4;
   int high = 6;
   if (pocket.m_iValue == 2) { low = 6; high = 8; }
   if (pocket.m_iValue >= 3) { low = 8; high = 10; }
   if (pocket.m_iGarrison < low || pocket.m_iGarrison > high) garrisons = false;
   foreach (ACR_Base start : starts)
   {
    float d = vector.DistanceXZ(pocket.GetOrigin(), start.GetOrigin());
    if (d < 1500) clear = false;
    if (start.m_sOwner == "US" && d < nearestToUS) { nearestToUS = d; s_Target = pocket; }
   }
   foreach (ACR_Base other : pockets)
    if (other != pocket && vector.DistanceXZ(pocket.GetOrigin(), other.GetOrigin()) < ACR_Pockets.MIN_SPACING) spaced = false;
   if (pocket.OwnerSeenBy("US") != "" || pocket.OwnerSeenBy("USSR") != "" || !manager.m_Bases.Describe(pocket, "US").StartsWith("Unclaimed")) hidden = false;
   detail += string.Format(" %1[%2]=%3", pocket.m_sName, pocket.m_iValue, pocket.m_iGarrison);
  }
  ACR_TestRegress.Check("pockets: owned by FIA", owners && !pockets.IsEmpty(), detail);
  ACR_TestRegress.Check("pockets: garrison by kind of site (4-6 / 6-8 / 8-10)", garrisons, detail);
  ACR_TestRegress.Check("pockets: 1500 m from both starting bases", clear, "");
  ACR_TestRegress.Check("pockets: 800 m apart", spaced, "");
  ACR_TestRegress.Check("pockets: both sides see them as unclaimed", hidden, "");
  if (!s_Target) return;
  // A US scout inside the garrison's spawn range but outside the capture circle.
  vector toward = vector.Zero;
  foreach (ACR_Base home : starts) if (home.m_sOwner == "US") toward = home.GetOrigin();
  vector dir = toward - s_Target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  vector spot = ACR_Manager.Ground(s_Target.GetOrigin() + dir * 300);
  s_Scout = ACR_Manager.SpawnPrefab(US_RIFLEMAN, spot);
  PrintFormat("[ACR] POCKETS test: scout at %1, 300 m from %2", spot, s_Target.m_sName);
 }

 protected static void Found(ACR_Manager manager)
 {
  if (!s_Target)
  {
   ACR_TestRegress.Complete("pockets", CHECKS);
   return;
  }
  ACR_TestRegress.Check("pockets: the scout finds the pocket", s_Target.OwnerSeenBy("US") == ACR_Pockets.FACTION && s_Target.m_aDetectedBy.Contains("US"), s_Target.m_sName);
  ACR_TestRegress.Check("pockets: the other side still sees it as unclaimed", s_Target.OwnerSeenBy("USSR") == "", "");
  int garrison;
  if (s_Target.m_GarrisonGroup) garrison = s_Target.m_GarrisonGroup.GetAgentsCount();
  ACR_TestRegress.Check("pockets: its FIA garrison deploys", garrison > 0, string.Format("%1 soldiers", garrison));
  // #73: at the town with the most buildings, garrison spots are clear (the garrison's own navmesh).
  IEntity navAgent;
  if (s_Target.m_GarrisonGroup) navAgent = s_Target.m_GarrisonGroup.GetLeaderAgent();
  ACR_TestGarrisonSpawn.Check(navAgent);
  if (s_Target.m_GarrisonGroup) s_iSurvivors = manager.m_Bases.GuardsAlive(s_Target.m_GarrisonGroup);
  s_iOtherMen = OtherPocketMen(manager);
  manager.m_Bases.DebugCapture(manager, s_Target, "US");
  ACR_TestRegress.Check("pockets: a capture ends the pocket", !s_Target.m_bPocket && s_Target.m_sOwner == "US" && s_Target.OwnerSeenBy("USSR") == "US", s_Target.m_sOwner);
  // Jacob, 6 Nov: an FIA team was simply gone once its town fell.
  int falling = manager.m_Bases.FallingBack(ACR_Pockets.FACTION);
  ACR_TestRegress.Check("pockets: its garrison's survivors fall back, not deleted", s_iSurvivors > 0 && falling == s_iSurvivors, string.Format("%1 of %2", falling, s_iSurvivors));
  // The scout goes: nobody near them, they pack up into another pocket's reserve.
  if (s_Scout) SCR_EntityHelper.DeleteEntityAndChildren(s_Scout);
 }

 protected static void Rejoined(ACR_Manager manager)
 {
  if (!s_Target) return;
  int falling = manager.m_Bases.FallingBack(ACR_Pockets.FACTION);
  int now = OtherPocketMen(manager);
  ACR_TestRegress.Check("pockets: clear of the enemy, they leave the field", falling == 0, string.Format("%1 still falling back", falling));
  ACR_TestRegress.Check("pockets: ... into another pocket's reserve", now == s_iOtherMen + s_iSurvivors, string.Format("other pockets %1 -> %2, survivors %3", s_iOtherMen, now, s_iSurvivors));
  ACR_TestRegress.Complete("pockets", CHECKS);
 }
}
