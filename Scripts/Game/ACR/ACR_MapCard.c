// The hover card on the command map (playtest 30 Oct): resting the cursor on a squad's icon shows
// its name and kind, what it is doing and where to, its health, ammo and fuel, and who gave its
// order and when; on a base, the base's own details (owner, garrison, supply, income) in the same
// card instead of its caption growing into a block of text. It follows the cursor and goes when
// the cursor leaves the icon (SCR_PlayerController.ACR_HoverCard, ACR_PlayerMapHover).
class ACR_MapCard : Managed
{
 static const float WIDTH = 380;
 static const int LINES = 5;

 Widget m_Root;
 protected TextWidget m_Title;
 protected ref array<TextWidget> m_Lines = {};
 protected string m_sShown = "-";

 void Create(Widget parent)
 {
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH, 12, 8, m_Root, ACR_TwoICStyle.Rgb(16, 19, 23, 0.95));
  FrameSlot.SetAnchorMin(m_Root, 0, 0);
  FrameSlot.SetAnchorMax(m_Root, 0, 0);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1040);
  // The card never takes the cursor: it sits beside it.
  m_Root.SetFlags(WidgetFlags.IGNORE_CURSOR);
  m_Title = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.White(), 19, true);
  for (int i = 0; i < LINES; i++)
  {
   TextWidget line = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Body(), 16);
   if (line) AlignableSlot.SetPadding(line, 0, 2, 0, 0);
   m_Lines.Insert(line);
  }
  m_Root.SetVisible(false);
 }

 // title and lines ("" lines are hidden); x, y: the cursor in native pixels.
 void Show(string title, array<string> lines, float x, float y)
 {
  if (!m_Root) return;
  string key = title;
  foreach (string l : lines) key += "|" + l;
  if (key != m_sShown)
  {
   m_sShown = key;
   m_Title.SetText(title);
   foreach (int i, TextWidget line : m_Lines)
   {
    if (!line) continue;
    bool used = i < lines.Count() && !lines[i].IsEmpty();
    line.SetVisible(used);
    if (used) line.SetText(lines[i]);
   }
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float ux = ws.DPIUnscale(x);
  float uy = ws.DPIUnscale(y);
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  float h = 40 + 26 * LINES;
  float px = ux + 28;
  float py = uy + 20;
  if (px + WIDTH > screenW - 8) px = ux - 28 - WIDTH;
  if (py + h > screenH - 8) py = uy - 20 - h;
  FrameSlot.SetPos(m_Root, Math.Max(8, px), Math.Max(8, py));
  m_Root.SetVisible(true);
 }

 void Hide()
 {
  if (m_Root) m_Root.SetVisible(false);
 }

 bool Shown() { return m_Root && m_Root.IsVisible(); }

 void Destroy()
 {
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
 }
}
