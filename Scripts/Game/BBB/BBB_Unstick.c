// Backing a stuck vehicle out: the game's AI driver often sits against a wall,
// a tree or another vehicle with the throttle down. The host decides when a
// vehicle is stuck (and what to do after); this does the manoeuvre: the driver's
// AI is parked, the vehicle reverses with the wheel turned for a few seconds and
// brakes, then the driver gets his AI back. Inputs are written every frame
// (BBB.Frame): the vehicle controller drops inputs set less often.

// Told when a manoeuvre is over (finished, driver lost or timed out).
class BBB_UnstickListener : Managed
{
 void OnUnstickEnded(IEntity vehicle) {}
}

class BBB_Unstick : Managed
{
 static const float REVERSE_MS = 3500;    // reversing, then a short brake
 static const float TOTAL_MS = 4300;

 protected static ref array<ref BBB_Unstick> s_Active = {};

 IEntity m_Vehicle;
 ChimeraCharacter m_Driver;
 protected float m_fStart;
 protected float m_fSteer;
 protected ref BBB_UnstickListener m_Listener;

 // Start backing this vehicle out; steer is the wheel while reversing (-1..1).
 static void Begin(IEntity vehicle, ChimeraCharacter driver, float steer, BBB_UnstickListener listener)
 {
  BBB_Unstick unstick = new BBB_Unstick();
  unstick.m_Vehicle = vehicle;
  unstick.m_Driver = driver;
  unstick.m_fStart = GetGame().GetWorld().GetWorldTime();
  unstick.m_fSteer = steer;
  unstick.m_Listener = listener;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
  s_Active.Insert(unstick);
 }

 // Drop a manoeuvre without finishing it (its owner is gone).
 static void Cancel(BBB_UnstickListener listener)
 {
  for (int i = s_Active.Count() - 1; i >= 0; i--)
   if (s_Active[i].m_Listener == listener) s_Active.RemoveOrdered(i);
 }

 static void StopAll() { s_Active.Clear(); }

 static void FrameAll()
 {
  for (int i = s_Active.Count() - 1; i >= 0; i--)
   if (!s_Active[i].Frame()) s_Active.RemoveOrdered(i);
 }

 // Returns false when finished.
 protected bool Frame()
 {
  float elapsed = GetGame().GetWorld().GetWorldTime() - m_fStart;
  IEntity vehicle = m_Vehicle;
  bool alive = vehicle != null && m_Driver != null && BBB.IsAlive(m_Driver) && BBB_Transport.VehicleOf(m_Driver) == vehicle;
  CarControllerComponent car;
  if (alive) car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetSimulation() || elapsed > TOTAL_MS)
  {
   End(car);
   return false;
  }
  VehicleWheeledSimulation sim = car.GetSimulation();
  Physics physics = vehicle.GetPhysics();
  if (physics && !physics.IsActive()) physics.SetActive(ActiveState.ACTIVE);
  if (!car.IsEngineOn()) car.ForceStartEngine();
  car.SetPersistentHandBrake(false);
  int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
  sim.SetClutch(1);
  if (elapsed <= REVERSE_MS)
  {
   if (sim.GetGear() >= neutral) sim.SetGear(neutral - 1);
   sim.SetBreak(0, false);
   sim.SetThrottle(0.7);
   sim.SetSteering(m_fSteer);
  }
  else
  {
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(1, false);
  }
  return true;
 }

 protected void End(CarControllerComponent car)
 {
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation sim = car.GetSimulation();
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(0, false);
   int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
   sim.SetGear(neutral + 1);
  }
  if (m_Driver && BBB.IsAlive(m_Driver) && !BBB.PlayerOf(m_Driver))
  {
   AIControlComponent ai = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
   if (ai && !ai.IsAIActivated()) ai.ActivateAI();
  }
  if (m_Listener) m_Listener.OnUnstickEnded(m_Vehicle);
 }
}
