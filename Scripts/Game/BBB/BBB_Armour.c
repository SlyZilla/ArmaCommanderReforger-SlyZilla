// Small arms vs armour (5 Oct 2026; Suggestion 6 in the BBB Combat AI Levers doc).
// Vanilla picks weapon and target in engine code from each magazine's "used against" list:
// rifle ammo is used against unarmoured vehicles, and an armoured Humvee counts as one
// (only wheeled APCs are "medium"), 7.62 machine guns are used against APCs too. So AI
// riflemen pour magazines into armour their rounds can't get through.
// BBB learns it the way soldiers do: every kinetic hit on a vehicle is checked against the
// struck part's armour (the game's own effective-damage sum: multipliers, flat reduction,
// threshold); a hit that wounds someone inside counts as getting through. Once a kind of
// round has hit a kind of vehicle MIN_HITS times and fewer than 1 in 10 did anything, AI
// soldiers know that round is useless against it (like training: shared by everyone).
// When such a pair is a soldier's pick he holds that fire and his target picker turns to
// targets he can hurt (see the modded combat component below). Works with any mod's
// vehicles and ammo; no tables.
// Host options: -bbbArmour 0 (off), -bbbArmourHits <n> (hits before a verdict, default 12).
class BBB_ArmourStat : Managed
{
 int m_iHits;
 int m_iThrough;
 bool m_bUseless;
}

class BBB_Armour
{
 static bool s_bRead;
 static bool s_bOn = true;
 static bool s_bLearn = true; // learning runs even while a test switches the behaviour off
 static int s_iMinHits = 12;
 static const float THROUGH_SHARE = 0.1;
 static const float HIT_EFFECTIVE = 1.0; // effective damage that counts as getting through
 static ref map<string, ref BBB_ArmourStat> s_Stats = new map<string, ref BBB_ArmourStat>();
 static int s_iSteered;     // tests read these
 static int s_iHeld;
 static ref array<IEntity> s_aVanilla = {}; // test only: soldiers who keep vanilla picks
 static bool s_bDiag;                       // test only: log what the shooter was doing
 static IEntity s_Watch;                    // test only: every hit on this vehicle, by type and source
 static ref map<string, int> s_mWatchHits = new map<string, int>();
 static ref map<string, float> s_mWatchDamage = new map<string, float>();
 static ref map<string, int> s_mDiag = new map<string, int>();

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbArmour", value) && value == "0") { s_bOn = false; s_bLearn = false; }
  if (System.GetCLIParam("bbbArmourHits", value)) s_iMinHits = Math.Max(1, value.ToInt());
  PrintFormat("[BBB] ARMOUR on=%1 hits=%2", s_bOn, s_iMinHits);
 }

 static bool On(IEntity soldier)
 {
  Read();
  return s_bOn && !(soldier && s_aVanilla.Contains(soldier));
 }

 // "path/Name.et" without the GUID, lower case.
 static string Key(ResourceName resource)
 {
  string s = resource;
  int close = s.IndexOf("}");
  if (close >= 0) s = s.Substring(close + 1, s.Length() - close - 1);
  s.ToLower();
  return s;
 }

 // The kind of round: its calibre ("545x39") when the name has one, so ball and tracer of
 // one calibre count together (a magazine names only its first round); else the prefab.
 static string Round(string key)
 {
  if (key == "") return key;
  // A HEAT/HEDP round's spall and penetrator are its own child projectiles: they belong to
  // the round the soldier fired (their hits are that round getting through).
  key.Replace("_spall.et", ".et");
  key.Replace("_penetrator.et", ".et");
  string name = Short(key);
  array<string> parts = {};
  name.Split("_", parts, true);
  foreach (string part : parts)
  {
   int x = part.IndexOf("x");
   if (x <= 0 || x >= part.Length() - 1) continue;
   string left = part.Substring(0, x);
   string right = part.Substring(x + 1, part.Length() - x - 1);
   if (left.ToInt().ToString() == left && right.ToInt().ToString() == right) return "cal " + part;
  }
  return key;
 }

 static string PrefabKey(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return "";
  return Key(entity.GetPrefabData().GetPrefabName());
 }

 static string Short(string key)
 {
  int slash = key.LastIndexOf("/");
  if (slash >= 0) key = key.Substring(slash + 1, key.Length() - slash - 1);
  key.Replace(".et", "");
  return key;
 }

 // The vehicle an entity is (or sits in), else null.
 static IEntity VehicleOf(IEntity entity)
 {
  if (!entity) return null;
  if (Vehicle.Cast(entity)) return entity;
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (character && character.IsInVehicle() && character.GetCompartmentAccessComponent())
  {
   BaseCompartmentSlot seat = character.GetCompartmentAccessComponent().GetCompartment();
   if (seat) return seat.GetVehicle();
  }
  IEntity root = entity.GetRootParent();
  if (Vehicle.Cast(root)) return root;
  return null;
 }

 protected static BBB_ArmourStat Stat(string vehicle, string ammo)
 {
  string key = vehicle + "|" + ammo;
  BBB_ArmourStat stat = s_Stats.Get(key);
  if (!stat)
  {
   stat = new BBB_ArmourStat();
   s_Stats.Set(key, stat);
  }
  return stat;
 }

 // Hull hits cannot establish whether exposed aircraft crew can be hurt. Check
 // simulation as well as perception type: mod aircraft may use a ground unit type.
 static bool Aircraft(IEntity vehicle)
 {
  if (!vehicle) return false;
  if (vehicle.FindComponent(VehicleHelicopterSimulation) || vehicle.FindComponent(VehicleFixedWingSimulation)) return true;
  PerceivableComponent perceivable = PerceivableComponent.Cast(vehicle.FindComponent(PerceivableComponent));
  return perceivable && perceivable.GetUnitType() == EAIUnitType.UnitType_Aircraft;
 }

 // Test only: tally every hit on the watched vehicle ("type source" -> hits, effective damage).
 static void Watch(IEntity vehicle, notnull BaseDamageContext context)
 {
  if (!s_Watch || vehicle != s_Watch) return;
  string key = typename.EnumToString(EDamageType, context.damageType) + " " + Short(PrefabKey(context.damageSource));
  float effective = 0;
  if (context.struckHitZone) effective = context.struckHitZone.ComputeEffectiveDamage(context, false);
  s_mWatchHits.Set(key, s_mWatchHits.Get(key) + 1);
  s_mWatchDamage.Set(key, s_mWatchDamage.Get(key) + effective);
 }

 // A kinetic hit on a vehicle part (vehicle damage managers).
 static void OnVehicleHit(IEntity vehicle, notnull BaseDamageContext context)
 {
  if (!s_bLearn || context.damageType != EDamageType.KINETIC || !context.damageSource || Aircraft(vehicle)) return;
  int t = System.GetTickCount();
  string ammo = Round(PrefabKey(context.damageSource));
  string kind = PrefabKey(vehicle);
  if (ammo != "" && kind != "")
  {
   float effective = 0;
   if (context.struckHitZone) effective = context.struckHitZone.ComputeEffectiveDamage(context, false);
   BBB_ArmourStat stat = Stat(kind, ammo);
   stat.m_iHits++;
   if (effective >= HIT_EFFECTIVE) stat.m_iThrough++;
   Judge(stat, kind, ammo);
   if (s_bDiag && stat.m_bUseless) Diag(context);
  }
  BBB_Perf.Add("ev.armour", System.GetTickCount() - t);
 }

 // A kinetic hit on someone sitting in a vehicle, from outside it: it got through.
 static void OnOccupantHit(IEntity occupant, notnull BaseDamageContext context)
 {
  if (!s_bLearn || context.damageType != EDamageType.KINETIC || !context.damageSource) return;
  IEntity vehicle = VehicleOf(occupant);
  if (!vehicle || Aircraft(vehicle)) return;
  string ammo = Round(PrefabKey(context.damageSource));
  string kind = PrefabKey(vehicle);
  if (ammo == "" || kind == "") return;
  BBB_ArmourStat stat = Stat(kind, ammo);
  stat.m_iThrough++;
  Judge(stat, kind, ammo);
 }

 protected static void Judge(BBB_ArmourStat stat, string kind, string ammo)
 {
  float through = stat.m_iThrough;
  float share = THROUGH_SHARE * stat.m_iHits; // float first: int * float is done in ints
  bool useless = stat.m_iHits >= s_iMinHits && through < share;
  if (useless == stat.m_bUseless) return;
  stat.m_bUseless = useless;
  if (useless) PrintFormat("[BBB] ARMOUR learned: %1 shrugs off %2 (%3 of %4 hits got through)", Short(kind), Short(ammo), stat.m_iThrough, stat.m_iHits);
  else PrintFormat("[BBB] ARMOUR relearned: %1 can be hurt by %2 (%3 of %4)", Short(kind), Short(ammo), stat.m_iThrough, stat.m_iHits);
 }

 // Is this magazine's round known to be useless against the vehicle this target is or sits in?
 static bool Useless(BaseTarget target, BaseMagazineComponent magazine)
 {
  if (!target || !magazine || s_Stats.IsEmpty()) return false;
  IEntity vehicle = VehicleOf(target.GetTargetEntity());
  if (!vehicle || Aircraft(vehicle)) return false;
  string kind = PrefabKey(vehicle);
  string ammo = Round(Key(magazine.GetAmmoType(0)));
  BBB_ArmourStat stat = s_Stats.Get(kind + "|" + ammo);
  return stat && stat.m_bUseless;
 }

 static bool UselessEntity(IEntity targetEntity, BaseMagazineComponent magazine)
 {
  if (!targetEntity || !magazine || s_Stats.IsEmpty()) return false;
  IEntity vehicle = VehicleOf(targetEntity);
  if (!vehicle || Aircraft(vehicle)) return false;
  BBB_ArmourStat stat = s_Stats.Get(PrefabKey(vehicle) + "|" + Round(Key(magazine.GetAmmoType(0))));
  return stat && stat.m_bUseless;
 }

 // Vanilla's target score constants (SCR_AICombatComponent.InitWeaponTargetSelector, 1.6).
 static void VanillaScore(EAIUnitType type, out float offset, out float slope)
 {
  offset = 100; slope = -0.1;
  if (type == EAIUnitType.UnitType_VehicleUnarmored) { offset = 99; slope = -0.08; }
  else if (type == EAIUnitType.UnitType_VehicleMedium) { offset = 150; slope = -0.15; }
  else if (type == EAIUnitType.UnitType_VehicleHeavy) { offset = 200; slope = -0.15; }
  else if (type == EAIUnitType.UnitType_Aircraft) { offset = 90; slope = -0.015; }
  else if (type == EAIUnitType.UnitType_Fortification) { offset = 40; slope = -0.1; }
 }

 // Test only: which behaviour the shooter of a wasted round was running.
 protected static void Diag(BaseDamageContext context)
 {
  string what = "no shooter";
  if (context.instigator && context.instigator.GetInstigatorEntity())
  {
   IEntity shooter = context.instigator.GetInstigatorEntity();
   AIControlComponent control = AIControlComponent.Cast(shooter.FindComponent(AIControlComponent));
   what = "no AI";
   if (control && control.GetAIAgent())
   {
    SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
    what = "no behaviour";
    if (utility && utility.GetCurrentBehavior()) what = utility.GetCurrentBehavior().Type().ToString();
   }
  }
  if (s_bOn) what = "on: " + what;
  else what = "off: " + what;
  s_mDiag.Set(what, s_mDiag.Get(what) + 1);
 }

 // Hits recorded on one kind of vehicle (all rounds), for tests.
 static int HitsOn(string kind)
 {
  int hits;
  foreach (string key, BBB_ArmourStat stat : s_Stats)
   if (key.StartsWith(kind + "|")) hits += stat.m_iHits;
  return hits;
 }

 // Summary for test logs.
 static void Dump()
 {
  foreach (string key, BBB_ArmourStat stat : s_Stats)
   PrintFormat("[BBB] ARMOUR stat %1: %2 hits, %3 through, useless %4", key, stat.m_iHits, stat.m_iThrough, stat.m_bUseless);
  foreach (string what, int n : s_mDiag)
   PrintFormat("[BBB] ARMOUR wasted rounds after learning, shooter busy with %1: %2", what, n);
 }
}

modded class SCR_DamageManagerComponent
{
 override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
 {
  bool hijacked = super.HijackDamageHandling(damageContext);
  if (!hijacked && Vehicle.Cast(GetOwner()))
  {
   if (BBB_Armour.s_Watch) BBB_Armour.Watch(GetOwner(), damageContext);
   BBB_Armour.OnVehicleHit(GetOwner(), damageContext);
   BBB_Transport.OnVehicleHit(GetOwner());
  }
  return hijacked;
 }
}

modded class SCR_CharacterDamageManagerComponent
{
 override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
 {
  bool hijacked = super.HijackDamageHandling(damageContext);
  if (!hijacked && damageContext.damageType == EDamageType.KINETIC)
  {
   ChimeraCharacter me = ChimeraCharacter.Cast(GetOwner());
   if (me && me.IsInVehicle())
   {
    BBB_Armour.OnOccupantHit(me, damageContext);
    BBB_Transport.OnVehicleHit(BBB_Transport.VehicleOf(me));
   }
  }
  return hijacked;
 }
}

// The soldier's own target pick is engine code and its result private, so BBB works around
// it: (1) an attack on a target his chosen round can't hurt is held (priority 0: no fire);
// (2) for 30 s that kind of vehicle scores low in his own target picker, so he turns to
// targets he can hurt if there are any. Launcher and grenade-launcher men aren't touched:
// their pick against a vehicle isn't a useless round.
modded class SCR_AICombatComponent
{
 protected float m_fBBBLoweredUntil;
 protected EAIUnitType m_eBBBLowered;
 protected bool m_bBBBLowered;
 static const float BBB_LOWER_S = 30;
 static const float BBB_LOW_OFFSET = 10;

 BaseMagazineComponent BBBSelectedMagazine() { return m_SelectedMagazineComp; }
 BaseWeaponComponent BBBSelectedWeapon() { return m_SelectedWeaponComp; }
 int BBBSelectedMuzzle() { return m_iSelectedMuzzle; }

 override void EvaluateWeaponAndTarget(out bool outWeaponEvent, out bool outSelectedTargetChanged,
  out BaseTarget outPrevTarget, out BaseTarget outCurrentTarget,
  out bool outRetreatTargetChanged, out bool outCompartmentChanged)
 {
  float stamp = m_fNextWeaponTargetEvaluation_ms;
  super.EvaluateWeaponAndTarget(outWeaponEvent, outSelectedTargetChanged, outPrevTarget, outCurrentTarget, outRetreatTargetChanged, outCompartmentChanged);
  if (stamp == m_fNextWeaponTargetEvaluation_ms) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_bBBBLowered && now > m_fBBBLoweredUntil) BBBRestoreScore();
  BaseTarget target = GetCurrentTarget();
  if (!target || !m_SelectedMagazineComp || m_bBBBLowered) return;
  if (!BBB_Armour.Useless(target, m_SelectedMagazineComp) || !BBB_Armour.On(GetOwner())) return;
  // Steer his picker away from this kind of vehicle for a while (not for a man inside it:
  // that would steer him off all infantry; the attack hold covers that case).
  if (target.GetUnitType() == EAIUnitType.UnitType_Infantry) return;
  m_eBBBLowered = target.GetUnitType();
  m_bBBBLowered = true;
  m_fBBBLoweredUntil = now + BBB_LOWER_S * 1000;
  float offset, slope;
  BBB_Armour.VanillaScore(m_eBBBLowered, offset, slope);
  m_WeaponTargetSelector.SetTargetScoreConstants(m_eBBBLowered, BBB_LOW_OFFSET, slope);
  BBB_Armour.s_iSteered++;
  BBB_Perf.Count("armour.steer");
  PrintFormat("[BBB] ARMOUR %1: his %2 can't hurt %3; looks for other targets", BBB_Armour.Short(BBB_Armour.PrefabKey(GetOwner())), BBB_Armour.Short(BBB_Armour.Key(m_SelectedMagazineComp.GetAmmoType(0))), BBB_Armour.Short(BBB_Armour.PrefabKey(BBB_Armour.VehicleOf(target.GetTargetEntity()))));
 }

 protected void BBBRestoreScore()
 {
  m_bBBBLowered = false;
  float offset, slope;
  BBB_Armour.VanillaScore(m_eBBBLowered, offset, slope);
  m_WeaponTargetSelector.SetTargetScoreConstants(m_eBBBLowered, offset, slope);
 }
}

modded class SCR_AIAttackBehavior
{
 protected float m_fBBBCheckAt;
 protected bool m_bBBBUseless;

 override float CustomEvaluate()
 {
  float priority = super.CustomEvaluate();
  if (priority <= 0 || !m_CombatComponent) return priority;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now >= m_fBBBCheckAt)
  {
   m_fBBBCheckAt = now + 1000;
   bool was = m_bBBBUseless;
   m_bBBBUseless = BBB_Armour.Useless(m_Target.m_Value, m_CombatComponent.BBBSelectedMagazine()) && BBB_Armour.On(m_Utility.m_OwnerEntity);
   if (m_bBBBUseless && !was) { BBB_Armour.s_iHeld++; BBB_Perf.Count("armour.hold"); }
  }
  if (m_bBBBUseless) return 0;
  return priority;
 }
}

// Suppressive fire on a cluster (the group's suppress order) is another way to empty
// magazines into armour: held too when every target in the cluster is a vehicle (or a man
// in one) that his round is known not to hurt.
modded class SCR_AISuppressGroupClusterBehavior
{
 protected float m_fBBBCheckAt;
 protected bool m_bBBBUseless;

 override float CustomEvaluate()
 {
  float priority = super.CustomEvaluate();
  if (priority <= 0 || !m_CombatComponent || !m_ClusterState || !m_ClusterState.m_Cluster) return priority;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now >= m_fBBBCheckAt)
  {
   m_fBBBCheckAt = now + 1000;
   bool was = m_bBBBUseless;
   m_bBBBUseless = false;
   BaseMagazineComponent magazine = m_CombatComponent.BBBSelectedMagazine();
   array<IEntity> entities = m_ClusterState.m_Cluster.m_aEntities;
   if (magazine && entities && !entities.IsEmpty() && BBB_Armour.On(m_Utility.m_OwnerEntity))
   {
    m_bBBBUseless = true;
    foreach (IEntity entity : entities)
     if (!BBB_Armour.UselessEntity(entity, magazine)) { m_bBBBUseless = false; break; }
   }
   if (m_bBBBUseless && !was) { BBB_Armour.s_iHeld++; BBB_Perf.Count("armour.holdsuppress"); }
  }
  if (m_bBBBUseless) return 0;
  return priority;
 }
}
