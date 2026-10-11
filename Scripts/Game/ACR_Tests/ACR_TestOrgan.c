// -acrOrganTest 1: force the rare urge on a real organ prefab; the soldier must
// walk there without teleporting, finish the piece, and relinquish his AI settings.
class ACR_TestOrgan
{
 static IEntity s_Organ;
 static IEntity s_Soldier;
 static ref ACR_OrganState s_State;
 static ref SCR_AIBehaviorBase s_Move;
 static ref SCR_AIBehaviorBase s_Wait;
 static vector s_Start;
 static bool s_bArrived;
 static bool s_bFinished;
 static int s_iStartedPlaying;
 static int s_iSerial;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (ACR_TestRegress.s_bCompleted) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (tick == 1) foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = h.Squad();
  if (tick == 5)
  {
   if (!alpha || !alpha.GetAnchor()) { ACR_TestRegress.Abort("organ", "no Alpha"); return; }
   alpha.Halt("Holding");
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   if (agents.Count() < 2) { ACR_TestRegress.Abort("organ", "no second soldier"); return; }
   s_Soldier = agents[1].GetControlledEntity();
   s_Start = s_Soldier.GetOrigin();
   s_iSerial = alpha.m_iOrderSerial;
   // Runtime static props do not update the baked navmesh. Approach this fixture
   // from its keyboard side, rather than asking the AI to path through its back.
   s_Organ = ACR_Manager.SpawnPrefab("{F7BB38C6DC5BF74B}Prefabs/Structures/BuildingParts/Cultural/OrganChurch_01.et", ACR_Manager.Ground(s_Start + Vector(0, 0, -16)));
  }
  if (tick == 7)
  {
   s_State = ACR_OrganEasterEgg.Find(s_Organ);
   ACR_TestRegress.Check("organ: vanilla toccata discovered", s_State != null && s_State.m_Action.GetActionDuration() == 25);
   if (!s_State) { ACR_TestRegress.Abort("organ", "organ action registration failed"); return; }
   // Keep production random rolls out of this forced test.
   s_State.m_fNextRoll = ACR_OrganEasterEgg.Now() + 3600000;
   vector saved = s_Soldier.GetOrigin();
   s_Soldier.SetOrigin(s_State.m_Keyboard + Vector(55, 0, 0));
   ACR_TestRegress.Check("organ: soldier outside radius refused", !ACR_OrganEasterEgg.Start(s_State, s_Soldier));
   s_Soldier.SetOrigin(saved);
   bool started = ACR_OrganEasterEgg.Start(s_State, s_Soldier);
   s_Move = s_State.m_Behavior;
   ACR_TestRegress.Check("organ: nearby AI immediately gets individual approach", started && s_Move != null && vector.Distance(s_Soldier.GetOrigin(), s_State.m_Keyboard) > 10 && alpha.m_iOrderSerial == s_iSerial);
   ACR_TestRegress.Check("organ: second organist refused", !ACR_OrganEasterEgg.Start(s_State, alpha.GetAnchor()));
   PrintFormat("[ACR] ORGAN TEST approach from %1 to %2", s_Start, s_State.m_Keyboard);
  }
  if (s_State && s_State.m_bPlaying && !s_bArrived)
  {
   s_bArrived = true;
   s_iStartedPlaying = tick;
   s_Wait = s_State.m_Behavior;
   ACR_TestRegress.Check("organ: AI reached keyboard and started toccata", vector.Distance(s_Soldier.GetOrigin(), s_State.m_Keyboard) <= 1.25 && s_State.m_Speed == null && s_Wait != s_Move, string.Format("walked %1 m in %2 s", vector.Distance(s_Start, s_Soldier.GetOrigin()), tick - 7));
  }
  if (s_State && s_State.m_Organist && tick % 5 == 0)
   PrintFormat("[ACR] ORGAN TEST t=%1 pos=%2 distance=%3 doing=%4 actionState=%5", tick, s_Soldier.GetOrigin(), vector.Distance(s_Soldier.GetOrigin(), s_State.m_Keyboard), BBB_Drag.Doing(s_Soldier), s_State.m_Behavior.GetActionState());
  if (s_bArrived && !s_bFinished && !s_State.m_Organist)
  {
   s_bFinished = true;
   ACR_TestRegress.Check("organ: full 25-second performance releases actions/settings", tick - s_iStartedPlaying >= 24 && s_State.m_Behavior == null && s_State.m_Speed == null && s_State.m_Stance == null && s_Wait.GetActionState() == EAIActionState.COMPLETED);
   ACR_TestRegress.Check("organ: prior squad order survives and cooldown prevents replay", alpha.m_iOrderSerial == s_iSerial && !ACR_OrganEasterEgg.Start(s_State, s_Soldier));
  }
  if (tick == 65)
  {
   if (!s_bFinished) { ACR_TestRegress.Abort("organ", "AI never completed approach/performance"); return; }
   s_State.m_fReady = 0;
   bool startedAgain = ACR_OrganEasterEgg.Start(s_State, s_Soldier);
   alpha.MoveTo(ACR_Manager.Ground(s_Start + Vector(-12, 0, 0)));
   ACR_OrganEasterEgg.Tick();
   ACR_TestRegress.Check("organ: new squad order cancels organist", startedAgain && s_State.m_Organist == null && s_State.m_Stance == null);
  }
  if (tick == 68)
  {
   s_State.m_fReady = 0;
   bool timeoutStarted = ACR_OrganEasterEgg.Start(s_State, s_Soldier);
   s_State.m_fDeadline = ACR_OrganEasterEgg.Now() - 1;
   ACR_OrganEasterEgg.Tick();
   ACR_TestRegress.Check("organ: approach timeout releases organist", timeoutStarted && s_State.m_Organist == null && s_State.m_Behavior == null);
  }
  if (tick == 70)
  {
   s_State.m_fReady = 0;
   bool deletedStarted = ACR_OrganEasterEgg.Start(s_State, s_Soldier);
   SCR_EntityHelper.DeleteEntityAndChildren(s_Organ);
   ACR_OrganEasterEgg.Tick();
   ACR_TestRegress.Check("organ: deleted organ cancels organist", deletedStarted && s_State.m_Organist == null && s_State.m_Behavior == null && s_State.m_Stance == null);
   ACR_TestRegress.Complete("organ", 10);
  }
 }
}
