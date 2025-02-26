#include <Core/CorePCH.h>

#include <Core/GameState/GameStateWindow.h>

ezGameStateWindow::ezGameStateWindow(const ezWindowCreationDesc& windowdesc, ezDelegate<void()> onClickClose)
  : m_OnClickClose(onClickClose)
{
  m_CreationDescription = windowdesc;
  m_CreationDescription.AdjustWindowSizeAndPosition().IgnoreResult();

  Initialize().IgnoreResult();
}

ezGameStateWindow::~ezGameStateWindow()
{
  Destroy().IgnoreResult();
}


void ezGameStateWindow::ResetOnClickClose(ezDelegate<void()> onClickClose)
{
  m_OnClickClose = onClickClose;
}

void ezGameStateWindow::OnClickClose()
{
  if (m_OnClickClose.IsValid())
  {
    m_OnClickClose();
  }
}

void ezGameStateWindow::OnResize(const ezSizeU32& newWindowSize)
{
  ezLog::Info("Resolution changed to {0} * {1}", newWindowSize.width, newWindowSize.height);

  m_CreationDescription.m_Resolution = newWindowSize;
}



EZ_STATICLINK_FILE(Core, Core_GameState_Implementation_GameStateWindow);
