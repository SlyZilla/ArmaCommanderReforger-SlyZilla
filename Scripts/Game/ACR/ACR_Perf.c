// Server tick profiling for the perf test (-acrPerfTest). Off otherwise.
// The engine only has a millisecond clock, so each stage's time is summed
// over many ticks; stages well under 1 ms show as 0 and that is the answer.
// tick = manager Tick only; frames is separate. ai.* / route.* are nested breakdowns:
// do not add them to the top-level stages. Maxima include all calls in a manager tick.
class ACR_Perf
{
 static bool s_bOn;
 static bool s_bChecks;          // test runs: invariant checks every tick
 static int s_iInvariantErrors;
 static ref map<string, int> s_Current = new map<string, int>();
 static ref map<string, int> s_Total = new map<string, int>();
 static ref map<string, int> s_Max = new map<string, int>();
 static ref array<string> s_Order = {};
 static int s_iTicks;
 static int s_iStatusTotal, s_iStatusMax;
 static int s_iFeedBytes;    // estimated map feed payload, summed since the last report
 static int s_iFeedSends;    // RPCs sent since the last report
 static int s_iFeedSkipped;  // RPCs skipped (unchanged) since the last report

 // Time since 'start' goes to this stage; returns the new start.
 static int Lap(string stage, int start)
 {
  int now = System.GetTickCount();
  if (!s_bOn) return now;
  int spent = now - start;
  int total;
  if (!s_Total.Find(stage, total)) s_Order.Insert(stage);
  s_Total.Set(stage, total + spent);
  // A stage can run several times per manager tick (two commanders, several routes).
  // Sum those calls before taking its tick maximum.
  s_Current.Set(stage, s_Current.Get(stage) + spent);
  return now;
 }

 static void CountStatus(int count)
 {
  if (!s_bOn) return;
  s_iStatusTotal += count;
  s_iStatusMax = Math.Max(s_iStatusMax, count);
 }

 static void EndTick()
 {
  if (!s_bOn) return;
  BBB_Perf.s_bOn = true; // Better Battle Buddies' own stages, reported below
  foreach (string stage, int spent : s_Current)
   if (spent > s_Max.Get(stage)) s_Max.Set(stage, spent);
  s_Current.Clear();
  s_iTicks++;
  if (s_iTicks % 30 != 0) return;
  string line = "";
  foreach (string reportStage : s_Order)
  {
   float average = s_Total.Get(reportStage) / 30.0;
   line += string.Format(" %1=%2/%3", reportStage, average.ToString(4, 1), s_Max.Get(reportStage));
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  int squads, soldiers;
  if (manager)
  {
   squads = manager.m_Squads.Count();
   foreach (ACR_Squad squad : manager.m_Squads) soldiers += squad.CountAlive();
  }
  PrintFormat("[ACR] PERF t=%1 squads=%2 soldiers=%3 | ms avg/max per tick:%4", s_iTicks, squads, soldiers, line);
  PrintFormat("[ACR] PERF status updates avg/max per tick: %1/%2", s_iStatusTotal / 30.0, s_iStatusMax);
  s_iStatusTotal = 0; s_iStatusMax = 0;
  PrintFormat("[ACR] PERF bbb:%1", BBB_Perf.Take());
  PrintFormat("[ACR] PERF feed: %1 bytes/s, %2 RPCs/s sent, %3 RPCs/s skipped as unchanged", s_iFeedBytes / 30, s_iFeedSends / 30.0, s_iFeedSkipped / 30.0);
  s_Total.Clear();
  s_Max.Clear();
  s_Order.Clear();
  s_iFeedBytes = 0;
  s_iFeedSends = 0;
  s_iFeedSkipped = 0;
 }
}
