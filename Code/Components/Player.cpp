#include "StdAfx.h"
#include "Player.h"
#include "SpawnPoint.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryInput/IHardwareMouse.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>
#include "Domino.h"

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

//----------------------------------------------------------------------------------


void CPlayerComponent::Initialize()
{
	//History.begin();
	//History.clear();
	CryLog("Player: Initialize");
	// Mark the entity to be replicated over the network
	m_pEntity->GetNetEntity()->BindToNetwork();

	debug = gEnv->pGameFramework->GetIPersistantDebug();
	debug->Begin("TargetGoal", false);
	// Register the RemoteReviveOnClient function as a Remote Method Invocation (RMI) that can be executed by the server on clients
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);

	m_cameraGoalPosition = m_cameraCurrentPosition = GetEntity()->GetWorldPos();
	
	//AABB aabb;
	//->GetLocalBounds(aabb);
	//float c = aabb.GetSize().y;
	
	//m_placementDistance = c
}

//----------------------------------------------------------------------------------

void CPlayerComponent::InitializeLocalPlayer()
{
	
	gEnv->pInput->ShowCursor(true);
	CryLog("Player: Initialize local player");

	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	m_desiredViewDistance = ((m_maxTacticalDistance + m_minTacticalDistance) / 2);

	//BuildBoatAttachments();
	BindInputs();
	auxDebug = gEnv->pAuxGeomRenderer->GetAux();
	
}

//----------------------------------------------------------------------------------

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::BecomeLocalPlayer |
		Cry::Entity::EEvent::Update |
		Cry::Entity::EEvent::Reset;
}

//----------------------------------------------------------------------------------

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{

	case Cry::Entity::EEvent::Reset:
	{

	}
	break;

	case Cry::Entity::EEvent::BecomeLocalPlayer:
	{
		InitializeLocalPlayer();
	}
	break;

	case Cry::Entity::EEvent::Update:
	{
		const float frameTime = event.fParam[0];
	
		UpdateDebug(frameTime);
		if (gEnv->IsEditor())
			return;

		if (IsLocalClient())
		{
			UpdateZoom(frameTime);
			UpdateCameraTargetGoal(frameTime);

			UpdateCursorPointer();

			UpdateTacticalViewDirection(frameTime);
			
			UpdateCamera(frameTime);
			float distance = Distance::Point_Point(m_clickPosition , GetSmoothPosition());

			if (m_mouseDown)
				m_isDragging = distance > m_dragThreshold ? true : false;
			else 
				m_isDragging = false;
			
			UpdateSmoothPoisition(frameTime);

			switch (m_activeToolMode)
			{
				case eTM_Editing:
				{

				//	if (m_isMoving)
					//	UpdateMoveDomino(m_readySelectDomino, GetPositionFromPointer(true), frameTime);

					if (m_readyToSelect && m_readySelectDomino && !m_isMoving)
					{
						Vec3 o = m_readySelectDomino->GetWorldPos();

						//if (Distance::Point_Point(o, GetPositionFromPointer(true)) > m_placementDistance)
						if (Distance::Point_Point(o, GetSmoothPosition()) > m_placementDistance)
						{
							m_isMoving = true;
						}
					}


					if (auto pEntity = GetDominoFromPointer())
						if (auto domino = pEntity->GetComponent<CDominoComponent>())
						{
							if(m_lastHoveredDomino != pEntity)
								m_lastHoveredDomino = pEntity;
							m_lastHoveredDomino->GetComponent<CDominoComponent>()->m_isCursorHovered = m_isHoveringEntity();
						}
				}
			break;
				case eTM_Placing:
				{

					if (!m_mouseDown)
					{
						return;
					}

					if (!m_isPlacing)
					{
						if (!m_isPlacingFromExistingDomino)
						{
							if (!m_pOriginGhostDomino)
								CreateOriginGhost(GetPositionFromPointer(true));

							UpdateOriginGhost(frameTime);
						}

						if (!m_pCursorGhostDomino)
							CreateCursorGhost(GetPositionFromPointer(true));


						if (m_isDragging)
						{
							
							if (m_pOriginGhostDomino)
							{

							}
							else
							{

							}

							if (distance > m_placementDistance)
							{
								
								m_isPlacing = true;
							}
						}
						else
						{
							if (m_pCursorGhostDomino)
								DestroyCursorGhost();
						}
					}

					if (m_pCursorGhostDomino)
					{
						UpdateCursorGhost(frameTime);
					}

					if (m_isPlacing)
					{

						if (m_pOriginGhostDomino)
						{
							PlaceDomino(m_pOriginGhostDomino->GetWorldPos());
							DestroyOriginGhost();
						}

						UpdateActivePlacementPosition(GetSmoothPosition(), frameTime);
						
					}
					if (m_pOriginGhostDomino)
					{

					}

				}
				break;

			}
		}
	}
	break;





	}
	
}
//----------------------------------------------------------------------------------

void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CryLog("Player: Ready for gameplay server");
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");

	const Matrix34 newTransform = CSpawnPointComponent::GetFirstSpawnPointTransform();

	Revive(newTransform);

	// Invoke the RemoteReviveOnClient function on all remote clients, to ensure that Revive is called across the network
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this, RemoteReviveParams{ newTransform.GetTranslation(), Quat(newTransform) });

	// Go through all other players, and send the RemoteReviveOnClient on their instances to the new player that is ready for gameplay
	const int channelId = m_pEntity->GetNetEntity()->GetChannelId();
	CGamePlugin::GetInstance()->IterateOverPlayers([this, channelId](CPlayerComponent& player)
		{
			// Don't send the event for the player itself (handled in the RemoteReviveOnClient event above sent to all clients)
			if (player.GetEntityId() == GetEntityId())
				return;

			// Only send the Revive event to players that have already respawned on the server
		//	if (!player.m_isAlive)
				//return;

			// Revive this player on the new player's machine, on the location the existing player was currently at
			const QuatT currentOrientation = QuatT(player.GetEntity()->GetWorldTM());
			SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnClient(&player, RemoteReviveParams{ currentOrientation.t, currentOrientation.q }, channelId);
		});
}

bool CPlayerComponent::RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel)
{
	// Call the Revive function on this client
	Revive(Matrix34::Create(Vec3(1.f), params.rotation, params.position));

	return true;
}

//----------------------------------------------------------------------------------

Vec3 CPlayerComponent::GetTacticalCameraMovementInputDirection() {
	Vec3 dir = ZERO;
	if (m_inputFlags & EInputFlag::MoveLeft)
		dir -= m_lookOrientation.GetColumn0();

	if (m_inputFlags & EInputFlag::MoveRight)
		dir += m_lookOrientation.GetColumn0();

	if (m_inputFlags & EInputFlag::MoveForward)
		dir += m_lookOrientation.GetColumn2();

	if (m_inputFlags & EInputFlag::MoveBack)
		dir -= m_lookOrientation.GetColumn2();

	dir.z = 0;
	return dir;
}

//----------------------------------------------------------------------------------

void CPlayerComponent::ShowCursor()
{
	gEnv->pInput->ShowCursor(true);
}

void CPlayerComponent::HideCursor()
{
	gEnv->pInput->ShowCursor(false);
}

//----------------------------------------------------------------------------------

void CPlayerComponent::PlaceDomino(Vec3 pos)
{
	m_placedDominoes++;
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();


	spawnParams.vPosition = pos;
	
	Vec3 dir = pos - m_lastPlacedPosition;
	dir.z = 0;

		spawnParams.qRotation = Quat::CreateRotationVDir(dir);

		if (m_pOriginGhostDomino)
			spawnParams.qRotation = m_pOriginGhostDomino->GetWorldRotation();
	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		if (auto dom = pEntity->CreateComponentClass<CDominoComponent>()) {

			Vec3 position = dom->GetEntity()->GetWorldPos();

			m_Dominoes.push_back(pEntity);

			dom->m_Index = m_placedDominoes;
			dom->m_position = position;
			dom->m_scale *= m_scaleModifier;
			dom->PostUpdate();
			



		}
	}

		m_lastPlacedPosition = pos;

}

//----------------------------------------------------------------------------------

void CPlayerComponent::BeginSimulation() {
	CryLog("Begin Simulation");
	for (IEntity* pDom : m_Dominoes) {		
		auto d = pDom->GetComponent<CDominoComponent>();
		d->Simulate();
		
	}

}

void CPlayerComponent::EndSimulation() {
	CryLog("End Simulation");
	for (IEntity* pDom : m_Dominoes) {
		pDom->GetComponent<CDominoComponent>()->EndSimulation();
	}

}

void CPlayerComponent::ResetDominoes() {
	CryLog("Reset Simulation");
		EndSimulation();
}

//----------------------------------------------------------------------------------

IEntity* CPlayerComponent::GetDominoFromPointer()
{

	float mouseX, mouseY;
	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

	// Invert mouse Y
	mouseY = gEnv->pRenderer->GetHeight() - mouseY;

	Vec3 vPos0(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 0, &vPos0.x, &vPos0.y, &vPos0.z);

	Vec3 vPos1(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 1, &vPos1.x, &vPos1.y, &vPos1.z);

	Vec3 vDir = vPos1 - vPos0;
	vDir.Normalize();

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_all, rayFlags, &hit, 1);

	if (hits > 0)
	{

		if (IEntity* pEnt = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider))
		{
			if (CDominoComponent* pDomino = pEnt->GetComponent<CDominoComponent>())
			{
				return pEnt;
			}
		}
	}
	return nullptr;
}

//----------------------------------------------------------------------------------

IEntity* CPlayerComponent::SelectDomino(IEntity* pDomino)
{
	m_SelectedDominoes.push_back(GetDominoFromPointer());
	pDomino->GetComponent<CDominoComponent>()->m_isSelected = true;
	return GetDominoFromPointer();
}

//----------------------------------------------------------------------------------

void CPlayerComponent::DeselectAllDominoes()
{
}

void CPlayerComponent::RemoveDomino(IEntity* Domino)
{


	gEnv->pEntitySystem->RemoveEntity(Domino->GetId());
	//m_Dominoes.shrink_to_fit();


}


//----------------------------------------------------------------------------------


void CPlayerComponent::UpdateMoveDomino(Vec3 pos,float fTime)
{

	m_lastFrameMovePosition = LERP(m_lastFrameMovePosition, pos, fTime);

	//float dist = Distance::Point_Point(m_lastFrameMovePosition, pos);
	Vec3 dir = m_lastFrameMovePosition - pos;
	dir.z = 0;
	//dir.normalize();

	//auxDebug->DrawSphere(m_lastFrameMovePosition, .1f, Col_BlueViolet, true);
	//auxDebug->DrawSphere(pos, .1f, Col_Wheat, true);

//	single->SetPosRotScale(pos, Quat::CreateRotationVDir(dir), Vec3(single->GetScale()));

	//single->GetComponent<CDominoComponent>()->m_position = pos;
//	single->GetComponent<CDominoComponent>()->m_rotation = Quat::CreateRotationVDir(dir);
	//m_lastFrameMovePosition = pos;
	
}

void CPlayerComponent::UpdateDebug(float fTime)
{
	int iter = 1;
	float xOffset = 16;
	//auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Physics Enabled: " + ToString(m_isDominoPhysicsEnabled));
	//auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Ready To Select: " + ToString(m_readyToSelect));
	//if(m_isHoveringEntity) auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Green, false, "Is Hovering Domino: " + ToString(m_isHoveringEntity()));
	//auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Moving Domino: " + ToString(m_isMoving));
	if(m_isSimulating) auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Green, false, "Is Simulating: " + ToString(m_isSimulating));
	if(m_isDragging)auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Green, false, "Is Dragging: " + ToString(m_isDragging));
	
	//auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Placement Distance: " + ToString(m_placementDistance));
	auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Scale Modifier: " + ToString(m_scaleModifier));
	auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Tool Mode: " + ToString(m_activeToolMode));

	float offset = .05;
	auxDebug->DrawSphere(GetPositionFromPointer(false) + Vec3(0, 0, offset * 2), .1f, Col_Green, true);
	auxDebug->DrawSphere(GetPositionFromPointer(true) + Vec3(0, 0, offset * 3), .1f, Col_Red, true);

	auxDebug->DrawSphere(GetSmoothPosition() + Vec3(0, 0, offset * 2), .1f, Col_Plum, false);
	auxDebug->DrawSphere(m_lastPlacedPosition + Vec3(0, 0, offset * 3), .1f, Col_Cyan, false);
	auxDebug->DrawSphere(m_firstPlacedPosition + Vec3(0, 0, offset * 4), .1f, Col_White, false);

	if (m_isHoveringEntity())
	{
		//Vec3 p = GetDominoFromPointer()->GetWorldPos();
	//	p.z += .5f;
		pe_status_dynamics d; 
	
		GetDominoFromPointer()->GetPhysics()->GetStatus(&d);;
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "KG: " + ToString(d.mass));
		//auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Tool Mode: " + ToString(m_activeToolMode));


	}

	
}

//----------------------------------------------------------------------------------

/*
void CPlayerComponent::DisableDominoPhysics()
{
	CryLog("Disable Physics");
	m_isDominoPhysicsEnabled = false;
	for (IEntity* pDom : m_Dominoes) {

		pDom->GetComponent<CDominoComponent>()GetPhysics()->Action(&awake);
		
	}
	
}

void CPlayerComponent::EnableDominoPhysics()
{
	CryLog("Enable Physics");
	m_isDominoPhysicsEnabled = true;
	for (IEntity* pDom : m_Dominoes) {
		pDom->EnablePhysics(true);
		pe_action_awake awake;
		awake.bAwake = 0;
		pDom->GetPhysics()->Action(&awake);
	
	}
	
}
*/
void CPlayerComponent::AddForceToDomino(IEntity* dom, float force)
{

	if (!dom)
		return;
	if (!dom->GetPhysics())
		return;

	pe_action_impulse impulse;
	AABB aabb;
	dom->GetWorldBounds(aabb);
	float z = aabb.GetCenter().z;
	Vec3 imp = dom->GetWorldPos();
	imp.z = z*2;
	impulse.point = imp;
	impulse.impulse = dom->GetForwardDir() * (force);// *m_scaleModifier);
	dom->GetPhysics()->Action(&impulse);
}

void CPlayerComponent::ChangeToolMode(int index)
{
	
	m_activeToolMode = (EToolMode)m_activeIndex;
	m_isSimulating = m_activeToolMode == EToolMode::eTM_Simulating ? true : false;
	if (m_isSimulating)
		BeginSimulation();
	else
		EndSimulation();
}

void CPlayerComponent::BeginSmoothPosition(Vec3 origin)
{

	m_smoothGoalPosition = origin;
	m_smoothCurrentPosition = m_smoothGoalPosition;
}

void CPlayerComponent::UpdateSmoothPoisition(float fTime)
{
	m_smoothCurrentPosition = LERP(m_smoothCurrentPosition, GetPositionFromPointer(true), fTime*2);

}

Vec3 CPlayerComponent::GetSmoothPosition()
{
	return m_smoothCurrentPosition;
}



//----------------------------------------------------------------------------------

void CPlayerComponent::CreateCursorGhost(Vec3 origin)
{
	CryLog("Attempting to create cursor ghost");
	if (m_pCursorGhostDomino)
		return;

	CryLog("successfully created ghost cursor");
	BeginSmoothPosition(origin);


	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.vPosition = GetPositionFromPointer(false);
	Vec3 dir = origin - GetPositionFromPointer(true);
	dir.z = 0;
	spawnParams.qRotation = Quat::CreateRotationVDir(dir);

	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		pEntity->CreateComponentClass<CDominoComponent>();
		auto* pDominoMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Objects/dominoghost");

		pEntity->SetMaterial(pDominoMaterial);
	
		m_pCursorGhostDomino = pEntity;
	}

	

}

void CPlayerComponent::UpdateCursorGhost(float fTime)
{
	if (!m_pCursorGhostDomino)
		return;

	//Vec3 dir = m_pOriginGhostDomino->GetWorldPos() - GetPositionFromPointer(true);
	Vec3 dir = m_lastPlacedPosition - GetSmoothPosition();
	dir.z = 0;
	//m_pCursorGhostDomino->SetPosRotScale(GetPositionFromPointer(true), Quat::CreateRotationVDir(dir), Vec3(m_scaleModifier));
	m_pCursorGhostDomino->SetPosRotScale(GetSmoothPosition(), Quat::CreateRotationVDir(dir), Vec3(m_scaleModifier));
	//m_placementCurrentPosition = LERP(m_placementCurrentPosition, m_placementGoalPosition, fTime);

}

void CPlayerComponent::DestroyCursorGhost()
{
	CryLog("Attempting to destroy cursor ghost");
	if (!m_pCursorGhostDomino)
		return;

	CryLog("Destroyed cursor ghost");
	gEnv->pEntitySystem->RemoveEntity(m_pCursorGhostDomino->GetId());
	m_pCursorGhostDomino = nullptr;

}

void CPlayerComponent::UpdateActivePlacementPosition(Vec3 cursorPosition, float fTime)
{
	float distanceToLastPlacedDomino = Distance::Point_Point(GetSmoothPosition(), m_lastPlacedPosition);
	
	if (m_isMoving)
		return;

	
	if (distanceToLastPlacedDomino > m_placementDistance)
	{
			Vec3 dir = GetSmoothPosition() - m_lastPlacedPosition;
			//dir.y *= -1;
			dir.normalize();

			Vec3 n = dir * m_placementDistance;
			Vec3 p = m_lastPlacedPosition + n;
			m_firstPlaced = true;
			PlaceDomino(p);
			m_lastPlacedPosition = p;
		}
	

}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateCursorPointer()
{


}

//----------------------------------------------------------------------------------
#include "Cry3DEngine/ITimeOfDay.h"
Vec3 CPlayerComponent::GetPositionFromPointer(bool ignoreDominoes)
{
	
	Vec3 curPos = GetSmoothPosition();

	float mouseX, mouseY;
	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

	// Invert mouse Y
	mouseY = gEnv->pRenderer->GetHeight() - mouseY;

	Vec3 vPos0(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 0, &vPos0.x, &vPos0.y, &vPos0.z);

	Vec3 vPos1(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 1, &vPos1.x, &vPos1.y, &vPos1.z);

	Vec3 vDir = vPos1 - vPos0;
	vDir.Normalize();

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = 0;
	

	if (ignoreDominoes)
		hits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_terrain | ent_static, rayFlags, &hit, 1);
	else
		hits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_all, rayFlags, &hit, 1);


	if (hits < 1)
		return Vec3(0);
	if (hits > 0)
	{
		curPos = hit.pt;
	}

	return curPos;

}


//----------------------------------------------------------------------------------


void CPlayerComponent::Revive(const Matrix34& transform)
{
	CryLog("Player: Revive");

	if (!gEnv->IsEditor())
	{
		m_pEntity->SetWorldTM(transform);
	}

	m_pEntity->SetWorldTM(transform);

	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);

	m_mouseDeltaRotation = ZERO;
	m_mouseDeltaSmoothingFilter.Reset();

	m_lookOrientation = IDENTITY;
	m_horizontalAngularVelocity = 0.0f;
	m_averagedHorizontalAngularVelocity.Reset();

}

void CPlayerComponent::CreateOriginGhost(Vec3 p)
{
	CryLog("Attempting to create origin ghost");
	if (m_pOriginGhostDomino)
		return;
	CryLog("Successfully created origin ghost");
	m_lastPlacedPosition = p;
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.vPosition = p;
	spawnParams.vScale = Vec3(m_scaleModifier);
	Vec3 dir = p - GetSmoothPosition();
	dir.z = 0;
	spawnParams.qRotation = Quat::CreateRotationVDir(dir);

	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		pEntity->CreateComponentClass<CDominoComponent>();
		m_pOriginGhostDomino = pEntity;
		auto* pDominoMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Objects/dominoghost");

		pEntity->SetMaterial(pDominoMaterial);
		pEntity->SetScale(Vec3(m_scaleModifier));
	}
}

void CPlayerComponent::UpdateOriginGhost(float fTime)
{
	if (!m_pOriginGhostDomino)
		return;

	//Vec3 v = m_pOriginGhostDomino->GetWorldPos()- GetPositionFromPointer(true);
	Vec3 v = m_pOriginGhostDomino->GetWorldPos()- GetSmoothPosition();
	v.z = 0;
	m_pOriginGhostDomino->SetRotation(Quat::CreateRotationVDir(v));
}

void CPlayerComponent::DestroyOriginGhost()
{

	CryLog("Attempting to destroy origin ghost");
	if (!m_pOriginGhostDomino)
		return;

	CryLog("Destroyed origin ghost");
	gEnv->pEntitySystem->RemoveEntity(m_pOriginGhostDomino->GetId());
	m_pOriginGhostDomino = nullptr;
	
}


//----------------------------------------------------------------------------------

void CPlayerComponent::BindInputs()
{
	CryLog("Player: Attempting to bind inputs");
	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value)
		{
			HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode);
		});
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value)
		{
			HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode);
		});
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value)
		{
			HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode);
		});
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value)
		{
			HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode);
		});
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("player", "mouse_rotateyaw", [this](int activationMode, float value)
		{
			m_mouseDeltaRotation.x -= value;
		});
	m_pInputComponent->BindAction("player", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value)
		{
			m_mouseDeltaRotation.y -= value;
		});
	m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);

	m_pInputComponent->RegisterAction("player", "mouse_scrolldown", [this](int activationMode, float value)
		{

			m_scrollY++;
			m_scrollY = m_scrollY * m_scrollSpeedMultiplier;


		});
	m_pInputComponent->BindAction("player", "mouse_scrolldown", eAID_KeyboardMouse, EKeyId::eKI_MouseWheelDown);

	m_pInputComponent->RegisterAction("player", "mouse_scrollup", [this](int activationMode, float value)
		{
			m_scrollY--;
			m_scrollY = m_scrollY * m_scrollSpeedMultiplier;

		});
	m_pInputComponent->BindAction("player", "mouse_scrollup", eAID_KeyboardMouse, EKeyId::eKI_MouseWheelUp);

	m_pInputComponent->RegisterAction("player", "action", [this](int activationMode, float value)
		{


		});
	m_pInputComponent->BindAction("player", "action", eAID_KeyboardMouse, EKeyId::eKI_F);


	
	m_pInputComponent->RegisterAction("player", "SwitchToolMode", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
				m_activeIndex = (m_activeIndex + 1) % 3;
				ChangeToolMode(m_activeIndex);
			}
		});
	m_pInputComponent->BindAction("player", "SwitchToolMode", eAID_KeyboardMouse, EKeyId::eKI_V);

	
	m_pInputComponent->RegisterAction("player", "simulate", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
				if(!m_isMoving)
				if (!m_isSimulating)
					BeginSimulation();
				else
					EndSimulation();
			}

		});
	m_pInputComponent->BindAction("player", "simulate", eAID_KeyboardMouse, EKeyId::eKI_Space);


	m_pInputComponent->RegisterAction("player", "undo", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
				//Undo(History.size());
			}

		});
	m_pInputComponent->BindAction("player", "undo", eAID_KeyboardMouse, EKeyId::eKI_Z);
		
	m_pInputComponent->RegisterAction("player", "ScaleUp", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
				m_scaleModifier += .1f;
				m_scaleModifier = CLAMP(m_scaleModifier, 0.3f, 1.f);
				//m_placementDistance = CLAMP(m_placementDistance += m_placementDistanceStep * .5f,.1f,10.f);//* m_scaleModifier;
			}

		});

	m_pInputComponent->BindAction("player", "ScaleUp", eAID_KeyboardMouse, EKeyId::eKI_T);

	m_pInputComponent->RegisterAction("player", "ScaleDown", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
				m_scaleModifier -= .1f;
				m_scaleModifier = CLAMP(m_scaleModifier, 0.3f, 1.f);
				//m_placementDistance = CLAMP(m_placementDistance -= m_placementDistanceStep * .5f, .1f, 10.f);
			}

		});

	m_pInputComponent->BindAction("player", "ScaleDown", eAID_KeyboardMouse, EKeyId::eKI_G);


	m_pInputComponent->RegisterAction("player", "pancam", [this](int activationMode, float value)
		{

			if (activationMode == eAAM_OnPress)
			{
				m_lookActive = true;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_lookActive = false;
			}


		});
	m_pInputComponent->BindAction("player", "pancam", eAID_KeyboardMouse, EKeyId::eKI_Mouse2);


	m_pInputComponent->RegisterAction("player", "select", [this](int activationMode, float value)
		{

			if (activationMode == eAAM_OnPress)
			{
				BeginSmoothPosition(GetPositionFromPointer(true));
				m_mouseDown = true;
				switch (m_activeToolMode)
				{
				case eTM_Editing:
				{
					m_clickPosition = GetPositionFromPointer(false);

					if (m_isHoveringEntity())
					{
						m_halfSelectDomino = GetDominoFromPointer();
					}

					if (!m_isHoveringEntity())
						DeselectAllDominoes();

				}
				break;

				case eTM_Placing:
				{
					
					if (m_isHoveringEntity())
					{
						m_isPlacingFromExistingDomino = true;

						m_lastPlacedPosition = GetPositionFromPointer(true);
						m_clickPosition = m_lastPlacedPosition;
						//CreateOriginGhost(GetDominoFromPointer()->GetWorldPos());
						CreateCursorGhost(GetDominoFromPointer()->GetWorldPos());
						//RemoveDomino(GetDominoFromPointer());
					}

					if (!m_isHoveringEntity())
						CreateOriginGhost(GetPositionFromPointer(true));

					m_clickPosition = GetPositionFromPointer(false);

				}
				break;
				case eTM_Simulating:
				{
					if (m_isHoveringEntity())
						AddForceToDomino(GetDominoFromPointer(), .1f);
				}
				break;

				}
			}


			if (activationMode == eAAM_OnRelease)
			{
				m_isDragging = false;
				if (m_pCursorGhostDomino)
					DestroyCursorGhost();
				switch (m_activeToolMode)
				{
				case eTM_Editing:
				{
					if (m_halfSelectDomino)
					{
						SelectDomino(GetDominoFromPointer());
					}

					if (m_isMoving)
					{
						moveSingleton = false;

					}

				}
				break;

				case eTM_Placing:
				{
					m_isPlacing = false;

					if (m_pOriginGhostDomino)
					{
						if(m_isDragging)
							return;

						Vec3 dir = GetPositionFromPointer(true) - m_pOriginGhostDomino->GetWorldPos();
						dir.z = 0;
						PlaceDomino(m_firstPlacedPosition);

						DestroyOriginGhost();
					}

				}
				break;

				}

				m_mouseDown = false;
			}

			});

	m_pInputComponent->BindAction("player", "select", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);


}

void CPlayerComponent::UpdateCameraTargetGoal(float fTime)
{


	Vec3 origin = m_cameraGoalPosition + Vec3(0,0,100);
	Vec3 dir = Vec3(0, 0, -1) ;

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(origin, dir * INFINITY, ent_all, rayFlags, &hit, 1);

	
	if (hits >= 1) 
	{
		m_cameraGoalPosition = hit.pt;
	}
	else 
{
		m_cameraGoalPosition.z = 32;
	}
	
	m_cameraGoalPosition.z = 0;
	if (!GetTacticalCameraMovementInputDirection().IsZero()) {
		m_goalSpeed = m_currViewDistance / (m_maxTacticalDistance + m_minTacticalDistance);
			m_cameraGoalPosition += ((GetTacticalCameraMovementInputDirection() * m_goalSpeed * m_panSensitivity) * fTime);
	}
	
	Vec3 pos = Lerp(m_cameraCurrentPosition, m_cameraGoalPosition, fTime  * m_goalTension);
	m_cameraCurrentPosition = pos;
	m_desiredCameraTranform.SetRotation33(Matrix33(IDENTITY));
	m_desiredCameraTranform.SetTranslation(m_cameraCurrentPosition);

	//debug->Add2DText("Goal Speed: " + ToString(m_goalSpeed) , 2, Col_Green, fTime);
	//debug->Add2DText("Cur view dist:" + ToString(m_currViewDistance), 2, Col_Green, fTime);
	//debug->Add2DText("Goal tension:" + ToString(m_goalTension), 2, Col_Green, fTime);

	

}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateTacticalViewDirection(float fTime) {


	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

	if (!m_mouseDeltaRotation.IsZero())
	{

		float rotationLimitsMinPitch = -1.2f;


		float rotationLimitsMaxPitch = -.4f;

		if (m_lookActive) {
			ypr.x += m_mouseDeltaRotation.x * m_rotationSensitivity;
			ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * m_rotationSensitivity, rotationLimitsMinPitch, rotationLimitsMaxPitch);
		}
		else
			ypr.y = CLAMP(ypr.y, rotationLimitsMinPitch, rotationLimitsMaxPitch);


		ypr.z = 0;


		m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
		NetMarkAspectsDirty(InputAspect);

		// Reset every frame
		m_mouseDeltaRotation = ZERO;
	}

	Matrix34 localTransform = IDENTITY;
	Vec3 v = Vec3(0, 0, m_currViewDistance);
	//m_pEntity->GetWorldRotation().GetInverted()) * 
	m_desiredCameraTranform.SetRotation33(Matrix33(m_pEntity->GetWorldRotation().GetInverted()) * CCamera::CreateOrientationYPR(ypr));

	m_desiredCameraTranform.AddTranslation(v);
}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateZoom(float frameTime)
{
	m_desiredViewDistance += m_scrollY;
	//CryLog("Player: Attempting to update view distance to %f", m_desiredViewDistance);


	m_desiredViewDistance = CLAMP(m_desiredViewDistance, m_minTacticalDistance, m_maxTacticalDistance);



	float f = Lerp(m_currViewDistance, m_desiredViewDistance, frameTime);// *m_zoomTension);
	m_currViewDistance = f;
	m_scrollY = 0;

}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateCamera(float fTime) {

	m_pCameraComponent->SetTransformMatrix(m_desiredCameraTranform);
	//m_pAudioListenerComponent->SetOffset(localTransform.GetTranslation());
	auto t = m_pCameraComponent->GetTransform()->GetTranslation();


}

//----------------------------------------------------------------------------------

void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
	{
		if (activationMode == eAAM_OnRelease)
		{
			m_inputFlags &= ~flags;
		}
		else
		{
			m_inputFlags |= flags;
		}
	}
	break;
	case EInputFlagType::Toggle:
	{
		if (activationMode == eAAM_OnRelease)
		{
			// Toggle the bit(s)
			m_inputFlags ^= flags;
		}
	}
	break;
	}

	// Input is replicated from the client to the server.
	if (IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
	}
}


