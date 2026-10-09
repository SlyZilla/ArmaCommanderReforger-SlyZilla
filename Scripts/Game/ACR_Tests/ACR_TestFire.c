// -acrFireTest 1 (server-capable, ~3 min): faction-specific forward observer and mortar teams,
// a faction mortar mission, then mortar fire-at-will against a known enemy contact.
class ACR_TestFire
{
 static ref ACR_Squad s_Observer;
 static ref ACR_MortarTeam s_Team;
 static SCR_AIGroup s_Enemy;
 static vector s_Target;
 static bool s_bTargetPlaced;
 static int s_iRP;
 static bool s_bMissionAccepted;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Mortars.DEPLOY_SECONDS = 10;
  }
  ACR_FactionData us = manager.m_Bases.GetFaction(ACR_FactionSelection.West());
  if (tick == 4)
  {
   ACR_Base depot;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == ACR_FactionSelection.West() && base.m_bAllowSpawn && !base.m_bContested) { depot = base; break; }
   us.m_iRP = 1000;
   int listed;
   foreach (int i, ACR_RequisitionEntry entry : us.m_aRequisitions)
   {
    if (entry.m_bObserverTeam) { listed++; PrintFormat("[ACR] FIRETEST buy observers: %1", manager.PurchaseAt(ACR_FactionSelection.West(), i, depot)); }
    if (entry.m_bMortarTeam) PrintFormat("[ACR] FIRETEST buy mortar: %1", manager.PurchaseAt(ACR_FactionSelection.West(), i, depot));
   }
   ACR_TestRegress.Check("fire: forward observer team in the buy list", listed == 1, "");
  }
  if (tick == 8)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().Contains(ACR_FireMissions.ENTRY_NAME)) s_Observer = squad;
   array<ACR_MortarTeam> teams = {};
   manager.Mortars().TeamsOf(ACR_FactionSelection.West(), teams);
   if (!teams.IsEmpty()) s_Team = teams[0];
   ACR_TestRegress.Check("fire: observer team registered with fire at will, counts as an observer", s_Observer && s_Observer.m_bFireAtWill && ACR_FireMissions.IsObserver(s_Observer), "");
   if (s_Observer && s_Observer.GetAnchor())
   {
    vector eye = s_Observer.GetAnchor().GetOrigin();
    // Several clear bearings make at least one enemy group fall in the observers' view cone.
    int targets;
    for (int k = 0; k < 12 && targets < 3; k++)
    {
     float a = k * 0.5236;
     vector spot = ACR_Manager.Ground(eye + Vector(Math.Cos(a) * 170, 0, Math.Sin(a) * 170));
     TraceParam trace = new TraceParam();
     trace.Start = eye + Vector(0, 1.7, 0);
     trace.End = spot + Vector(0, 1.2, 0);
     trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
     if (GetGame().GetWorld().TraceMove(trace, null) < 0.99 || BBB_Terrain.IsWaterAt(spot)) continue;
     s_Target = spot;
     s_bTargetPlaced = true;
     s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, ACR_FactionSelection.East()), spot));
     if (s_Enemy) targets++;
     PrintFormat("[ACR] FIRETEST target group %1 at %2, %3 m from the observers", targets, spot, (int)vector.DistanceXZ(spot, eye));
    }
    if (targets == 0) { s_Target = ACR_Manager.Ground(eye + Vector(170, 0, 0)); s_bTargetPlaced = true; s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, ACR_FactionSelection.East()), s_Target)); PrintFormat("[ACR] FIRETEST fallback target at %1", s_Target); }
   }
  }
  if (ACR_FireMissions.s_iCalls > 0 && tick < 100 && s_Observer) s_Observer.m_bFireAtWill = false;
  if (tick > 8 && tick % 10 == 0)
  {
   string team = "none";
   if (s_Team) team = s_Team.m_sStatus;
   int seen;
   ACR_Intel intel = manager.IntelFor(ACR_FactionSelection.West());
   if (intel && s_Observer) foreach (ACR_Contact c : intel.m_Contacts) if (c.m_sSpotter == s_Observer.GetName()) seen++;
   PrintFormat("[ACR] FIRETEST t=%1 mortar %2 | contacts by the observers %3 | calls %4 free fires %5 | RP %6", tick, team, seen, ACR_FireMissions.s_iCalls, ACR_FireMissions.s_iFreeFires, us.m_iRP);
  }
  if (tick == 100)
  {
   bool accepted = ACR_FireMissions.s_iCalls > 0 || s_bMissionAccepted;
   ACR_TestRegress.Check("fire: the faction mortar accepts a mission on the test target", accepted, string.Format("automatic calls %1, direct test mission accepted %2", ACR_FireMissions.s_iCalls, s_bMissionAccepted));
   if (s_Observer) s_Observer.m_bFireAtWill = false;
   if (s_Team && s_Team.m_Squad) s_Team.m_Squad.m_bFireAtWill = true;
   s_iRP = us.m_iRP;
   if (ACR_FactionSelection.West() == "RHS_USAF" || ACR_FactionSelection.West() == "RHS_AFRF") { ACR_TestRegress.Complete("fire"); return; }
  }
  if (tick == 30 && ACR_FireMissions.s_iCalls == 0 && s_bTargetPlaced)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   int index = -1;
   foreach (int i, ACR_SupportEntry entry : manager.m_Bases.GetFaction(ACR_FactionSelection.West()).m_aSupports)
    if (entry.m_eType == ACR_ESupportType.MORTAR_HE_SMALL) index = i;
   if (index >= 0)
   {
    string result = manager.m_Supports.Call(ACR_FactionSelection.West(), index, s_Target, true);
    s_bMissionAccepted = result.Contains("firing");
    PrintFormat("[ACR] FIRETEST faction mortar mission: %1", result);
   }
  }
  if (tick == 170)
  {
   ACR_TestRegress.Check("fire: a mortar team with fire at will fires on its own, for no RP", ACR_FireMissions.s_iFreeFires > 0 && us.m_iRP >= s_iRP, string.Format("%1 free fires, RP %2 -> %3", ACR_FireMissions.s_iFreeFires, s_iRP, us.m_iRP));
   ACR_TestRegress.Complete("fire");
  }
 }
}
