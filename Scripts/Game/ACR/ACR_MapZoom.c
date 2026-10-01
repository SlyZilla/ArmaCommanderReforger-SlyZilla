// The mouse wheel over the command panel scrolls its lists (requisition, supports,
// intel) instead of also zooming the map underneath.
modded class SCR_MapCursorModule
{
 protected bool ACR_WheelOnPanel()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  return pc && pc.ACR_CursorOnPanel();
 }

 override protected void OnInputZoomWheelUp(float value, EActionTrigger reason)
 {
  if (ACR_WheelOnPanel()) return;
  super.OnInputZoomWheelUp(value, reason);
 }

 override protected void OnInputZoomWheelDown(float value, EActionTrigger reason)
 {
  if (ACR_WheelOnPanel()) return;
  super.OnInputZoomWheelDown(value, reason);
 }
}
