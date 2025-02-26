#include <RendererCore/RendererCorePCH.h>

#include <RendererCore/Lights/SphereReflectionProbeComponent.h>

#include <../../Data/Base/Shaders/Common/LightData.h>
#include <Core/Messages/TransformChangedMessage.h>
#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <RendererCore/Lights/Implementation/ReflectionPool.h>
#include <RendererCore/Pipeline/View.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezSphereReflectionProbeComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Radius", GetRadius, SetRadius)->AddAttributes(new ezClampValueAttribute(0.0f, {}), new ezDefaultValueAttribute(5.0f)),
    EZ_ACCESSOR_PROPERTY("Falloff", GetFalloff, SetFalloff)->AddAttributes(new ezClampValueAttribute(0.0f, 1.0f), new ezDefaultValueAttribute(0.1f)),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_FUNCTIONS
  {
    EZ_FUNCTION_PROPERTY(OnObjectCreated),
  }
  EZ_END_FUNCTIONS;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgUpdateLocalBounds, OnUpdateLocalBounds),
    EZ_MESSAGE_HANDLER(ezMsgExtractRenderData, OnMsgExtractRenderData),
    EZ_MESSAGE_HANDLER(ezMsgTransformChanged, OnTransformChanged),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Rendering/Lighting"),
    new ezSphereVisualizerAttribute("Radius", ezColor::AliceBlue),
    new ezSphereManipulatorAttribute("Radius"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE
// clang-format on

ezSphereReflectionProbeComponentManager::ezSphereReflectionProbeComponentManager(ezWorld* pWorld)
  : ezComponentManager<ezSphereReflectionProbeComponent, ezBlockStorageType::Compact>(pWorld)
{
}

//////////////////////////////////////////////////////////////////////////

ezSphereReflectionProbeComponent::ezSphereReflectionProbeComponent() = default;
ezSphereReflectionProbeComponent::~ezSphereReflectionProbeComponent() = default;

void ezSphereReflectionProbeComponent::SetRadius(float fRadius)
{
  m_fRadius = ezMath::Max(fRadius, 0.0f);
  m_bStatesDirty = true;
}

float ezSphereReflectionProbeComponent::GetRadius() const
{
  return m_fRadius;
}

void ezSphereReflectionProbeComponent::SetFalloff(float fFalloff)
{
  m_fFalloff = ezMath::Clamp(fFalloff, ezMath::DefaultEpsilon<float>(), 1.0f);
}

void ezSphereReflectionProbeComponent::OnActivated()
{
  GetOwner()->EnableStaticTransformChangesNotifications();
  m_Id = ezReflectionPool::RegisterReflectionProbe(GetWorld(), m_desc, this);
  GetOwner()->UpdateLocalBounds();
}

void ezSphereReflectionProbeComponent::OnDeactivated()
{
  ezReflectionPool::DeregisterReflectionProbe(GetWorld(), m_Id);
  m_Id.Invalidate();

  GetOwner()->UpdateLocalBounds();
}

void ezSphereReflectionProbeComponent::OnObjectCreated(const ezAbstractObjectNode& node)
{
  m_desc.m_uniqueID = node.GetGuid();
}

void ezSphereReflectionProbeComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& msg)
{
  msg.SetAlwaysVisible(ezDefaultSpatialDataCategories::RenderDynamic);
}

void ezSphereReflectionProbeComponent::OnMsgExtractRenderData(ezMsgExtractRenderData& msg) const
{
  // Don't trigger reflection rendering in shadow or other reflection views.
  if (msg.m_pView->GetCameraUsageHint() == ezCameraUsageHint::Shadow || msg.m_pView->GetCameraUsageHint() == ezCameraUsageHint::Reflection)
    return;

  if (m_bStatesDirty)
  {
    m_bStatesDirty = false;
    ezReflectionPool::UpdateReflectionProbe(GetWorld(), m_Id, m_desc, this);
  }

  auto pRenderData = ezCreateRenderDataForThisFrame<ezReflectionProbeRenderData>(GetOwner());
  pRenderData->m_GlobalTransform = GetOwner()->GetGlobalTransform();
  pRenderData->m_vProbePosition = pRenderData->m_GlobalTransform * m_desc.m_vCaptureOffset;
  pRenderData->m_vHalfExtents = ezVec3(m_fRadius);
  pRenderData->m_vInfluenceScale = ezVec3(1.0f);
  pRenderData->m_vInfluenceShift = ezVec3(0.5f);
  pRenderData->m_vPositiveFalloff = ezVec3(m_fFalloff);
  pRenderData->m_vNegativeFalloff = ezVec3(m_fFalloff);
  pRenderData->m_Id = m_Id;
  pRenderData->m_uiIndex = REFLECTION_PROBE_IS_SPHERE;

  const ezVec3 vScale = pRenderData->m_GlobalTransform.m_vScale * m_fRadius;
  constexpr float fSphereConstant = (4.0f / 3.0f) * ezMath::Pi<float>();
  const float fEllipsoidVolume = fSphereConstant * ezMath::Abs(vScale.x * vScale.y * vScale.z);

  float fPriority = ComputePriority(msg, pRenderData, fEllipsoidVolume, vScale);
  ezReflectionPool::ExtractReflectionProbe(this, msg, pRenderData, GetWorld(), m_Id, fPriority);
}

void ezSphereReflectionProbeComponent::OnTransformChanged(ezMsgTransformChanged& msg)
{
  m_bStatesDirty = true;
}

void ezSphereReflectionProbeComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);

  ezStreamWriter& s = stream.GetStream();

  s << m_fRadius;
  s << m_fFalloff;
}

void ezSphereReflectionProbeComponent::DeserializeComponent(ezWorldReader& stream)
{
  SUPER::DeserializeComponent(stream);
  //const ezUInt32 uiVersion = stream.GetComponentTypeVersion(GetStaticRTTI());
  ezStreamReader& s = stream.GetStream();

  s >> m_fRadius;
  s >> m_fFalloff;
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_Lights_Implementation_SphereReflectionProbeComponent);
