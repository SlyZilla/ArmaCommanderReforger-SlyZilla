// -acrPocketsTest 1 (server, Everon whole map, ~40 s): FIA pockets of resistance (ACR_Pockets).
// Checks: a few neutral bases start held by FIA with garrisons by kind of site, away from both
// starting bases and from each other; both sides see them as unclaimed; a US soldier 300 m from
// one finds it (US now sees FIA, USSR still doesn't) and its garrison deploys; a capture ends it.
class ACR_TestPockets
{
 static const ResourceName US_RIFLEMAN = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 protected static ACR_Base s_Target;
 protected static IEntity s_Scout;

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
   ACR_TestRegress.Complete("pockets", 10);
   return;
  }
  ACR_TestRegress.Check("pockets: the scout finds the pocket", s_Target.OwnerSeenBy("US") == ACR_Pockets.FACTION && s_Target.m_aDetectedBy.Contains("US"), s_Target.m_sName);
  ACR_TestRegress.Check("pockets: the other side still sees it as unclaimed", s_Target.OwnerSeenBy("USSR") == "", "");
  int garrison;
  if (s_Target.m_GarrisonGroup) garrison = s_Target.m_GarrisonGroup.GetAgentsCount();
  ACR_TestRegress.Check("pockets: its FIA garrison deploys", garrison > 0, string.Format("%1 soldiers", garrison));
  manager.m_Bases.DebugCapture(manager, s_Target, "US");
  ACR_TestRegress.Check("pockets: a capture ends the pocket", !s_Target.m_bPocket && s_Target.m_sOwner == "US" && s_Target.OwnerSeenBy("USSR") == "US", s_Target.m_sOwner);
  if (s_Scout) SCR_EntityHelper.DeleteEntityAndChildren(s_Scout);
  ACR_TestRegress.Complete("pockets", 10);
 }
}
