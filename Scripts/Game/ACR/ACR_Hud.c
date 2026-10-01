// Widgets we put straight on the workspace (HUD ping icons and labels, the
// capture bar, the ping-key hint) outlive the mission: the workspace stays
// when the world unloads, so they stayed on screen in the main menu. Every such
// widget is registered here and removed when the mode stops (ACR_Manager.Stop).
class ACR_Hud
{
 protected static ref array<Widget> s_Widgets = {};
 static int s_iCleared; // for the tests

 static Widget Track(Widget widget)
 {
  if (widget && !s_Widgets.Contains(widget)) s_Widgets.Insert(widget);
  return widget;
 }

 static void Clear()
 {
  foreach (Widget widget : s_Widgets)
  {
   if (!widget) continue;
   widget.RemoveFromHierarchy();
   s_iCleared++;
  }
  s_Widgets.Clear();
  ACR_PingDisplay.Forget();
 }
}
