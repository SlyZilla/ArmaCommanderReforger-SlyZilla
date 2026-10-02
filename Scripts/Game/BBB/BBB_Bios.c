// Extra Personal Belongings identities: bios, plus rare reference names, birthdays and
// blood types, all mixed into the game's own bio pool. Two bio groups:
// Configs/BBB/BBB_Bios.conf ("BBB_Flavor", soldiers: military factions only) and
// Configs/BBB/BBB_CivBios.conf ("BBB_Civilian", civilian factions only; the CIV faction must
// be in the world's faction manager, as in Conflict/Game Master, not in AC: Actual's worlds).
// This file appends both groups to the identity manager's character bios on every machine
// (same position everywhere, so the replicated group/bio indexes match, also for JIP).
//
// Names, birthdays and blood types ride on bio entries through the game's own bio
// overwrite (SCR_IdentityBioCharacterExtendedOverwrite): a picked entry sets the name,
// month/day of birth (the year still follows the random age) and/or blood type on every
// machine. Our blood types are codes from BLOOD_BASE up, shown through
// GetBloodTypeUIInfo; the vanilla blood type list and its random pick are untouched.
//
// Odds: bios carry their own weight (common 100 ... legendary 0.05). The other entries
// belong to a tier (m_aTiers in the conf: name_subtle, name_direct, name_blatant,
// birthday, blood_rare, blood_fiction, jackpot) whose total weight is shared by its
// entries (by their m_fWeight), so tiers are tuned in one place. The group's m_iWeight
// sets its share of all bios (2850 is about 1 in 5 for a US/USSR soldier). Names are
// AI only and per faction (US or USSR); ultra rare/legendary bios, names, fictional blood
// types and jackpots are unique per session; recently used entries are held back.
// Host options: -bbbBios 0 switches it all off, -bbbIdentityEggs 0 keeps the bios but
// drops names, birthdays and blood types (a number above 1 multiplies their odds, for
// testing), -bbbIdentityTest 1 spawns test civilians/soldiers and logs their identities.
// Entries that set a name/birthday/blood type log what the soldier ended up
// with: "[BBB] BIO <key> -> <name>, born <d/m>, blood <type>".

[BaseContainerProps(), BaseContainerCustomDoubleCheckTitleField("m_bIsUnique", "m_sKey", "1", "UNIQUE - %1", "%1")]
class BBB_Bio : SCR_IdentityBioCharacterExtendedOverwrite
{
 [Attribute(desc: "Short id for log lines")]
 protected string m_sKey;
 [Attribute("100", desc: "Bios: chance weight (common 100, uncommon 25, rare 5, easter egg 1, ultra rare 0.2, legendary 0.05). Tier entries: weight within the tier.")]
 protected float m_fWeight;
 [Attribute(desc: "Tier (see the group's m_aTiers). Empty = a plain bio using m_fWeight directly.")]
 protected string m_sTier;
 [Attribute(desc: "Faction key the soldier must have (empty = any)")]
 protected string m_sFaction;
 [Attribute("0", desc: "Only for AI soldiers (entries that set a name)")]
 protected bool m_bAIOnly;
 [Attribute("0", desc: "Month of birth to set (0 = keep random)")]
 protected int m_iMonth;
 [Attribute("0", desc: "Day of birth to set (with m_iMonth)")]
 protected int m_iDay;
 [Attribute(desc: "Blood type to show (O+ ... AB- or any text; empty = keep random)")]
 protected string m_sBlood;

 protected int m_iBloodCode = -1;
 protected float m_fPick;
 protected ref SCR_ExtendedCharacterIdentity m_Overwrite;
 protected bool m_bBuilt;

 string GetKey() { return m_sKey; }
 float GetWeight() { return m_fWeight; }
 string GetTier() { return m_sTier; }
 string GetBloodLabel() { return m_sBlood; }
 void SetBloodCode(int code) { m_iBloodCode = code; }
 float GetPick() { return m_fPick; }
 void SetPick(float pick) { m_fPick = pick; }

 bool Fits(string factionKey, bool isPlayer)
 {
  if (m_bAIOnly && isPlayer) return false;
  if (!m_sFaction.IsEmpty() && m_sFaction != factionKey) return false;
  return true;
 }

 // Built on first use on each machine; attribute defaults don't apply to script-built
 // objects, so every field the vanilla overwrite reads is set explicitly.
 override SCR_ExtendedIdentity GetOverwriteExtendedIdentity()
 {
  if (m_bBuilt) return m_Overwrite;
  m_bBuilt = true;
  if (m_iMonth <= 0 && m_iBloodCode < 0) return null;
  m_Overwrite = new SCR_ExtendedCharacterIdentity();
  m_Overwrite.SetAge(-1);
  if (m_iMonth > 0 && m_iDay > 0)
  {
   m_Overwrite.SetMonthOfCreation(m_iMonth);
   m_Overwrite.SetDayOfCreation(m_iDay);
  }
  else
  {
   m_Overwrite.SetMonthOfCreation(-1);
   m_Overwrite.SetDayOfCreation(-1);
  }
  m_Overwrite.SetBloodType(m_iBloodCode);
  return m_Overwrite;
 }
}

[BaseContainerProps()]
class BBB_BioTier
{
 [Attribute(desc: "Tier name used by entries' m_sTier")]
 string m_sTier;
 [Attribute("1", desc: "Total weight of all entries in this tier (same units as the bio weights; all bios together are about 3000)")]
 float m_fWeight;
}

[BaseContainerProps(configRoot: true), BaseContainerCustomDoubleTitleField("m_sBioGroupID", "m_iWeight")]
class BBB_BioGroup : SCR_CharacterIdentityBioGroupConfig
{
 [Attribute(desc: "Tier weights for names, birthdays, blood types and jackpots")]
 protected ref array<ref BBB_BioTier> m_aTiers;

 // Entries assigned lately (non-unique); skipped until this many others were given out.
 protected static const int RECENT = 12;
 protected ref array<int> m_aRecent = {};
 protected ref array<float> m_aWeights = {};

 void SetEnabled(bool on) { m_bEnabled = on; }

 int BioCount()
 {
  if (!m_aIdentityList) return 0;
  return m_aIdentityList.Count();
 }

 // Effective pick weight per entry (tier weight shared by its entries).
 void PrepareWeights(float eggs)
 {
  map<string, float> tierTotal = new map<string, float>();
  map<string, float> tierSum = new map<string, float>();
  if (m_aTiers)
  {
   foreach (BBB_BioTier tier : m_aTiers)
   {
    if (tier) tierTotal.Set(tier.m_sTier, tier.m_fWeight);
   }
  }
  foreach (SCR_IdentityBio entry : m_aIdentityList)
  {
   BBB_Bio bio = BBB_Bio.Cast(entry);
   if (!bio || bio.GetTier().IsEmpty()) continue;
   float sum;
   tierSum.Find(bio.GetTier(), sum);
   tierSum.Set(bio.GetTier(), sum + bio.GetWeight());
  }
  foreach (SCR_IdentityBio item : m_aIdentityList)
  {
   BBB_Bio ours = BBB_Bio.Cast(item);
   if (!ours) continue;
   string name = ours.GetTier();
   if (name.IsEmpty())
   {
    ours.SetPick(ours.GetWeight());
    continue;
   }
   float total, part;
   tierTotal.Find(name, total);
   tierSum.Find(name, part);
   if (eggs <= 0 || part <= 0)
    ours.SetPick(0);
   else
    ours.SetPick(eggs * total * ours.GetWeight() / part);
  }
 }

 static string FactionKeyOf(IEntity entity)
 {
  if (!entity) return string.Empty;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
  if (!affiliation) return string.Empty;
  Faction faction = affiliation.GetAffiliatedFaction();
  if (!faction) faction = affiliation.GetDefaultAffiliatedFaction();
  if (!faction) return string.Empty;
  return faction.GetFactionKey();
 }

 protected static bool IsPlayer(IEntity entity)
 {
  if (!entity) return false;
  SCR_ExtendedCharacterIdentityComponent identity = SCR_ExtendedCharacterIdentityComponent.Cast(entity.FindComponent(SCR_ExtendedCharacterIdentityComponent));
  if (identity && identity.GetPlayerID() > 0) return true;
  return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0;
 }

 // Weighted pick (vanilla picks evenly inside a group). Draws exactly one number from
 // the identity's seeded randomizer, like vanilla, so the age/birthday the clients
 // re-roll from the same seed match the server; the pick itself uses a 30-bit roll so
 // the 1-in-100,000 entries keep their odds.
 override void AssignRandomAvailableBio(RandomGenerator randomizer, IEntity entity, out int index, out SCR_IdentityBio bio)
 {
  if (m_aAvailableIdentityBioIndexList.IsEmpty()) return;
  randomizer.RandFloat01();
  string factionKey = FactionKeyOf(entity);
  bool player = IsPlayer(entity);
  m_aWeights.Clear();
  float total = 0;
  foreach (int i : m_aAvailableIdentityBioIndexList)
  {
   float w = 0;
   BBB_Bio candidate = BBB_Bio.Cast(m_aIdentityList[i]);
   if (!m_aRecent.Contains(i))
   {
    if (!candidate) w = 1;
    else if (candidate.Fits(factionKey, player)) w = candidate.GetPick();
   }
   m_aWeights.Insert(w);
   total += w;
  }
  int count = m_aAvailableIdentityBioIndexList.Count();
  index = m_aAvailableIdentityBioIndexList[Math.RandomInt(0, count)];
  if (total > 0)
  {
   float roll = (Math.RandomInt(0, 32768) + Math.RandomFloat01()) / 32768.0 * total;
   for (int j = 0; j < count; j++)
   {
    float weight = m_aWeights[j];
    if (weight <= 0) continue;
    index = m_aAvailableIdentityBioIndexList[j];
    roll -= weight;
    if (roll <= 0) break;
   }
  }
  bio = OnCharacterBioAssigned(entity, index);
  BBB_Bio ours = BBB_Bio.Cast(bio);
  if (!ours) return;
  if (ours.GetTier().IsEmpty())
   Print("[BBB] BIO " + ours.GetKey() + " (" + factionKey + ")", LogLevel.NORMAL);
  else
   GetGame().GetCallqueue().CallLater(LogIdentity, 1000, false, entity, ours.GetKey() + " (" + factionKey + ")");
 }

 // What the soldier ended up with (names are set a frame after the bio).
 static void LogIdentity(IEntity entity, string key)
 {
  if (!entity) return;
  string name, born, blood;
  CharacterIdentityComponent names = CharacterIdentityComponent.Cast(entity.FindComponent(CharacterIdentityComponent));
  if (names && names.GetIdentity()) name = names.GetIdentity().GetName() + " " + names.GetIdentity().GetSurname();
  SCR_ExtendedCharacterIdentityComponent extended = SCR_ExtendedCharacterIdentityComponent.Cast(entity.FindComponent(SCR_ExtendedCharacterIdentityComponent));
  if (extended)
  {
   SCR_ExtendedCharacterIdentity identity = SCR_ExtendedCharacterIdentity.Cast(extended.GetExtendedIdentity());
   SCR_IdentityManagerComponent manager = SCR_IdentityManagerComponent.GetInstance();
   if (identity)
   {
    born = identity.GetDayOfCreation().ToString() + "/" + identity.GetMonthOfCreation().ToString();
    if (manager)
    {
     SCR_UIInfo info = manager.GetBloodTypeUIInfo(identity.GetBloodType());
     if (info) blood = info.GetName();
    }
   }
  }
  Print(string.Format("[BBB] BIO %1 -> %2, born %3, blood %4", key, name, born, blood), LogLevel.NORMAL);
 }

 // Unique entries leave the pool for good; the others stay (vanilla would hand out every
 // other entry of the group first, which would make the rare ones common late in a session).
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
 static const ResourceName CIV_CONFIG = "{BBB0B10500000002}Configs/BBB/BBB_CivBios.conf";
 // Our blood type codes start here (vanilla's are 0-7).
 static const int BLOOD_BASE = 7700;
 protected static ref array<ref SCR_UIInfo> s_aBloodInfos = {};
 protected static ref array<string> s_aBloodLabels = {};
 protected static const string VANILLA_BLOOD = "O+|O-|A+|A-|B+|B-|AB+|AB-";

 static SCR_UIInfo BloodInfo(int code)
 {
  int i = code - BLOOD_BASE;
  if (i < 0 || i >= s_aBloodInfos.Count()) return null;
  return s_aBloodInfos[i];
 }

 // Same order on every machine (conf order), so the codes match everywhere.
 protected static void AssignBloodCodes(BBB_BioGroup group, notnull array<ref SCR_IdentityBio> bios)
 {
  array<string> stock = {};
  VANILLA_BLOOD.Split("|", stock, false);
  foreach (SCR_IdentityBio entry : bios)
  {
   BBB_Bio bio = BBB_Bio.Cast(entry);
   if (!bio || bio.GetBloodLabel().IsEmpty()) continue;
   string label = bio.GetBloodLabel();
   int code = stock.Find(label);
   if (code < 0)
   {
    code = s_aBloodLabels.Find(label);
    if (code < 0)
    {
     s_aBloodLabels.Insert(label);
     s_aBloodInfos.Insert(SCR_UIInfo.CreateInfo(label));
     code = s_aBloodLabels.Count() - 1;
    }
    code += BLOOD_BASE;
   }
   bio.SetBloodCode(code);
  }
 }

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
  s_aBloodInfos.Clear();
  s_aBloodLabels.Clear();
  bool on = true;
  float eggs = 1;
  string value;
  if (System.GetCLIParam("bbbBios", value) && value == "0") on = false;
  if (System.GetCLIParam("bbbIdentityEggs", value)) eggs = value.ToFloat();
  AddGroup(holder, CONFIG, on, eggs);
  AddGroup(holder, CIV_CONFIG, on, eggs);
  if (System.IsCLIParam("bbbIdentityTest") && Replication.IsServer()) GetGame().GetCallqueue().CallLater(SelfTest, 8000);
 }

 protected static void AddGroup(SCR_IdentityBioTypeHolderConfig holder, ResourceName config, bool on, float eggs)
 {
  BBB_BioGroup group = BBB_BioGroup.Cast(SCR_BaseContainerTools.CreateInstanceFromPrefab(config, true));
  if (!group)
  {
   Print("[BBB] BIOS failed to load " + config, LogLevel.ERROR);
   return;
  }
  holder.m_aIdentityBioGroups.Insert(group);
  array<ref SCR_IdentityBio> bios = {};
  for (int i = 0; i < group.BioCount(); i++)
  {
   bios.Insert(group.GetIdentityBio(null, i));
  }
  AssignBloodCodes(group, bios);
  group.SetEnabled(on);
  group.PrepareWeights(eggs);
  Print(string.Format("[BBB] BIOS %1: %2 entries added as bio group %3 (weight %4, on %5, names/birthdays/blood %6, %7 extra blood types so far)", group.GetBioGroupID(), group.BioCount(), holder.m_aIdentityBioGroups.Count() - 1, group.GetWeight(), on, eggs, s_aBloodInfos.Count()), LogLevel.NORMAL);
 }

 // -bbbIdentityTest 1 (server): spawns civilians and US riflemen out of the way, logs what
 // identity each got, then deletes them. Run it in a world with a CIV faction (Game Master).
 protected static ref array<IEntity> s_aTestEntities = {};
 protected static const ResourceName TEST_CIV = "{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et";
 protected static const ResourceName TEST_US = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";

 protected static void SelfTest()
 {
  vector origin = "100 0 100";
  BaseGameMode mode = GetGame().GetGameMode();
  if (mode) origin = mode.GetOrigin();
  for (int i = 0; i < 40; i++)
  {
   int odd = i % 2;
   ResourceName prefab = TEST_CIV;
   if (odd == 1) prefab = TEST_US;
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   int column = i % 8;
   int row = i / 8;
   vector at = origin;
   at[0] = at[0] + column * 3;
   at[2] = at[2] + row * 3;
   params.Transform[3] = BBB.Ground(at);
   IEntity spawned = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
   if (spawned) s_aTestEntities.Insert(spawned);
  }
  GetGame().GetCallqueue().CallLater(SelfTestReport, 4000);
 }

 protected static void SelfTestReport()
 {
  SCR_IdentityManagerComponent manager = SCR_IdentityManagerComponent.GetInstance();
  foreach (IEntity entity : s_aTestEntities)
  {
   if (!entity) continue;
   string group = "none";
   SCR_ExtendedCharacterIdentityComponent extended = SCR_ExtendedCharacterIdentityComponent.Cast(entity.FindComponent(SCR_ExtendedCharacterIdentityComponent));
   if (extended)
   {
    BBB_Bio ours = BBB_Bio.Cast(extended.GetIdentityBio());
    if (ours) group = ours.GetKey();
    else if (extended.GetIdentityBio()) group = "vanilla";
   }
   BBB_BioGroup.LogIdentity(entity, "TEST " + BBB_BioGroup.FactionKeyOf(entity) + " " + group);
   SCR_EntityHelper.DeleteEntityAndChildren(entity);
  }
  s_aTestEntities.Clear();
  Print("[BBB] BIOS self-test done", LogLevel.NORMAL);
 }
}

modded class SCR_IdentityManagerComponent
{
 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  if (m_sInstance == this) BBB_Bios.Install(m_aIdentityBioTypeHolders);
 }

 override SCR_UIInfo GetBloodTypeUIInfo(SCR_EBloodType bloodType)
 {
  SCR_UIInfo ours = BBB_Bios.BloodInfo(bloodType);
  if (ours) return ours;
  return super.GetBloodTypeUIInfo(bloodType);
 }
}
