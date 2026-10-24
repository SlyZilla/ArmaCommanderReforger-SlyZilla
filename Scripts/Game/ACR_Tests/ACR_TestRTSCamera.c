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
  ACR_TestRegress.Check("RTS: cursor context activated", camera.m_bContextActivated);
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
  vector look = camera.m_Camera.GetWorldTransformAxis(2);
  vector moved = after - before;
  look[1] = 0;
  moved[1] = 0;
  ACR_TestRegress.Check("RTS: forward pan follows view heading", vector.Dot(look.Normalized(), moved.Normalized()) > 0.95);
  input.SetActionValue("ACR_MapShift", 1);
  input.SetActionValue("ACR_RTSPanForward", 1);
  camera.ApplyInput(input);
  input.SetActionValue("ACR_MapShift", 0);
  input.SetActionValue("ACR_RTSPanForward", 0);
  vector fast = camera.m_Camera.GetOrigin();
  ACR_TestRegress.Check("RTS: Shift physically accelerates pan", vector.DistanceXZ(after, fast) > vector.DistanceXZ(before, after) * 2);
  ACR_TestRegress.Check("RTS: body did not pan with camera", vector.DistanceXZ(bodyBefore, camera.m_Body.GetOrigin()) < 0.1);
  vector angleBefore = camera.m_Camera.GetYawPitchRoll();
  camera.BeginOrbit(200, 200);
  camera.ApplyOrbit(260, 200);
  vector angleMid = camera.m_Camera.GetYawPitchRoll();
  ACR_TestRegress.Check("RTS: horizontal RMB drag turns yaw only", Math.AbsFloat(angleMid[0] - angleBefore[0]) > 5 && Math.AbsFloat(angleMid[1] - angleBefore[1]) < 0.5);
  camera.ApplyOrbit(260, 240);
  camera.EndOrbit(260, 240, true);
  vector angleAfter = camera.m_Camera.GetYawPitchRoll();
  ACR_TestRegress.Check("RTS: vertical RMB drag tilts pitch only", Math.AbsFloat(angleAfter[1] - angleMid[1]) > 5 && Math.AbsFloat(angleAfter[0] - angleMid[0]) < 0.5);
  ACR_TestRegress.Check("RTS: RMB release clears orbit", !camera.m_bOrbit && camera.m_bDragged);
  // Look back at the anchor and exercise the actual screen ray, not a fake point.
  camera.m_Camera.SetOrigin(before);
  camera.m_Camera.SetYawPitchRoll(angleBefore);
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
  // Wheel: one raw notch (120) is a small step; the full range takes many notches.
  float atlBefore = camera.m_fTargetAtl;
  camera.ApplyZoom(120);
  float step = 1 - camera.m_fTargetAtl / atlBefore;
  ACR_TestRegress.Check("RTS: one wheel notch is a fine altitude step", step > 0.05 && step < 0.25);
  int notches = 0;
  while (camera.m_fTargetAtl < ATL_MAX && notches < 100) { camera.ApplyZoom(-120); notches++; }
  PrintFormat("[ACR] RTS ZOOM step=%1 notchesToMax=%2", step, notches);
  ACR_TestRegress.Check("RTS: altitude range spans many wheel notches", notches >= 10 && notches < 100);
  camera.m_fTargetAtl = atlBefore;
  // Click the first on-screen squad marker the way the mouse would (native pixels).
  camera.UpdateMarkers(pc);
  ACR_MapData data = pc.ACR_GetData();
  int shown = -1;
  for (int i = 0; i < camera.m_SquadMarkerPos.Count(); i++)
  {
   vector mp = camera.m_SquadMarkerPos[i];
   if (mp[2] > 0) { shown = i; break; }
  }
  if (shown < 0) ACR_TestRegress.Skip("RTS: clicking a squad marker selects it", "no friendly squad in view");
  else
  {
   vector clickAt = camera.m_SquadMarkerPos[shown];
   WorkspaceWidget ws = GetGame().GetWorkspace();
   camera.HandleLeftClick(ws.DPIScale(clickAt[0]), ws.DPIScale(clickAt[1]));
   ACR_TestRegress.Check("RTS: clicking a squad marker selects it", data.IsSelected(data.m_GroupIds[shown]));
  }
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
  if (h.m_Ticks == 8) ACR_TestRegress.Complete("rtscamera", 16);
 }
}
