// A Defend activity still running after a new order replaced its waypoint reads the group's
// current waypoint, now the new one (or its own, already gone): vanilla's Defend nodes then raise
// "Wrong class of provided Waypoint!" / "Defend Waypoint not provided to the node!" (crash.log,
// 25 Oct playtests; srv:defendswap reproduces it). The activity ends on the group's next
// evaluation anyway. Like vanilla's own SCR_AIGetWaypointParameters for a deleted waypoint ("we
// didnt react on it, yet"), these nodes now just fail, without the error.
modded class SCR_AIGetDefendWaypointParameters
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  IEntity waypoint;
  if (!GetVariableIn(PORT_WAYPOINT_IN, waypoint))
  {
   AIGroup group = AIGroup.Cast(owner);
   if (group) waypoint = group.GetCurrentWaypoint();
  }
  if (waypoint && !SCR_DefendWaypoint.Cast(waypoint)) return ENodeResult.FAIL;
  return super.EOnTaskSimulate(owner, dt);
 }
}

modded class SCR_AIAllocateActionsForDefendActivity
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  IEntity waypoint;
  GetVariableIn(PORT_WAYPOINT, waypoint);
  if (!SCR_DefendWaypoint.Cast(waypoint)) return ENodeResult.FAIL;
  return super.EOnTaskSimulate(owner, dt);
 }
}
