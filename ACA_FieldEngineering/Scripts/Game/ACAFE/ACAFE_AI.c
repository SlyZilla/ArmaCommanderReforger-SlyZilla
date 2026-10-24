// Optional (match setting "Enemy AI commander uses it", -acafeAI 1): the ACA AI commander now
// and then puts a checkpoint on the road out of a frontline base towards the enemy, or an AP
// minefield on the enemy side of one of its FOBs. It pays RP like a player and keeps a reserve
// for troops. -acafeAITest makes it eager and chatty in the log (see ACAFE_Config).
modded class ACR_AICommander
{
 protected float m_fACAFE_NextPlan = -1;

 override void Tick(bool noPlayerCommander)
 {
  super.Tick(noPlayerCommander);
  if (m_bActive && ACR_Setup.Started() && ACAFE_Settings.AIEnabled()) ACAFE_Think();
 }

 // Test runs say why the commander did nothing.
 protected void ACAFE_Note(string why)
 {
  if (ACAFE_Config.AITest()) PrintFormat("[ACAFE] AI %1: %2", m_sFaction, why);
 }

 protected void ACAFE_Think()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float interval = ACAFE_Config.AI_PLAN_SECONDS * 1000;
  if (ACAFE_Config.AITest()) interval = 30000;
  // Not in the opening minutes: troops first.
  if (m_fACAFE_NextPlan < 0) m_fACAFE_NextPlan = now + interval;
  if (now < m_fACAFE_NextPlan) return;
  m_fACAFE_NextPlan = now + interval;
  ACAFE_Engineering engineering = ACAFE_Engineering.Get();
  if (engineering.ActiveJobs(m_sFaction, true) >= ACAFE_Config.AI_MAX_JOBS)
  {
   ACAFE_Note("already has an engineering job going");
   return;
  }
  if (!ACAFE_Config.AITest() && Math.RandomFloat01() > ACAFE_Config.AI_CHANCE) return;
  bool minesFirst = Math.RandomFloat01() < 0.5;
  if (minesFirst && ACAFE_TryMines(engineering)) return;
  if (ACAFE_TryRoadblock(engineering)) return;
  if (!minesFirst) ACAFE_TryMines(engineering);
 }

 protected bool ACAFE_CanAfford(int order)
 {
  int reserve = ACAFE_Config.AI_RP_RESERVE;
  if (ACAFE_Config.AITest()) reserve = 0;
  return m_Manager.m_Bases.GetRP(m_sFaction) >= ACAFE_Orders.CostRP(order) + reserve;
 }

 protected bool ACAFE_TryRoadblock(ACAFE_Engineering engineering)
 {
  if (engineering.CountStructures(m_sFaction) >= ACAFE_Config.AI_MAX_STRUCTURES)
  {
   ACAFE_Note("has enough checkpoints");
   return false;
  }
  int order = ACAFE_EOrder.CHECKPOINT_M;
  if (!ACAFE_CanAfford(order)) order = ACAFE_EOrder.ROADBLOCK;
  if (!ACAFE_CanAfford(order))
  {
   ACAFE_Note(string.Format("no RP for a roadblock (has %1)", m_Manager.m_Bases.GetRP(m_sFaction)));
   return false;
  }
  array<ACR_Base> front = {};
  FrontlineBases(front);
  if (front.IsEmpty()) ACAFE_Note("no frontline base for a checkpoint");
  foreach (ACR_Base base : front)
  {
   vector away;
   if (!ACAFE_ThreatDirection(base.GetOrigin(), away)) continue;
   for (float reach = base.m_fRadius + 60; reach <= base.m_fRadius + 180; reach += 40)
   {
    vector guess = base.GetOrigin() + away * reach;
    vector road;
    vector roadDir;
    float roadWidth;
    if (!ACAFE_Roads.Snap(guess, 50, road, roadDir, roadWidth)) continue;
    if (engineering.StructureNear(road, 150))
    {
     ACAFE_Note(base.m_sName + ": its road already has a checkpoint");
     break;
    }
    vector seenAt;
    if (EnemyNear(road, 300, seenAt))
    {
     ACAFE_Note(base.m_sName + ": enemies near the road out");
     break;
    }
    if (ACAFE_StartWith(engineering, order, road)) return true;
    break;
   }
  }
  return false;
 }

 protected bool ACAFE_TryMines(ACAFE_Engineering engineering)
 {
  if (ACAFE_Mines.CountFields(m_sFaction) >= ACAFE_Config.AI_MAX_MINEFIELDS)
  {
   ACAFE_Note("has enough minefields");
   return false;
  }
  int order = ACAFE_EOrder.MINES_AP;
  if (!ACAFE_CanAfford(order))
  {
   ACAFE_Note(string.Format("no RP for a minefield (has %1)", m_Manager.m_Bases.GetRP(m_sFaction)));
   return false;
  }
  // Its FOBs, outside their 50 m defence ring; test runs fall back to frontline bases.
  array<ACR_Base> sites = {};
  array<float> rings = {};
  foreach (ACR_Base fob : ACR_Base.s_All)
  {
   if (!fob || fob.m_bRetired || !fob.m_bBuilt || fob.m_bHqLost || fob.m_sOwner != m_sFaction) continue;
   sites.Insert(fob);
   rings.Insert(ACR_FobDefences.RING + ACAFE_Config.MINEFIELD_SIZE);
  }
  if (sites.IsEmpty() && ACAFE_Config.AITest())
  {
   array<ACR_Base> front = {};
   FrontlineBases(front);
   foreach (ACR_Base frontBase : front)
   {
    sites.Insert(frontBase);
    rings.Insert(frontBase.m_fRadius + ACAFE_Config.MINEFIELD_SIZE * 0.5 + 10);
   }
  }
  if (sites.IsEmpty()) ACAFE_Note("no FOB to mine round");
  foreach (int i, ACR_Base base : sites)
  {
   vector away;
   if (!ACAFE_ThreatDirection(base.GetOrigin(), away)) continue;
   vector spot = ACR_Manager.Ground(base.GetOrigin() + away * rings[i]);
   if (ACAFE_Mines.FieldNear("", spot, 120))
   {
    ACAFE_Note(base.m_sName + ": already mined");
    continue;
   }
   vector seenAt;
   if (EnemyNear(spot, 300, seenAt))
   {
    ACAFE_Note(base.m_sName + ": enemies near the minefield spot");
    continue;
   }
   if (ACAFE_StartWith(engineering, order, spot)) return true;
  }
  return false;
 }

 // Flat unit vector towards the nearest base that isn't ours.
 protected bool ACAFE_ThreatDirection(vector from, out vector dir)
 {
  float best = float.MAX;
  bool found;
  vector toward;
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (!other || other.m_bRetired || other.m_sOwner == m_sFaction) continue;
   float d = vector.DistanceXZ(other.GetOrigin(), from);
   if (d < 50 || d >= best) continue;
   best = d;
   toward = other.GetOrigin() - from;
   found = true;
  }
  if (!found) return false;
  toward[1] = 0;
  toward.Normalize();
  dir = toward;
  return true;
 }

 // Free infantry squads within 1.5 km (not the reserve, not vehicle crews; engineers only when
 // the match says so), nearest first; the first that the rules accept goes (kit mines, a truck).
 protected bool ACAFE_StartWith(ACAFE_Engineering engineering, int order, vector point)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  array<ACR_Squad> candidates = {};
  foreach (ACR_Squad squad : free)
  {
   if (squad == m_ReserveSquad || ACR_Transport.CrewedVehicle(squad) || squad.CountAlive() < 2) continue;
   if (engineering.JobOf(squad)) continue;
   if (ACAFE_Settings.EngineersOnly() && !ACAFE_Engineering.HasEngineers(squad)) continue;
   if (vector.DistanceXZ(PositionOf(squad), point) > 1500) continue;
   candidates.Insert(squad);
  }
  if (candidates.IsEmpty())
  {
   string kind = "infantry";
   if (ACAFE_Settings.EngineersOnly()) kind = "engineer";
   ACAFE_Note(string.Format("no free %1 squad within 1.5 km of the %2 site (%3 free)", kind, ACAFE_Orders.Name(order), free.Count()));
   return false;
  }
  SortByDistance(candidates, point);
  string refusal;
  for (int i = 0; i < candidates.Count() && i < 3; i++)
  {
   ACR_Squad pick = candidates[i];
   if (engineering.StartAI(pick, order, point, refusal))
   {
    PrintFormat("[ACAFE] AI %1: %2 sent off, %3 at %4", m_sFaction, pick.GetName(), ACAFE_Engineering.Doing(order), point);
    return true;
   }
   PrintFormat("[ACAFE] AI %1: %2 can't do the %3 at %4 (%5)", m_sFaction, pick.GetName(), ACAFE_Orders.Name(order), point, refusal);
  }
  return false;
 }
}
