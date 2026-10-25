// -acrBuildBudgetTest 1 (server-capable, ~30 s): the AI building budget (-acrAIBuildShare). Off
// by default (nothing set aside, unit purchases keep nothing back); at 10% a 200 RP gain puts
// 20 RP in it, which unit purchases keep back and the status line shows; it stops at 100 RP
// and at what the side holds; engineering spending comes out of it.
class ACR_TestBuildBudget
{
 static const int CHECKS = 5;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_AICommander ai = manager.AICommanderFor("US");
  if (!ai) ai = new ACR_AICommander(manager, "US");
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!us)
  {
   ACR_TestRegress.Abort("budget", "no US faction data");
   return;
  }

  // Off by default.
  ACR_Options.Unset("acrAIBuildShare");
  us.m_iRP = 100;
  ai.AccrueBuildBudget();
  us.m_iRP = 300;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: off by default, nothing set aside", ACR_AICommander.BuildShare() == 0 && ai.BuildBudget() == 0 && ai.BuildKeep() == 0 && ai.BuildStatus().IsEmpty(), string.Format("share %1, budget %2", ACR_AICommander.BuildShare(), ai.BuildBudget()));

  // 10%: a 200 RP gain puts 20 RP in, kept back from units and shown.
  ACR_Options.Set("acrAIBuildShare", "10");
  us.m_iRP = 100;
  ai.AccrueBuildBudget();
  us.m_iRP = 300;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: at 10% a 200 RP gain sets 20 RP aside, kept back from unit purchases", ai.BuildBudget() == 20 && ai.BuildKeep() == 20 && ai.BuildStatus().Contains("build budget 20 RP"), string.Format("budget %1, keep %2, status '%3'", ai.BuildBudget(), ai.BuildKeep(), ai.BuildStatus()));

  // At most 100, and never more than the side holds.
  us.m_iRP = 5300;
  ai.AccrueBuildBudget();
  int capped = ai.BuildBudget();
  us.m_iRP = 40;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: at most 100 RP, and never more than the side holds", capped == 100 && ai.BuildBudget() == 40, string.Format("after +5000: %1, at 40 RP: %2", capped, ai.BuildBudget()));

  // Engineering spending comes out of it (never below zero).
  ai.SpendBuildBudget(25);
  int afterSpend = ai.BuildBudget();
  ai.SpendBuildBudget(500);
  ACR_TestRegress.Check("budget: engineering spending comes out of it", afterSpend == 15 && ai.BuildBudget() == 0, string.Format("after 25: %1, after 500 more: %2", afterSpend, ai.BuildBudget()));

  // The share is clamped to 30%.
  ACR_Options.Set("acrAIBuildShare", "80");
  bool clamped = ACR_AICommander.BuildShare() == 30;
  ACR_Options.Unset("acrAIBuildShare");
  ACR_TestRegress.Check("budget: the share is at most 30%", clamped, "");
  ACR_TestRegress.Complete("budget", CHECKS);
 }
}
