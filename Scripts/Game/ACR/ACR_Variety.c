// Dynamism, for replay value (the user, 27 Oct: "one key philosophy i want you to follow from now
// on, its really important for me and replay ability: dynamism"): an AI choice comes from the
// situation (scored candidates: terrain, cover, threat, money), and among the good ones, those
// within NEAR_BEST of the best score, it is picked weighted at random (the best most likely, never
// a poor one just to vary). Seeded from the match seed (ACR_Generator.Seed, the setup screen's
// seed) and the decision's own key (what is chosen, where), so the same match makes the same
// choices. -acrVariety 0 (setup off) always takes the best; srv tests that check one exact pick
// set s_bOff.
class ACR_Variety
{
 static const float NEAR_BEST = 0.12;    // within 12% of the best score (of its size)
 static const float WEIGHT_FLOOR = 0.2;  // a pick at the edge is a fifth as likely as the best
 static bool s_bOff;

 static bool Enabled()
 {
  return !s_bOff && ACR_LogisticsRules.Choice("acrVariety", "1") != "0";
 }

 // A generator for one decision: the match seed and its key hashed together, its first draws
 // dropped (a fresh generator's first draw follows its seed closely, and keys differing in one
 // digit made the weakest option the likeliest, srv:variety).
 static RandomGenerator For(string key)
 {
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed((key + "|" + ACR_Generator.Seed().ToString()).Hash());
  rng.RandFloat01();
  rng.RandFloat01();
  return rng;
 }

 // The index to take from 'scores' (higher is better): the best, or with variety on, one of those
 // within 'near' of it (relative to the best's size, at least 'near' itself), weighted from 1 at
 // the best down to WEIGHT_FLOOR at the edge. -1 for an empty list.
 static int Pick(notnull array<float> scores, string key, float near = NEAR_BEST)
 {
  int best = Best(scores);
  if (best < 0 || !Enabled()) return best;
  float margin = Math.Max(Math.AbsFloat(scores[best]) * near, near);
  float edge = scores[best] - margin;
  array<float> weights = {};
  float total;
  foreach (float score : scores)
  {
   float weight = 0;
   if (score >= edge) weight = WEIGHT_FLOOR + (1 - WEIGHT_FLOOR) * (score - edge) / margin;
   weights.Insert(weight);
   total += weight;
  }
  float roll = For(key).RandFloat01() * total;
  foreach (int i, float w : weights)
  {
   if (w <= 0) continue;
   if (roll <= w) return i;
   roll -= w;
  }
  return best;
 }

 static int Best(notnull array<float> scores)
 {
  int best = -1;
  foreach (int i, float score : scores)
  {
   if (best < 0 || score > scores[best]) best = i;
  }
  return best;
 }

 // 'value' (a time, a count) varied by up to 'spread' either way (0.2: +-20%), seeded as Pick.
 static float Jitter(float value, float spread, string key)
 {
  if (!Enabled()) return value;
  return value * (1 + (For(key).RandFloat01() * 2 - 1) * spread);
 }

 // A whole number of 'total' things for one side, from its share (0..1): the share's floor, plus
 // one more with the chance of the fraction (seeded as Pick). Off: rounded.
 static int Share(int total, float share, string key)
 {
  float exact = total * Math.Clamp(share, 0, 1);
  int whole = Math.Floor(exact);
  if (!Enabled()) return Math.Round(exact);
  if (For(key).RandFloat01() < exact - whole) whole++;
  return Math.ClampInt(whole, 0, total);
 }
}
