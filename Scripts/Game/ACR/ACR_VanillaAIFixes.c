// Vanilla processes a group's failed move by reading its leader's position without checking that
// it has one (SCR_AIProcessFailedMovementResult.c:59, m_Group.GetLeaderEntity().GetOrigin()). A
// group with no leader entity just then (its leader dead, despawned or between members) threw a
// "NULL pointer to instance" VM exception on each failed move: 4 in a playtest session (25 Oct).
// With no leader there is no move to process: the node fails, as it would have after the
// exception, without it.
modded class SCR_AIProcessFailedMovementResult
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  if (m_Group && !m_Group.GetLeaderEntity()) return ENodeResult.FAIL;
  return super.EOnTaskSimulate(owner, dt);
 }
}
