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
