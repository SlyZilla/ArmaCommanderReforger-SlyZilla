// Our actions in the game's Controls menu: an "AC: Actual" category (the
// world ping key ships unbound; players pick a key there).
modded class SCR_KeybindSetting
{
 override protected void InsertCategoriesToComboBox()
 {
  if (m_KeybindConfig && m_KeybindConfig.m_KeyBindingCategories)
  {
   bool present = false;
   foreach (SCR_KeyBindingCategory existing : m_KeybindConfig.m_KeyBindingCategories)
    if (existing && existing.m_sName == "ACR") present = true;
   if (!present)
   {
    SCR_KeyBindingCategory category = new SCR_KeyBindingCategory();
    category.m_sName = "ACR";
    category.m_sDisplayName = "AC: Actual";
    category.m_KeyBindingEntries = {};
    category.m_KeyBindingEntries.Insert(Entry("ACR_Ping", "Ping (tap: look, hold: pick)"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Map", "Command map"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Possess", "Take over soldier"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Return", "Command unit"));
    m_KeybindConfig.m_KeyBindingCategories.Insert(category);
   }
  }
  super.InsertCategoriesToComboBox();
 }

 protected SCR_KeyBindingEntry Entry(string action, string name)
 {
  SCR_KeyBindingEntry entry = new SCR_KeyBindingEntry();
  entry.m_sActionName = action;
  entry.m_sDisplayName = name;
  // Script-made entries skip the config defaults: the Controls menu reads the platform
  // list unchecked (a null one crashed it and hid our category).
  entry.m_aPlatforms = {};
  entry.m_ePrefixType = SCR_EActionPrefixType.NON_CONTINUOUS;
  return entry;
 }
}
