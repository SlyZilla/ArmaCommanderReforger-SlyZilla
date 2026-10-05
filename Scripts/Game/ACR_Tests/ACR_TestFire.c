// -acrFireTest 1 (server-capable, ~3 min): fire missions. The US buys a mortar team and a forward
// observer team at its base; a Soviet fire team stands ~280 m from the observers. The observers
// (fire at will on, as bought) must call a mortar mission on it; then the observers stop and the
// mortar team, with fire at will, must fire on the (still seen) Soviets on its own, for no RP.
class ACR_TestFire
{
 static ref ACR_Squad s_Observer;
 static ref ACR_MortarTeam s_Team;
 static SCR_AIGroup s_Enemy;
 static int s_iRP;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Mortars.DEPLOY_SECONDS = 10;
  }
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (tick == 4)
  {
   ACR_Base depot;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == "US" && base.m_bAllowSpawn && !base.m_bContested) { depot = base; break; }
   us.m_iRP = 1000;
   int listed;
   foreach (int i, ACR_RequisitionEntry entry : us.m_aRequisitions)
   {
    if (entry.m_bObserverTeam) { listed++; PrintFormat("[ACR] FIRETEST buy observers: %1", manager.PurchaseAt("US", i, depot)); }
    if (entry.m_bMortarTeam) PrintFormat("[ACR] FIRETEST buy mortar: %1", manager.PurchaseAt("US", i, depot));
   }
   ACR_TestRegress.Check("fire: forward observer team in the buy list", listed == 1, "");
  }
  if (tick == 8)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().Contains(ACR_FireMissions.ENTRY_NAME)) s_Observer = squad;
   array<ACR_MortarTeam> teams = {};
   manager.Mortars().TeamsOf("US", teams);
   if (!teams.IsEmpty()) s_Team = teams[0];
   ACR_TestRegress.Check("fire: observer team registered with fire at will, counts as an observer", s_Observer && s_Observer.m_bFireAtWill && ACR_FireMissions.IsObserver(s_Observer), "");
   if (s_Observer && s_Observer.GetAnchor())
   {
    vector eye = s_Observer.GetAnchor().GetOrigin();
    // Open ground ~280 m away: the first direction whose line is clear at head height.
    vector spot = ACR_Manager.Ground(eye + Vector(280, 0, 0));
    for (int k = 0; k < 12; k++)
    {
     float a = k * 0.5236;
     vector probe = ACR_Manager.Ground(eye + Vector(Math.Cos(a) * 280, 0, Math.Sin(a) * 280));
     TraceParam trace = new TraceParam();
     trace.Start = eye + Vector(0, 1.7, 0);
     trace.End = probe + Vector(0, 1.2, 0);
     trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
     if (GetGame().GetWorld().TraceMove(trace, null) >= 0.99 && !BBB_Terrain.IsWaterAt(probe)) { spot = probe; break; }
    }
    s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, "USSR"), spot));
    PrintFormat("[ACR] FIRETEST Soviet fire team at %1, %2 m from the observers", spot, (int)vector.DistanceXZ(spot, eye));
   }
  }
  if (tick > 8 && tick % 10 == 0)
  {
   string team = "none";
   if (s_Team) team = s_Team.m_sStatus;
   int seen;
   ACR_Intel intel = manager.IntelFor("US");
   if (intel && s_Observer) foreach (ACR_Contact c : intel.m_Contacts) if (c.m_sSpotter == s_Observer.GetName()) seen++;
   PrintFormat("[ACR] FIRETEST t=%1 mortar %2 | contacts by the observers %3 | calls %4 free fires %5 | RP %6", tick, team, seen, ACR_FireMissions.s_iCalls, ACR_FireMissions.s_iFreeFires, us.m_iRP);
  }
  if (tick == 100)
  {
   ACR_TestRegress.Check("fire: the observers call a mortar mission on what they see", ACR_FireMissions.s_iCalls > 0, string.Format("%1 calls", ACR_FireMissions.s_iCalls));
   if (s_Observer) s_Observer.m_bFireAtWill = false;
   if (s_Team && s_Team.m_Squad) s_Team.m_Squad.m_bFireAtWill = true;
   s_iRP = us.m_iRP;
  }
  if (tick == 170)
  {
   ACR_TestRegress.Check("fire: a mortar team with fire at will fires on its own, for no RP", ACR_FireMissions.s_iFreeFires > 0 && us.m_iRP >= s_iRP, string.Format("%1 free fires, RP %2 -> %3", ACR_FireMissions.s_iFreeFires, s_iRP, us.m_iRP));
   ACR_TestRegress.Complete("fire");
  }
 }
}
