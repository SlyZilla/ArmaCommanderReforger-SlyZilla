// Extra Personal Belongings bios (flavour text) for soldiers, mixed into the game's own
// bio pool. The bios are in Configs/BBB/BBB_Bios.conf (one group, "BBB_Flavor"); this
// file appends that group to the identity manager's character bios on every machine
// (same position everywhere, so the replicated group/bio indexes match).
//
// Inside the group each bio has its own weight (common 100, uncommon 25, rare 5,
// easter egg 1, ultra rare 0.2, legendary 0.05). Ultra rare and legendary bios are
// unique: at most once per session. The bios just used are held back for a while so
// a squad rarely shows the same one twice. The group's m_iWeight sets its share of
// all bios (2850 is about 1 in 5 for a US/USSR soldier).
// Host option: -bbbBios 0 switches them off (server side; clients need nothing).

[BaseContainerProps(), BaseContainerCustomDoubleCheckTitleField("m_bIsUnique", "m_sKey", "1", "UNIQUE - %1", "%1")]
class BBB_Bio : SCR_IdentityBio
{
 [Attribute(desc: "Short id for log lines")]
 protected string m_sKey;
 [Attribute("100", desc: "Chance weight within BBB's bios: common 100, uncommon 25, rare 5, easter egg 1, ultra rare 0.2, legendary 0.05")]
 protected float m_fWeight;

 string GetKey() { return m_sKey; }
 float GetWeight() { return m_fWeight; }
}

[BaseContainerProps(configRoot: true), BaseContainerCustomDoubleTitleField("m_sBioGroupID", "m_iWeight")]
class BBB_BioGroup : SCR_CharacterIdentityBioGroupConfig
{
 // Bios assigned lately (non-unique); skipped until this many others were given out.
 protected static const int RECENT = 12;
 protected ref array<int> m_aRecent = {};

 void SetEnabled(bool on) { m_bEnabled = on; }
 int BioCount()
 {
  if (!m_aIdentityList) return 0;
  return m_aIdentityList.Count();
 }

 protected float WeightOf(int index)
 {
  if (m_aRecent.Contains(index)) return 0;
  BBB_Bio bio = BBB_Bio.Cast(m_aIdentityList[index]);
  if (!bio) return 1;
  return bio.GetWeight();
 }

 // Weighted pick (vanilla picks evenly inside a group).
 override void AssignRandomAvailableBio(RandomGenerator randomizer, IEntity entity, out int index, out SCR_IdentityBio bio)
 {
  if (m_aAvailableIdentityBioIndexList.IsEmpty()) return;
  float total = 0;
  foreach (int i : m_aAvailableIdentityBioIndexList) total += WeightOf(i);
  int count = m_aAvailableIdentityBioIndexList.Count();
  if (total <= 0)
  {
   index = m_aAvailableIdentityBioIndexList[randomizer.RandInt(0, count)];
  }
  else
  {
   float roll = randomizer.RandFloat01() * total;
   index = -1;
   foreach (int j : m_aAvailableIdentityBioIndexList)
   {
    float w = WeightOf(j);
    if (w <= 0) continue;
    index = j;
    roll -= w;
    if (roll <= 0) break;
   }
  }
  bio = OnCharacterBioAssigned(entity, index);
  BBB_Bio ours = BBB_Bio.Cast(bio);
  if (ours) Print("[BBB] BIO " + ours.GetKey(), LogLevel.NORMAL);
 }

 // Unique bios leave the pool for good; the others stay (vanilla would hand out every
 // other bio of the group first, which would make the rare ones common late in a session).
 override SCR_IdentityBio OnCharacterBioAssigned(IEntity entity, int index)
 {
  if (!m_aIdentityList || !m_aIdentityList.IsIndexValid(index)) return null;
  SCR_IdentityBio bio = m_aIdentityList[index];
  if (!bio || m_iWeight <= 0) return bio;
  if (bio.IsUnique())
  {
   if (m_aAvailableIdentityBioIndexList.Count() > 1) m_aAvailableIdentityBioIndexList.RemoveItem(index);
   return bio;
  }
  m_aRecent.RemoveItem(index);
  m_aRecent.Insert(index);
  if (m_aRecent.Count() > RECENT) m_aRecent.RemoveOrdered(0);
  return bio;
 }

 override void ResetAvailable()
 {
  super.ResetAvailable();
  if (m_aRecent) m_aRecent.Clear();
 }
}

class BBB_Bios
{
 static const ResourceName CONFIG = "{BBB0B10500000001}Configs/BBB/BBB_Bios.conf";

 static void Install(array<ref SCR_IdentityBioTypeHolderConfig> holders)
 {
  if (SCR_Global.IsEditMode() || !holders) return;
  SCR_IdentityBioTypeHolderConfig holder;
  foreach (SCR_IdentityBioTypeHolderConfig candidate : holders)
  {
   if (candidate && candidate.m_eIdentityType == SCR_EIdentityType.CHARACTER) holder = candidate;
  }
  if (!holder)
  {
   Print("[BBB] BIOS no character bio list found, bios not added", LogLevel.WARNING);
   return;
  }
  if (!holder.m_aIdentityBioGroups) holder.m_aIdentityBioGroups = {};
  foreach (SCR_IdentityBioGroupConfig existing : holder.m_aIdentityBioGroups)
  {
   if (BBB_BioGroup.Cast(existing)) return;
  }
  BBB_BioGroup group = BBB_BioGroup.Cast(SCR_BaseContainerTools.CreateInstanceFromPrefab(CONFIG, true));
  if (!group)
  {
   Print("[BBB] BIOS failed to load " + CONFIG, LogLevel.ERROR);
   return;
  }
  holder.m_aIdentityBioGroups.Insert(group);
  bool on = true;
  string value;
  if (System.GetCLIParam("bbbBios", value) && value == "0") on = false;
  group.SetEnabled(on);
  Print(string.Format("[BBB] BIOS %1 bios added as bio group %2 (weight %3, %4)", group.BioCount(), holder.m_aIdentityBioGroups.Count() - 1, group.GetWeight(), on), LogLevel.NORMAL);
 }
}

modded class SCR_IdentityManagerComponent
{
 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  if (m_sInstance == this) BBB_Bios.Install(m_aIdentityBioTypeHolders);
 }
}
