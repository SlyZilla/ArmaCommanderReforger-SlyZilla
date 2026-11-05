// -acrVarietyTest 1 (server-capable, ~20 s): the dynamism picker (ACR_Variety). Off, it takes the
// best; on, the same key picks the same; over many keys it picks only among those within NEAR_BEST
// of the best, the best most often, and more than one of them; a jittered value stays within its
// spread; a share of a count rounds to its floor or ceiling.
class ACR_TestVariety
{
 static const int CHECKS = 4;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  array<float> scores = {10, 9.5, 9.0, 5, 2};

  ACR_Variety.s_bOff = true;
  int offPick = ACR_Variety.Pick(scores, "test");
  float offJitter = ACR_Variety.Jitter(100, 0.2, "test");
  int offShare = ACR_Variety.Share(4, 0.4, "test");
  ACR_Variety.s_bOff = false;
  ACR_TestRegress.Check("variety: off, the best and the plain values", offPick == 0 && offJitter == 100 && offShare == 2, string.Format("pick %1, jitter %2, share %3", offPick, offJitter, offShare));

  bool same = ACR_Variety.Pick(scores, "same key") == ACR_Variety.Pick(scores, "same key") && ACR_Variety.Jitter(100, 0.2, "same key") == ACR_Variety.Jitter(100, 0.2, "same key");
  ACR_TestRegress.Check("variety: the same match and key make the same choice", same, "");

  array<int> counts = {0, 0, 0, 0, 0};
  for (int k = 0; k < 300; k++)
  {
   int pick = ACR_Variety.Pick(scores, "key " + k.ToString());
   if (pick >= 0) counts[pick] = counts[pick] + 1;
  }
  int kinds;
  for (int c = 0; c < 3; c++) if (counts[c] > 0) kinds++;
  bool spread = counts[3] == 0 && counts[4] == 0 && counts[0] >= counts[1] && counts[0] >= counts[2] && kinds >= 2;
  ACR_TestRegress.Check("variety: picks only among those within 12% of the best, the best most often, more than one of them", spread, string.Format("10 / 9.5 / 9 / 5 / 2 picked %1 / %2 / %3 / %4 / %5 times of 300", counts[0], counts[1], counts[2], counts[3], counts[4]));

  bool within = true;
  bool floorOrCeil = true;
  for (int j = 0; j < 50; j++)
  {
   float jittered = ACR_Variety.Jitter(100, 0.2, "jitter " + j.ToString());
   if (jittered < 80 || jittered > 120) within = false;
   int share = ACR_Variety.Share(4, 0.4, "share " + j.ToString());
   if (share != 1 && share != 2) floorOrCeil = false;
  }
  ACR_TestRegress.Check("variety: a jittered value stays within its spread, a share of 4 at 40% is 1 or 2", within && floorOrCeil, string.Format("within +-20%% %1, 1 or 2 %2", within, floorOrCeil));
  ACR_TestRegress.Complete("variety", CHECKS);
 }
}
