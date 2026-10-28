// -acrRTSOverlayTest 1 (server-capable, ~20 s): the maths behind the RTS camera's order lines
// and contact markers (#66, ACR_RTSOverlay). A line wholly behind the camera isn't drawn; one
// crossing the camera's plane is cut there, on the line itself; one in front is kept as is. A
// contact is full while fresh, fades once it's suspected and is faint, never gone, when forgotten.
class ACR_TestRTSOverlay
{
 static const int CHECKS = 3;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  vector eye = Vector(100, 50, 100);
  vector forward = Vector(0, 0, 1);
  vector a, b;

  bool behind = ACR_RTSOverlay.Clip(eye, forward, Vector(90, 0, 80), Vector(120, 0, 99.5), a, b);
  bool ahead = ACR_RTSOverlay.Clip(eye, forward, Vector(90, 0, 150), Vector(120, 0, 300), a, b);
  bool aheadKept = vector.Distance(a, Vector(90, 0, 150)) < 0.01 && vector.Distance(b, Vector(120, 0, 300)) < 0.01;
  ACR_TestRegress.Check("rts overlay: lines behind the camera are dropped, lines ahead kept", !behind && ahead && aheadKept, string.Format("behind drawn %1, ahead drawn %2 as %3 to %4", behind, ahead, a, b));

  // From the squad behind the camera (z 60) to its point ahead (z 160): cut at z 101, 41% of the way.
  bool crossing = ACR_RTSOverlay.Clip(eye, forward, Vector(100, 0, 60), Vector(140, 0, 160), a, b);
  float depthA = vector.Dot(a - eye, forward);
  bool cut = crossing && Math.AbsFloat(depthA - ACR_RTSOverlay.NEAR) < 0.01 && Math.AbsFloat(a[0] - 116.4) < 0.01 && vector.Distance(b, Vector(140, 0, 160)) < 0.01;
  // The same, the other way round: the far end is the one that's cut.
  bool reversed = ACR_RTSOverlay.Clip(eye, forward, Vector(140, 0, 160), Vector(100, 0, 60), a, b);
  cut = cut && reversed && Math.AbsFloat(vector.Dot(b - eye, forward) - ACR_RTSOverlay.NEAR) < 0.01 && vector.Distance(a, Vector(140, 0, 160)) < 0.01;
  ACR_TestRegress.Check("rts overlay: a line crossing the camera is cut at its near plane", cut, string.Format("cut start %1, depth %2", a, depthA));

  float fresh = ACR_RTSOverlay.ContactAlpha(5);
  float suspected = ACR_RTSOverlay.ContactAlpha(SCR_PlayerController.ACR_CONTACT_SUSPECTED);
  float older = ACR_RTSOverlay.ContactAlpha(60);
  int forgetS = ACR_Intel.MEMORY_MS / 1000;
  float forgotten = ACR_RTSOverlay.ContactAlpha(forgetS);
  float late = ACR_RTSOverlay.ContactAlpha(1000);
  bool fades = fresh == 1 && suspected == 1 && older < 1 && older > forgotten && Math.AbsFloat(forgotten - ACR_RTSOverlay.FADED) < 0.01 && late == forgotten;
  ACR_TestRegress.Check("rts overlay: contacts fade from suspected to forgotten", fades, string.Format("5s %1, %2s %3, 60s %4, forgotten %5, 1000s %6", fresh, SCR_PlayerController.ACR_CONTACT_SUSPECTED, suspected, older, forgotten, late));
  ACR_TestRegress.Complete("rtsoverlay", CHECKS);
 }
}
