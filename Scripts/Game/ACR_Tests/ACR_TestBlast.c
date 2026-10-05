// -acrBlastTest 1 (server-capable, ~40 s): how lethal one heavy blast (bomb run / howitzer
// battery round) is. Soldiers stand in rings 4-40 m round a point in the open, six per ring, plus
// six crouched behind a wall-less rise check... (kept simple: open ground only). One blast, then
// per ring: dead, down (unconscious), wounded, unhurt. -acrBlastOldTest 1 = without the scripted
// blast damage (ACR_Supports.BlastDamage), for comparison.
class ACR_TestBlast
{
 static ref array<IEntity> s_Soldiers = {};
 static ref array<int> s_Ring = {};
 static ref array<float> s_Rings = {4, 8, 12, 16, 20, 25, 30, 40};
 static vector s_Center;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 4)
  {
   s_Center = ACR_Manager.Ground(alpha.m_Home + Vector(-200, 0, 700));
   array<ResourceName> pool = manager.m_Bases.GetFaction("USSR").m_aGarrison;
   if (pool.IsEmpty()) { Print("[ACR] BLAST no soldier prefab"); return; }
   foreach (int r, float radius : s_Rings)
   {
    for (int k = 0; k < 6; k++)
    {
     float angle = Math.PI2 * k / 6 + r * 0.4;
     vector at = ACR_Manager.Ground(s_Center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius));
     IEntity soldier = ACR_Manager.SpawnPrefab(pool[0], at);
     if (!soldier) continue;
     s_Soldiers.Insert(soldier);
     s_Ring.Insert(r);
    }
   }
   PrintFormat("[ACR] BLAST %1 soldiers placed round %2", s_Soldiers.Count(), s_Center);
  }
  if (tick == 10)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Supports.ExplodeAt(s_Center);
  }
  if (tick == 18)
  {
   int deadNear, total;
   foreach (int r, float radius : s_Rings)
   {
    int dead, down, hurt, fine;
    foreach (int i, IEntity e : s_Soldiers)
    {
     if (s_Ring[i] != r) continue;
     ChimeraCharacter c = ChimeraCharacter.Cast(e);
     if (!c || !c.GetCharacterController() || c.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD) { dead++; continue; }
     if (c.GetCharacterController().GetLifeState() == ECharacterLifeState.INCAPACITATED) { down++; continue; }
     SCR_CharacterDamageManagerComponent dm = SCR_CharacterDamageManagerComponent.Cast(c.GetDamageManager());
     if (dm && (dm.GetHealthScaled() < 0.95 || dm.IsBleeding())) hurt++;
     else fine++;
    }
    PrintFormat("[ACR] BLAST ring %1 m: dead %2, down %3, wounded %4, unhurt %5", radius, dead, down, hurt, fine);
    if (radius <= 12) deadNear += dead + down;
    if (radius <= 12) total += 6;
   }
   if (!System.IsCLIParam("acrBlastOldTest"))
    ACR_TestRegress.Check("blast: soldiers within 12 m in the open are killed or downed", deadNear >= total - 2, string.Format("%1 of %2", deadNear, total));
   Print("[ACR] TEST COMPLETE");
  }
 }
}
