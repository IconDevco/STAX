#include "StdAfx.h"
#include "Domino.h"
#include <CryCore/StaticInstanceList.h>



namespace
{
	static void RegisterDominoComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CDominoComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterDominoComponent);
}

void CDominoComponent::Initialize()
{
	// Set the model
	const int geometrySlot = 0;
	GetEntity()->LoadGeometry(geometrySlot, "Objects/DominoMesh.cgf");

	GetEntity()->LoadGeometry(geometrySlot + 1, "Objects/Domino/1.cgf");
	GetEntity()->LoadGeometry(geometrySlot + 2, "Objects/Domino/2.cgf");

	auto* pDominoMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Objects/dominoes1");
	m_pEntity->SetMaterial(pDominoMaterial);

	for (int i = 1; i < 5; i++) {
		int r = cry_random(1, 6);
		string name("materials/domino/" + ToString(r));
		auto* pNumMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(name);

		//IRenderAuxGeom* g = gEnv->pAuxGeomRenderer->GetAux();
		//g->Draw2dLabel(10,10,3,Col_Yellow,false, name);

		m_pEntity->SetSlotMaterial(geometrySlot + i, pNumMaterial);
	}
	
	// Ratio is 0 - 255, 255 being 100% visibility
	GetEntity()->SetViewDistRatio(255);
	GetEntity()->SetLodRatio(50);

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(GetEntity()->GetWorldPos(), Vec3(0, 0, -1) * INFINITE, ent_terrain, rayFlags, &hit, 1);

	if (hits > 0)
		GetEntity()->SetPos(hit.pt);

	m_position = GetEntity()->GetWorldPos();
	m_rotation = GetEntity()->GetWorldRotation();
	GetEntity()->SetScale(Vec3(m_scale));
	m_mass *= m_scale;
	Physicalize();
}

Cry::Entity::EventFlags CDominoComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::EditorPropertyChanged |
		Cry::Entity::EEvent::Update |
		Cry::Entity::EEvent::Reset;
}


void CDominoComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{

	case Cry::Entity::EEvent::Reset:
	{ 

	}
	break;

	case Cry::Entity::EEvent::EditorPropertyChanged:
	{
		SetScale(m_scale);
		Physicalize();
		
	}
	break;

	case Cry::Entity::EEvent::Update:
	{
		const float frameTime = event.fParam[0];

		if (gEnv->IsEditor())
			return;


	}
	break;
	}

	if (event.event == ENTITY_EVENT_COLLISION)
	{
		// Collision info can be retrieved using the event pointer
		//EventPhysCollision *physCollision = reinterpret_cast<EventPhysCollision *>(event.ptr);

		// Queue removal of this entity, unless it has already been done
		//gEnv->pEntitySystem->RemoveEntity(GetEntityId());
	}
}

void CDominoComponent::Physicalize()
{
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_RIGID;
	physParams.mass = m_mass;
	m_pEntity->Physicalize(physParams);



	pe_action_awake awake;
	
	//if(m_ForceSimulate)
	//	awake.bAwake = 1;
//	else
		//awake.bAwake = 0;
	
	GetEntity()->GetPhysics()->Action(&awake);

}

void CDominoComponent::SetScale(float y)
{
	GetEntity()->SetScale(Vec3(y));
	m_mass *= m_scale;
}

void CDominoComponent::PostUpdate()
{
	SetScale(m_scale);
	Physicalize();
	
}
