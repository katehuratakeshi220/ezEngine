#include <EditorFramework/EditorFrameworkPCH.h>

#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorFramework/Preferences/ProjectPreferences.h>
#include <Foundation/Profiling/Profiling.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezProjectPreferencesUser, 1, ezRTTIDefaultAllocator<ezProjectPreferencesUser>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ARRAY_MEMBER_PROPERTY("Players", m_PlayerApps)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezProjectPreferencesUser::ezProjectPreferencesUser()
  : ezPreferences(Domain::Project, "General")
{
}


void ezQtEditorApp::LoadProjectPreferences()
{
  EZ_PROFILE_SCOPE("LoadProjectPreferences");
  ezPreferences::QueryPreferences<ezProjectPreferencesUser>();
}
