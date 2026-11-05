// -acrPositionGunTest 1 (server-capable, ~2 min): a Fortify position's reserve defenders man its gun
// while its squad is away on another job (the user, 26 Oct: holders sent to lay mines in front left
// the nest empty). A tier II position (an MG nest, 2 defenders in reserve) near Alpha, Alpha holds
// it, then Alpha is given a minefield order in front: the nest is manned from the reserve within
// 20 s, Alpha still on its way to lay the field; Alpha holds the position again: the gun crew is
// back in reserve within 15 s.
class ACR_TestPositionGun
{
 static const int CHECKS = 2;
 static int s_iStep;
 static int s_iStepAt;
 static ACR_Fortification s_Fort;
 static int s_iReserve;
 static string s_sDetail;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick > 230) ACR_TestRegress.Abort("positiongun", string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  if (tick < 8) return;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;
  string why;

  // 0: a tier II position (built at once, tier I then its upgrade), Alpha holds it.
  if (s_iStep == 0)
  {
   us.m_iRP = 1000;
   vector site;
   if (!ACR_TestFortify.FindSite(alpha, site))
   {
    ACR_TestRegress.Abort("positiongun", "no site for the position near Alpha");
    return;
   }
   ACR_FortifyPlan first = ACR_Fortify.Plan(alpha, site, why);
   if (first) s_Fort = ACR_Fortify.Build(first, why);
   ACR_FortifyPlan upgrade;
   if (s_Fort) upgrade = ACR_Fortify.Plan(alpha, s_Fort.m_vPos, why);
   if (upgrade) s_Fort = ACR_Fortify.Build(upgrade, why);
   array<IEntity> turrets = {};
   ACR_Fortify.Turrets(s_Fort, turrets);
   if (!s_Fort || s_Fort.m_iTier < 2 || turrets.IsEmpty() || !s_Fort.m_Defenders)
   {
    ACR_TestRegress.Abort("positiongun", string.Format("no tier II position with a gun and defenders (%1)", why));
    return;
   }
   s_iReserve = s_Fort.m_Defenders.m_iReserve;
   bool holds = alpha.ManFortification(s_Fort);
   PrintFormat("[ACR] POSITIONGUN tier %1 at %2: %3 gun(s), %4 in reserve; Alpha holds it: %5", s_Fort.m_iTier, s_Fort.m_vPos, turrets.Count(), s_iReserve, holds);
   Next(tick);
   return;
  }
  // 1: Alpha settled in (one of its men on the gun, or 40 s on), then a minefield in front.
  if (s_iStep == 1)
  {
   if (Gunners(alpha.m_Group) == 0 && waited < 40) return;
   vector field;
   if (!ACR_TestMinefields.FindSite(alpha, ACR_EMineKind.MIXED, field))
   {
    ACR_TestRegress.Abort("positiongun", "no minefield site near Alpha");
    return;
   }
   bool laying = ACR_Minefields.Order(alpha, ACR_EMineKind.MIXED, field, why);
   PrintFormat("[ACR] POSITIONGUN Alpha's gunners after %1 s: %2; minefield order at %3: %4 (%5)", waited, Gunners(alpha.m_Group), field, laying, why);
   if (!laying)
   {
    ACR_TestRegress.Abort("positiongun", "the minefield order was refused: " + why);
    return;
   }
   Next(tick);
   return;
  }
  // 2: the gun manned from the reserve within 20 s, Alpha still on its job.
  if (s_iStep == 2)
  {
   SCR_AIGroup crew = s_Fort.m_Defenders.m_GunCrew;
   int manned = Gunners(crew);
   s_sDetail = string.Format("gun crew %1, %2 of it on a gun, %3 in reserve, Alpha's order '%4' (%5), Alpha's gunners %6", crew != null, manned, s_Fort.m_Defenders.m_iReserve, alpha.m_Order, typename.EnumToString(ACR_EOrderState, alpha.OrderState()), Gunners(alpha.m_Group));
   if (manned == 0 && waited < 20) return;
   bool ok = manned > 0 && alpha.OrderState() == ACR_EOrderState.LAY_MINES && alpha.m_Fortification == null;
   ACR_TestRegress.Check("positiongun: while its squad lays mines, the position's reserve defenders man its gun within 20 s", ok, string.Format("after %1 s: %2", waited, s_sDetail));
   alpha.ManFortification(s_Fort);
   Next(tick);
   return;
  }
  // 3: Alpha holds it again: the crew back in reserve within 15 s.
  if (s_iStep == 3)
  {
   s_sDetail = string.Format("gun crew %1, %2 in reserve (%3 before), Alpha's order '%4'", s_Fort.m_Defenders.m_GunCrew != null, s_Fort.m_Defenders.m_iReserve, s_iReserve, alpha.m_Order);
   if (s_Fort.m_Defenders.m_GunCrew && waited < 15) return;
   bool back = !s_Fort.m_Defenders.m_GunCrew && s_Fort.m_Defenders.m_iReserve == s_iReserve && alpha.m_Fortification == s_Fort;
   ACR_TestRegress.Check("positiongun: the gun crew steps back into the reserve when the squad holds the position again", back, string.Format("after %1 s: %2", waited, s_sDetail));
   ACR_TestRegress.Complete("positiongun", CHECKS);
   Next(tick);
  }
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // The group's living men sitting in the position's guns.
 protected static int Gunners(SCR_AIGroup group)
 {
  if (!group || !s_Fort) return 0;
  array<IEntity> turrets = {};
  ACR_Fortify.Turrets(s_Fort, turrets);
  int count;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && turrets.Contains(ACR_Transport.VehicleOf(body))) count++;
  }
  return count;
 }
}
