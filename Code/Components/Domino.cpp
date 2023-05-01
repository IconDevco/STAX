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
	m_pEntity->LoadGeometry(geometrySlot, "Objects/DominoMesh.cgf");

	m_pEntity->LoadGeometry(geometrySlot + 1, "Objects/Domino/1.cgf");
	m_pEntity->LoadGeometry(geometrySlot + 2, "Objects/Domino/2.cgf");

	auto* pDominoMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Objects/dominoes1");
	m_pEntity->SetMaterial(pDominoMaterial);


		int bottom = cry_random(1, 6);
		int top = cry_random(1, bottom);
		string nameBottom("materials/domino/" + ToString(bottom));
		string nameTop("materials/domino/" + ToString(top));
		auto* pNumMaterialBottom = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(nameBottom);
		auto* pNumMaterialTop = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(nameTop);

		//IRenderAuxGeom* g = gEnv->pAuxGeomRenderer->GetAux();
		//g->Draw2dLabel(10,10,3,Col_Yellow,false, name);

		m_pEntity->SetSlotMaterial(geometrySlot + 2, pNumMaterialBottom);
		m_pEntity->SetSlotMaterial(geometrySlot + 1, pNumMaterialTop);
	
	
	// Ratio is 0 - 255, 255 being 100% visibility
	m_pEntity->SetViewDistRatio(255);
	m_pEntity->SetLodRatio(50);

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(m_pEntity->GetWorldPos(), Vec3(0, 0, -1) * INFINITE, ent_terrain, rayFlags, &hit, 1);

	if (hits > 0)
		m_pEntity->SetPos(hit.pt);

	m_position = m_pEntity->GetWorldPos();
	m_rotation = m_pEntity->GetWorldRotation();
	m_pEntity->SetScale(Vec3(m_scale));
	m_mass *= m_scale;
	Physicalize();

	//m_pPointConstraint = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CPointConstraintComponent>();

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

		Hover(frameTime);
		UpdateOutline();
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

	m_pEntity->GetPhysics()->Release();

	pe_action_awake awake;
	awake.bAwake = 0;
	m_pEntity->GetPhysics()->Action(&awake);

	//pe_action_awake awake;
	
	//if(m_ForceSimulate)
	//	awake.bAwake = 1;
//	else
		//awake.bAwake = 0;
	
	//m_pEntity->GetPhysics()->Action(&awake);
	LockPhysics();
}

void CDominoComponent::SetScale(float y)
{
	m_pEntity->SetScale(Vec3(y));
	m_mass *= m_scale;
}

void CDominoComponent::PostUpdate()
{
	SetScale(m_scale);
	Physicalize();
	
}

void CDominoComponent::Hover(float fTime)
{
	if (m_isSimulating)
		return;

	Vec3 wp = m_pEntity->GetWorldPos();
	//float distance = m_pEntity->GetWorldPos().z - (m_position.z + m_hoverHeight);

//	if (distance)
		//return;

	if (m_isCursorHovered)
		m_hoverHeight = .07f;
	else
		m_hoverHeight = 0;


	m_currHoverHeight = LERP(m_currHoverHeight, m_hoverHeight, fTime * 5);
	Vec3 pos = Vec3(wp.x, wp.y, m_position.z + m_currHoverHeight);
	Matrix34 tm = m_pEntity->GetWorldTM();
	tm.SetTranslation(pos);
	m_pEntity->SetWorldTM(tm);


	m_isCursorHovered = false;
}

void CDominoComponent::Reset()
{
	Physicalize();

	m_pEntity->SetPosRotScale(m_position, m_rotation, Vec3(m_scale));

}

void CDominoComponent::Simulate()
{
	m_isSimulating = true;
	pe_action_awake awake;
	awake.bAwake = 0;
	m_pEntity->GetPhysics()->Action(&awake);

	UnlockPhysics();
	//m_pEntity->EnablePhysics(false);



}

void CDominoComponent::EndSimulation()
{
	m_isSimulating = false;
	Reset();
}

void CDominoComponent::LockPhysics()
{
	//m_pPointConstraint->Activate(true);
	//m_pPointConstraint->ConstrainToPoint();

	pe_action_add_constraint constraint;
	constraint.flags = world_frames | constraint_no_tears;

	constraint.xlimits[0] = 0;
		constraint.xlimits[1] = 0;
	constraint.yzlimits[0] = 0;
	constraint.yzlimits[1] = 0;


	m_pEntity->GetPhysics()->Action(&constraint);


	SEntityPhysicalizeParams physParams;
	physParams.type = PE_STATIC;
	physParams.mass = m_mass;
	m_pEntity->Physicalize(physParams);

}

void CDominoComponent::UnlockPhysics()
{
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_RIGID;
	physParams.mass = m_mass;
	m_pEntity->Physicalize(physParams);

	m_pEntity->GetPhysics()->Release();

	pe_action_awake awake;
	awake.bAwake = 0;
	m_pEntity->GetPhysics()->Action(&awake);

}

void CDominoComponent::RenderDebug()
{
}

void CDominoComponent::UpdateOutline()
{

	//create outline
	if (m_isCursorHovered || m_isSelected ) 
	{
		m_pEntity->GetRenderNode()->m_nHUDSilhouettesParam = RGBA8(255, 255, 255, 255);
	}

	//remove outline
	if (!m_isCursorHovered || !m_isSelected)
	{
		m_pEntity->GetRenderNode()->m_nHUDSilhouettesParam = RGBA8(0, 0, 0, 0);
	}


}
