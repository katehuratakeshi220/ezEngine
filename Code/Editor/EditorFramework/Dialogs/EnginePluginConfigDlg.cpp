#include <EditorFramework/EditorFrameworkPCH.h>

#include <EditorFramework/Dialogs/EnginePluginConfigDlg.moc.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>

ezQtEnginePluginConfigDlg::ezQtEnginePluginConfigDlg(QWidget* parent)
  : QDialog(parent)
{
  setupUi(this);

  FillPluginList();
}

void ezQtEnginePluginConfigDlg::FillPluginList()
{
  ezPluginSet& Plugins = ezQtEditorApp::GetSingleton()->GetEnginePlugins();

  ListPlugins->blockSignals(true);

  for (auto it = Plugins.m_Plugins.GetIterator(); it.IsValid(); ++it)
  {
    QListWidgetItem* pItem = new QListWidgetItem();
    pItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsUserCheckable);

    ezStringBuilder sText = it.Key();

    if (sText.FindSubString_NoCase("EnginePlugin") != nullptr)
    {
      // hide engine plugins from the list, these are indirectly loaded by editor plugin dependencies
      // the user is not supposed to configure them
      continue;
    }

    if (it.Value().m_bLoadCopy)
    {
      sText.Append(" (load copy)");
    }

    if (!it.Value().m_bAvailable)
    {
      sText.Append(" (missing)");

      pItem->setBackground(Qt::red);
    }
    else if (it.Value().m_bActive)
    {
      sText.Append(" (active)");
    }

    pItem->setText(sText.GetData());
    pItem->setData(Qt::UserRole + 1, QString(it.Key().GetData()));
    pItem->setCheckState(it.Value().m_bToBeLoaded ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    ListPlugins->addItem(pItem);
  }

  ListPlugins->blockSignals(false);
}

void ezQtEnginePluginConfigDlg::on_ButtonOK_clicked()
{
  ezPluginSet& Plugins = ezQtEditorApp::GetSingleton()->GetEnginePlugins();

  for (int i = 0; i < ListPlugins->count(); ++i)
  {
    QListWidgetItem* pItem = ListPlugins->item(i);

    const bool bLoad = pItem->checkState() == Qt::CheckState::Checked;

    bool& ToBeLoaded = Plugins.m_Plugins[pItem->data(Qt::UserRole + 1).toString().toUtf8().data()].m_bToBeLoaded;

    ToBeLoaded = bLoad;
  }

  ezQtEditorApp::GetSingleton()->StoreEnginePluginsToBeLoaded();

  accept();
}

void ezQtEnginePluginConfigDlg::on_ButtonCancel_clicked()
{
  reject();
}
