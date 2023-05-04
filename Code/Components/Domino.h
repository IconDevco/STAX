
#pragma once
#include "StdAfx.h"
#include "CryEntitySystem/IEntityComponent.h"
#include "CryPhysics/IPhysics.h"
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
//#include "DefaultComponents/Constraints/PointConstraint.h"


#include "CryRenderer/ITexture.h"
#include "CryMath/Random.h"
//#include "CryRenderer/IShader.h"
//#include "CryRenderer/IShader_info.h"

class CDominoComponent final : public IEntityComponent
{
public:
	CDominoComponent() = default;
	virtual ~CDominoComponent() {}

	// IEntityComponent
	virtual void Initialize() override;
	Vec3 m_position = Vec3(0);
	Quat m_rotation = IDENTITY;
	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CDominoComponent>& desc)
	{
		desc.SetGUID("{B53A9A5F-F27A-42CB-82C7-B1E379C41A2A}"_cry_guid);
		desc.SetLabel("Domino");
		desc.AddMember(&CDominoComponent::m_mass, 'mass', "Mass","Mass","Mass",.1);
		desc.AddMember(&CDominoComponent::m_scale, 'scle', "Scale","Scale","Scale",1);
		desc.AddMember(&CDominoComponent::m_ForceSimulate, 'sim', "Simulate","Force Simulate","Mass",false);
	}

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	void Physicalize();
	void SetScale(float y);
public:
	int m_Index = 0;
	float m_mass = 1;
	bool m_ForceSimulate = false;
	float m_scale = 1.f;
	void PostUpdate();

	float m_hoverHeight = 0.7f;
	float m_currHoverHeight = 0.7f;
	bool m_isCursorHovered = false;
	void Hover(float fTime);

	void Reset();
	void Simulate();
	void EndSimulation();

	bool m_isSimulating = true;

	void LockPhysics();
	void UnlockPhysics();

	void RenderDebug();

	void DeleteDueToSpawnCollision();

	bool m_isOutlined = false;
	void UpdateOutline();
	
	bool m_isSelected = false;

	//Cry::DefaultComponents::CPointConstraintComponent* m_pPointConstraint = nullptr;

};