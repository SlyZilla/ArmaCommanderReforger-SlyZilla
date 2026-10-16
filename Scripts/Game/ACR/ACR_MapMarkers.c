// Vanilla parks off-screen static markers in a "disabled" list and only revives them
// on pan end. A marker removed while parked stayed in that list and came back as a
// widget nobody updates (the "stuck marker"). ACR markers are never parked, and are
// removed from both lists.
modded class SCR_MapMarkerManagerComponent
{
 void ACR_RemoveMarker(SCR_MapMarkerBase marker)
 {
  if (!marker) return;
  marker.OnDelete();
  m_aStaticMarkers.RemoveItem(marker);
  m_aDisabledMarkers.RemoveItem(marker);
 }
}

// Vanilla's SetTextVisible(true) enables hover text, then immediately hides the widget.
// Base captions identify economic/tactical CPs, so keep those visible without hovering.
// Other marker types retain their normal hover behaviour.
modded class SCR_MapMarkerWidgetComponent
{
 protected void ACR_KeepBaseCaptionVisible()
 {
  ACR_UnitMarker marker = ACR_UnitMarker.Cast(m_MarkerObject);
  if (marker && marker.m_BaseIndex >= 0 && m_wMarkerText) m_wMarkerText.SetVisible(true);
 }

 override void SetTextVisible(bool state)
 {
  super.SetTextVisible(state);
  ACR_KeepBaseCaptionVisible();
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  bool handled = super.OnMouseLeave(w, enterW, x, y);
  ACR_KeepBaseCaptionVisible();
  return handled;
 }
}
