// -acrAIDefencesTest 1 (server-capable, ~30 s): the AI's defences (playtest 25 Oct). The build
// budget's trickle and the doctrine's share (Fortress twice); engineering paid from the budget;
// front bases nearest the enemy first; no second minefield where a squad of ours is already on its
// way to lay one, and the AI sends one layer at a time; its fields and positions keep 60 m apart,
// either order; a builder holds what it built while fewer
// than its doctrine's holders do; a tier I position holds a defender.
class ACR_TestAIDefences
{
 static const int CHECKS = 10;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!alpha || !bravo || !alpha.GetAnchor() || !us)
  {
   ACR_TestRegress.Abort("aidefences", "no Alpha, Bravo or US faction data");
   return;
  }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ACR_AIDoctrine balanced = ACR_AIDoctrine.Find("balanced");
  ACR_AIDoctrine fortress = ACR_AIDoctrine.Find("fortress");
  ai.SetDoctrineForTest(balanced);

  // 1. The trickle: 2 RP a strategy tick while the share is on, none while it is off.
  ACR_Options.Set("acrAIBuildShare", "10");
  us.m_iRP = 300;
  ai.AccrueBuildBudget();
  int before = ai.BuildBudget();
  ai.TrickleBuildBudget();
  int trickled = ai.BuildBudget() - before;
  ACR_Options.Unset("acrAIBuildShare");
  ai.AccrueBuildBudget();
  ai.TrickleBuildBudget();
  ACR_TestRegress.Check("aidefences: the build budget gets a 2 RP trickle each strategy tick while the share is on, none while off", trickled == 2 && ai.BuildBudget() == 0, string.Format("trickle %1, budget off %2", trickled, ai.BuildBudget()));

  // 2. Fortress sets twice the share and the trickle aside; Balanced keeps the old values.
  ACR_AICommander fort = new ACR_AICommander(manager, "US");
  fort.SetDoctrineForTest(fortress);
  ACR_Options.Set("acrAIBuildShare", "10");
  us.m_iRP = 100;
  fort.AccrueBuildBudget();
  us.m_iRP = 300;
  fort.AccrueBuildBudget();
  int share = fort.BuildBudget();
  fort.TrickleBuildBudget();
  int fortressTrickle = fort.BuildBudget() - share;
  ACR_Options.Unset("acrAIBuildShare");
  bool fortressMore = share == 40 && fortressTrickle == 4 && fortress.m_iRoadblocks == 3 && fortress.m_iMinefields == 4 && fortress.m_iFortifyPositions == 4 && fortress.m_iHolders == 4 && fortress.m_bHoldersStay;
  bool balancedSame = balanced.m_fBuildShare == 1 && balanced.m_iRoadblocks == 2 && balanced.m_iMinefields == 2 && balanced.m_iFortifyPositions == 2 && balanced.m_iHolders == 2 && !balanced.m_bHoldersStay;
  ACR_TestRegress.Check("aidefences: Fortress sets twice the share and trickle aside and builds more; Balanced as before", fortressMore && balancedSame, string.Format("Fortress: 200 RP gain at 10%% -> %1, trickle %2, roadblocks %3, fields %4, positions %5, holders %6 (stay %7); Balanced same %8", share, fortressTrickle, fortress.m_iRoadblocks, fortress.m_iMinefields, fortress.m_iFortifyPositions, fortress.m_iHolders, fortress.m_bHoldersStay, balancedSame));

  // 3. Engineering is paid from the budget while the share is on, else from RP with a reserve.
  ACR_Options.Set("acrAIBuildShare", "10");
  us.m_iRP = 500;
  ai.m_fBuildBudget = 15;
  bool shortBudget = !ai.CanBuild(20, 25);
  ai.m_fBuildBudget = 25;
  bool enough = ai.CanBuild(20, 25);
  ACR_Options.Unset("acrAIBuildShare");
  us.m_iRP = 40;
  bool shortRP = !ai.CanBuild(20, 25);
  us.m_iRP = 50;
  bool enoughRP = ai.CanBuild(20, 25);
  ACR_TestRegress.Check("aidefences: engineering is paid from the budget while the share is on (else from RP with a reserve)", shortBudget && enough && shortRP && enoughRP, string.Format("15 of 20 in the budget refused %1, 25 paid %2; share off: 40 RP refused %3, 50 paid %4", shortBudget, enough, shortRP, enoughRP));

  // 4. Front bases: ours and uncontested, nearest an enemy base first.
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  ai.FrontBasesForTest(fronts, enemies);
  bool ordered = !fronts.IsEmpty();
  float last = -1;
  foreach (int i, ACR_Base base : fronts)
  {
   float gap = vector.DistanceXZ(base.GetOrigin(), enemies[i]);
   if (base.m_sOwner != "US" || base.m_bContested || gap < last) ordered = false;
   last = gap;
  }
  ACR_TestRegress.Check("aidefences: our front bases are ours and uncontested, nearest an enemy base first", ordered, string.Format("%1 front bases", fronts.Count()));

  // 5. No second field where a squad of ours is already on its way to lay one.
  us.m_iRP = 500;
  vector field = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(0, 0, 120));
  string why, secondWhy, afterWhy;
  bool first = ACR_Minefields.Order(alpha, ACR_EMineKind.AP, field, why);
  bool second = ACR_Minefields.Order(bravo, ACR_EMineKind.AP, field + Vector(8, 0, 0), secondWhy);
  alpha.Halt("Holding");
  bool after = ACR_Minefields.Order(bravo, ACR_EMineKind.AP, field + Vector(8, 0, 0), afterWhy);
  bravo.Halt("Holding");
  ACR_TestRegress.Check("aidefences: no second minefield where a squad of ours is already on its way to lay one", first && !second && secondWhy.Contains("already laying") && after, string.Format("first %1 (%2), second %3 '%4', once the first is called off %5 (%6)", first, why, second, secondWhy, after, afterWhy));

  // 6. The AI sends one layer at a time (it once sent two squads to the same spot).
  ACR_Options.Set("acrMinesAI", "1");
  us.m_iRP = 500;
  ai.StartTickForTest();
  ai.ResetBuildTimersForTest();
  ai.ManageMinefields();
  int layers = Layers(manager);
  ai.StartTickForTest();
  ai.ResetBuildTimersForTest();
  ai.ManageMinefields();
  int layersAfter = Layers(manager);
  ACR_Options.Unset("acrMinesAI");
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_MinePlan) squad.Halt("Holding");
  }
  ACR_TestRegress.Check("aidefences: the AI sends one squad to lay a field, and no second while it is on its way", layers == 1 && layersAfter == 1, string.Format("after one pass %1 laying, after two %2", layers, layersAfter));

  // 6b. Our defences keep apart, either order (playtest 25 Oct: twice an AI field 10 m from one of
  // its own positions). A position of ours where the line out from the front base meets the
  // mines' first spot: the field goes DEFENCE_STANDOFF or more from it.
  string apartDetail = "no front base";
  bool minesApart, digApart;
  if (!fronts.IsEmpty())
  {
   vector toEnemy = enemies[0] - fronts[0].GetOrigin();
   toEnemy[1] = 0;
   toEnemy.Normalize();
   ACR_Fortification pretend = new ACR_Fortification();
   pretend.m_sFaction = "US";
   pretend.m_iTier = 1;
   pretend.m_vPos = ACR_Manager.Ground(fronts[0].GetOrigin() + toEnemy * 150);
   for (int s = 0; s < ACR_Fortify.SPOT_COUNT; s++) pretend.m_aPieces.Insert(null);
   ACR_Fortify.s_Positions.Insert(pretend);
   ACR_Options.Set("acrMinesAI", "1");
   us.m_iRP = 500;
   ai.StartTickForTest();
   ai.ResetBuildTimersForTest();
   ai.ManageMinefields();
   ACR_Options.Unset("acrMinesAI");
   vector fieldAt = LayingAt(manager);
   ACR_Fortify.s_Positions.RemoveItem(pretend);
   float fieldGap = vector.DistanceXZ(fieldAt, pretend.m_vPos);
   bool inFront = vector.Dot(fieldAt - pretend.m_vPos, toEnemy) > 0;
   minesApart = fieldAt != vector.Zero && fieldGap >= ACR_AICommander.DEFENCE_STANDOFF;
   foreach (ACR_Squad layer : manager.m_Squads)
   {
    if (layer && layer.m_MinePlan) layer.Halt("Holding");
   }
   // A field of ours where the line out meets the AI's new positions: the dig goes as far from it.
   ACR_Minefield field2 = new ACR_Minefield();
   field2.m_sFaction = "US";
   field2.m_vPos = ACR_Manager.Ground(fronts[0].GetOrigin() + toEnemy * 120);
   ACR_Minefields.s_Fields.Insert(field2);
   ACR_Options.Set("acrFortifyAI", "1");
   us.m_iRP = 500;
   ai.StartTickForTest();
   ai.ResetBuildTimersForTest();
   ai.ManageFortify();
   ACR_Options.Unset("acrFortifyAI");
   vector digAt = DiggingAt(manager);
   ACR_Minefields.s_Fields.RemoveItem(field2);
   float digGap = vector.DistanceXZ(digAt, field2.m_vPos);
   digApart = digAt != vector.Zero && digGap >= ACR_AICommander.DEFENCE_STANDOFF;
   foreach (ACR_Squad digger : manager.m_Squads)
   {
    if (digger && digger.m_FortifyPlan) digger.Halt("Holding");
   }
   apartDetail = string.Format("field %1 m from our position (in front %2); position %3 m from our field", (int)fieldGap, inFront, (int)digGap);
  }
  ACR_TestRegress.Check("aidefences: the AI lays no field within 60 m of its own position", minesApart, apartDetail);
  ACR_TestRegress.Check("aidefences: the AI digs no position within 60 m of its own field", digApart, apartDetail);

  // 7. A builder holds its roadblock while fewer than the doctrine's holders do; with none
  // allowed it is posted again.
  bravo.m_iAITask = ACR_EAITask.ROADBLOCK;
  bravo.m_ActiveRoadblock = new ACR_Roadblock();
  bravo.SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
  ai.ReleaseBuildersForTest();
  bool held = bravo.m_iAITask == ACR_EAITask.ROADBLOCK && ACR_AICommander.IsHolder(bravo);
  ACR_AIDoctrine noHolders = new ACR_AIDoctrine();
  noHolders.m_iHolders = 0;
  ai.SetDoctrineForTest(noHolders);
  ai.ReleaseBuildersForTest();
  bool released = bravo.m_iAITask == ACR_EAITask.POSTED;
  bravo.m_ActiveRoadblock = null;
  bravo.m_iAITask = ACR_EAITask.FREE;
  bravo.Halt("Holding");
  ACR_TestRegress.Check("aidefences: a builder holds its roadblock while fewer than its doctrine's holders do (none allowed: posted again)", held && released, string.Format("held %1, released %2", held, released));

  // 8. A tier I position holds a defender.
  bool tierOne = ACR_Fortify.Defenders(0) == 0 && ACR_Fortify.Defenders(1) == 1 && ACR_Fortify.Defenders(2) == 2;
  ACR_TestRegress.Check("aidefences: a tier I position holds 1 defender (tier II 2)", tierOne, string.Format("tier I %1, tier II %2", ACR_Fortify.Defenders(1), ACR_Fortify.Defenders(2)));
  ACR_TestRegress.Complete("aidefences", CHECKS);
 }

 protected static vector LayingAt(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_MinePlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") return squad.m_MinePlan.m_vSite;
  }
  return vector.Zero;
 }

 protected static vector DiggingAt(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_FortifyPlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") return squad.m_FortifyPlan.m_vSite;
  }
  return vector.Zero;
 }

 protected static int Layers(ACR_Manager manager)
 {
  int count;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_MinePlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") count++;
  }
  return count;
 }
}
