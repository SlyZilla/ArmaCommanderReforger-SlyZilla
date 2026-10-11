// Aircraft hulls are already filtered by vanilla group perception, but a gunshot
// reported under the seated gunner's character bypasses that aircraft check.
class BBB_SuppressionSafety
{
 static bool s_bRead;
 static bool s_bOn = true;

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbSkySuppression", value) && value == "0") s_bOn = false;
 }

 static bool AircraftSource(IEntity source)
 {
  Read();
  return s_bOn && BBB_Armour.Aircraft(BBB_Armour.VehicleOf(source));
 }

 static bool GroundSector(vector position)
 {
  Read();
  return !s_bOn || BBB_Combat.OnGround(position);
 }
}

modded class SCR_AIGroupPerception
{
 override void AddOrUpdateGunshot(notnull IEntity shooter, vector worldPos, Faction faction, float timestamp, bool endangering)
 {
  // This is classification of the reported source, never a live position query.
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  bool aircraft = BBB_SuppressionSafety.AircraftSource(shooter);
  if (BBB_Perf.s_bOn) BBB_Perf.Add("ev.skyFilter", System.GetTickCount() - started);
  if (aircraft)
  {
   BBB_Perf.Count("suppression.aircraftSkipped");
   return;
  }
  super.AddOrUpdateGunshot(shooter, worldPos, faction, timestamp, endangering);
 }
}
