// Contact diagnostics for the loaded-airframe fixture; stripped from player builds.
modded class SCR_RotorDamageManagerComponent
{
 override void OnFilteredContact(IEntity owner, IEntity other, Contact contact)
 {
  if (System.IsCLIParam("acrAirLiftContactTest"))
  {
   IEntity otherRoot;
   if (other) otherRoot = other.GetRootParent();
   PrintFormat("[ACR] LIFTCONTACT rotor %1 of %2 hit %3 (root %4) at %5, impulse %6", owner, owner.GetRootParent(), other, otherRoot, contact.Position, contact.Impulse);
  }
  super.OnFilteredContact(owner, other, contact);
 }
}
