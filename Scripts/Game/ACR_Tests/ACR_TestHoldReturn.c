// -acrHoldReturnTest 1 (server-capable, ~1 min): holders go back to their posts, and the reason a
// build waits (ACR_AICommander.ReturnToPosts, NoBuilderWhy; pn9 playtest findings, 26 Oct).
// - Alpha holds a position, leaves it to defend: while it defends it waits; free again, it goes
//   back to hold it;
// - one whose position another squad holds by then doesn't go back, and is forgotten;
// - a roadblock's holder the same: left to defend, free again, it goes back to hold it;
// - "waits for a free squad" says what the free squads are and how many are busy.
class ACR_TestHoldReturn
{
 static const int CHECKS = 4;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   ACR_TestRegress.Abort("holdreturn", "no Alpha or Bravo squad");
   return;
  }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ACR_Fortification fort = new ACR_Fortification();
  fort.m_sFaction = "US";
  fort.m_iTier = 1;
  fort.m_vPos = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(40, 0, 0));
  for (int s = 0; s < ACR_Fortify.SPOT_COUNT; s++) fort.m_aPieces.Insert(null);
  ACR_Fortify.s_Positions.Insert(fort);

  // Alpha holds it, then leaves it to defend a base.
  alpha.ManFortification(fort);
  alpha.m_iAITask = ACR_EAITask.FORTIFY;
  ai.NoteLeftPostForTest(alpha);
  alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-200, 0, 0));
  alpha.m_iAITask = ACR_EAITask.DEFEND;
  ai.ReturnToPostsForTest();
  bool waits = ai.LeftPostsForTest() == 1 && alpha.m_Fortification == null && alpha.m_iAITask == ACR_EAITask.DEFEND;
  // The defence is over: free again, it goes back.
  alpha.m_iAITask = ACR_EAITask.FREE;
  ai.ReturnToPostsForTest();
  bool back = alpha.m_Fortification == fort && alpha.m_iAITask == ACR_EAITask.FORTIFY && ai.LeftPostsForTest() == 0;
  ACR_TestRegress.Check("holdreturn: a holder that left its position to defend waits while it defends, then goes back to hold it", waits && back, string.Format("while defending: waits %1; free again: holds it %2, task %3, left posts %4", waits, alpha.m_Fortification == fort, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), ai.LeftPostsForTest()));

  // Left again, but Bravo holds the position by the time Alpha is free.
  ai.NoteLeftPostForTest(alpha);
  alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-200, 0, 0));
  alpha.m_iAITask = ACR_EAITask.FREE;
  bravo.ManFortification(fort);
  bravo.m_iAITask = ACR_EAITask.FORTIFY;
  ai.ReturnToPostsForTest();
  bool notBack = alpha.m_Fortification == null && bravo.m_Fortification == fort && ai.LeftPostsForTest() == 0;
  ACR_TestRegress.Check("holdreturn: one whose position another squad holds by then doesn't go back, and is forgotten", notBack, string.Format("Alpha holds it %1, Bravo %2, left posts %3", alpha.m_Fortification == fort, bravo.m_Fortification == fort, ai.LeftPostsForTest()));
  bravo.Halt("Holding");
  bravo.m_iAITask = ACR_EAITask.FREE;
  alpha.Halt("Holding");

  // A roadblock's holder the same (#64 review): left to defend, free again, it goes back to it.
  ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock("US", ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(0, 0, 60)), 0, "Hold return test");
  bool rbBack = false;
  string rbDetail = "no roadblock spawned";
  if (rb)
  {
   alpha.ManRoadblock(rb);
   alpha.m_iAITask = ACR_EAITask.ROADBLOCK;
   ai.NoteLeftPostForTest(alpha);
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-200, 0, 0));
   alpha.m_iAITask = ACR_EAITask.DEFEND;
   ai.ReturnToPostsForTest();
   bool rbWaits = ai.LeftPostsForTest() == 1 && alpha.m_ActiveRoadblock == null;
   alpha.m_iAITask = ACR_EAITask.FREE;
   ai.ReturnToPostsForTest();
   rbBack = rbWaits && alpha.m_ActiveRoadblock == rb && alpha.m_iAITask == ACR_EAITask.ROADBLOCK && ACR_AICommander.IsHolder(alpha) && ai.LeftPostsForTest() == 0;
   rbDetail = string.Format("while defending: waits %1; free again: holds it %2, task %3, left posts %4", rbWaits, alpha.m_ActiveRoadblock == rb, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), ai.LeftPostsForTest());
   alpha.Halt("Holding");
   alpha.m_iAITask = ACR_EAITask.FREE;
   ACR_Roadblocks.DismantleRoadblock(rb);
  }
  ACR_TestRegress.Check("holdreturn: a roadblock's holder that left to defend goes back to hold it", rbBack, rbDetail);

  // The reason line: Bravo attacking, the rest as they are.
  bravo.m_iAITask = ACR_EAITask.ATTACK;
  string why = ai.NoBuilderWhyForTest(fort.m_vPos, ACR_AICommander.BUILDER_FORTIFY);
  bravo.m_iAITask = ACR_EAITask.FREE;
  bool says = why.StartsWith("(") && why.Contains("busy:") && why.Contains("attacking") && !why.Contains(" 0 attacking") && (why.Contains("free:") || why.Contains("none free"));
  ACR_TestRegress.Check("holdreturn: 'waits for a free squad' says what the free squads are and how many are busy", says, why);
  ACR_Fortify.s_Positions.RemoveItem(fort);
  ACR_TestRegress.Complete("holdreturn", CHECKS);
 }
}
