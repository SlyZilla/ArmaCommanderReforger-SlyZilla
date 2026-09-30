// Arma 3's "gunner drives": a player in a vehicle turret with an AI driver steers the
// vehicle with WASD. The client sends its keys to the server, which parks the driver's
// AI and feeds throttle/steering/brake straight into the vehicle simulation.
// A new map order for the squad, leaving the turret, or 20 s without input hands the
// vehicle back to the AI.
class ACR_GunnerDrive
{
 static const float IDLE_RELEASE_MS = 20000;
 static const float SHIFT_COOLDOWN_MS = 700;


 IEntity m_Vehicle;
 ChimeraCharacter m_Driver;
 ACR_Squad m_Squad;
 int m_PlayerId;
 float m_Throttle;
 float m_Steer;
 float m_LastInput;
 float m_LastShift;
 bool m_AIWasOn;
 string m_EndReason;

 // Vehicle the body may drive from its turret (null if not a turret seat, no AI driver, ...).
 static IEntity DrivableFrom(IEntity body, out ChimeraCharacter driver)
 {
  driver = null;
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character) return null;
  CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
  if (!access) return null;
  BaseCompartmentSlot slot = access.GetCompartment();
  if (!slot || slot.GetType() != ECompartmentType.TURRET) return null;
  IEntity vehicle = slot.GetVehicle();
  if (!vehicle) return null;
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetSimulation()) return null;
  PilotCompartmentSlot pilot = car.GetPilotCompartmentSlot();
  if (!pilot) return null;
  driver = ChimeraCharacter.Cast(pilot.GetOccupant());
  if (!driver || !driver.GetCharacterController() || driver.GetCharacterController().IsDead()) return null;
  if (ACR_Squad.FindController(driver)) return null; // a player is driving
  return vehicle;
 }

 void Begin()
 {
  m_Squad.Halt("Driven by gunner");
  AIControlComponent ai = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated())
  {
   ai.DeactivateAI();
   m_AIWasOn = true;
  }
  CarControllerComponent car = Car();
  if (car)
  {
   car.SetPersistentHandBrake(false);
   if (!car.IsEngineOn()) car.ForceStartEngine();
  }
 }

 CarControllerComponent Car()
 {
  if (!m_Vehicle) return null;
  return CarControllerComponent.Cast(m_Vehicle.FindComponent(CarControllerComponent));
 }

 // Forward speed in m/s (negative = rolling backwards).
 float ForwardSpeed()
 {
  Physics physics = m_Vehicle.GetPhysics();
  if (!physics) return 0;
  return vector.Dot(physics.GetVelocity(), m_Vehicle.GetTransformAxis(2));
 }

 // Returns false when the session should end.
 bool Apply(float now)
 {
  if (!m_Vehicle || !m_Driver || m_Driver.GetCharacterController().IsDead()) { m_EndReason = "vehicle or driver lost"; return false; }
  PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(m_PlayerId);
  ChimeraCharacter driver;
  if (!pc || DrivableFrom(pc.GetControlledEntity(), driver) != m_Vehicle || driver != m_Driver) { m_EndReason = "gunner left"; return false; }
  if (now - m_LastInput > IDLE_RELEASE_MS) { m_EndReason = "idle"; return false; }
  if (m_Squad.m_Group)
  {
   array<AIWaypoint> orders = {};
   m_Squad.m_Group.GetWaypoints(orders);
   if (!orders.IsEmpty()) { m_EndReason = "new order"; return false; }
  }
  CarControllerComponent car = Car();
  if (!car) { m_EndReason = "no car controller"; return false; }
  VehicleWheeledSimulation sim = car.GetSimulation();
  if (!sim) { m_EndReason = "no simulation"; return false; }
  if (m_Throttle != 0 && !car.IsEngineOn()) car.ForceStartEngine();
  // The parked AI driver (or the game) can leave the handbrake on: the rear wheels
  // lock, the front skids sideways and the vehicle crawls up hills. Keep it off
  // whenever the gunner wants to move.
  if (m_Throttle != 0 || m_Steer != 0) car.SetPersistentHandBrake(false);
  // A stopped vehicle's physics falls asleep and then ignores throttle.
  Physics physics = m_Vehicle.GetPhysics();
  if (physics && (m_Throttle != 0 || m_Steer != 0) && !physics.IsActive()) physics.SetActive(ActiveState.ACTIVE);

  int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
  int gear = sim.GetGear();
  float speed = ForwardSpeed();
  float throttle = 0;
  float brake = 0;
  if (m_Throttle > 0)
  {
   if (speed < -1) brake = 1;                   // still rolling back: stop first
   else
   {
    if (gear <= neutral) Shift(sim, neutral + 1, now, true);
    throttle = m_Throttle;
   }
  }
  else if (m_Throttle < 0)
  {
   if (speed > 1) brake = 1;                    // still rolling forward: stop first
   else
   {
    if (gear >= neutral) Shift(sim, neutral - 1, now, true);
    throttle = -m_Throttle;
   }
  }
  else brake = 1;                               // keys released: stop and hold

  // Simple automatic gearbox for the forward gears.
  gear = sim.GetGear();
  if (throttle > 0 && gear > neutral)
  {
   float rpm = sim.EngineGetRPM();
   if (rpm > sim.EngineGetRPMPeakPower() * 0.95 && gear < sim.GearboxGearsCount() - 1) Shift(sim, gear + 1, now, false);
   else if (rpm < sim.EngineGetRPMPeakTorque() * 0.6 && gear > neutral + 1) Shift(sim, gear - 1, now, false);
  }

  sim.SetClutch(1);
  sim.SetThrottle(throttle);
  sim.SetBreak(brake, false);
  if (brake == 0 && sim.IsHandbrakeOn()) car.SetPersistentHandBrake(false);
  // Less steering lock at speed, so a tap of A/D doesn't spin the vehicle out.
  float steerScale = Math.Clamp(1 - Math.AbsFloat(speed) / 25, 0.35, 1);
  sim.SetSteering(m_Steer * steerScale);
  return true;
 }

 void Shift(VehicleWheeledSimulation sim, int gear, float now, bool force)
 {
  if (!force && now - m_LastShift < SHIFT_COOLDOWN_MS) return;
  if (System.IsCLIParam("acrGunnerTest")) PrintFormat("[ACR] GUNNER shift %1 -> %2 rpm=%3 peakPower=%4 peakTorque=%5 max=%6", sim.GetGear(), gear, (int)sim.EngineGetRPM(), (int)sim.EngineGetRPMPeakPower(), (int)sim.EngineGetRPMPeakTorque(), (int)sim.EngineGetRPMMax());
  sim.SetGear(gear);
  m_LastShift = now;
 }

 void End()
 {
  CarControllerComponent car = Car();
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation sim = car.GetSimulation();
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(1, false);
  }
  if (m_AIWasOn && m_Driver && !ACR_Squad.FindController(m_Driver))
  {
   AIControlComponent ai = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
   if (ai && !ai.IsAIActivated()) ai.ActivateAI();
  }
  if (m_Squad && m_Squad.m_Order == "Driven by gunner") m_Squad.m_Order = "Holding";
 }
}
