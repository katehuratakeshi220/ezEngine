#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorPluginAssets/AnimationControllerAsset/AnimationControllerAsset.h>
#include <EditorPluginAssets/AnimationControllerAsset/AnimationControllerGraphQt.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraph.h>
#include <ToolsFoundation/Serialization/DocumentObjectConverter.h>
#include <ToolsFoundation/Serialization/ToolsSerializationUtils.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationControllerAssetDocument, 3, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationControllerNodePin, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_SUBSYSTEM_DECLARATION(EditorPluginAssets, AnimationController)

  BEGIN_SUBSYSTEM_DEPENDENCIES
    "Core"
  END_SUBSYSTEM_DEPENDENCIES

  ON_CORESYSTEMS_STARTUP
  {
    //ezQtNodeScene::GetPinFactory().RegisterCreator(ezGetStaticRTTI<ezVisualScriptPin>(), [](const ezRTTI* pRtti)->ezQtPin* { return new ezQtVisualScriptPin(); }).IgnoreResult();
    //ezQtNodeScene::GetConnectionFactory().RegisterCreator(ezGetStaticRTTI<ezVisualScriptConnection>(), [](const ezRTTI* pRtti)->ezQtConnection* { return new ezQtVisualScriptConnection(); }).IgnoreResult();
    ezQtNodeScene::GetNodeFactory().RegisterCreator(ezGetStaticRTTI<ezAnimGraphNode>(), [](const ezRTTI* pRtti)->ezQtNode* { return new ezQtAnimationControllerNode(); }).IgnoreResult();
  }

EZ_END_SUBSYSTEM_DECLARATION;
// clang-format on

bool ezAnimationControllerNodeManager::InternalIsNode(const ezDocumentObject* pObject) const
{
  auto pType = pObject->GetTypeAccessor().GetType();
  return pType->IsDerivedFrom<ezAnimGraphNode>();
}

void ezAnimationControllerNodeManager::InternalCreatePins(const ezDocumentObject* pObject, NodeInternal& node)
{
  auto pType = pObject->GetTypeAccessor().GetType();
  if (!pType->IsDerivedFrom<ezAnimGraphNode>())
    return;

  ezHybridArray<ezAbstractProperty*, 32> properties;
  pType->GetAllProperties(properties);

  const ezColor triggerPinColor = ezColorGammaUB(0xa1, 0x12, 0x6c);
  const ezColor numberPinColor = ezColor::OliveDrab;
  const ezColor weightPinColor = ezColor::LightSeaGreen;
  const ezColor localPosePinColor = ezColor::SteelBlue;
  const ezColor modelPosePinColor = ezColorGammaUB(0x52, 0x46, 0xa0);
  // EXTEND THIS if a new type is introduced

  for (ezAbstractProperty* pProp : properties)
  {
    if (pProp->GetCategory() != ezPropertyCategory::Member || !pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphPin>())
      continue;

    if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphTriggerInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), triggerPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::Trigger;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphTriggerOutputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Output, pProp->GetPropertyName(), triggerPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::Trigger;
      node.m_Outputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphNumberInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), numberPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::Number;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphNumberOutputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Output, pProp->GetPropertyName(), numberPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::Number;
      node.m_Outputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphBoneWeightsInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), weightPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::BoneWeights;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphBoneWeightsOutputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Output, pProp->GetPropertyName(), weightPinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::BoneWeights;
      node.m_Outputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphLocalPoseInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), localPosePinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::LocalPose;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphLocalPoseMultiInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), localPosePinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::LocalPose;
      pPin->m_bMultiInputPin = true;
      pPin->m_Shape = ezPin::Shape::RoundRect;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphLocalPoseOutputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Output, pProp->GetPropertyName(), localPosePinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::LocalPose;
      node.m_Outputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphModelPoseInputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Input, pProp->GetPropertyName(), modelPosePinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::ModelPose;
      node.m_Inputs.PushBack(pPin);
    }
    else if (pProp->GetSpecificType()->IsDerivedFrom<ezAnimGraphModelPoseOutputPin>())
    {
      ezAnimationControllerNodePin* pPin = EZ_DEFAULT_NEW(ezAnimationControllerNodePin, ezPin::Type::Output, pProp->GetPropertyName(), modelPosePinColor, pObject);
      pPin->m_DataType = ezAnimGraphPin::ModelPose;
      node.m_Outputs.PushBack(pPin);
    }
    else
    {
      // EXTEND THIS if a new type is introduced
      EZ_ASSERT_NOT_IMPLEMENTED;
    }
  }
}

void ezAnimationControllerNodeManager::InternalDestroyPins(const ezDocumentObject* pObject, NodeInternal& node)
{
  for (ezPin* pPin : node.m_Inputs)
  {
    EZ_DEFAULT_DELETE(pPin);
  }
  node.m_Inputs.Clear();
  for (ezPin* pPin : node.m_Outputs)
  {
    EZ_DEFAULT_DELETE(pPin);
  }
  node.m_Outputs.Clear();
}


void ezAnimationControllerNodeManager::GetCreateableTypes(ezHybridArray<const ezRTTI*, 32>& Types) const
{
  ezSet<const ezRTTI*> typeSet;
  ezReflectionUtils::GatherTypesDerivedFromClass(ezGetStaticRTTI<ezAnimGraphNode>(), typeSet, false);

  Types.Clear();
  for (auto pType : typeSet)
  {
    if (pType->GetTypeFlags().IsAnySet(ezTypeFlags::Abstract))
      continue;

    Types.PushBack(pType);
  }
}

ezStatus ezAnimationControllerNodeManager::InternalCanConnect(const ezPin* pSource0, const ezPin* pTarget0, CanConnectResult& out_Result) const
{
  const ezAnimationControllerNodePin* pSourcePin = ezStaticCast<const ezAnimationControllerNodePin*>(pSource0);
  const ezAnimationControllerNodePin* pTargetPin = ezStaticCast<const ezAnimationControllerNodePin*>(pTarget0);

  out_Result = CanConnectResult::ConnectNever;

  if (pSourcePin->m_DataType != pTargetPin->m_DataType)
    return ezStatus("Can't connect pins of different data types");

  if (pSourcePin->GetType() == pTargetPin->GetType())
    return ezStatus("Can only connect input pins with output pins.");

  switch (pSourcePin->m_DataType)
  {
    case ezAnimGraphPin::Trigger:
      out_Result = CanConnectResult::ConnectNtoN;
      break;

    case ezAnimGraphPin::Number:
      out_Result = CanConnectResult::ConnectNto1;
      break;

    case ezAnimGraphPin::BoneWeights:
      out_Result = CanConnectResult::ConnectNto1;
      break;

    case ezAnimGraphPin::LocalPose:
      if (pTargetPin->m_bMultiInputPin)
        out_Result = CanConnectResult::ConnectNtoN;
      else
        out_Result = CanConnectResult::ConnectNto1;
      break;

    case ezAnimGraphPin::ModelPose:
      out_Result = CanConnectResult::ConnectNto1;
      break;

      // EXTEND THIS if a new type is introduced
      EZ_DEFAULT_CASE_NOT_IMPLEMENTED;
  }

  return ezStatus(EZ_SUCCESS);
}

ezAnimationControllerAssetDocument::ezAnimationControllerAssetDocument(const char* szDocumentPath)
  : ezAssetDocument(szDocumentPath, EZ_DEFAULT_NEW(ezAnimationControllerNodeManager), ezAssetDocEngineConnection::None)
{
}

ezStatus ezAnimationControllerAssetDocument::InternalTransformAsset(ezStreamWriter& stream, const char* szOutputTag, const ezPlatformProfile* pAssetProfile, const ezAssetFileHeader& AssetHeader, ezBitflags<ezTransformFlags> transformFlags)
{
  const auto* pNodeManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());

  ezDynamicArray<const ezDocumentObject*> allNodes;
  ezMap<ezUInt8, PinCount> pinCounts;
  CountPinTypes(pNodeManager, allNodes, pinCounts);

  // if the asset is entirely empty, don't complain
  if (allNodes.IsEmpty())
    return ezStatus(EZ_SUCCESS);

  SortNodesByPriority(allNodes);

  if (allNodes.IsEmpty())
  {
    return ezStatus("Animation controller graph doesn't have any output nodes.");
  }

  ezAnimGraph animController;
  animController.m_TriggerInputPinStates.SetCount(pinCounts[ezAnimGraphPin::Trigger].m_uiInputCount);
  animController.m_NumberInputPinStates.SetCount(pinCounts[ezAnimGraphPin::Number].m_uiInputCount);
  animController.m_BoneWeightInputPinStates.SetCount(pinCounts[ezAnimGraphPin::BoneWeights].m_uiInputCount);
  animController.m_LocalPoseInputPinStates.SetCount(pinCounts[ezAnimGraphPin::LocalPose].m_uiInputCount);
  animController.m_ModelPoseInputPinStates.SetCount(pinCounts[ezAnimGraphPin::ModelPose].m_uiInputCount);
  // EXTEND THIS if a new type is introduced

  for (ezUInt32 i = 0; i < ezAnimGraphPin::ENUM_COUNT; ++i)
  {
    animController.m_OutputPinToInputPinMapping[i].SetCount(pinCounts[i].m_uiOutputCount);
  }

  auto pIdxProperty = static_cast<ezAbstractMemberProperty*>(ezAnimGraphPin::GetStaticRTTI()->FindPropertyByName("PinIdx", false));
  EZ_ASSERT_DEBUG(pIdxProperty, "Missing PinIdx property");
  auto pNumProperty = static_cast<ezAbstractMemberProperty*>(ezAnimGraphPin::GetStaticRTTI()->FindPropertyByName("NumConnections", false));
  EZ_ASSERT_DEBUG(pNumProperty, "Missing NumConnections property");

  ezDynamicArray<ezAnimGraphNode*> newNodes;
  newNodes.Reserve(allNodes.GetCount());

  CreateOutputGraphNodes(allNodes, animController, newNodes);

  ezMap<const ezPin*, ezUInt16> inputPinIndices;
  SetInputPinIndices(newNodes, allNodes, pNodeManager, pinCounts, inputPinIndices, pIdxProperty, pNumProperty);
  SetOutputPinIndices(newNodes, allNodes, pNodeManager, pinCounts, animController, pIdxProperty, inputPinIndices);

  ezMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  EZ_SUCCEED_OR_RETURN(animController.Serialize(writer));

  stream << storage.GetStorageSize();
  return stream.WriteBytes(storage.GetData(), storage.GetStorageSize());
}

static void AssignNodePriority(const ezDocumentObject* pNode, ezUInt16 curPrio, ezMap<const ezDocumentObject*, ezUInt16>& prios, const ezDocumentNodeManager* pNodeManager)
{
  prios[pNode] = ezMath::Min(prios[pNode], curPrio);

  const auto inputPins = pNodeManager->GetInputPins(pNode);

  for (auto pPin : inputPins)
  {
    for (auto pConnection : pPin->GetConnections())
    {
      AssignNodePriority(pConnection->GetSourcePin()->GetParent(), curPrio - 1, prios, pNodeManager);
    }
  }
}

void ezAnimationControllerAssetDocument::SortNodesByPriority(ezDynamicArray<const ezDocumentObject*>& allNodes)
{
  // starts at output nodes (which have no output pins) and walks back recursively over the connections on their input nodes
  // until it reaches the end of the graph
  // assigns decreasing priorities to the nodes that it finds
  // thus it generates a weak order in which the nodes should be stepped at runtime

  const auto* pNodeManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());

  ezMap<const ezDocumentObject*, ezUInt16> prios;
  for (const ezDocumentObject* pNode : allNodes)
  {
    prios[pNode] = 0xFFFF;
  }

  for (const ezDocumentObject* pNode : allNodes)
  {
    // only look at the final nodes in the graph
    if (pNodeManager->GetOutputPins(pNode).IsEmpty())
    {
      AssignNodePriority(pNode, 0xFFFE, prios, pNodeManager);
    }
  }

  // remove unreachable nodes
  for (ezUInt32 i = allNodes.GetCount(); i > 0; --i)
  {
    if (prios[allNodes[i - 1]] == 0xFFFF)
    {
      allNodes.RemoveAtAndSwap(i - 1);
    }
  }

  allNodes.Sort([&](auto lhs, auto rhs) -> bool {
    return prios[lhs] < prios[rhs];
  });
}

void ezAnimationControllerAssetDocument::SetOutputPinIndices(const ezDynamicArray<ezAnimGraphNode*>& newNodes, const ezDynamicArray<const ezDocumentObject*>& allNodes, const ezDocumentNodeManager* pNodeManager, ezMap<ezUInt8, PinCount>& pinCounts, ezAnimGraph& animController, ezAbstractMemberProperty* pIdxProperty, const ezMap<const ezPin*, ezUInt16>& inputPinIndices) const
{
  // this function is generic and doesn't need to be extended for new types

  for (ezUInt32 nodeIdx = 0; nodeIdx < newNodes.GetCount(); ++nodeIdx)
  {
    const ezDocumentObject* pNode = allNodes[nodeIdx];
    auto* pNewNode = newNodes[nodeIdx];
    const auto outputPins = pNodeManager->GetOutputPins(pNode);

    for (auto pPin : outputPins)
    {
      const ezAnimationControllerNodePin* pCtrlPin = ezStaticCast<const ezAnimationControllerNodePin*>(pPin);

      if (pCtrlPin->GetConnections().IsEmpty())
        continue;

      const ezUInt8 pinType = pCtrlPin->m_DataType;

      const ezUInt32 idx = pinCounts[pinType].m_uiOutputIdx++;

      animController.m_OutputPinToInputPinMapping[pinType][idx].Reserve(pCtrlPin->GetConnections().GetCount());

      auto pPinProp = static_cast<ezAbstractMemberProperty*>(pNewNode->GetDynamicRTTI()->FindPropertyByName(pPin->GetName()));
      EZ_ASSERT_DEBUG(pPinProp, "Pin with name '{}' has no equally named property", pPin->GetName());

      // set the output index to use by this pin
      ezReflectionUtils::SetMemberPropertyValue(pIdxProperty, pPinProp->GetPropertyPointer(pNewNode), idx);

      // set output pin to input pin mapping

      for (const auto pCon : pCtrlPin->GetConnections())
      {
        const ezUInt16 uiTargetIdx = inputPinIndices.GetValueOrDefault(pCon->GetTargetPin(), 0xFFFF);

        if (uiTargetIdx != 0xFFFF)
        {
          animController.m_OutputPinToInputPinMapping[pinType][idx].PushBack(uiTargetIdx);
        }
      }
    }
  }
}

void ezAnimationControllerAssetDocument::SetInputPinIndices(const ezDynamicArray<ezAnimGraphNode*>& newNodes, const ezDynamicArray<const ezDocumentObject*>& allNodes, const ezDocumentNodeManager* pNodeManager, ezMap<ezUInt8, PinCount>& pinCounts, ezMap<const ezPin*, ezUInt16>& inputPinIndices, ezAbstractMemberProperty* pIdxProperty, ezAbstractMemberProperty* pNumProperty) const
{
  // this function is generic and doesn't need to be extended for new types

  for (ezUInt32 nodeIdx = 0; nodeIdx < newNodes.GetCount(); ++nodeIdx)
  {
    const ezDocumentObject* pNode = allNodes[nodeIdx];
    auto* pNewNode = newNodes[nodeIdx];

    const auto inputPins = pNodeManager->GetInputPins(pNode);

    for (auto pPin : inputPins)
    {
      const ezAnimationControllerNodePin* pCtrlPin = ezStaticCast<const ezAnimationControllerNodePin*>(pPin);

      if (pCtrlPin->GetConnections().IsEmpty())
        continue;

      const ezUInt16 idx = pinCounts[(ezUInt8)pCtrlPin->m_DataType].m_uiInputIdx++;
      inputPinIndices[pCtrlPin] = idx;

      auto pPinProp = static_cast<ezAbstractMemberProperty*>(pNewNode->GetDynamicRTTI()->FindPropertyByName(pPin->GetName()));
      EZ_ASSERT_DEBUG(pPinProp, "Pin with name '{}' has no equally named property", pPin->GetName());

      ezReflectionUtils::SetMemberPropertyValue(pIdxProperty, pPinProp->GetPropertyPointer(pNewNode), idx);
      ezReflectionUtils::SetMemberPropertyValue(pNumProperty, pPinProp->GetPropertyPointer(pNewNode), pCtrlPin->GetConnections().GetCount());
    }
  }
}

void ezAnimationControllerAssetDocument::CreateOutputGraphNodes(const ezDynamicArray<const ezDocumentObject*>& allNodes, ezAnimGraph& animController, ezDynamicArray<ezAnimGraphNode*>& newNodes) const
{
  // this function is generic and doesn't need to be extended for new types

  for (const ezDocumentObject* pNode : allNodes)
  {
    animController.m_Nodes.PushBack(pNode->GetType()->GetAllocator()->Allocate<ezAnimGraphNode>());
    newNodes.PushBack(animController.m_Nodes.PeekBack().Borrow());
    auto pNewNode = animController.m_Nodes.PeekBack().Borrow();

    // copy all the non-hidden properties
    ezToolsSerializationUtils::CopyProperties(pNode, GetObjectManager(), pNewNode, pNewNode->GetDynamicRTTI(), [](const ezAbstractProperty* p) {
      return p->GetAttributeByType<ezHiddenAttribute>() == nullptr;
    });
  }
}

void ezAnimationControllerAssetDocument::CountPinTypes(const ezDocumentNodeManager* pNodeManager, ezDynamicArray<const ezDocumentObject*>& allNodes, ezMap<ezUInt8, PinCount>& pinCounts) const
{
  // this function is generic and doesn't need to be extended for new types

  for (auto pNode : pNodeManager->GetRootObject()->GetChildren())
  {
    if (!pNodeManager->IsNode(pNode))
      continue;

    allNodes.PushBack(pNode);

    // input pins
    {
      const auto pins = pNodeManager->GetInputPins(pNode);

      for (auto pPin : pins)
      {
        const ezAnimationControllerNodePin* pCtrlPin = ezStaticCast<const ezAnimationControllerNodePin*>(pPin);

        if (pCtrlPin->GetConnections().IsEmpty())
          continue;

        pinCounts[(ezUInt8)pCtrlPin->m_DataType].m_uiInputCount++;
      }
    }

    // output pins
    {
      const auto pins = pNodeManager->GetOutputPins(pNode);

      for (auto pPin : pins)
      {
        const ezAnimationControllerNodePin* pCtrlPin = ezStaticCast<const ezAnimationControllerNodePin*>(pPin);

        if (pCtrlPin->GetConnections().IsEmpty())
          continue;

        pinCounts[(ezUInt8)pCtrlPin->m_DataType].m_uiOutputCount++;
      }
    }
  }
}

void ezAnimationControllerAssetDocument::InternalGetMetaDataHash(const ezDocumentObject* pObject, ezUInt64& inout_uiHash) const
{
  // without this, changing connections only (no property value) may not result in a different asset document hash and therefore no transform

   const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());
   if (pManager->IsNode(pObject))
  {
    auto outputs = pManager->GetOutputPins(pObject);
    for (const ezPin* pPinSource : outputs)
    {
      auto inputs = pPinSource->GetConnections();
      for (const ezConnection* pConnection : inputs)
      {
        const ezPin* pPinTarget = pConnection->GetTargetPin();

        inout_uiHash = ezHashingUtils::xxHash64(&pPinSource->GetParent()->GetGuid(), sizeof(ezUuid), inout_uiHash);
        inout_uiHash = ezHashingUtils::xxHash64(&pPinTarget->GetParent()->GetGuid(), sizeof(ezUuid), inout_uiHash);
        inout_uiHash = ezHashingUtils::xxHash64(pPinSource->GetName(), ezStringUtils::GetStringElementCount(pPinSource->GetName()), inout_uiHash);
        inout_uiHash = ezHashingUtils::xxHash64(pPinTarget->GetName(), ezStringUtils::GetStringElementCount(pPinTarget->GetName()), inout_uiHash);
      }
    }
  }
}

void ezAnimationControllerAssetDocument::AttachMetaDataBeforeSaving(ezAbstractObjectGraph& graph) const
{
  SUPER::AttachMetaDataBeforeSaving(graph);
  const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());
  pManager->AttachMetaDataBeforeSaving(graph);
}

void ezAnimationControllerAssetDocument::RestoreMetaDataAfterLoading(const ezAbstractObjectGraph& graph, bool bUndoable)
{
  SUPER::RestoreMetaDataAfterLoading(graph, bUndoable);
  ezDocumentNodeManager* pManager = static_cast<ezDocumentNodeManager*>(GetObjectManager());
  pManager->RestoreMetaDataAfterLoading(graph, bUndoable);
}



void ezAnimationControllerAssetDocument::GetSupportedMimeTypesForPasting(ezHybridArray<ezString, 4>& out_MimeTypes) const
{
  out_MimeTypes.PushBack("application/ezEditor.AnimationControllerGraph");
}

bool ezAnimationControllerAssetDocument::CopySelectedObjects(ezAbstractObjectGraph& out_objectGraph, ezStringBuilder& out_MimeType) const
{
  out_MimeType = "application/ezEditor.AnimationControllerGraph";

  const auto& selection = GetSelectionManager()->GetSelection();

  if (selection.IsEmpty())
    return false;

  const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());

  ezDocumentObjectConverterWriter writer(&out_objectGraph, pManager);

  for (const ezDocumentObject* pNode : selection)
  {
    // objects are required to be named root but this is not enforced or obvious by the interface.
    writer.AddObjectToGraph(pNode, "root");
  }

  pManager->AttachMetaDataBeforeSaving(out_objectGraph);

  return true;
}

bool ezAnimationControllerAssetDocument::Paste(const ezArrayPtr<PasteInfo>& info, const ezAbstractObjectGraph& objectGraph, bool bAllowPickedPosition, const char* szMimeType)
{
  bool bAddedAll = true;

  ezDeque<const ezDocumentObject*> AddedNodes;

  for (const PasteInfo& pi : info)
  {
    // only add nodes that are allowed to be added
    if (GetObjectManager()->CanAdd(pi.m_pObject->GetTypeAccessor().GetType(), nullptr, "Children", pi.m_Index).m_Result.Succeeded())
    {
      AddedNodes.PushBack(pi.m_pObject);
      GetObjectManager()->AddObject(pi.m_pObject, nullptr, "Children", pi.m_Index);
    }
    else
    {
      bAddedAll = false;
    }
  }

  m_DocumentObjectMetaData->RestoreMetaDataFromAbstractGraph(objectGraph);

  RestoreMetaDataAfterLoading(objectGraph, true);

  if (!AddedNodes.IsEmpty() && bAllowPickedPosition)
  {
    ezDocumentNodeManager* pManager = static_cast<ezDocumentNodeManager*>(GetObjectManager());

    ezVec2 vAvgPos(0);
    for (const ezDocumentObject* pNode : AddedNodes)
    {
      vAvgPos += pManager->GetNodePos(pNode);
    }

    vAvgPos /= AddedNodes.GetCount();

    const ezVec2 vMoveNode = -vAvgPos + ezQtNodeScene::GetLastMouseInteractionPos();

    for (const ezDocumentObject* pNode : AddedNodes)
    {
      ezMoveNodeCommand move;
      move.m_Object = pNode->GetGuid();
      move.m_NewPos = pManager->GetNodePos(pNode) + vMoveNode;
      GetCommandHistory()->AddCommand(move);
    }

    if (!bAddedAll)
    {
      ezLog::Info("[EditorStatus]Not all nodes were allowed to be added to the document");
    }
  }

  GetSelectionManager()->SetSelection(AddedNodes);
  return true;
}

ezAnimationControllerNodePin::ezAnimationControllerNodePin(Type type, const char* szName, const ezColorGammaUB& color, const ezDocumentObject* pObject)
  : ezPin(type, szName, color, pObject)
{
}

ezAnimationControllerNodePin::~ezAnimationControllerNodePin() = default;
