// -acrMortarGiveUpTest 1 (server-capable, ~60 s): Jacob's 27 Oct game sent Red's mortar team to
// Bovemont eight times in 30 min; each time it packed up, couldn't path there, gave up within a
// minute and set up again at home. A team sent to one of our bases (as PositionMortars sends it)
// that gives up on the way leaves that base out of the AI's mortar moves for 15 min; while it is
// still on its way, nothing is blocked.
class ACR_TestMortarGiveUp
{
 static ACR_MortarTeam s_Team;
 static ACR_Base s_Base;

 static string State()
 {
  if (!s_Team || !s_Team.m_Squad) return "no team";
  return string.Format("state %1, order '%2', serial %3", typename.EnumToString(ACR_EMortarState, s_Team.m_eState), s_Team.m_Squad.m_Order, s_Team.m_Squad.m_iOrderSerial);
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string west = ACR_FactionSelection.West();
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find(ACR_FactionSelection.East());
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find(west);
   if (us) us.m_bAICommander = false;
   ACR_Mortars.DEPLOY_SECONDS = 10;
   ACR_Mortars.PACK_SECONDS = 6;
  }
  if (tick == 3)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == west && !base.m_bRetired) { s_Base = base; break; }
   if (!s_Base) { ACR_TestRegress.Abort("mortargiveup", "no base of " + west); return; }
   manager.Mortars().SpawnTeam(west, "Mortar team 1", ACR_Manager.Ground(s_Base.GetOrigin() + Vector(s_Base.m_fRadius + 250, 0, 0)));
   s_Team = manager.Mortars().m_Teams[manager.Mortars().m_Teams.Count() - 1];
  }
  if (!s_Team || !s_Team.m_Squad || tick <= 3) return;
  if (tick % 5 == 0) PrintFormat("[ACR] MORTARGIVEUP t=%1: %2", tick, State());
  ACR_AICommander ai = manager.FobAICommanderFor(west);
  if (tick == 25) ai.SendMortarForTest(s_Team.m_Squad, s_Base);
  if (tick == 40)
  {
   ai.NoteMortarGiveUpsForTest();
   ACR_TestRegress.Check("mortargiveup: a team still on its way blocks nothing", !ai.MortarBlockedForTest(s_Base), State());
   s_Team.m_Squad.GiveUpForTest();
  }
  if (tick == 45)
  {
   ai.NoteMortarGiveUpsForTest();
   ACR_TestRegress.Check("mortargiveup: given up on the way (after packing up), the base is left out of mortar moves", ai.MortarBlockedForTest(s_Base), State());
   ACR_TestRegress.Complete("mortargiveup", 2);
  }
  if (tick == 47) Print("[ACR] TEST COMPLETE");
 }
}
