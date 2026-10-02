// Construction silhouettes (the ghost of a placed composition) stayed in the world:
// after closing build mode, without a shovel in hand, even after the structure was
// built (live 1 Oct, building from a possessed soldier). Vanilla DeletePreview gives
// up when the player's main entity isn't known on this machine, which is the case
// while possessing a soldier far from the command unit (it is streamed out, so the
// player controller's main entity is null on the client). Delete the preview anyway.
modded class SCR_CampaignBuildingLayoutComponent
{
 override void DeletePreview()
 {
  if (!m_PreviewEntity) return;
  IEntity main = SCR_PlayerController.GetLocalMainEntity();
  if (main && main.FindComponent(SCR_CharacterControllerComponent))
  {
   super.DeletePreview();
   return;
  }
  SCR_EditorModeEntity modeEntity = GetBuildingModeEntity();
  if (modeEntity) modeEntity.GetOnClosed().Remove(DeletePreview);
  IEntity root = m_PreviewEntity.GetRootParent();
  if (!root && GetOwner()) root = GetOwner().GetRootParent();
  if (root && root != m_PreviewEntity) root.RemoveChild(m_PreviewEntity);
  m_PreviewEntity.Update();
  SCR_EntityHelper.DeleteEntityAndChildren(m_PreviewEntity);
  m_PreviewEntity = null;
 }
}
