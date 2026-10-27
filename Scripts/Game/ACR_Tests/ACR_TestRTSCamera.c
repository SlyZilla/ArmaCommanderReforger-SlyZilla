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

 // -acrRTSAimTest 1 (a game client, ~1 min): the pn12d RTS notes. A right-drag with a tool armed draws
 // the gold arrow on the HUD from the pressed spot to the cursor, keeps it while the button is held and
 // clears it on release; emptying or changing the selection disarms the tool (a right-drag turns the
 // camera again), while arming with nothing selected and then selecting keeps it. Two screenshots by
 // the game itself (the window needn't be in front): $profile:acr-shots/rts_aim_arrow.png, rts_disarmed.png.
 protected static int s_iFromX;
 protected static int s_iFromY;
 protected static int s_iAimX;
 protected static int s_iAimY;

 static void CheckAimDrag()
 {
  ACR_RTSCameraManager camera = s_Instance;
  if (!camera || !camera.m_Camera) { ACR_TestRegress.Abort("rtsaim", "camera did not open"); return; }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_MapData data = pc.ACR_GetData();
  if (!data || data.m_GroupIds.IsEmpty()) { ACR_TestRegress.Abort("rtsaim", "no friendly squad"); return; }
  // Look down at the ground ahead, so both ends of the drag land on it.
  vector angles = camera.m_Camera.GetYawPitchRoll();
  angles[1] = -55;
  camera.m_Camera.SetYawPitchRoll(angles);
  float w, hgt;
  camera.m_InteractionLayer.GetScreenSize(w, hgt);
  s_iFromX = w * 0.38;
  s_iFromY = hgt * 0.7;
  s_iAimX = w * 0.62;
  s_iAimY = hgt * 0.5;
  data.m_Selected.Clear();
  camera.WatchSelection(pc);
  pc.ACR_SelectGroup(data.m_GroupIds[0]);
  camera.WatchSelection(pc);
  camera.Disarm();
  camera.m_bDefendMode = true;
  camera.BeginOrbit(s_iFromX, s_iFromY);
  ACR_TestRegress.Check("RTS aim: a right-press with Defend armed starts aiming", camera.m_bAiming, string.Format("pressed at %1,%2 on a %3 x %4 screen, the ground at %5", s_iFromX, s_iFromY, w, hgt, camera.m_vAimFrom));
  GetGame().GetInputManager().GetMouseDeviceHandler().SetCursorPosition(s_iAimX, s_iAimY);
  camera.ApplyOrbit(s_iAimX, s_iAimY);
  string detail;
  bool drawn = ArrowAt(camera, s_iAimX, s_iAimY, detail);
  ACR_TestRegress.Check("RTS aim: a drag draws the gold arrow on the HUD, from the pressed spot to the cursor", drawn, detail);
 }

 // The arrow's six strokes (a dark edge, then gold: shaft and two head strokes each), the gold
 // shaft from the pressed spot's projection (on the pressed pixel, so the units agree) to (x, y).
 static bool ArrowAt(ACR_RTSCameraManager camera, int x, int y, out string detail)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  vector from = ws.ProjWorldToScreenNative(camera.m_vAimFrom + Vector(0, 0.5, 0), GetGame().GetWorld());
  int strokes = camera.m_AimCommands.Count();
  detail = string.Format("%1 strokes, pressed at %2,%3 (its spot projects to %4), the cursor at %5,%6", strokes, s_iFromX, s_iFromY, from, x, y);
  if (strokes != 6 || !camera.m_AimCanvas || !camera.m_AimCanvas.IsVisible()) return false;
  LineDrawCommand shaft = LineDrawCommand.Cast(camera.m_AimCommands[3]);
  if (!shaft || shaft.m_Vertices.Count() != 4) return false;
  detail += string.Format("; the gold shaft %1,%2 to %3,%4", shaft.m_Vertices[0], shaft.m_Vertices[1], shaft.m_Vertices[2], shaft.m_Vertices[3]);
  bool ends = Math.AbsFloat(shaft.m_Vertices[0] - from[0]) < 1 && Math.AbsFloat(shaft.m_Vertices[1] - from[1]) < 1;
  ends = ends && Math.AbsFloat(shaft.m_Vertices[2] - x) < 1 && Math.AbsFloat(shaft.m_Vertices[3] - y) < 1;
  float offPress = Math.Sqrt((from[0] - s_iFromX) * (from[0] - s_iFromX) + (from[1] - s_iFromY) * (from[1] - s_iFromY));
  detail += string.Format(", %1 px off the pressed pixel", offPress);
  return ends && offPress < ws.DPIScale(30);
 }

 static void Shot(string name)
 {
  // The engine saves the last frame drawn: called a tick after the state to show.
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/" + name);
  PrintFormat("[ACR] RTSAIM shot %1", name);
 }

 static void CheckAimRelease()
 {
  ACR_RTSCameraManager camera = s_Instance;
  if (!camera) return;
  // Frames since then redrew it at the real cursor (moved there by SetCursorPosition).
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  ACR_TestRegress.Check("RTS aim: the arrow stays while the button is held", camera.m_bAiming && camera.m_AimCommands.Count() == 6, string.Format("the cursor at %1,%2, %3 strokes", mx, my, camera.m_AimCommands.Count()));
  camera.EndOrbit(s_iAimX, s_iAimY, true);
  ACR_TestRegress.Check("RTS aim: letting go clears the arrow", !camera.m_bAiming && camera.m_AimCommands.IsEmpty(), string.Format("%1 strokes left", camera.m_AimCommands.Count()));
 }

 static void CheckDisarmOnEmpty()
 {
  ACR_RTSCameraManager camera = s_Instance;
  if (!camera) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_MapData data = pc.ACR_GetData();
  camera.Disarm();
  camera.m_bRoadblockMode = true;
  data.m_Selected.Clear();
  camera.WatchSelection(pc);
  bool off = !camera.AimArmed();
  vector before = camera.m_Camera.GetYawPitchRoll();
  camera.BeginOrbit(200, 200);
  bool aiming = camera.m_bAiming;
  camera.ApplyOrbit(260, 200);
  camera.EndOrbit(260, 200, true);
  vector after = camera.m_Camera.GetYawPitchRoll();
  ACR_TestRegress.Check("RTS: emptying the selection disarms Roadblock, and a right-drag turns the camera again", off && !aiming && Math.AbsFloat(after[0] - before[0]) > 5, string.Format("armed after %1, aiming %2, yaw %3 to %4", !off, aiming, before[0], after[0]));
 }

 static void CheckDisarmOnChange()
 {
  ACR_RTSCameraManager camera = s_Instance;
  if (!camera) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_MapData data = pc.ACR_GetData();
  pc.ACR_SelectGroup(data.m_GroupIds[0]);
  camera.WatchSelection(pc);
  camera.m_bRoadblockMode = true;
  if (data.m_GroupIds.Count() < 2) ACR_TestRegress.Skip("RTS: selecting another squad disarms the tool", "one friendly squad");
  else
  {
   pc.ACR_SelectGroup(data.m_GroupIds[1]);
   camera.WatchSelection(pc);
   ACR_TestRegress.Check("RTS: selecting another squad disarms the tool", !camera.AimArmed(), string.Format("%1 squads selected", data.m_Selected.Count()));
  }
  data.m_Selected.Clear();
  camera.WatchSelection(pc);
  camera.m_bRoadblockMode = true;
  pc.ACR_SelectGroup(data.m_GroupIds[0]);
  camera.WatchSelection(pc);
  ACR_TestRegress.Check("RTS: arming with nothing selected, then selecting a squad, keeps the tool", camera.m_bRoadblockMode, "");
  camera.Disarm();
 }
}

modded class ACR_TestCommand
{
 override void TestRespawn()
 {
  if (System.IsCLIParam("acrRTSCameraTest")) { TestRTSCamera(); return; }
  if (System.IsCLIParam("acrRTSAimTest")) { TestRTSAim(); return; }
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

 void TestRTSAim()
 {
  if (h.m_Ticks == 5)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
   if (!pc || !ACR_Respawn.IsAlive(pc.GetControlledEntity())) { ACR_TestRegress.Abort("rtsaim", "live client required"); return; }
   ACR_RTSCameraManager.Open();
  }
  if (h.m_Ticks == 7) ACR_RTSCameraManager.CheckAimDrag();
  if (h.m_Ticks == 8) ACR_RTSCameraManager.Shot("rts_aim_arrow");
  if (h.m_Ticks == 10) ACR_RTSCameraManager.CheckAimRelease();
  if (h.m_Ticks == 12) ACR_RTSCameraManager.CheckDisarmOnEmpty();
  if (h.m_Ticks == 13) ACR_RTSCameraManager.Shot("rts_disarmed");
  if (h.m_Ticks == 15) ACR_RTSCameraManager.CheckDisarmOnChange();
  if (h.m_Ticks == 16)
  {
   ACR_RTSCameraManager.Close();
   ACR_TestRegress.Complete("rtsaim", 7);
  }
 }
}
