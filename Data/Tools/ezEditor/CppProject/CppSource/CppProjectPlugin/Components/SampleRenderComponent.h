#pragma once

#include <Core/World/Component.h>
#include <Core/World/ComponentManager.h>
#include <Core/World/World.h>
#include <CppProjectPlugin/CppProjectPluginDLL.h>

struct ezMsgSetColor;

using ezTexture2DResourceHandle = ezTypedResourceHandle<class ezTexture2DResource>;

// Bitmask to allow the user to select what debug rendering the component should do
struct SampleRenderComponentMask
{
  using StorageType = ezUInt8;

  // the enum names for the bits
  enum Enum
  {
    Box = EZ_BIT(0),
    Sphere = EZ_BIT(1),
    Cross = EZ_BIT(2),
    Quad = EZ_BIT(3),
    All = 0xFF,

    // required enum member; used by ezBitflags for default initialization
    Default = All
  };

  // this allows the debugger to show us names for a bitmask
  // just try this out by looking at an ezBitflags variable in a debugger
  struct Bits
  {
    ezUInt8 Box : 1;
    ezUInt8 Sphere : 1;
    ezUInt8 Cross : 1;
    ezUInt8 Quad : 1;
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_CPPPROJECTPLUGIN_DLL, SampleRenderComponentMask);

// use ezComponentUpdateType::Always for this component to have 'Update' called even inside the editor when it is not simulating
// otherwise we would see the debug render output only when simulating the scene
using SampleRenderComponentManager = ezComponentManagerSimple<class SampleRenderComponent, ezComponentUpdateType::Always>;

class SampleRenderComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(SampleRenderComponent, ezComponent, SampleRenderComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // ezComponent

public:
  virtual void SerializeComponent(ezWorldWriter& stream) const override;
  virtual void DeserializeComponent(ezWorldReader& stream) override;

  //////////////////////////////////////////////////////////////////////////
  // SampleRenderComponent

public:
  SampleRenderComponent();
  ~SampleRenderComponent();

  float m_fSize = 1.0f;             // [ property ]
  ezColor m_Color = ezColor::White; // [ property ]

  void SetTexture(const ezTexture2DResourceHandle& hTexture);
  const ezTexture2DResourceHandle& GetTexture() const;

  void SetTextureFile(const char* szFile); // [ property ]
  const char* GetTextureFile(void) const;  // [ property ]

  ezBitflags<SampleRenderComponentMask> m_RenderTypes; // [ property ]

  void OnSetColor(ezMsgSetColor& msg); // [ msg handler ]

  void SetRandomColor(); // [ scriptable ]

private:
  void Update();

  ezTexture2DResourceHandle m_hTexture;
};
