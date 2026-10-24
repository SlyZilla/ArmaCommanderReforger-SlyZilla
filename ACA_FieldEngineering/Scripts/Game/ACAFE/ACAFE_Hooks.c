// The few places ACA Field Engineering hooks into ACA and the game (kept small on purpose:
// ACA is alpha and updates often). Everything else is in our own classes.

// Our jobs tick with ACA's own server tick (every second, server only, after setup).
modded class ACR_Manager
{
 override void Tick()
 {
  super.Tick();
  if (ACR_Setup.Started()) ACAFE_Engineering.Get().Tick();
 }
}

// The owner's command map: ACA draws these zones (danger rings, and "~" pale rings).
modded class ACR_Mortars
{
 override void Ranges(string factionKey, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  super.Ranges(factionKey, positions, radii, labels);
  ACAFE_Engineering.Zones(factionKey, positions, radii, labels);
 }
}

// Friendly fire: our mines stay quiet for the side that laid them (ACAFE_Config).
// AP mines reach this through SCR_AntiPersonnelMineCollisionHandlerComponent, AT mines
// through physics contacts; both before the vanilla weight check.
modded class SCR_PressureTriggerComponent
{
 override void EOnContact(IEntity owner, IEntity other, Contact contact)
 {
  if (ACAFE_Mines.Spares(owner, other)) return;
  super.EOnContact(owner, other, contact);
 }
}
