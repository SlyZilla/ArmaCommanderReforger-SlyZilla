// Client acceptance of native camera transforms, picking and ownership. Input is
// injected through the engine action manager; physical keyboard/cursor UX is manual.
modded class ACR_RTSCameraManager
{
 static void CheckNativeRTS()
 {
  ACR_RTSCameraManager camera = s_Instance;
  if (!camera || !camera.m_Camera) { ACR_TestRegress.Abort("rtscamera", "camera did not open"); return; }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  InputManager input = GetGame().GetInputManager();
  ACR_TestRegress.Check("RTS: owns current native camera", GetGame().GetCameraManager().CurrentCamera() == camera.m_Camera);
  ACR_TestRegress.Check("RTS: cursor context activated", input.IsContextActive("ACR_RTSContext"));
  float width, height;
  camera.m_InteractionLayer.GetScreenSize(width, height);
  float screenW = GetGame().GetWorkspace().GetWidth();
  float screenH = GetGame().GetWorkspace().GetHeight();
  ACR_TestRegress.Check("RTS: scene interaction covers viewport", width >= screenW * 0.95 && height >= screenH * 0.95);
  vector before = camera.m_Camera.GetOrigin();
  vector bodyBefore = camera.m_Body.GetOrigin();
  input.SetActionValue("ACR_RTSPanForward", 1);
  camera.ApplyInput(input);
  input.SetActionValue("ACR_RTSPanForward", 0);
  vector after = camera.m_Camera.GetOrigin();
  ACR_TestRegress.Check("RTS: forward action physically pans camera", vector.DistanceXZ(before, after) > 0.01);
  input.SetActionValue("ACR_MapShift", 1);
  input.SetActionValue("ACR_RTSPanForward", 1);
  camera.ApplyInput(input);
  input.SetActionValue("ACR_MapShift", 0);
  input.SetActionValue("ACR_RTSPanForward", 0);
  vector fast = camera.m_Camera.GetOrigin();
  ACR_TestRegress.Check("RTS: Shift physically accelerates pan", vector.DistanceXZ(after, fast) > vector.DistanceXZ(before, after) * 2);
  ACR_TestRegress.Check("RTS: body did not pan with camera", vector.DistanceXZ(bodyBefore, camera.m_Body.GetOrigin()) < 0.1);
  vector angleBefore = camera.m_Camera.GetAngles();
  camera.BeginOrbit(200, 200);
  camera.ApplyOrbit(260, 220);
  camera.EndOrbit(260, 220, true);
  vector angleAfter = camera.m_Camera.GetAngles();
  ACR_TestRegress.Check("RTS: RMB drag physically rotates camera", Math.AbsFloat(angleAfter[0] - angleBefore[0]) > 5);
  ACR_TestRegress.Check("RTS: RMB release clears orbit", !camera.m_bOrbit && camera.m_bDragged);
  // Look back at the anchor and exercise the actual screen ray, not a fake point.
  camera.m_Camera.SetOrigin(before);
  camera.m_Camera.SetAngles(angleBefore);
  vector ground = camera.m_Body.GetOrigin();
  ground[1] = GetGame().GetWorld().GetSurfaceY(ground[0], ground[2]);
  vector transform[4];
  camera.m_Camera.GetWorldTransform(transform);
  vector target = before + transform[2] * 60;
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  vector pixel = GetGame().GetWorkspace().ProjWorldToScreenNative(target, GetGame().GetWorld());
  vector hit;
  IEntity hitEntity;
  bool picked = ScreenToWorld(pixel[0], pixel[1], hit, hitEntity);
  PrintFormat("[ACR] RTS PICK pixel=%1 target=%2 hit=%3 direction=%4", pixel, target, hit, transform[2]);
  ACR_TestRegress.Check("RTS: projected terrain resolves actual world hit", picked && vector.DistanceXZ(target, hit) < 5);
  CameraBase previous = camera.m_PreviousCamera;
  Widget oldRoot = camera.m_RootWidget;
  Close();
  ACR_TestRegress.Check("RTS: exit restores previous native camera", !IsActive() && GetGame().GetCameraManager().CurrentCamera() == previous);
  ACR_TestRegress.Check("RTS: exit removes interactive overlay", !oldRoot || !oldRoot.GetParent());
 }
}

modded class ACR_TestCommand
{
 override void TestRespawn()
 {
  if (System.IsCLIParam("acrRTSCameraTest")) { TestRTSCamera(); return; }
  super.TestRespawn();
 }

 void TestRTSCamera()
 {
  if (h.m_Ticks == 5)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
   if (!pc || !ACR_Respawn.IsAlive(pc.GetControlledEntity())) { ACR_TestRegress.Abort("rtscamera", "live client required"); return; }
   ACR_RTSCameraManager.Open();
  }
  if (h.m_Ticks == 6) ACR_RTSCameraManager.CheckNativeRTS();
  if (h.m_Ticks == 8) ACR_TestRegress.Complete("rtscamera", 11);
 }
}
