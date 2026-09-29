// End the old soldier's physical map use without closing the RTS map window.
// Native map ModeClear skips local focus cleanup once possession has changed.
modded class SCR_MapGadgetComponent
{
 void ACR_EndMapUse()
 {
  m_bFocused = false;
  m_bIsMapOpen = false;
  GetGame().GetCallqueue().Remove(ToggleMapGadget);
  SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
  SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
 }
}
class ACR_MapHandoff
{
 static void SetMenuControls(IEntity entity, bool disabled)
 {
  ChimeraCharacter body = ChimeraCharacter.Cast(entity);
  if (!body) return;
  CharacterControllerComponent controller = body.GetCharacterController();
  controller.SetDisableViewControls(disabled);
  controller.SetDisableWeaponControls(disabled);
  controller.SetDisableMovementControls(disabled);
 }
 static void Release(SCR_PlayerController pc, IEntity entity, bool releaseControls = true)
 {
  ChimeraCharacter body = ChimeraCharacter.Cast(entity);
  if (!body) return;
  CharacterControllerComponent controller = body.GetCharacterController();
  SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(body);
  if (gadgets)
  {
   IEntity mapItem = gadgets.GetGadgetByType(EGadgetType.MAP);
   if (mapItem)
   {
    SCR_MapGadgetComponent mapComponent = SCR_MapGadgetComponent.Cast(mapItem.FindComponent(SCR_MapGadgetComponent));
    if (mapComponent) mapComponent.ACR_EndMapUse();
    CharacterInputContext input = controller.GetInputContext();
    if (controller.GetAttachedGadgetAtLeftHandSlot() == mapItem || (input && input.GetWantedLefHandGadgetEntity() == mapItem))
     controller.RemoveGadgetFromHand(true);
    // Removal can schedule map callbacks through native gadget events.
    if (mapComponent) mapComponent.ACR_EndMapUse();
   }
  }
  if (releaseControls) SetMenuControls(body, false);
  pc.SetGadgetFocus(false);
  controller.ResetPersistentStates();
  controller.SetForcedFreeLook(false);
  controller.SetFreeLook(false, false, false);
  CharacterInputContext context = controller.GetInputContext();
  if (context) context.SetFreelook(false);
  ACR_TestRange.ACR_ClearCharacterInput(body);
 }
}

