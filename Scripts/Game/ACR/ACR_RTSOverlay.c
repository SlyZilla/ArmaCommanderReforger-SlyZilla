// Client, RTS camera (F10, #66): our squads' orders and the enemies our side has spotted,
// drawn over the 3D view the way the command map draws them. Reads ACR_MapData and the
// controller's contacts; owns no game state.
//
// Orders: every squad's current point and queued points, chained from the squad, in the
// order's colour (ACR_OrderDisplay draws them, so solid/dashed and colours match the map).
// The selected squads' lines are bright with a ring on each point; the others are faint and
// only drawn near the camera. Contacts: a red diamond and a label at the last known spot,
// fading from the moment they turn suspected until our side forgets them. A right-click on
// one hunts it (ACR_RTSCameraManager.HandleRightClick), as on the map.
class ACR_RTSOverlay : Managed
{
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 static const float NEAR = 1.0;           // m in front of the camera: lines are cut here
 static const float LIFT = 1.5;           // m above the ground: order points float this high
 static const float ORDER_RANGE = 1500;   // m: unselected squads' orders farther than this are left out
 static const float CONTACT_RANGE = 2500; // m: contacts farther than this aren't drawn
 static const float FADED = 0.35;         // a contact's alpha just before it's forgotten

 protected CanvasWidget m_Canvas;
 protected Widget m_Layer;
 protected ref ACR_OrderDisplay m_Draw = new ACR_OrderDisplay();
 protected ref array<ref CanvasWidgetCommand> m_Commands = {};
 protected ref array<TextWidget> m_Labels = {};
 protected ref array<vector> m_ContactScreen = {}; // per contact: reference px, z = depth (<= 0 not drawn)

 // Under everything else on the layer: call before the squad and base pins are made.
 void ACR_RTSOverlay(Widget layer)
 {
  m_Layer = layer;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_Canvas = CanvasWidget.Cast(ws.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, layer));
  if (!m_Canvas) return;
  FrameSlot.SetAnchorMin(m_Canvas, 0, 0);
  FrameSlot.SetAnchorMax(m_Canvas, 1, 1);
  FrameSlot.SetOffsets(m_Canvas, 0, 0, 0, 0);
  m_Draw.SetCanvas(m_Canvas);
  m_Draw.UseCommands(m_Commands);
 }

 // Cuts the segment a-b at the plane NEAR m in front of the eye; false when all of it is behind.
 static bool Clip(vector eye, vector forward, vector a, vector b, out vector clippedA, out vector clippedB)
 {
  float da = vector.Dot(a - eye, forward);
  float db = vector.Dot(b - eye, forward);
  clippedA = a;
  clippedB = b;
  if (da < NEAR && db < NEAR) return false;
  if (da < NEAR) clippedA = a + (b - a) * ((NEAR - da) / (db - da));
  else if (db < NEAR) clippedB = b + (a - b) * ((NEAR - db) / (da - db));
  return true;
 }

 // Full while fresh, then fading to FADED until our side forgets the contact.
 static float ContactAlpha(int age)
 {
  float suspected = SCR_PlayerController.ACR_CONTACT_SUSPECTED;
  float forgotten = ACR_Intel.MEMORY_MS / 1000;
  float since = age;
  if (since <= suspected) return 1;
  float t = Math.Clamp((since - suspected) / (forgotten - suspected), 0, 1);
  return 1 - (1 - FADED) * t;
 }

 int CommandCount() { return m_Commands.Count(); }
 array<vector> ContactScreen() { return m_ContactScreen; }

 // ---------------------------------------------------------------- frame
 void Update(SCR_PlayerController pc, ACR_MapData data, CameraBase camera)
 {
  m_Commands.Clear();
  m_ContactScreen.Clear();
  int labels = 0;
  if (camera && m_Canvas)
  {
   vector transform[4];
   camera.GetWorldTransform(transform);
   if (data) DrawOrders(data, transform[3], transform[2]);
   labels = DrawContacts(pc, transform[3], transform[2]);
  }
  if (m_Canvas) m_Canvas.SetDrawCommands(m_Commands);
  for (int i = labels; i < m_Labels.Count(); i++) m_Labels[i].SetVisible(false);
 }

 protected vector Lifted(vector pos)
 {
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]) + LIFT;
  return pos;
 }

 protected void DrawOrders(ACR_MapData data, vector eye, vector forward)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  BaseWorld world = GetGame().GetWorld();
  foreach (int i, RplId groupId : data.m_OrderGroups)
  {
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderIds.Count() || i >= data.m_OrderTargets.Count()) continue;
   vector from = data.LeaderPos(g) + Vector(0, LIFT, 0);
   // Later entries of the same squad are the rest of its plan: chain them.
   if (i > 0 && data.m_OrderGroups[i - 1] == groupId) from = Lifted(data.m_OrderTargets[i - 1]);
   vector to = Lifted(data.m_OrderTargets[i]);
   bool selected = data.IsSelected(groupId);
   if (!selected && vector.Distance(eye, from) > ORDER_RANGE && vector.Distance(eye, to) > ORDER_RANGE) continue;
   vector a, b;
   if (!Clip(eye, forward, from, to, a, b)) continue;
   vector sa = ws.ProjWorldToScreenNative(a, world);
   vector sb = ws.ProjWorldToScreenNative(b, world);
   float alpha = 0.5;
   float width = ws.DPIScale(2);
   if (selected)
   {
    alpha = 1;
    width = ws.DPIScale(3);
   }
   int kind = 0;
   if (i < data.m_OrderKinds.Count()) kind = data.m_OrderKinds[i];
   int color = ACR_OrderDisplay.OrderColor(kind, alpha);
   int issuer = 0;
   if (i < data.m_OrderIssuers.Count()) issuer = data.m_OrderIssuers[i];
   // A dark edge under the line, so it reads over grass and sky alike.
   Color dark = new Color(0, 0, 0, alpha * 0.5);
   int edge = dark.PackToInt();
   if (issuer > 0 && issuer != data.m_iMyId) m_Draw.DashedLine(sa[0], sa[1], sb[0], sb[1], color, width);
   else
   {
    m_Draw.Line(sa[0], sa[1], sb[0], sb[1], edge, width + ws.DPIScale(2));
    m_Draw.Line(sa[0], sa[1], sb[0], sb[1], color, width);
   }
   if (vector.Dot(to - eye, forward) < NEAR) continue;
   m_Draw.Dot(sb[0], sb[1], ws.DPIScale(5), color);
   if (selected) m_Draw.Ring(sb[0], sb[1], ws.DPIScale(10), color, ws.DPIScale(2));
  }
 }

 // Returns how many labels it used.
 protected int DrawContacts(SCR_PlayerController pc, vector eye, vector forward)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  BaseWorld world = GetGame().GetWorld();
  int used = 0;
  int count = pc.ACR_ContactCount();
  for (int i = 0; i < count; i++)
  {
   vector pos;
   int kind, members, age;
   pc.ACR_GetContact(i, pos, kind, members, age);
   vector top = pos + Vector(0, 2, 0);
   float distance = vector.Distance(eye, top);
   if (distance > CONTACT_RANGE || vector.Dot(top - eye, forward) < NEAR)
   {
    m_ContactScreen.Insert(Vector(0, 0, -1));
    continue;
   }
   float alpha = ContactAlpha(age);
   Color red = new Color(1, 0.25, 0.2, alpha);
   // The diamond, the NATO hostile frame: a dark one under a red one.
   Color shade = new Color(0, 0, 0, alpha * 0.6);
   vector pixel = ws.ProjWorldToScreenNative(top, world);
   Diamond(pixel[0], pixel[1], ws.DPIScale(9), shade.PackToInt());
   Diamond(pixel[0], pixel[1], ws.DPIScale(7), red.PackToInt());
   vector screen = ws.ProjWorldToScreen(top, world);
   m_ContactScreen.Insert(screen);
   TextWidget label = Label(used);
   used++;
   if (!label) continue;
   label.SetVisible(true);
   FrameSlot.SetPos(label, screen[0], screen[1] - 10);
   int contactId, icons, heading;
   pc.ACR_GetContactSymbol(i, contactId, icons, heading);
   label.SetText(string.Format("%1  %2  %3m", ACR_Intel.DescribeSymbol(icons, kind, members), ACR_Intel.AgeText(age), Math.Round(distance)));
   label.SetColor(red);
  }
  return used;
 }

 protected void Diamond(float x, float y, float radius, int color)
 {
  PolygonDrawCommand diamond = new PolygonDrawCommand();
  diamond.m_iColor = color;
  diamond.m_Vertices = {x, y - radius, x + radius, y, x, y + radius, x - radius, y};
  m_Draw.AddPolygon(diamond);
 }

 protected TextWidget Label(int index)
 {
  while (m_Labels.Count() <= index)
  {
   TextWidget made = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(LAYOUT_TEXT, m_Layer));
   if (!made) return null;
   made.SetFlags(WidgetFlags.IGNORE_CURSOR);
   FrameSlot.SetAlignment(made, 0.5, 1);
   FrameSlot.SetSizeToContent(made, true);
   made.SetOutline(1);
   m_Labels.Insert(made);
  }
  return m_Labels[index];
 }
}
