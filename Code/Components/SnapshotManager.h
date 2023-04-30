#pragma once
#include "CryEntitySystem/IEntityComponent.h"
#include "CryPhysics/IPhysics.h"
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>


#include "CryRenderer/ITexture.h"
#include "CryMath/Random.h"
//#include "CryRenderer/IShader.h"
//#include "CryRenderer/IShader_info.h"

class CSnapshotManager final : public IEntityComponent
{
public:
	CSnapshotManager() = default;
	virtual ~CSnapshotManager() {}

	// IEntityComponent
	virtual void Initialize() override;
	Vec3 m_position = Vec3(0);
	Quat m_rotation = IDENTITY;
	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CSnapshotManager>& desc)
	{
		desc.SetGUID("{E5B66599-C6FA-4DEB-801B-5903A60AB8A9}"_cry_guid);
		desc.SetLabel("Snapshot Manager");

	}

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent


	const int maxIndices = 32;
	int currentSelectedIndex;
	int totalIndices;

	void AddSnapshot();
	void UpdateSnapshotStates();
	void UndoSnapshot();

	void UndoSnapshot(int index); //also redo
	void MarkSnapshot();

	struct SSnapshot {
		int cacheIndex;
		bool markedForDeletion = false;
	};

	DynArray<SSnapshot*> m_Snapshots;

	void CheckActivity(SSnapshot* snapshot);
	void SetSnapshot();

	void DeleteSnapshotContents(SSnapshot* snapshot);
};