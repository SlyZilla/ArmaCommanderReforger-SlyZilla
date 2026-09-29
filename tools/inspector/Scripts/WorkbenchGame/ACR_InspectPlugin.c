// Headless Workbench plugin used by the test launcher's "inspect" preset.
// Writes a listing of all game resources to $profile:files.txt and copies each
// path named in $profile:requests.txt to $profile:export_<n>.txt, so Claude can
// read vanilla scripts, prefabs and layouts it needs to build against.
[WorkbenchPluginAttribute(name: "ACR Inspect", wbModules: {"ResourceManager"})]
class ACR_InspectPlugin : WorkbenchPlugin
{
 ref FileHandle m_Listing;
 void Found(ResourceName resourceName, string filePath)
 {
  m_Listing.WriteLine(resourceName + "|" + filePath);
 }
 override void RunCommandline()
 {
  m_Listing = FileIO.OpenFile("$profile:files.txt", FileMode.WRITE);
  Workbench.SearchResources(Found, {"et", "ent", "conf", "c", "layout", "imageset", "styles"});
  m_Listing.Close();
  FileHandle requests = FileIO.OpenFile("$profile:requests.txt", FileMode.READ);
  if (requests)
  {
   string line;
   int i = 0;
   while (requests.ReadLine(line) >= 0)
   {
    if (line.IsEmpty()) continue;
    bool ok = FileIO.CopyFile(line, string.Format("$profile:export_%1.txt", i));
    PrintFormat("[ACR] Export %1 %2: %3", i, line, ok);
    i++;
   }
   requests.Close();
  }
  Workbench.Exit(0);
 }
}
