#pragma once
#include "StdAfx.h"
#include "CryEntitySystem/IEntityComponent.h"

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

#include "CryMath/Random.h"


#include <DefaultComponents/Cameras/CameraComponent.h>
#include "DefaultComponents/Geometry/AdvancedAnimationComponent.h"
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>
#include "DefaultComponents/Physics/CharacterControllerComponent.h"
#include <CryAISystem/Components/IEntityNavigationComponent.h>


class CDominoEditor final : public IEntityComponent
{
public:
	CDominoEditor() = default;
	virtual ~CDominoEditor() {}

	// IEntityComponent
	virtual void Initialize() override;

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CDominoEditor>& desc)
	{
		desc.SetGUID("{521A8FC3-76B9-4B1F-9B3C-FB03B3A69DD1}"_cry_guid);
		desc.SetLabel("Editor");
	}

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	void DeselectDomino(IEntity* pDomino);
	void DeselectAllDominoes();
	void RemoveDomino(IEntity* pDomino);
	void DeleteDomino(IEntity* pDomino) { RemoveDomino(pDomino); }
	void DestroyDomino(IEntity* pDomino) { RemoveDomino(pDomino); }

	IEntity* SelectDomino(IEntity* pDomino);

	DynArray<IEntity*> m_SelectedDominoes;

};