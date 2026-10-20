// RHS folds its flip-up backup sights (MBUS, KAC, Matech attachments) down by default,
// expecting an optic in front of them. A rifle that spawns without an optic then has no
// sights at all (Jacob 19 Oct: some US soldiers). Shortly after a weapon is created, each
// machine flips such sights up on its own copy when the weapon carries no other optic; the
// server's state is what RHS hands clients that stream the weapon in later.
// Only Front/BackSight flips on attachments are touched: stock folds use the same action, and
// rifles that fold their own irons do so only in factory-optic variants.
class ACR_FlipSights : Managed
{
 bool m_bFlip;                  // the prefab has an RHS flip action (so it is not an optic)
 ref array<string> m_aSignals = {};
 ref array<float> m_aUp = {};   // the unfolded value of each signal
}

class ACR_IronSights
{
 static const int DELAY_MS = 2000; // after RHS folds them (server) or loads the server state (client)
 protected static ref map<ResourceName, ref ACR_FlipSights> s_Prefabs = new map<ResourceName, ref ACR_FlipSights>();
 static int s_iFlipped;         // tests: sights flipped up

 static void Schedule(IEntity weapon)
 {
  if (weapon) GetGame().GetCallqueue().CallLater(Apply, DELAY_MS, false, weapon);
 }

 static void Apply(IEntity weapon)
 {
  if (!weapon || weapon.IsDeleted()) return;
  // Rails carry attachment storage too: only whole weapons see every optic.
  if (!weapon.FindComponent(BaseWeaponComponent) || weapon.GetParent() && weapon.GetParent().FindComponent(BaseWeaponComponent)) return;
  array<IEntity> parts = {};
  Collect(weapon, parts);
  array<IEntity> flips = {};
  foreach (IEntity part : parts)
  {
   ACR_FlipSights info = Info(part);
   if (info.m_bFlip)
   {
    if (!info.m_aSignals.IsEmpty()) flips.Insert(part);
   }
   // An optic: RHS's folded default is right. An underbarrel launcher has its own sights
   // and muzzle but is not an optic for the rifle.
   else if (part.FindComponent(BaseSightsComponent) && !part.FindComponent(BaseMuzzleComponent)) return;
  }
  foreach (IEntity sight : flips)
  {
   SignalsManagerComponent signals = SignalsManagerComponent.Cast(sight.FindComponent(SignalsManagerComponent));
   if (!signals) continue;
   ACR_FlipSights up = Info(sight);
   foreach (int i, string name : up.m_aSignals)
   {
    int index = signals.FindSignal(name);
    if (index < 0 || signals.GetSignalValue(index) == up.m_aUp[i]) continue;
    signals.SetSignalValue(index, up.m_aUp[i]);
    s_iFlipped++;
   }
  }
 }

 protected static void Collect(IEntity parent, notnull array<IEntity> parts)
 {
  for (IEntity child = parent.GetChildren(); child; child = child.GetSibling())
  {
   parts.Insert(child);
   Collect(child, parts);
  }
 }

 // What the part's prefab says about RHS flip actions (read once per prefab).
 static ACR_FlipSights Info(IEntity part)
 {
  EntityPrefabData data = part.GetPrefabData();
  ResourceName key;
  if (data) key = data.GetPrefabName();
  ACR_FlipSights info = s_Prefabs.Get(key);
  if (info) return info;
  info = new ACR_FlipSights();
  s_Prefabs.Set(key, info);
  BaseContainer prefab;
  if (data) prefab = data.GetPrefab();
  BaseContainerList components;
  if (prefab) components = prefab.GetObjectArray("components");
  if (!components) return info;
  for (int c = 0; c < components.Count(); c++)
  {
   BaseContainer component = components.Get(c);
   if (component.GetClassName() != "ActionsManagerComponent") continue;
   BaseContainerList actions = component.GetObjectArray("additionalActions");
   if (!actions) continue;
   for (int a = 0; a < actions.Count(); a++)
   {
    BaseContainer action = actions.Get(a);
    if (action.GetClassName() != "RHS_FlipSightUserAction") continue;
    info.m_bFlip = true;
    string signal;
    bool folded;
    float upValue;
    action.Get("m_sSignalName", signal);
    action.Get("m_bShouldBeFoldedByDefault", folded);
    action.Get("m_fUnfoldedAngle", upValue);
    if (!folded || (signal != "FrontSight" && signal != "BackSight")) continue;
    info.m_aSignals.Insert(signal);
    info.m_aUp.Insert(upValue);
   }
  }
  return info;
 }
}

modded class SCR_WeaponAttachmentsStorageComponent
{
 override protected void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  ACR_IronSights.Schedule(owner);
 }
}
