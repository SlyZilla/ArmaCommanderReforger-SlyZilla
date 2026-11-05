// One right-click on the map, one order (drag to aim). While an order that takes a facing is
// armed, a short press is both a press and release (MapPanDrag: ACR_AimUp orders) and a click
// (MapContextualMenu: ACR_HandleMapClick). The first order resets the cursor to Move, so the
// click then sent a plain move as well, 14 ms later, and the move ended the order at once
// (playtest 25 Oct: every clicked Ambush and Fortify upgrade). The click straight after a
// release that ordered is swallowed.
class ACR_ClickGate
{
 static const int SAME_PRESS_MS = 250;
 protected int m_iReleaseOrderAt = -100000;

 // A release has just given its order.
 void ReleaseOrdered(int now)
 {
  m_iReleaseOrderAt = now;
 }

 // A click: true when it is that release's own press (swallowed once).
 bool SwallowClick(int now)
 {
  if (now - m_iReleaseOrderAt > SAME_PRESS_MS) return false;
  m_iReleaseOrderAt = -100000;
  return true;
 }
}

// The server's half, for a client without the gate: a move for a squad within SAME_CLICK_MS of
// an order for it that takes a facing (ACR_Requests.TakesFacing), from the same player, is the
// same click and is dropped.
class ACR_SameClickGuard
{
 static const float SAME_CLICK_MS = 150;
 protected ref array<RplId> m_aSquads = {};
 protected ref array<float> m_aAt = {};

 // Before an order is handled: true to drop it.
 bool Drop(int command, RplId squad, float now)
 {
  if (command != ACR_ERequest.MOVE_TO && command != ACR_ERequest.QUEUE_MOVE && command != ACR_ERequest.ORDER_HUNT) return false;
  int i = m_aSquads.Find(squad);
  return i >= 0 && now - m_aAt[i] <= SAME_CLICK_MS;
 }

 // After it is handled: an order that takes a facing starts its squad's window.
 void Note(int command, RplId squad, float now)
 {
  for (int k = m_aSquads.Count() - 1; k >= 0; k--)
  {
   if (now - m_aAt[k] <= SAME_CLICK_MS) continue;
   m_aSquads.Remove(k);
   m_aAt.Remove(k);
  }
  if (!ACR_Requests.TakesFacing(command)) return;
  int i = m_aSquads.Find(squad);
  if (i >= 0)
  {
   m_aAt[i] = now;
   return;
  }
  m_aSquads.Insert(squad);
  m_aAt.Insert(now);
 }
}
