// Tactical 3D Commander Camera (aca-f02).
// Overhead RTS perspective hovering above the battlefield with smooth WASD ground movement,
// terrain-following altitude (ATL 12m-160m), RMB orbit rotation, mouse wheel zoom, 300m proximity
// leash to friendly squads and bases, and full 3D selection and authoritative order dispatching.

class ACR_RTSCameraHandler : ScriptedWidgetEventHandler
{
 ACR_RTSCameraManager m_Manager;
 protected bool m_bLeftDown;

 void ACR_RTSCameraHandler(ACR_RTSCameraManager manager) { m_Manager = manager; }

 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (!m_Manager) return false;
  if (button == 0) m_bLeftDown = true;
  if (button == 1) m_Manager.BeginOrbit(x, y);
  return true;
 }

 override bool OnMouseButtonUp(Widget w, int x, int y, int button)
 {
  if (!m_Manager) return false;
  if (button == 0 && m_bLeftDown)
  {
   m_bLeftDown = false;
   if (WidgetManager.GetWidgetUnderCursor() == w) m_Manager.HandleLeftClick(x, y);
  }
  if (button == 1) m_Manager.EndOrbit(x, y, WidgetManager.GetWidgetUnderCursor() == w);
  return true;
 }
}

class ACR_RTSCameraManager : Managed
{
	protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
	protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
	protected static const ResourceName LAYOUT_HEADING = "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout";


	static ref ACR_RTSCameraManager s_Instance;
	static bool s_bAdminUnbound = false;

 protected CameraBase m_Camera;
 protected CameraBase m_PreviousCamera;
 protected IEntity m_Body;
 protected int m_iPointCommand;
 protected bool m_bOrbit;
 protected bool m_bDragged;
 protected int m_iMouseX;
 protected int m_iMouseY;
 protected int m_iDownX;
 protected int m_iDownY;
	protected Widget m_RootWidget;
	protected Widget m_InteractionLayer;
	protected TextWidget m_HeadingText;
	protected TextWidget m_LeashText;
	protected TextWidget m_WarningText;
	protected TextWidget m_SquadText;
	protected TextWidget m_SquadDetailText;
	protected TextWidget m_ToastText;

	protected Widget m_BottomBar;
	protected Widget m_UnitActionsRow;
	protected SCR_ButtonTextComponent m_ExitBtn;
	protected SCR_ButtonTextComponent m_DefendBtn;
	protected SCR_ButtonTextComponent m_GarrisonBtn;
	protected SCR_ButtonTextComponent m_GetInBtn;
	protected SCR_ButtonTextComponent m_SwitchBtn;
	protected SCR_ButtonTextComponent m_HaltBtn;
	protected SCR_ButtonTextComponent m_RoadblockBtn;
	protected bool m_bRoadblockMode = false;

	protected ref map<SCR_ButtonBaseComponent, int> m_Actions = new map<SCR_ButtonBaseComponent, int>();
	protected ref ACR_RTSCameraHandler m_Handler;

	protected float m_fLastWarningAt = -10000;
	protected float m_fToastUntil = 0;
	protected float m_fLastHealth = 100;
	protected bool m_bDefendMode = false;
	protected bool m_bContextActivated; // last frame's cursor-context activation (tests read it)

	// World markers: one pin per friendly squad and base, re-projected every frame.
	// Screen positions are kept (reference pixels, z = depth) so clicks pick markers.
	protected static const float MARKER_PICK_RADIUS = 28.0;
	protected Widget m_MarkerLayer;
	protected ref array<Widget> m_SquadMarkers = {};
	protected ref array<Widget> m_BaseMarkers = {};
	protected ref array<vector> m_SquadMarkerPos = {};
	protected ref array<vector> m_BaseMarkerPos = {};

	// ---------------------------------------------------------------- lifecycle
	static bool IsActive()
	{
		return s_Instance != null && s_Instance.m_Camera != null;
	}

	static void Toggle()
	{
		if (IsActive()) Close();
		else Open();
	}

	static void Open()
	{
		if (IsActive()) return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		IEntity localBody = pc.GetControlledEntity();
		if (!ACR_Respawn.IsAlive(localBody)) return;

		s_Instance = new ACR_RTSCameraManager();
		s_Instance.InitCamera(pc, localBody);
		if (!IsActive()) Close();
	}

	static void Close()
	{
		if (!s_Instance) return;
		s_Instance.Cleanup();
		s_Instance = null;
	}

	static void Frame(SCR_PlayerController pc)
	{
		if (s_Instance) s_Instance.FrameUpdate(pc);
	}

	// ---------------------------------------------------------------- initialization
	protected void InitCamera(SCR_PlayerController pc, IEntity localBody)
	{
		m_Body = localBody;
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras) m_PreviousCamera = cameras.CurrentCamera();
		// 1. Mute physical soldier controls so commander doesn't move or shoot while surveying
		ACR_MapHandoff.ClearCharacterInput(localBody);
		ACR_MapHandoff.SetMenuControls(localBody, true);

		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(localBody.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmg) m_fLastHealth = dmg.GetHealth();

		// 2. Anchor position & orientation: selected squad / base or player's location
		vector eye = vector.Zero;
		vector angles = Vector(0, -45, 0); // Yaw, Pitch (-45 deg down), Roll

		ACR_MapData data = pc.ACR_GetData();
		RplId selSquad = pc.ACR_GetSelectedGroup();
		if (selSquad.IsValid() && data)
		{
			int sqIdx = data.m_GroupIds.Find(selSquad);
			if (sqIdx >= 0) eye = data.LeaderPos(sqIdx);
		}
		else if (!pc.ACR_SelectedBase().IsEmpty() && data)
		{
			int bIdx = data.m_BaseNames.Find(pc.ACR_SelectedBase());
			if (bIdx >= 0) eye = data.m_BasePositions[bIdx];
		}

		if (eye == vector.Zero)
		{
			eye = localBody.GetOrigin();
			vector bodyAngles = localBody.GetYawPitchRoll();
			angles[0] = bodyAngles[0]; // Face player's facing yaw
		}

		float groundY = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]);
		eye[1] = Math.Max(groundY, eye[1]) + 35.0; // Initial 35m altitude

		// 3. Spawn Camera Entity
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = eye;

  // Keep the native spectator rendering setup, but let RTS alone own input
  // and transforms instead of the editor's free-flight component stack.
  Resource prefab = Resource.Load("{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et");
  if (prefab && prefab.IsValid()) m_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), params));
  if (!m_Camera) m_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), params));
  if (!m_Camera) return;
  SCR_ManualCamera manual = SCR_ManualCamera.Cast(m_Camera);
  if (manual)
  {
   manual.SetInputEnabled(false);
   manual.ClearEventMask(EntityEvent.POSTFRAME);
  }
  m_Camera.SetWorldTransform(params.Transform);
  m_Camera.SetVerticalFOV(60);
  m_Camera.SetNearPlane(0.5);
  m_Camera.SetFarPlane(8000);
  if (cameras) cameras.SetCamera(m_Camera);

		// 4. Build HUD Overlay & Interactive Canvas
		CreateUI();
		if (!m_InteractionLayer) { Cleanup(); return; }
		GetGame().GetInputManager().ActivateContext("ACR_RTSContext");
		ShowToast("RTS Camera active - [F10 / Esc] to exit");
	}

	// ---------------------------------------------------------------- UI creation
	protected void CreateUI()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws) return;

		// Root overlay covering full screen
		m_RootWidget = ACR_Hud.CreateLayer(900, true);
		if (!m_RootWidget) return;

		// Fullscreen interactive click layer
		m_InteractionLayer = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0, 0, 0, 0.001), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(m_InteractionLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_InteractionLayer, 1, 1);
		FrameSlot.SetOffsets(m_InteractionLayer, 0, 0, 0, 0);
		m_Handler = new ACR_RTSCameraHandler(this);
		m_InteractionLayer.AddHandler(m_Handler);

		// Markers sit above the click layer but ignore the cursor, so clicks still land on it.
		m_MarkerLayer = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(m_MarkerLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_MarkerLayer, 1, 1);
		FrameSlot.SetOffsets(m_MarkerLayer, 0, 0, 0, 0);

		// Top Status Banner
		Widget topOverlay = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(topOverlay, 0.5, 0);
		FrameSlot.SetAnchorMax(topOverlay, 0.5, 0);
		FrameSlot.SetAlignment(topOverlay, 0.5, 0);
		FrameSlot.SetPos(topOverlay, 0, 16);
		FrameSlot.SetSizeToContent(topOverlay, true);

		Widget topBg = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0.02, 0.13, 0.30, 0.88), 0, topOverlay);
		AlignableSlot.SetHorizontalAlign(topBg, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(topBg, LayoutVerticalAlign.Stretch);

		SizeLayoutWidget topSize = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, topOverlay));
		topSize.EnableWidthOverride(true);
		topSize.SetWidthOverride(760);
		AlignableSlot.SetPadding(topSize, 16, 10, 16, 10);

		Widget topContent = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, topSize);
		Widget topLeft = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, topContent);
		AlignableSlot.SetHorizontalAlign(topLeft, LayoutHorizontalAlign.Stretch);

		m_HeadingText = AddText(topLeft, LAYOUT_HEADING, "AC: ACTUAL - 3D COMMAND CAMERA");
		m_LeashText = AddText(topLeft, LAYOUT_TEXT, "LEASH: CONNECTED (0m / 300m)");
		m_WarningText = AddText(topLeft, LAYOUT_TEXT, "");
		if (m_WarningText) m_WarningText.SetColor(new Color(0.95, 0.25, 0.25, 1.0));

		Widget topRight = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, topContent);
		AlignableSlot.SetHorizontalAlign(topRight, LayoutHorizontalAlign.Right);
		m_ExitBtn = AddButton(topRight, "Exit Camera [F10]", 1);

		// Bottom Tactical Bar
		m_BottomBar = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(m_BottomBar, 0.5, 1);
		FrameSlot.SetAnchorMax(m_BottomBar, 0.5, 1);
		FrameSlot.SetAlignment(m_BottomBar, 0.5, 1);
		FrameSlot.SetPos(m_BottomBar, 0, -20);
		FrameSlot.SetSizeToContent(m_BottomBar, true);

		Widget botBg = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0.02, 0.13, 0.30, 0.88), 0, m_BottomBar);
		AlignableSlot.SetHorizontalAlign(botBg, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(botBg, LayoutVerticalAlign.Stretch);

		SizeLayoutWidget botSize = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_BottomBar));
		botSize.EnableWidthOverride(true);
		botSize.SetWidthOverride(860);
		AlignableSlot.SetPadding(botSize, 18, 12, 18, 12);

		Widget botCol = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, botSize);

		m_SquadText = AddText(botCol, LAYOUT_HEADING, "No Squad Selected");
		m_SquadDetailText = AddText(botCol, LAYOUT_TEXT, "Left-click a squad or base to select. Right-click ground to order.");

		m_UnitActionsRow = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, botCol);
		AlignableSlot.SetPadding(m_UnitActionsRow, 0, 6, 0, 6);
		m_DefendBtn = AddButton(m_UnitActionsRow, "Defend", 2, true);
		m_GarrisonBtn = AddButton(m_UnitActionsRow, "Garrison", 3, true);
		m_GetInBtn = AddButton(m_UnitActionsRow, "Get In", 4, true);
		m_SwitchBtn = AddButton(m_UnitActionsRow, "Take Over", 5, true);
		m_HaltBtn = AddButton(m_UnitActionsRow, "Halt", 6, true);
		m_RoadblockBtn = AddButton(m_UnitActionsRow, "Roadblock", 7, true);

		AddText(botCol, LAYOUT_TEXT, "[WASD] Pan  |  [RMB Drag] Rotate  |  [Scroll] Zoom  |  [LMB] Select  |  [RMB Click] Order  |  [Shift+RMB] Queue  |  [F10/Esc] Exit");

		// Toast notification label
		m_ToastText = AddText(m_RootWidget, LAYOUT_HEADING, "");
		FrameSlot.SetAnchorMin(m_ToastText, 0.5, 0.8);
		FrameSlot.SetAnchorMax(m_ToastText, 0.5, 0.8);
		FrameSlot.SetAlignment(m_ToastText, 0.5, 0.5);
		m_ToastText.SetColor(new Color(0.9, 0.85, 0.3, 1.0));
		m_ToastText.SetVisible(false);

		WidgetManager.SetCursor(0);
	}

	protected TextWidget AddText(Widget parent, ResourceName layout, string text)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		TextWidget tw = TextWidget.Cast(ws.CreateWidgets(layout, parent));
		if (tw) tw.SetText(text);
		return tw;
	}

	protected SCR_ButtonTextComponent AddButton(Widget parent, string text, int action, bool small = false)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		Widget root = ws.CreateWidgets(LAYOUT_BUTTON, parent);
		SCR_ButtonTextComponent btn = SCR_ButtonTextComponent.Cast(root.FindHandler(SCR_ButtonTextComponent));
		if (btn)
		{
			btn.SetText(text);
			btn.m_OnClicked.Insert(OnButtonClicked);
			m_Actions.Insert(btn, action);
		}
		if (small)
		{
			SizeLayoutWidget size = SizeLayoutWidget.Cast(root);
			if (size)
			{
				size.EnableWidthOverride(true);
				size.SetWidthOverride(130);
			}
		}
		return btn;
	}

	protected void OnButtonClicked(SCR_ButtonBaseComponent btn)
	{
		if (!m_Actions.Contains(btn)) return;
		int action = m_Actions.Get(btn);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());

		switch (action)
		{
			case 1: // Exit
				Close();
				return;
			case 2: // Defend toggle
				m_iPointCommand = 0;
				m_bDefendMode = !m_bDefendMode;
				if (m_bDefendMode)
				{
					m_bRoadblockMode = false;
					if (m_RoadblockBtn) m_RoadblockBtn.SetText("Roadblock");
				}
				if (m_DefendBtn)
				{
					if (m_bDefendMode) m_DefendBtn.SetText("Defend [ON]");
					else m_DefendBtn.SetText("Defend");
				}
				if (m_bDefendMode) ShowToast("Defend mode armed: Right-click ground to place defence");
				else ShowToast("Defend mode off");
				return;
   case 3: // Garrison: choose the building in the scene.
    m_iPointCommand = ACR_ERequest.ORDER_GARRISON;
    m_bDefendMode = false;
    m_bRoadblockMode = false;
    ShowToast("Right-click a building to garrison");
    return;
   case 4: // Get in: choose the vehicle in the scene.
    m_iPointCommand = ACR_ERequest.ORDER_GET_IN;
    m_bDefendMode = false;
    m_bRoadblockMode = false;
    ShowToast("Right-click a vehicle to board");
    return;
			case 5: // Switch / Possess
				if (pc && pc.ACR_GetSelectedGroup().IsValid())
				{
					pc.ACR_Request(ACR_ERequest.SWITCH_TO_GROUP, pc.ACR_GetSelectedGroup());
					Close();
				}
				return;
			case 6: // Halt
				if (pc && pc.ACR_GetSelectedGroup().IsValid())
				{
					pc.ACR_Request(ACR_ERequest.ORDER_HALT, pc.ACR_GetSelectedGroup());
					ShowToast("Halt order dispatched");
				}
				return;
			case 7: // Roadblock toggle
				m_iPointCommand = 0;
				m_bRoadblockMode = !m_bRoadblockMode;
				if (m_bRoadblockMode)
				{
					m_bDefendMode = false;
					if (m_DefendBtn) m_DefendBtn.SetText("Defend");
				}
				if (m_RoadblockBtn)
				{
					if (m_bRoadblockMode) m_RoadblockBtn.SetText("Roadblock [ON]");
					else m_RoadblockBtn.SetText("Roadblock");
				}
				if (m_bRoadblockMode) ShowToast("Roadblock mode armed: Right-click road to construct (20 RP)");
				else ShowToast("Roadblock mode off");
				return;
		}
	}

	// ---------------------------------------------------------------- screen raycasting
	static bool ScreenToWorld(int mouseX, int mouseY, out vector worldPos, out IEntity hitEntity)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!ws || !world) return false;

		vector outDir;
		vector startPos = ws.ProjScreenToWorld(ws.DPIUnscale(mouseX), ws.DPIUnscale(mouseY), outDir, world, -1);
		outDir *= 1600.0;

		TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = startPos + outDir;
		trace.Flags = TraceFlags.WORLD | TraceFlags.OCEAN | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;

		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1.0) return false;

		worldPos = startPos + outDir * fraction;
		hitEntity = trace.TraceEnt;
		return true;
	}

	// ---------------------------------------------------------------- click handlers
	void HandleLeftClick(int x, int y)
	{
		vector hitPos;
		IEntity hitEnt;
		if (!ScreenToWorld(x, y, hitPos, hitEnt)) return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data) return;

		// 0. Markers first: what the player sees is what they pick.
		WorkspaceWidget ws = GetGame().GetWorkspace();
		int markerSq = NearestMarker(m_SquadMarkerPos, ws.DPIUnscale(x), ws.DPIUnscale(y));
		if (markerSq >= 0 && markerSq < data.m_GroupIds.Count())
		{
			pc.ACR_SelectGroup(data.m_GroupIds[markerSq]);
			string markerName = "Squad";
			if (markerSq < data.m_GroupNames.Count()) markerName = data.m_GroupNames[markerSq];
			ShowToast("Selected: " + markerName);
			return;
		}
		int markerBase = NearestMarker(m_BaseMarkerPos, ws.DPIUnscale(x), ws.DPIUnscale(y));
		if (markerBase >= 0 && markerBase < data.m_BaseNames.Count())
		{
			pc.ACR_SelectBase(data.m_BaseNames[markerBase]);
			ShowToast("Selected Base: " + data.m_BaseNames[markerBase]);
			return;
		}

		// 1. Select squad near hit
		int bestSq = -1;
		float bestSqDist = 20.0;
		for (int s = 0; s < data.m_LeaderPositions.Count(); s++)
		{
			float d = vector.DistanceXZ(hitPos, data.LeaderPos(s));
			if (d < bestSqDist)
			{
				bestSqDist = d;
				bestSq = s;
			}
		}

		if (bestSq >= 0)
		{
			pc.ACR_SelectGroup(data.m_GroupIds[bestSq]);
			string sqName = "Squad";
			if (bestSq < data.m_GroupNames.Count()) sqName = data.m_GroupNames[bestSq];
			ShowToast("Selected: " + sqName);
			return;
		}

		// 2. Select base near hit
		int bestBase = -1;
		float bestBaseDist = 45.0;
		for (int b = 0; b < data.m_BasePositions.Count(); b++)
		{
			float rad = 35.0;
			if (b < data.m_BaseRadii.Count()) rad = Math.Max(rad, data.m_BaseRadii[b]);
			float d = vector.DistanceXZ(hitPos, data.m_BasePositions[b]);
			if (d < rad && d < bestBaseDist)
			{
				bestBaseDist = d;
				bestBase = b;
			}
		}

		if (bestBase >= 0)
		{
			pc.ACR_SelectBase(data.m_BaseNames[bestBase]);
			ShowToast("Selected Base: " + data.m_BaseNames[bestBase]);
			return;
		}

		// 3. Clicked empty terrain -> clear selection
		pc.ACR_SelectBase("");
		pc.ACR_SelectGroup(RplId.Invalid());
	}

	void HandleRightClick(int x, int y)
	{
		vector hitPos;
		IEntity hitEnt;
		if (!ScreenToWorld(x, y, hitPos, hitEnt)) return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data || data.m_Selected.IsEmpty())
		{
			ShowToast("Select a friendly squad first [Left-Click]");
			return;
		}

		bool append = pc.ACR_ModifierHeld("ACR_MapShift");
		int command = ACR_ERequest.MOVE_TO;
		string actionName = "Move";

		if (m_bRoadblockMode)
		{
			command = ACR_ERequest.ORDER_ROADBLOCK;
			actionName = "Roadblock";
		}
		else if (m_bDefendMode)
		{
			command = ACR_ERequest.ORDER_DEFEND;
			actionName = "Defend";
		}
  else if (m_iPointCommand != 0)
  {
   command = m_iPointCommand;
   actionName = "Garrison";
   if (command == ACR_ERequest.ORDER_GET_IN) actionName = "Get In";
  }
		else if (hitEnt && Vehicle.Cast(hitEnt))
		{
			command = ACR_ERequest.ORDER_GET_IN;
			actionName = "Get In";
		}
		else if (hitEnt && SCR_DestructibleBuildingEntity.Cast(hitEnt))
		{
			command = ACR_ERequest.ORDER_GARRISON;
			actionName = "Garrison";
		}

		if (append)
		{
			if (command == ACR_ERequest.MOVE_TO) command = ACR_ERequest.QUEUE_MOVE;
			else if (command == ACR_ERequest.ORDER_DEFEND) command = ACR_ERequest.QUEUE_DEFEND;
			else if (command == ACR_ERequest.ORDER_GARRISON) command = ACR_ERequest.QUEUE_GARRISON;
			actionName = "Queued " + actionName;
		}

		if (pc.ACR_IssueOrder(command, hitPos, append))
		{
			m_iPointCommand = 0;
			ShowToast(actionName + " order requested");
		}
	}

 void BeginOrbit(int x, int y)
 {
  m_bOrbit = true;
  m_bDragged = false;
  m_iMouseX = x;
  m_iMouseY = y;
  m_iDownX = x;
  m_iDownY = y;
 }

 void EndOrbit(int x, int y, bool onScene)
 {
  if (!m_bOrbit) return;
  if (Math.AbsInt(x - m_iDownX) >= 8 || Math.AbsInt(y - m_iDownY) >= 8) m_bDragged = true;
  m_bOrbit = false;
  if (!m_bDragged && onScene) HandleRightClick(x, y);
 }

 protected void ApplyOrbit(int x, int y)
 {
  if (Math.AbsInt(x - m_iDownX) >= 8 || Math.AbsInt(y - m_iDownY) >= 8) m_bDragged = true;
  if (m_bDragged)
  {
   vector angles = m_Camera.GetYawPitchRoll();
   angles[0] = angles[0] + (x - m_iMouseX) * 0.2;
   angles[1] = Math.Clamp(angles[1] - (y - m_iMouseY) * 0.2, -85, -15);
   m_Camera.SetYawPitchRoll(angles);
  }
  m_iMouseX = x;
  m_iMouseY = y;
 }

 protected void ApplyInput(InputManager input)
 {
  if (m_bOrbit)
  {
   int mouseX, mouseY;
   WidgetManager.GetMousePos(mouseX, mouseY);
   ApplyOrbit(mouseX, mouseY);
  }
  vector angles = m_Camera.GetYawPitchRoll();
  vector heading[4];
  Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), heading);
  vector movement = heading[2] * (input.GetActionValue("ACR_RTSPanForward") - input.GetActionValue("ACR_RTSPanBack"));
  movement = movement + heading[0] * (input.GetActionValue("ACR_RTSPanRight") - input.GetActionValue("ACR_RTSPanLeft"));
  if (movement.LengthSq() > 1) movement.Normalize();
  float speed = 35;
  if (input.GetActionValue("ACR_MapShift") > 0.5) speed = 100;
  vector pos = m_Camera.GetOrigin();
  BaseWorld world = GetGame().GetWorld();
  float height = pos[1] - world.GetSurfaceY(pos[0], pos[2]);
  pos = pos + movement * speed * Math.Min(input.GetLastTimeDelta(), 0.1);
  if (WidgetManager.GetWidgetUnderCursor() == m_InteractionLayer)
   height = height - input.GetActionValue("MouseWheel") * 8;
  pos[1] = world.GetSurfaceY(pos[0], pos[2]) + Math.Clamp(height, 12, 160);
  m_Camera.SetOrigin(pos);
 }

	// ---------------------------------------------------------------- frame updates
	protected void FrameUpdate(SCR_PlayerController pc)
	{
		if (!m_Camera) return;

		IEntity localBody = pc.GetControlledEntity();
		if (!localBody || localBody != m_Body)
		{
			Close();
			return;
		}

  ChimeraCharacter character = ChimeraCharacter.Cast(localBody);
  if (character && character.GetCharacterController().IsUnconscious()) { Close(); return; }
  ACR_MapHandoff.SetMenuControls(localBody, true);
  ACR_MapHandoff.ClearCharacterInput(localBody);

		// 1. Vitality check: commander damage/death transitions back immediately
		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(localBody.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmg)
		{
			if (dmg.GetState() == EDamageState.DESTROYED || dmg.GetHealth() <= 0)
			{
				Close();
				return;
			}
			if (dmg.GetHealth() < m_fLastHealth - 0.5)
			{
				m_fLastHealth = dmg.GetHealth();
				ShowWarning("WARNING: Commander body is taking damage!");
			}
		}

  // Menus own their cursor; discard a drag interrupted by a dialog.
  if (GetGame().GetMenuManager().IsAnyMenuOpen() || GetGame().GetMenuManager().IsAnyDialogOpen())
  {
   m_bOrbit = false;
   return;
  }

		// 2. Escape or MenuBack exits camera
		InputManager input = GetGame().GetInputManager();
		m_bContextActivated = input.ActivateContext("ACR_RTSContext");
		if (input.GetActionTriggered("MenuBack") || input.GetActionTriggered("ACR_RTSCamera"))
		{
			Close();
			return;
		}

		ApplyInput(input);
		vector trans[4];
		m_Camera.GetWorldTransform(trans);
		vector camPos = trans[3];
		BaseWorld world = GetGame().GetWorld();
		float surfaceY = world.GetSurfaceY(camPos[0], camPos[2]);

		// 3. Altitude clamp: [12m, 160m] ATL
		float atl = camPos[1] - surfaceY;
		bool modifiedPos = false;
		if (atl < 12.0)
		{
			camPos[1] = surfaceY + 12.0;
			modifiedPos = true;
		}
		else if (atl > 160.0)
		{
			camPos[1] = surfaceY + 160.0;
			modifiedPos = true;
		}

		// 4. 300m Proximity Leash
		if (!s_bAdminUnbound)
		{
			float minDisSq = vector.DistanceSqXZ(camPos, localBody.GetOrigin());
			vector closestAnchor = localBody.GetOrigin();

			ACR_MapData data = pc.ACR_GetData();
			if (data)
			{
				for (int i = 0; i < data.m_LeaderPositions.Count(); i++)
				{
					float d = vector.DistanceSqXZ(camPos, data.LeaderPos(i));
					if (d < minDisSq)
					{
						minDisSq = d;
						closestAnchor = data.LeaderPos(i);
					}
				}
				Faction myFaction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
				for (int b = 0; b < data.m_BasePositions.Count(); b++)
				{
					if (myFaction && b < data.m_BaseOwners.Count() && data.m_BaseOwners[b] == myFaction.GetFactionKey())
					{
						float d = vector.DistanceSqXZ(camPos, data.m_BasePositions[b]);
						if (d < minDisSq)
						{
							minDisSq = d;
							closestAnchor = data.m_BasePositions[b];
						}
					}
				}
			}

			float curDis = Math.Sqrt(minDisSq);
			if (curDis > 300.0)
			{
				vector dir = camPos - closestAnchor;
				dir[1] = 0;
				dir.Normalize();
				camPos[0] = closestAnchor[0] + dir[0] * 300.0;
				camPos[2] = closestAnchor[2] + dir[2] * 300.0;
				camPos[1] = world.GetSurfaceY(camPos[0], camPos[2]) + Math.Clamp(atl, 12, 160);
				curDis = 300.0;
				modifiedPos = true;
				ShowWarning("LEASH LIMIT: Tethered within 300m of friendly forces");
			}
			UpdateLeashUI(curDis);
		}
		else
		{
			UpdateLeashUI(-1);
		}

		if (modifiedPos)
		{
			trans[3] = camPos;
			m_Camera.SetWorldTransform(trans);
		}

		UpdateTacticalUI(pc);
		UpdateMarkers(pc);

		// Toast timeout
		if (m_ToastText && m_ToastText.IsVisible() && System.GetTickCount() > m_fToastUntil)
		{
			m_ToastText.SetVisible(false);
		}

		// Clear warning after 4s
		if (m_WarningText && !m_WarningText.GetText().IsEmpty() && (System.GetTickCount() - m_fLastWarningAt > 4000))
		{
			m_WarningText.SetText("");
		}
	}

	static string OrderKindName(int kind)
	{
		switch (kind)
		{
			case ACR_EOrderKind.MOVE: return "Moving";
			case ACR_EOrderKind.DEFEND: return "Defending";
			case ACR_EOrderKind.GET_IN: return "Mounting Vehicle";
			case ACR_EOrderKind.GET_OUT: return "Dismounting";
			case ACR_EOrderKind.HUNT: return "Hunting Contact";
		}
		return "Moving";
	}

	protected void UpdateLeashUI(float curDis)
	{
		if (!m_LeashText) return;
		if (curDis < 0)
		{
			m_LeashText.SetText("LEASH: UNBOUND (Admin Mode)");
			m_LeashText.SetColor(new Color(0.3, 0.8, 1.0, 1.0));
			return;
		}
		string text = string.Format("LEASH: %1m / 300m  [CONNECTED]", Math.Round(curDis));
		m_LeashText.SetText(text);
		if (curDis > 270.0) m_LeashText.SetColor(new Color(0.95, 0.4, 0.2, 1.0));
		else m_LeashText.SetColor(new Color(0.4, 0.9, 0.4, 1.0));
	}

	protected void UpdateTacticalUI(SCR_PlayerController pc)
	{
  if (m_DefendBtn)
  {
   if (m_bDefendMode) m_DefendBtn.SetText("Defend [ON]");
   else m_DefendBtn.SetText("Defend");
  }
  if (m_RoadblockBtn)
  {
   if (m_bRoadblockMode) m_RoadblockBtn.SetText("Roadblock [ON]");
   else m_RoadblockBtn.SetText("Roadblock");
  }
  if (m_GarrisonBtn)
  {
   if (m_iPointCommand == ACR_ERequest.ORDER_GARRISON) m_GarrisonBtn.SetText("Garrison [ON]");
   else m_GarrisonBtn.SetText("Garrison");
  }
  if (m_GetInBtn)
  {
   if (m_iPointCommand == ACR_ERequest.ORDER_GET_IN) m_GetInBtn.SetText("Get In [ON]");
   else m_GetInBtn.SetText("Get In");
  }
		if (!m_SquadText) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data) return;

		RplId selSquad = pc.ACR_GetSelectedGroup();
		if (selSquad.IsValid())
		{
			int sqIdx = data.m_GroupIds.Find(selSquad);
			string name = "Selected Squad";
			if (sqIdx >= 0 && sqIdx < data.m_GroupNames.Count()) name = data.m_GroupNames[sqIdx];
			m_SquadText.SetText(name);

			string detail = "Standing by";
			int ordIdx = data.m_OrderGroups.Find(selSquad);
			if (ordIdx >= 0 && ordIdx < data.m_OrderKinds.Count())
			{
				detail = "Order: " + OrderKindName(data.m_OrderKinds[ordIdx]);
			}
			m_SquadDetailText.SetText(detail + " | Right-click ground to order");
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(true);
		}
		else if (!pc.ACR_SelectedBase().IsEmpty())
		{
			string bName = pc.ACR_SelectedBase();
			int bIdx = data.m_BaseNames.Find(bName);
			string detail = "Base";
			if (bIdx >= 0)
			{
				string owner = data.m_BaseOwners[bIdx];
				int sup = 0;
				if (bIdx < data.m_BaseSupply.Count()) sup = data.m_BaseSupply[bIdx];
				detail = string.Format("Owner: %1 | Supplies: %2", owner, sup);
			}
			m_SquadText.SetText(bName);
			m_SquadDetailText.SetText(detail);
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(false);
		}
		else
		{
			m_SquadText.SetText("Tactical Overview");
			m_SquadDetailText.SetText("Left-click squad or base to select | Right-click ground to order");
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(false);
		}
	}

	// ---------------------------------------------------------------- world markers
	protected static int NearestMarker(array<vector> positions, float x, float y)
	{
		int best = -1;
		float bestSq = MARKER_PICK_RADIUS * MARKER_PICK_RADIUS;
		for (int i = 0; i < positions.Count(); i++)
		{
			vector p = positions[i];
			if (p[2] <= 0) continue;
			float dx = p[0] - x;
			float dy = p[1] - y;
			float d = dx * dx + dy * dy;
			if (d < bestSq)
			{
				bestSq = d;
				best = i;
			}
		}
		return best;
	}

	protected Widget CreateMarker()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		Widget root = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_MarkerLayer);
		FrameSlot.SetSize(root, 0, 0);
		Widget pin = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, root);
		pin.SetName("Pin");
		TextWidget label = TextWidget.Cast(ws.CreateWidgets(LAYOUT_TEXT, root));
		if (label)
		{
			label.SetName("Label");
			label.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetAlignment(label, 0.5, 1);
			FrameSlot.SetPos(label, 0, -12);
			FrameSlot.SetSizeToContent(label, true);
			label.SetOutline(1);
		}
		return root;
	}

	// Places (or hides) one marker; returns its screen position, z <= 0 when hidden.
	protected vector PlaceMarker(array<Widget> pool, int index, vector worldPos, string text, Color color, bool selected)
	{
		while (pool.Count() <= index) pool.Insert(CreateMarker());
		Widget root = pool[index];
		WorkspaceWidget ws = GetGame().GetWorkspace();
		vector screen = ws.ProjWorldToScreen(worldPos, GetGame().GetWorld());
		root.SetVisible(screen[2] > 0);
		if (screen[2] <= 0) return screen;
		FrameSlot.SetPos(root, screen[0], screen[1]);
		float size = 12;
		if (selected) size = 18;
		Widget pin = root.FindAnyWidget("Pin");
		if (pin)
		{
			FrameSlot.SetPos(pin, -size * 0.5, -size * 0.5);
			FrameSlot.SetSize(pin, size, size);
			pin.SetColor(color);
		}
		TextWidget label = TextWidget.Cast(root.FindAnyWidget("Label"));
		if (label)
		{
			label.SetText(text);
			label.SetColor(color);
		}
		return screen;
	}

	protected void UpdateMarkers(SCR_PlayerController pc)
	{
		if (!m_MarkerLayer) return;
		m_SquadMarkerPos.Clear();
		m_BaseMarkerPos.Clear();
		ACR_MapData data = pc.ACR_GetData();
		int squads = 0;
		int bases = 0;
		if (data)
		{
			vector eye = m_Camera.GetOrigin();
			Color selectedColor = new Color(1.0, 0.85, 0.2, 1.0);
			Color commandColor = new Color(0.35, 0.65, 1.0, 1.0);
			Color playerColor = new Color(0.45, 0.9, 0.45, 1.0);
			squads = Math.Min(data.m_LeaderPositions.Count(), data.m_GroupIds.Count());
			for (int i = 0; i < squads; i++)
			{
				vector pos = data.LeaderPos(i);
				bool selected = data.IsSelected(data.m_GroupIds[i]);
				Color color = playerColor;
				if (i < data.m_Commandable.Count() && data.m_Commandable[i] == 1) color = commandColor;
				if (selected) color = selectedColor;
				string name = "Squad";
				if (i < data.m_GroupNames.Count()) name = data.m_GroupNames[i];
				string text = string.Format("%1  %2m", name, Math.Round(vector.Distance(eye, pos)));
				m_SquadMarkerPos.Insert(PlaceMarker(m_SquadMarkers, i, pos + Vector(0, 2.5, 0), text, color, selected));
			}

			Faction myFaction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
			string selectedBase = pc.ACR_SelectedBase();
			bases = data.m_BasePositions.Count();
			for (int b = 0; b < bases; b++)
			{
				string baseName = "";
				if (b < data.m_BaseNames.Count()) baseName = data.m_BaseNames[b];
				Color baseColor = new Color(0.75, 0.75, 0.75, 1.0);
				if (b < data.m_BaseOwners.Count() && !data.m_BaseOwners[b].IsEmpty())
				{
					if (myFaction && data.m_BaseOwners[b] == myFaction.GetFactionKey()) baseColor = commandColor;
					else baseColor = new Color(0.95, 0.3, 0.25, 1.0);
				}
				bool baseSelected = !baseName.IsEmpty() && baseName == selectedBase;
				if (baseSelected) baseColor = selectedColor;
				m_BaseMarkerPos.Insert(PlaceMarker(m_BaseMarkers, b, data.m_BasePositions[b] + Vector(0, 6, 0), baseName, baseColor, baseSelected));
			}
		}
		for (int h = squads; h < m_SquadMarkers.Count(); h++) m_SquadMarkers[h].SetVisible(false);
		for (int k = bases; k < m_BaseMarkers.Count(); k++) m_BaseMarkers[k].SetVisible(false);
	}

	void ShowToast(string message)
	{
		if (!m_ToastText) return;
		m_ToastText.SetText(message);
		m_ToastText.SetVisible(true);
		m_fToastUntil = System.GetTickCount() + 2500;
	}

	void ShowWarning(string warning)
	{
		if (!m_WarningText) return;
		m_WarningText.SetText(warning);
		m_fLastWarningAt = System.GetTickCount();
	}

	// ---------------------------------------------------------------- cleanup
	protected void Cleanup()
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera() == m_Camera)
		{
			cameras.SetCamera(m_PreviousCamera);
		}
		if (m_Camera)
		{
   SCR_ManualCamera manual = SCR_ManualCamera.Cast(m_Camera);
   if (manual)
   {
    manual.Terminate();
    Widget cameraWidget = manual.GetWidget();
    if (cameraWidget) cameraWidget.RemoveFromHierarchy();
   }
			delete m_Camera;
			m_Camera = null;
		}

		if (m_RootWidget)
		{
			ACR_Hud.Remove(m_RootWidget);
			m_RootWidget = null;
		}
		m_MarkerLayer = null;
		m_SquadMarkers.Clear();
		m_BaseMarkers.Clear();
		m_SquadMarkerPos.Clear();
		m_BaseMarkerPos.Clear();

  ACR_MapHandoff.ClearCharacterInput(m_Body);
  ACR_MapHandoff.SetMenuControls(m_Body, false);
  m_Body = null;
  m_Handler = null;
  m_InteractionLayer = null;
  m_bOrbit = false;
 }
}
