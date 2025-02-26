#include <ParticlePlugin/ParticlePluginPCH.h>

#include <Core/Interfaces/WindWorldModule.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <Core/World/World.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Time/Clock.h>
#include <ParticlePlugin/Effect/ParticleEffectDescriptor.h>
#include <ParticlePlugin/Effect/ParticleEffectInstance.h>
#include <ParticlePlugin/Emitter/ParticleEmitter.h>
#include <ParticlePlugin/Events/ParticleEventReaction.h>
#include <ParticlePlugin/Initializer/ParticleInitializer.h>
#include <ParticlePlugin/Resources/ParticleEffectResource.h>
#include <ParticlePlugin/System/ParticleSystemDescriptor.h>
#include <ParticlePlugin/System/ParticleSystemInstance.h>
#include <ParticlePlugin/WorldModule/ParticleWorldModule.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

ezParticleEffectInstance::ezParticleEffectInstance()
{
  m_pTask = EZ_DEFAULT_NEW(ezParticleEffectUpdateTask, this);
  m_pTask->ConfigureTask("Particle Effect Update", ezTaskNesting::Maybe);

  m_pOwnerModule = nullptr;

  Destruct();
}

ezParticleEffectInstance::~ezParticleEffectInstance()
{
  Destruct();
}

void ezParticleEffectInstance::Construct(ezParticleEffectHandle hEffectHandle, const ezParticleEffectResourceHandle& hResource, ezWorld* pWorld, ezParticleWorldModule* pOwnerModule, ezUInt64 uiRandomSeed, bool bIsShared, ezArrayPtr<ezParticleEffectFloatParam> floatParams, ezArrayPtr<ezParticleEffectColorParam> colorParams)
{
  m_hEffectHandle = hEffectHandle;
  m_pWorld = pWorld;
  m_pOwnerModule = pOwnerModule;
  m_hResource = hResource;
  m_bIsSharedEffect = bIsShared;
  m_bEmitterEnabled = true;
  m_bIsFinishing = false;
  m_BoundingVolume.SetInvalid();
  m_ElapsedTimeSinceUpdate.SetZero();
  m_EffectIsVisible.SetZero();
  m_iMinSimStepsToDo = 4;
  m_Transform.SetIdentity();
  m_TransformForNextFrame.SetIdentity();
  m_vVelocity.SetZero();
  m_vVelocityForNextFrame.SetZero();
  m_TotalEffectLifeTime.SetZero();
  m_pVisibleIf = nullptr;
  m_uiRandomSeed = uiRandomSeed;

  if (uiRandomSeed == 0)
    m_Random.InitializeFromCurrentTime();
  else
    m_Random.Initialize(uiRandomSeed);

  Reconfigure(true, floatParams, colorParams);
}

void ezParticleEffectInstance::Destruct()
{
  Interrupt();

  m_SharedInstances.Clear();
  m_hEffectHandle.Invalidate();

  m_Transform.SetIdentity();
  m_TransformForNextFrame.SetIdentity();
  m_bIsSharedEffect = false;
  m_pWorld = nullptr;
  m_hResource.Invalidate();
  m_hEffectHandle.Invalidate();
  m_uiReviveTimeout = 5;
}

void ezParticleEffectInstance::Interrupt()
{
  ClearParticleSystems();
  ClearEventReactions();
  m_bEmitterEnabled = false;
}

void ezParticleEffectInstance::SetEmitterEnabled(bool enable)
{
  m_bEmitterEnabled = enable;

  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i])
    {
      m_ParticleSystems[i]->SetEmitterEnabled(m_bEmitterEnabled);
    }
  }
}


bool ezParticleEffectInstance::HasActiveParticles() const
{
  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i])
    {
      if (m_ParticleSystems[i]->HasActiveParticles())
        return true;
    }
  }

  return false;
}


void ezParticleEffectInstance::ClearParticleSystem(ezUInt32 index)
{
  if (m_ParticleSystems[index])
  {
    m_pOwnerModule->DestroySystemInstance(m_ParticleSystems[index]);
    m_ParticleSystems[index] = nullptr;
  }
}

void ezParticleEffectInstance::ClearParticleSystems()
{
  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    ClearParticleSystem(i);
  }

  m_ParticleSystems.Clear();
}


void ezParticleEffectInstance::ClearEventReactions()
{
  for (ezUInt32 i = 0; i < m_EventReactions.GetCount(); ++i)
  {
    if (m_EventReactions[i])
    {
      m_EventReactions[i]->GetDynamicRTTI()->GetAllocator()->Deallocate(m_EventReactions[i]);
    }
  }

  m_EventReactions.Clear();
}

bool ezParticleEffectInstance::IsContinuous() const
{
  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i])
    {
      if (m_ParticleSystems[i]->IsContinuous())
        return true;
    }
  }

  return false;
}

void ezParticleEffectInstance::PreSimulate()
{
  if (m_PreSimulateDuration.GetSeconds() == 0.0)
    return;

  PassTransformToSystems();

  // Pre-simulate the effect, if desired, to get it into a 'good looking' state

  // simulate in large steps to get close
  {
    const ezTime tDiff = ezTime::Seconds(0.5);
    while (m_PreSimulateDuration.GetSeconds() > 10.0)
    {
      StepSimulation(tDiff);
      m_PreSimulateDuration -= tDiff;
    }
  }

  // finer steps
  {
    const ezTime tDiff = ezTime::Seconds(0.2);
    while (m_PreSimulateDuration.GetSeconds() > 5.0)
    {
      StepSimulation(tDiff);
      m_PreSimulateDuration -= tDiff;
    }
  }

  // even finer
  {
    const ezTime tDiff = ezTime::Seconds(0.1);
    while (m_PreSimulateDuration.GetSeconds() >= 0.1)
    {
      StepSimulation(tDiff);
      m_PreSimulateDuration -= tDiff;
    }
  }

  // final step if necessary
  if (m_PreSimulateDuration.GetSeconds() > 0.0)
  {
    StepSimulation(m_PreSimulateDuration);
    m_PreSimulateDuration = ezTime::Seconds(0);
  }

  if (!IsContinuous())
  {
    // Can't check this at the beginning, because the particle systems are only set up during StepSimulation.
    ezLog::Warning("Particle pre-simulation is enabled on an effect that is not continuous.");
  }
}

void ezParticleEffectInstance::SetIsVisible() const
{
  // if it is visible this frame, also render it the next few frames
  // this has multiple purposes:
  // 1) it fixes the transition when handing off an effect from a
  //    ezParticleComponent to a ezParticleFinisherComponent
  //    though this would only need one frame overlap
  // 2) The bounding volume for culling is only computed every couple of frames
  //    so it may be too small and culling could be imprecise
  //    by just rendering it the next 100ms, no matter what, the bounding volume
  //    does not need to be updated so frequently
  m_EffectIsVisible = ezClock::GetGlobalClock()->GetAccumulatedTime() + ezTime::Seconds(0.1);
}


void ezParticleEffectInstance::SetVisibleIf(ezParticleEffectInstance* pOtherVisible)
{
  EZ_ASSERT_DEV(pOtherVisible != this, "Invalid effect");
  m_pVisibleIf = pOtherVisible;
}

bool ezParticleEffectInstance::IsVisible() const
{
  if (m_pVisibleIf != nullptr)
  {
    return m_pVisibleIf->IsVisible();
  }

  return m_EffectIsVisible >= ezClock::GetGlobalClock()->GetAccumulatedTime();
}

void ezParticleEffectInstance::Reconfigure(bool bFirstTime, ezArrayPtr<ezParticleEffectFloatParam> floatParams, ezArrayPtr<ezParticleEffectColorParam> colorParams)
{
  if (!m_hResource.IsValid())
  {
    ezLog::Error("Effect Reconfigure: Effect Resource is invalid");
    return;
  }

  ezResourceLock<ezParticleEffectResource> pResource(m_hResource, ezResourceAcquireMode::BlockTillLoaded);

  const auto& desc = pResource->GetDescriptor().m_Effect;
  const auto& systems = desc.GetParticleSystems();

  m_Transform.SetIdentity();
  m_TransformForNextFrame.SetIdentity();
  m_vVelocity.SetZero();
  m_vVelocityForNextFrame.SetZero();
  m_fApplyInstanceVelocity = desc.m_fApplyInstanceVelocity;
  m_bSimulateInLocalSpace = desc.m_bSimulateInLocalSpace;
  m_InvisibleUpdateRate = desc.m_InvisibleUpdateRate;

  // parameters
  {
    m_FloatParameters.Clear();
    m_ColorParameters.Clear();

    for (auto it = desc.m_FloatParameters.GetIterator(); it.IsValid(); ++it)
    {
      SetParameter(ezTempHashedString(it.Key().GetData()), it.Value());
    }

    for (auto it = desc.m_ColorParameters.GetIterator(); it.IsValid(); ++it)
    {
      SetParameter(ezTempHashedString(it.Key().GetData()), it.Value());
    }

    // shared effects do not support per-instance parameters
    if (m_bIsSharedEffect)
    {
      if (!floatParams.IsEmpty() || !colorParams.IsEmpty())
      {
        ezLog::Warning("Shared particle effects do not support effect parameters");
      }
    }
    else
    {
      for (ezUInt32 p = 0; p < floatParams.GetCount(); ++p)
      {
        SetParameter(floatParams[p].m_sName, floatParams[p].m_Value);
      }

      for (ezUInt32 p = 0; p < colorParams.GetCount(); ++p)
      {
        SetParameter(colorParams[p].m_sName, colorParams[p].m_Value);
      }
    }
  }

  if (bFirstTime)
  {
    m_PreSimulateDuration = desc.m_PreSimulateDuration;
  }

  // TODO Check max number of particles etc. to reset

  if (m_ParticleSystems.GetCount() != systems.GetCount())
  {
    // reset everything
    ClearParticleSystems();
  }

  m_ParticleSystems.SetCount(systems.GetCount());

  struct MulCount
  {
    EZ_DECLARE_POD_TYPE();

    float m_fMultiplier = 1.0f;
    ezUInt32 m_uiCount = 0;
  };

  ezHybridArray<MulCount, 8> systemMaxParticles;
  {
    systemMaxParticles.SetCountUninitialized(systems.GetCount());
    for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
    {
      ezUInt32 uiMaxParticlesAbs = 0, uiMaxParticlesPerSec = 0;
      for (const ezParticleEmitterFactory* pEmitter : systems[i]->GetEmitterFactories())
      {
        ezUInt32 uiMaxParticlesAbs0 = 0, uiMaxParticlesPerSec0 = 0;
        pEmitter->QueryMaxParticleCount(uiMaxParticlesAbs0, uiMaxParticlesPerSec0);

        uiMaxParticlesAbs += uiMaxParticlesAbs0;
        uiMaxParticlesPerSec += uiMaxParticlesPerSec0;
      }

      const ezTime tLifetime = systems[i]->GetAvgLifetime();

      const ezUInt32 uiMaxParticles = ezMath::Max(32u, ezMath::Max(uiMaxParticlesAbs, (ezUInt32)(uiMaxParticlesPerSec * tLifetime.GetSeconds())));

      float fMultiplier = 1.0f;

      for (const ezParticleInitializerFactory* pInitializer : systems[i]->GetInitializerFactories())
      {
        fMultiplier *= pInitializer->GetSpawnCountMultiplier(this);
      }

      systemMaxParticles[i].m_fMultiplier = ezMath::Max(0.0f, fMultiplier);
      systemMaxParticles[i].m_uiCount = (ezUInt32)(uiMaxParticles * systemMaxParticles[i].m_fMultiplier);
    }
  }
  // delete all that have important changes
  {
    for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
    {
      if (m_ParticleSystems[i] != nullptr)
      {
        if (m_ParticleSystems[i]->GetMaxParticles() != systemMaxParticles[i].m_uiCount)
          ClearParticleSystem(i);
      }
    }
  }

  // recreate where necessary
  {
    for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
    {
      if (m_ParticleSystems[i] == nullptr)
      {
        m_ParticleSystems[i] = m_pOwnerModule->CreateSystemInstance(systemMaxParticles[i].m_uiCount, m_pWorld, this, systemMaxParticles[i].m_fMultiplier);
      }
    }
  }

  const ezVec3 vStartVelocity = m_vVelocity * m_fApplyInstanceVelocity;

  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    m_ParticleSystems[i]->ConfigureFromTemplate(systems[i]);
    m_ParticleSystems[i]->SetTransform(m_Transform, vStartVelocity);
    m_ParticleSystems[i]->SetEmitterEnabled(m_bEmitterEnabled);
    m_ParticleSystems[i]->Finalize();
  }

  // recreate event reactions
  {
    ClearEventReactions();

    m_EventReactions.SetCount(desc.GetEventReactions().GetCount());

    const auto& er = desc.GetEventReactions();
    for (ezUInt32 i = 0; i < er.GetCount(); ++i)
    {
      if (m_EventReactions[i] == nullptr)
      {
        m_EventReactions[i] = er[i]->CreateEventReaction(this);
      }
    }
  }
}

bool ezParticleEffectInstance::Update(const ezTime& tDiff)
{
  EZ_PROFILE_SCOPE("PFX: Effect Update");

  ezTime tMinStep = ezTime::Seconds(0);

  if (!IsVisible() && m_iMinSimStepsToDo == 0)
  {
    // shared effects always get paused when they are invisible
    if (IsSharedEffect())
      return true;

    switch (m_InvisibleUpdateRate)
    {
      case ezEffectInvisibleUpdateRate::FullUpdate:
        tMinStep = ezTime::Seconds(1.0 / 60.0);
        break;

      case ezEffectInvisibleUpdateRate::Max20fps:
        tMinStep = ezTime::Milliseconds(50);
        break;

      case ezEffectInvisibleUpdateRate::Max10fps:
        tMinStep = ezTime::Milliseconds(100);
        break;

      case ezEffectInvisibleUpdateRate::Max5fps:
        tMinStep = ezTime::Milliseconds(200);
        break;

      case ezEffectInvisibleUpdateRate::Pause:
      {
        if (m_bEmitterEnabled)
        {
          // during regular operation, pause
          return m_uiReviveTimeout > 0;
        }

        // otherwise do infrequent updates to shut the effect down
        tMinStep = ezTime::Milliseconds(200);
        break;
      }

      case ezEffectInvisibleUpdateRate::Discard:
        Interrupt();
        return false;
    }
  }

  m_ElapsedTimeSinceUpdate += tDiff;
  PassTransformToSystems();

  // if the time step is too big, iterate multiple times
  {
    const ezTime tMaxTimeStep = ezTime::Milliseconds(200); // in sync with Max5fps
    while (m_ElapsedTimeSinceUpdate > tMaxTimeStep)
    {
      m_ElapsedTimeSinceUpdate -= tMaxTimeStep;

      if (!StepSimulation(tMaxTimeStep))
        return false;
    }
  }

  if (m_ElapsedTimeSinceUpdate < tMinStep)
    return m_uiReviveTimeout > 0;

  // do the remainder
  const ezTime tUpdateDiff = m_ElapsedTimeSinceUpdate;
  m_ElapsedTimeSinceUpdate.SetZero();

  return StepSimulation(tUpdateDiff);
}

bool ezParticleEffectInstance::StepSimulation(const ezTime& tDiff)
{
  m_TotalEffectLifeTime += tDiff;

  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i] != nullptr)
    {
      auto state = m_ParticleSystems[i]->Update(tDiff);

      if (state == ezParticleSystemState::Inactive)
      {
        ClearParticleSystem(i);
      }
      else if (state != ezParticleSystemState::OnlyReacting)
      {
        // this is used to delay particle effect death by a couple of frames
        // that way, if an event is in the pipeline that might trigger a reacting emitter,
        // or particles are in the spawn queue, but not yet created, we don't kill the effect too early
        m_uiReviveTimeout = 3;
      }
    }
  }

  CombineSystemBoundingVolumes();

  m_iMinSimStepsToDo = ezMath::Max<ezInt8>(m_iMinSimStepsToDo - 1, 0);

  --m_uiReviveTimeout;
  return m_uiReviveTimeout > 0;
}


void ezParticleEffectInstance::AddParticleEvent(const ezParticleEvent& pe)
{
  // drop events when the capacity is full
  if (m_EventQueue.GetCount() == m_EventQueue.GetCapacity())
    return;

  m_EventQueue.PushBack(pe);
}

void ezParticleEffectInstance::UpdateWindSamples()
{
  const ezUInt32 uiDataIdx = ezRenderWorld::GetDataIndexForExtraction();

  m_vSampleWindResults[uiDataIdx].Clear();

  if (m_vSampleWindLocations[uiDataIdx].IsEmpty())
    return;

  m_vSampleWindResults[uiDataIdx].SetCount(m_vSampleWindLocations[uiDataIdx].GetCount(), ezVec3::ZeroVector());

  if (auto pWind = GetWorld()->GetModuleReadOnly<ezWindWorldModuleInterface>())
  {
    for (ezUInt32 i = 0; i < m_vSampleWindLocations[uiDataIdx].GetCount(); ++i)
    {
      m_vSampleWindResults[uiDataIdx][i] = pWind->GetWindAt(m_vSampleWindLocations[uiDataIdx][i]);
    }
  }

  m_vSampleWindLocations[uiDataIdx].Clear();
}

void ezParticleEffectInstance::SetTransform(const ezTransform& transform, const ezVec3& vParticleStartVelocity)
{
  m_Transform = transform;
  m_TransformForNextFrame = transform;

  m_vVelocity = vParticleStartVelocity;
  m_vVelocityForNextFrame = vParticleStartVelocity;
}

void ezParticleEffectInstance::SetTransformForNextFrame(const ezTransform& transform, const ezVec3& vParticleStartVelocity)
{
  m_TransformForNextFrame = transform;
  m_vVelocityForNextFrame = vParticleStartVelocity;
}

ezInt32 ezParticleEffectInstance::AddWindSampleLocation(const ezVec3& pos)
{
  const ezUInt32 uiDataIdx = ezRenderWorld::GetDataIndexForRendering();

  if (m_vSampleWindLocations[uiDataIdx].GetCount() < m_vSampleWindLocations[uiDataIdx].GetCapacity())
  {
    m_vSampleWindLocations[uiDataIdx].PushBack(pos);
    return m_vSampleWindLocations[uiDataIdx].GetCount() - 1;
  }

  return -1;
}

ezVec3 ezParticleEffectInstance::GetWindSampleResult(ezInt32 idx) const
{
  const ezUInt32 uiDataIdx = ezRenderWorld::GetDataIndexForRendering();

  if (idx >= 0 && m_vSampleWindResults[uiDataIdx].GetCount() > (ezUInt32)idx)
  {
    return m_vSampleWindResults[uiDataIdx][idx];
  }

  return ezVec3::ZeroVector();
}

void ezParticleEffectInstance::PassTransformToSystems()
{
  if (!m_bSimulateInLocalSpace)
  {
    const ezVec3 vStartVel = m_vVelocity * m_fApplyInstanceVelocity;

    for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
    {
      if (m_ParticleSystems[i] != nullptr)
      {
        m_ParticleSystems[i]->SetTransform(m_Transform, vStartVel);
      }
    }
  }
}

void ezParticleEffectInstance::AddSharedInstance(const void* pSharedInstanceOwner)
{
  m_SharedInstances.Insert(pSharedInstanceOwner);
}

void ezParticleEffectInstance::RemoveSharedInstance(const void* pSharedInstanceOwner)
{
  m_SharedInstances.Remove(pSharedInstanceOwner);
}

bool ezParticleEffectInstance::ShouldBeUpdated() const
{
  if (m_hEffectHandle.IsInvalidated())
    return false;

  // do not update shared instances when there is no one watching
  if (m_bIsSharedEffect && m_SharedInstances.GetCount() == 0)
    return false;

  return true;
}

void ezParticleEffectInstance::GetBoundingVolume(ezBoundingBoxSphere& volume) const
{
  if (!m_BoundingVolume.IsValid())
  {
    volume = ezBoundingSphere(ezVec3::ZeroVector(), 0.25f);
    return;
  }

  volume = m_BoundingVolume;

  if (!m_bSimulateInLocalSpace)
  {
    // transform the bounding volume to local space, unless it was already created there
    const ezMat4 invTrans = GetTransform().GetAsMat4().GetInverse();
    volume.Transform(invTrans);
  }
}

void ezParticleEffectInstance::CombineSystemBoundingVolumes()
{
  ezBoundingBoxSphere effectVolume;
  effectVolume.SetInvalid();

  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i])
    {
      const ezBoundingBoxSphere& systemVolume = m_ParticleSystems[i]->GetBoundingVolume();
      if (systemVolume.IsValid())
      {
        effectVolume.ExpandToInclude(systemVolume);
      }
    }
  }

  m_BoundingVolume = effectVolume;
}

void ezParticleEffectInstance::ProcessEventQueues()
{
  m_Transform = m_TransformForNextFrame;
  m_vVelocity = m_vVelocityForNextFrame;

  if (m_EventQueue.IsEmpty())
    return;

  EZ_PROFILE_SCOPE("PFX: Effect Event Queue");
  for (ezUInt32 i = 0; i < m_ParticleSystems.GetCount(); ++i)
  {
    if (m_ParticleSystems[i])
    {
      m_ParticleSystems[i]->ProcessEventQueue(m_EventQueue);
    }
  }

  for (const ezParticleEvent& e : m_EventQueue)
  {
    ezUInt32 rnd = m_Random.UIntInRange(100);

    for (ezParticleEventReaction* pReaction : m_EventReactions)
    {
      if (pReaction->m_sEventName != e.m_EventType)
        continue;

      if (pReaction->m_uiProbability > rnd)
      {
        pReaction->ProcessEvent(e);
        break;
      }

      rnd -= pReaction->m_uiProbability;
    }
  }

  m_EventQueue.Clear();
}

ezParticleEffectUpdateTask::ezParticleEffectUpdateTask(ezParticleEffectInstance* pEffect)
{
  m_pEffect = pEffect;
  m_UpdateDiff.SetZero();
}

void ezParticleEffectUpdateTask::Execute()
{
  if (HasBeenCanceled())
    return;

  if (m_UpdateDiff.GetSeconds() != 0.0)
  {
    m_pEffect->PreSimulate();

    if (!m_pEffect->Update(m_UpdateDiff))
    {
      const ezParticleEffectHandle hEffect = m_pEffect->GetHandle();
      EZ_ASSERT_DEBUG(!hEffect.IsInvalidated(), "Invalid particle effect handle");

      m_pEffect->GetOwnerWorldModule()->DestroyEffectInstance(hEffect, true, nullptr);
    }
  }
}

void ezParticleEffectInstance::SetParameter(const ezTempHashedString& name, float value)
{
  // shared effects do not support parameters
  if (m_bIsSharedEffect)
    return;

  for (ezUInt32 i = 0; i < m_FloatParameters.GetCount(); ++i)
  {
    if (m_FloatParameters[i].m_uiNameHash == name.GetHash())
    {
      m_FloatParameters[i].m_fValue = value;
      return;
    }
  }

  auto& ref = m_FloatParameters.ExpandAndGetRef();
  ref.m_uiNameHash = name.GetHash();
  ref.m_fValue = value;
}

void ezParticleEffectInstance::SetParameter(const ezTempHashedString& name, const ezColor& value)
{
  // shared effects do not support parameters
  if (m_bIsSharedEffect)
    return;

  for (ezUInt32 i = 0; i < m_ColorParameters.GetCount(); ++i)
  {
    if (m_ColorParameters[i].m_uiNameHash == name.GetHash())
    {
      m_ColorParameters[i].m_Value = value;
      return;
    }
  }

  auto& ref = m_ColorParameters.ExpandAndGetRef();
  ref.m_uiNameHash = name.GetHash();
  ref.m_Value = value;
}

ezInt32 ezParticleEffectInstance::FindFloatParameter(const ezTempHashedString& name) const
{
  for (ezUInt32 i = 0; i < m_FloatParameters.GetCount(); ++i)
  {
    if (m_FloatParameters[i].m_uiNameHash == name.GetHash())
      return i;
  }

  return -1;
}

float ezParticleEffectInstance::GetFloatParameter(const ezTempHashedString& name, float defaultValue) const
{
  if (name.IsEmpty())
    return defaultValue;

  for (ezUInt32 i = 0; i < m_FloatParameters.GetCount(); ++i)
  {
    if (m_FloatParameters[i].m_uiNameHash == name.GetHash())
      return m_FloatParameters[i].m_fValue;
  }

  return defaultValue;
}

ezInt32 ezParticleEffectInstance::FindColorParameter(const ezTempHashedString& name) const
{
  for (ezUInt32 i = 0; i < m_ColorParameters.GetCount(); ++i)
  {
    if (m_ColorParameters[i].m_uiNameHash == name.GetHash())
      return i;
  }

  return -1;
}

const ezColor& ezParticleEffectInstance::GetColorParameter(const ezTempHashedString& name, const ezColor& defaultValue) const
{
  if (name.IsEmpty())
    return defaultValue;

  for (ezUInt32 i = 0; i < m_ColorParameters.GetCount(); ++i)
  {
    if (m_ColorParameters[i].m_uiNameHash == name.GetHash())
      return m_ColorParameters[i].m_Value;
  }

  return defaultValue;
}

EZ_STATICLINK_FILE(ParticlePlugin, ParticlePlugin_Effect_ParticleEffectInstance);
