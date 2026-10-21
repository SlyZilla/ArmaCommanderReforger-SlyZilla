// -acrDrivelessGunTest 1 (server-capable): a stopped armed vehicle with no driver keeps its
// gunner (Jacob 20 Oct, Everon: a man on a parked HMMWV M2 was pulled out every second by
// "driver lost under fire", BTR and M1025 crews left working guns). One squad member walks to
// the M2 of an empty M1025 and climbs in, another squad boards its cargo seats; then the
// vehicle is hit every second. After 10 s of that the gunner must still be in the turret and
// the riders out of the vehicle.
class ACR_TestDrivelessGun
{
 static const ResourceName US_TEAM = "{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et";
 static const ResourceName M1025_M2 = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
 static const int EXPECTED = 4;
 static IEntity s_Vehicle;
 static SCR_AIGroup s_GunGroup;
 static SCR_AIGroup s_RideGroup;
 static ChimeraCharacter s_Gunner;
 static BaseCompartmentSlot s_Turret;
 static bool s_bSeated;

 static vector Origin()
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Farm") return base.GetOrigin() + Vector(60, 0, 60);
  return "1460 0 2900".ToVector();
 }

 static void Run(ACR_TestHarness harness, int tick)
 {
  if (tick == 1) Setup();
  if (tick == 4) Board();
  if (tick >= 6 && tick < 30 && !s_bSeated) Seated(tick);
  if (s_bSeated && tick <= 60 && s_Vehicle) BBB_Transport.OnVehicleHit(s_Vehicle);
  if (tick == 30 && !s_bSeated) { ACR_TestRegress.Check("gunner seated in the M2 turret of the empty M1025, riders aboard", false, "never both"); Finish(); }
  if (s_bSeated && tick == s_iCheckAt) Verify();
 }

 static int s_iCheckAt = -1;

 static void Setup()
 {
  vector at = ACR_Manager.Ground(Origin());
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = at;
  Resource vehicle = Resource.Load(M1025_M2);
  if (vehicle.IsValid()) s_Vehicle = GetGame().SpawnEntityPrefab(vehicle, GetGame().GetWorld(), params);
  s_GunGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(US_TEAM, ACR_Manager.Ground(at + Vector(6, 0, 0))));
  s_RideGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(US_TEAM, ACR_Manager.Ground(at + Vector(-6, 0, 0))));
  if (!s_Vehicle || !s_GunGroup || !s_RideGroup) { ACR_TestRegress.Abort("drivelessgun", "fixture did not spawn"); return; }
  array<BaseCompartmentSlot> turrets = {};
  BBB_VehicleHelpers.GetVehicleCompartments(s_Vehicle, turrets, ECompartmentType.TURRET);
  if (!turrets.IsEmpty()) s_Turret = turrets[0];
  PrintFormat("[ACR] DRIVELESSGUN info: M1025 at %1, turret %2 with ammo %3", at, s_Turret != null, BBB_VehicleHelpers.TurretHasAmmo(s_Turret));
 }

 static void Board()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Squad gun = null;
  ACR_Squad ride = null;
  if (manager) { gun = manager.FindSquadByGroup(s_GunGroup); ride = manager.FindSquadByGroup(s_RideGroup); }
  if (!gun || !ride || !s_Turret) { ACR_TestRegress.Abort("drivelessgun", string.Format("squads registered %1/%2, turret %3", gun != null, ride != null, s_Turret != null)); return; }
  array<AIAgent> agents = {};
  s_GunGroup.GetAgents(agents);
  if (agents.IsEmpty()) return;
  s_Gunner = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
  // He walks to the gun and climbs in with the game's own get-in (no placing).
  if (s_Gunner) s_Gunner.GetCompartmentAccessComponent().GetInVehicle(s_Vehicle, s_Turret, false, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
  // Riders are only setup here (placed in the cargo seats); the check is that they get out.
  int riders = ride.BoardCargo(s_Vehicle, 30, true);
  PrintFormat("[ACR] DRIVELESSGUN info: gunner walking to the turret, %1 riders boarding", riders);
 }

 static void Seated(int tick)
 {
  if (!s_Gunner || !s_Turret || s_Turret.GetOccupant() != s_Gunner) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Squad ride = manager.FindSquadByGroup(s_RideGroup);
  // Fire starts only once the gunner sits and the riders are aboard.
  if (!ride || !ride.IsMounted()) return;
  s_bSeated = true;
  s_iCheckAt = tick + 10;
  ACR_TestRegress.Check("gunner seated in the M2 turret of the empty M1025, riders aboard", true, string.Format("tick %1, driver %2", tick, ACR_Squad.DriverOf(s_Vehicle) != null));
 }

 static void Verify()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Squad gun = manager.FindSquadByGroup(s_GunGroup);
  ACR_Squad ride = manager.FindSquadByGroup(s_RideGroup);
  bool stayed = s_Turret.GetOccupant() == s_Gunner && BBB.IsAlive(s_Gunner);
  bool hit = BBB_Transport.VehicleHitRecently(s_Vehicle, 3000);
  ACR_TestRegress.Check("vehicle under fire with no driver while the gunner sits", hit && ACR_Squad.DriverOf(s_Vehicle) == null, string.Format("hit %1, driver %2", hit, ACR_Squad.DriverOf(s_Vehicle) != null));
  ACR_TestRegress.Check("gunner with ammo still on the M2 after 10 s under fire", stayed && gun && gun.HasArmedGunner(s_Vehicle), string.Format("occupant is gunner %1, armed gunner %2", s_Turret.GetOccupant() == s_Gunner, gun && gun.HasArmedGunner(s_Vehicle)));
  int aboard = 0;
  array<AIAgent> agents = {};
  s_RideGroup.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (man && ACR_Transport.VehicleOf(man) == s_Vehicle) aboard++;
  }
  ACR_TestRegress.Check("riding squad got out of the stopped driverless vehicle", ride && aboard == 0, string.Format("%1 still aboard", aboard));
  Finish();
 }

 static void Finish()
 {
  ACR_TestRegress.Complete("drivelessgun", EXPECTED);
 }
}
