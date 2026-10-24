// Client acceptance of BBB's Think overlay as ACA hosts it: aim the RTS camera at a real
// soldier and the card shows; look at the sky and it hides. Checks widgets, not wording.
modded class ACR_RTSCameraManager
{
 static void LookAtForTest(IEntity soldier)
 {
  vector at = soldier.GetOrigin() + "0 1.2 0";
  vector eye = at + "0 0 -30";
  eye[1] = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]) + 35;
  vector transform[4];
  Math3D.DirectionAndUpMatrix((at - eye).Normalized(), "0 1 0", transform);
  transform[3] = eye;
  s_Instance.m_Camera.SetWorldTransform(transform);
 }

 static void LookAtSkyForTest()
 {
  vector angles = s_Instance.m_Camera.GetYawPitchRoll();
  angles[1] = 80;
  s_Instance.m_Camera.SetYawPitchRoll(angles);
 }
}

modded class ACR_TestCommand
{
 override void TestRespawn()
 {
  if (System.IsCLIParam("acrThinkOverlayTest")) { TestThinkOverlay(); return; }
  super.TestRespawn();
 }

 // The AI soldier nearest the player, from the groups BBB itself sees.
 protected IEntity NearestBBBSoldier(IEntity body)
 {
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  array<AIAgent> agents = {};
  IEntity best;
  float bestDistance = 250;
  foreach (SCR_AIGroup group : groups)
  {
   BBB.s_Hooks.GetMembers(group, agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier;
    if (agent) soldier = agent.GetControlledEntity();
    if (!soldier || soldier == body || BBB.PlayerOf(soldier) || !BBB.IsAlive(soldier)) continue;
    float distance = vector.Distance(soldier.GetOrigin(), body.GetOrigin());
    if (distance >= bestDistance) continue;
    bestDistance = distance;
    best = soldier;
   }
  }
  return best;
 }

 void TestThinkOverlay()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (h.m_Ticks == 5)
  {
   if (!pc || !ACR_Respawn.IsAlive(pc.GetControlledEntity())) { ACR_TestRegress.Abort("thinkoverlay", "live client required"); return; }
   IEntity soldier = NearestBBBSoldier(pc.GetControlledEntity());
   if (!soldier) { ACR_TestRegress.Abort("thinkoverlay", "no AI soldier within 250 m"); return; }
   BBB_Think.s_bOn = true;
   ACR_RTSCameraManager.Open();
   if (!ACR_RTSCameraManager.IsActive()) { ACR_TestRegress.Abort("thinkoverlay", "camera did not open"); return; }
   ACR_RTSCameraManager.LookAtForTest(soldier);
   BBB_Think.s_Picked = soldier;
  }
  if (h.m_Ticks == 6)
  {
   PrintFormat("[ACR] THINK OVERLAY line0=%1 line1=%2", BBB_ThinkOverlay.LineText(0), BBB_ThinkOverlay.LineText(1));
   ACR_TestRegress.Check("Think: card shows for a soldier in view", BBB_ThinkOverlay.IsCardVisible());
   ACR_TestRegress.Check("Think: card names him and what he is doing", BBB_ThinkOverlay.LineText(0).Contains("|") && !BBB_ThinkOverlay.LineText(1).IsEmpty());
   ACR_RTSCameraManager.LookAtSkyForTest();
  }
  if (h.m_Ticks == 7)
  {
   ACR_TestRegress.Check("Think: card hides when he is off screen", !BBB_ThinkOverlay.IsCardVisible());
   ACR_RTSCameraManager.Close();
   BBB_Think.s_bOn = false;
   BBB_ThinkOverlay.Remove();
  }
  if (h.m_Ticks == 9) ACR_TestRegress.Complete("thinkoverlay", 3);
 }
}
