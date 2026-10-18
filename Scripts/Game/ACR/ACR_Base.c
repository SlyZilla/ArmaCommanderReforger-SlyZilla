// A base (control/resource point) placed by the mission maker in World Editor.
// Mirrors Arma Commander's base module: owner, garrison, RP value, income and
// hidden flags, and a capture radius. All runtime logic lives in ACR_BaseSystem.
class ACR_BaseClass : GenericEntityClass {}
class ACR_Base : GenericEntity
{
 [Attribute("", UIWidgets.EditBox, "Base name shown on the map")]
 string m_sName;
 [Attribute("", UIWidgets.EditBox, "Starting owner faction key (e.g. US, USSR). Empty = nobody")]
 string m_sOwner;
 [Attribute("10", UIWidgets.EditBox, "Garrison: AI soldiers guarding the base")]
 int m_iGarrison;
 [Attribute("1", UIWidgets.EditBox, "Requisition points this base adds to its owner's income")]
 int m_iValue;
 [Attribute("1", UIWidgets.CheckBox, "Produces income. Off = a purely tactical control point")]
 bool m_bIncome;
 [Attribute("0", UIWidgets.CheckBox, "Hidden from other factions until one of their units enters it")]
 bool m_bHidden;
 [Attribute("1", UIWidgets.CheckBox, "Purchased units and reinforcements can deploy here")]
 bool m_bAllowSpawn;
 [Attribute("1", UIWidgets.CheckBox, "Allows RP infantry replacements; separate from new-unit deployment")]
 bool m_bAllowReinforce;
 [Attribute("0", UIWidgets.ComboBox, "Logistics role: Authored preserves existing deployment flags", enums: ParamEnumArray.FromEnum(ACR_EBaseRole))]
 ACR_EBaseRole m_eLogisticsRole;
 [Attribute("50", UIWidgets.EditBox, "Capture radius in metres")]
 float m_fRadius;

 static ref array<ACR_Base> s_All = {};

 // ---- server runtime state
 float m_fScore = 100;              // owned: 100 = secure, -100 = lost. Neutral: 0..100 progress for m_sCapturer
 string m_sCapturer;                // neutral base: faction currently taking it
 bool m_bContested;
 int m_iGarrisonLeft;               // soldiers still in reserve (virtual while not deployed)
 int m_iHousingGarrison;            // capacity supplied by completed friendly FOB barracks
 SCR_AIGroup m_GarrisonGroup;       // spawned only while enemies are near
 ref array<string> m_aDetectedBy = {};
 bool m_bBuilt;                     // a forward base made by a built HQ (ACR_Structures)
 bool m_bRetired;                   // its HQ is gone: no longer a base
 IEntity m_FobSupply;               // replicated building provider; supplies are paid, never generated
 int m_iSupplyDelivery;
 float m_fSupplyDeliveryLeft;
 float m_fSupplyDeliveryUpdated;
 string m_sSupplyDeliveryFaction;

 // A built forward base whose HQ is gone: off the map and out of every base list.
 // Also used for bases left out of a skirmish layout (ACR_Skirmish).
 void Retire(string why = "its HQ is gone")
 {
  if (m_bRetired) return;
  m_bRetired = true;
  m_iSupplyDelivery = 0;
  m_fSupplyDeliveryLeft = 0;
  if (m_FobSupply) SCR_EntityHelper.DeleteEntityAndChildren(m_FobSupply);
  m_FobSupply = null;
  if (s_All) s_All.RemoveItem(this);
  if (m_GarrisonGroup) SCR_EntityHelper.DeleteEntityAndChildren(m_GarrisonGroup);
  PrintFormat("[ACR] BASE %1 retired (%2)", m_sName, why);
 }

 void ACR_Base(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  if (m_sName.IsEmpty()) m_sName = string.Format("Base %1", s_All.Count() + 1);
  m_iGarrisonLeft = m_iGarrison;
  if (m_sOwner.IsEmpty()) m_fScore = 0;
  s_All.Insert(this);
 }

 Faction GetOwnerFaction()
 {
  if (m_sOwner.IsEmpty()) return null;
  return GetGame().GetFactionManager().GetFactionByKey(m_sOwner);
 }

 // Name as shown on the map, e.g. "Farm [1]".
 string GetLabel()
 {
  if (m_bIncome) return string.Format("%1 [%2]", m_sName, m_iValue);
  return m_sName;
 }

 bool IsVisibleTo(string factionKey)
 {
  return !m_bHidden || m_sOwner == factionKey || m_aDetectedBy.Contains(factionKey);
 }

 void ~ACR_Base()
 {
  if (s_All) s_All.RemoveItem(this);
 }
}
