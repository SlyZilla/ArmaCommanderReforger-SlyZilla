// Server cost of BBB for perf tests. Off unless a host turns it on (AC: Actual's srv:perf
// does, through ACR_Perf; vanilla hosts can pass -bbbPerf 1 for a "[BBB] PERF" line every
// 30 s). The engine clock is in whole milliseconds, so costs are summed over the 30 s
// window: tick stages (once a second) as ms per tick, event work (AI reactions, damage
// hooks) as ms per second plus how many times it ran. Frame = the server's average
// frame time over the window (wall clock), to compare runs with a feature on and off.
class BBB_Perf
{
 static bool s_bOn;
 static bool s_bOwnReport; // -bbbPerf 1: BBB prints its own line (no host report)
 protected static ref map<string, int> s_Ms = new map<string, int>();
 protected static ref map<string, int> s_Max = new map<string, int>();
 protected static ref map<string, int> s_Count = new map<string, int>();
 protected static ref array<string> s_Order = {};
 protected static int s_iTicks;
 protected static int s_iWindowTicks;
 protected static int s_iFrames;
 protected static int s_iWindowStart;

 static void Read()
 {
  string value;
  if (System.GetCLIParam("bbbPerf", value) && value == "1") { s_bOn = true; s_bOwnReport = true; }
 }

 // Time since 'start' goes to this stage; returns the new start.
 static int Lap(string stage, int start)
 {
  int now = System.GetTickCount();
  if (!s_bOn) return now;
  Add(stage, now - start);
  return now;
 }

 static void Add(string stage, int ms)
 {
  if (!s_bOn) return;
  int total;
  if (!s_Ms.Find(stage, total)) s_Order.Insert(stage);
  s_Ms.Set(stage, total + ms);
  s_Count.Set(stage, s_Count.Get(stage) + 1);
  if (ms > s_Max.Get(stage)) s_Max.Set(stage, ms);
 }

 // How many times something happened (no time).
 static void Count(string what)
 {
  if (!s_bOn) return;
  if (!s_Count.Contains(what)) s_Order.Insert(what);
  s_Count.Set(what, s_Count.Get(what) + 1);
 }

 static void OnTick()
 {
  if (!s_bOn) return;
  s_iTicks++;
  s_iWindowTicks++;
  if (s_bOwnReport && s_iTicks % 30 == 0) PrintFormat("[BBB] PERF t=%1 |%2", s_iTicks, Take());
 }

 static void OnFrame()
 {
  if (!s_bOn) return;
  if (s_iFrames == 0) s_iWindowStart = System.GetTickCount();
  s_iFrames++;
 }

 // The window's summary (and a fresh window). Tick stages "name=avg/max ms per tick",
 // events "name=ms/s xN", counters "name xN".
 static string Take()
 {
  string line = "";
  float ticks = Math.Max(1, s_iWindowTicks);
  foreach (string stage : s_Order)
  {
   int count = s_Count.Get(stage);
   if (!s_Ms.Contains(stage)) line += string.Format(" %1 x%2", stage, count);
   else if (stage.StartsWith("tick."))
   {
    float average = s_Ms.Get(stage) / ticks;
    line += string.Format(" %1=%2/%3", stage, average.ToString(4, 2), s_Max.Get(stage));
   }
   else
   {
    float perSecond = s_Ms.Get(stage) / 30.0;
    line += string.Format(" %1=%2ms/s x%3", stage, perSecond.ToString(4, 2), count);
   }
  }
  if (s_iFrames > 1)
  {
   float frame = (System.GetTickCount() - s_iWindowStart);
   frame = frame / (s_iFrames - 1);
   line += string.Format(" frame=%1ms", frame.ToString(4, 1));
  }
  s_Ms.Clear();
  s_Max.Clear();
  s_Count.Clear();
  s_Order.Clear();
  s_iFrames = 0;
  s_iWindowTicks = 0;
  return line;
 }
}
