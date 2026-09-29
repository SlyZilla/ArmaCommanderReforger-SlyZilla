[WorkbenchPluginAttribute(name: "Run ACR range", wbModules: {"ResourceManager"})]
class ACR_RunRangePlugin : WorkbenchPlugin
{
 override void RunCommandline()
 {
  Workbench.OpenModule(WorldEditor);
  WorldEditor editor = Workbench.GetModule(WorldEditor);
  bool opened = editor.SetOpenedResource("Worlds/ACR_TestRange.ent");
  PrintFormat("[ACR] World opened: %1", opened);
  if (opened)
  {
   Sleep(1000);
   editor.SwitchToGameMode(false, true);
   Sleep(95000);
   editor.SwitchToEditMode();
  }
  Workbench.Exit(0);
 }
}


