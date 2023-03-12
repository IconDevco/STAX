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
	History.begin();
	History.clear();
	CryLog("Player: Initialize");
	// Mark the entity to be replicated over the network
	m_pEntity->GetNetEntity()->BindToNetwork();

	debug = gEnv->pGameFramework->GetIPersistantDebug();
	debug->Begin("TargetGoal", false);
	// Register the RemoteReviveOnClient function as a Remote Method Invocation (RMI) that can be executed by the server on clients
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);

	m_cameraDesiredGoalPosition = m_cameraCurrentGoalPosition = GetEntity()->GetWorldPos();
}

//----------------------------------------------------------------------------------

void CPlayerComponent::InitializeLocalPlayer()
{
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
		
		if (IsLocalClient())
		{
			UpdateZoom(frameTime);
			UpdateCameraTargetGoal(frameTime);

			if (!m_isSimulating)
				UpdateCursorPointer();

			UpdateTacticalViewDirection(frameTime);
			
			UpdateCamera(frameTime);

			if (m_placementActive)
			{
				if (m_isDominoPhysicsEnabled)
					DisableDominoPhysics();

				if (m_pGhostFirstDomino)
					UpdateFirstGhost(frameTime);

				UpdatePlacementPosition(GetPositionFromPointer(), frameTime);
			}
			else
			{
				if (!m_isDominoPhysicsEnabled)
					EnableDominoPhysics();
			}

			if (m_readyToSelect && m_readySelectDomino && !m_isMoving)
			{
				Vec3 o = m_readySelectDomino->GetWorldPos();

				if (Distance::Point_Point(o, GetPositionFromPointer()) > m_placementDistance)
					m_isMoving = true;

			}
	
			m_isHoveringEntity = GetEntityFromPointer() ? true : false;
			if (m_isMoving)
			{
				if (m_isDominoPhysicsEnabled)
					DisableDominoPhysics();
				//UpdatePlacementPosition(GetPositionFromPointer(), frameTime);
				UpdateMoveDomino(m_readySelectDomino, GetPositionFromPointer(), frameTime);
			}
			else 
			{
				if(!m_isDominoPhysicsEnabled)
				EnableDominoPhysics();

			}
		}

		int iter = 1;
		float xOffset = 16;
		
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Physics Enabled: " + ToString(m_isDominoPhysicsEnabled));
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Ready To Select: " + ToString(m_readyToSelect));
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is ready to select entity: " + ToString(m_readySelectDomino));
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Hovering Domino: " + ToString(m_isHoveringEntity));
		auxDebug->Draw2dLabel(0, xOffset * iter++, 2, Col_Yellow, false, "Is Hovering Domino Entity: " + ToString(GetEntityFromPointer()));
		auxDebug->Draw2dLabel(0, xOffset* iter++, 2, Col_Yellow, false, "Is Simulating: " + ToString(m_isSimulating));


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

//----------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------

void CPlayerComponent::HideCursor()
{
	gEnv->pInput->ShowCursor(false);
}

//----------------------------------------------------------------------------------

void CPlayerComponent::PlaceDomino(Vec3 pos, Quat rot)
{
	m_placedDominoes++;
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

	spawnParams.vPosition = pos;
	

	Vec3 dir = pos - m_lastPlacedPosition;
	dir.z = 0;
	
	if (!m_firstPlaced)
		spawnParams.qRotation =rot;
	else
	spawnParams.qRotation = Quat::CreateRotationVDir(dir);

	// Spawn the entity
	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		if (auto dom = pEntity->CreateComponentClass<CDominoComponent>()) {
			//auto e = pEntity->CreateComponentClass<CDominoComponent>();

			//Vec3 m_position = e->GetEntity()->GetWorldPos();
		//	Quat m_rotation = e->GetEntity()->GetWorldRotation();
			m_Dominoes.push_back(pEntity);
			m_ActiveHistory->m_Dominoes.push_back(pEntity);
			pEntity->EnablePhysics(true);
		}
	}

		m_lastPlacedPosition = pos;

		
}

//----------------------------------------------------------------------------------

void CPlayerComponent::BeginSimulation() {
	for (IEntity* pDom : m_Dominoes) {		
		pDom->EnablePhysics(true);
		pe_action_awake awake;
		awake.bAwake = 1;
		pDom->GetPhysics()->Action(&awake);
		
	}
	m_isSimulating = true;
}

void CPlayerComponent::EndSimulation() {
	ResetDominoes();
	for (IEntity* pDom : m_Dominoes) {
		SEntityPhysicalizeParams physParams;
		physParams.type = PE_RIGID;
		physParams.mass = .04f;
		m_pEntity->Physicalize(physParams);


		pe_action_awake awake;
		awake.bAwake = 0;
		pDom->GetPhysics()->Action(&awake);

	}
	m_isSimulating = false;
}

void CPlayerComponent::ResetDominoes() {
	for (IEntity* pDom : m_Dominoes) {
		CDominoComponent* dom = pDom->GetComponent<CDominoComponent>();
		SEntityPhysicalizeParams physParams;
		physParams.type = PE_RIGID;
		physParams.mass = .04f;
		m_pEntity->Physicalize(physParams);


		pDom->SetPosRotScale(dom->m_position, dom->m_rotation, Vec3(1));
	}
}

//----------------------------------------------------------------------------------

IEntity* CPlayerComponent::GetEntityFromPointer()
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
		if(IEntity* pEnt = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider))
		return pEnt;
		
	}
	return nullptr;
}

//----------------------------------------------------------------------------------

IEntity* CPlayerComponent::SelectEntity()
{
	m_SelectedEntities.push_back(GetEntityFromPointer());
	return nullptr;
}

//----------------------------------------------------------------------------------

void CPlayerComponent::RemoveDomino(IEntity* Domino)
{


	gEnv->pEntitySystem->RemoveEntity(Domino->GetId());
	//m_Dominoes.shrink_to_fit();


}

//----------------------------------------------------------------------------------

void CPlayerComponent::InsertHistorySet(SHistorySet* historySet)
{
	debug->Add2DText("Added history set "+ ToString(historySet->m_index), 2, Col_White, 2);
	History.append(historySet);
	//History[m_historyStep]=historySet;
	//m_historyStep++;

}

//----------------------------------------------------------------------------------

void CPlayerComponent::Undo(int stepToRemove)
{	
	

	if (m_placementActive)
		return;
	if (History.empty())
		return;

	for (IEntity* pDomino : History[stepToRemove-1]->m_Dominoes) {
		pDomino->Hide(true);
	}

	History.erase(stepToRemove-1);

}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateMoveDomino(IEntity* single, Vec3 pos,float fTime)
{
	if (!single)
		return;

	single->EnablePhysics(true);
	m_lastFrameMovePosition = LERP(m_lastFrameMovePosition, pos, fTime);

	//float dist = Distance::Point_Point(m_lastFrameMovePosition, pos);
	Vec3 dir = m_lastFrameMovePosition - pos;
	dir.z = 0;
	//dir.normalize();

	auxDebug->DrawSphere(m_lastFrameMovePosition, 2, Col_BlueViolet, true);
	auxDebug->DrawSphere(pos, 2, Col_BlueViolet, true);

	single->SetPosRotScale(pos, Quat::CreateRotationVDir(dir), Vec3(1));

	single->GetComponent<CDominoComponent>()->m_position = pos;
	//m_lastFrameMovePosition = pos;
	
}

//----------------------------------------------------------------------------------


void CPlayerComponent::DisableDominoPhysics()
{
	m_isDominoPhysicsEnabled = false;
	for (IEntity* pDom : m_Dominoes) {
		pDom->EnablePhysics(false);
		pe_action_awake awake;
		awake.bAwake = 0;
		pDom->GetPhysics()->Action(&awake);
		
	}
	
}

void CPlayerComponent::EnableDominoPhysics()
{
	m_isDominoPhysicsEnabled = true;
	for (IEntity* pDom : m_Dominoes) {
		pDom->EnablePhysics(true);
		pe_action_awake awake;
		awake.bAwake = 0;
		pDom->GetPhysics()->Action(&awake);
	
	}
	
}
//----------------------------------------------------------------------------------


/*
void CPlayerComponent::RestartHistory(int fromIndex)
{
	CryLog("Attempting to restart history from index %i", fromIndex);


	int historySize = History.size();
	//int restarted = historySize - m_undoSteps;
	CryLog("History size %i", historySize);

	for (int i = historySize-1; i > fromIndex-1; i--)
	{
		CryLog("Clearing entry %i", i);
		for (IEntity* pDomino : History[i]->m_Dominoes) {
			RemoveDomino(pDomino);
		}
		History[i]->m_Dominoes.clear();
		CryLog("Successfully removed m_Dominoes");
		debug->Add2DText("Successfully removed m_Dominoes", 2, Col_Blue, 5);

		History.erase(i);
		History.destroy(i);
		History.resize(i);// +1);
		
		//DynArray<SHistorySet*> TempHistory;
		//TempHistory.append(i);
		//History.clear();
		//History = TempHistory;

	}
	//History.erase(restarted, historySize);
	History.shrink_to_fit();
	historySize = History.size();
	m_undoSteps = 0;
	CryLog("Resulting in %i entries", historySize);
	
}
*/

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdatePlacementPosition(Vec3 o, float fTime)
{

	m_placementDesiredGoalPosition = o;

	if (!m_firstPlaced)
	{
		m_placementCurrentGoalPosition = m_placementDesiredGoalPosition;
		
		if (!m_pGhostFirstDomino)
			m_firstPlacedPosition = m_placementDesiredGoalPosition;
		else 
			m_firstPlacedPosition = m_pGhostFirstDomino->GetWorldPos();
		
		//if (!m_pGhostFirstDomino)
		m_lastPlacedPosition = m_firstPlacedPosition;

		CreateFirstGhost(m_firstPlacedPosition);
	
	}

	m_placementCurrentGoalPosition = LERP(m_placementCurrentGoalPosition, m_placementDesiredGoalPosition, fTime);
	float offset = .05;
	
	auxDebug->DrawSphere(m_placementCurrentGoalPosition + Vec3(0, 0, offset *2), .1f, Col_Red, false);
	auxDebug->DrawSphere(m_lastPlacedPosition + Vec3(0, 0, offset*3), .1f, Col_Cyan, false);
	auxDebug->DrawSphere(m_firstPlacedPosition + Vec3(0, 0, offset*4), .1f, Col_White, false);

	float dist = Distance::Point_Point(m_placementCurrentGoalPosition, m_lastPlacedPosition);

	Vec3 dir = m_placementCurrentGoalPosition - m_lastPlacedPosition;
	dir.normalize();

	Vec3 n = dir * m_placementDistance;
	Vec3 p = m_lastPlacedPosition + n;
	
	if (m_isMoving)
	{
		//m_readySelectDomino->SetPosRotScale(m_placementCurrentGoalPosition, Quat::CreateRotationVDir(dir), Vec3(1));
		return;

	}
	if (dist > m_placementDistance)
	{
		if (m_firstPlaced) {
			//PlaceDomino(m_placementCurrentGoalPosition);
			PlaceDomino(p);
			
		}
		if (!m_firstPlaced)
		{
			Quat firstDomRot = m_pGhostFirstDomino->GetWorldRotation();
			DestroyFirstGhost();
			PlaceDomino(m_firstPlacedPosition,firstDomRot);
			Vec3 dir = m_placementCurrentGoalPosition - m_lastPlacedPosition;
			dir.normalize();

			Vec3 n = dir * m_placementDistance;
			Vec3 p = m_lastPlacedPosition + n;
			m_firstPlaced = true;
			PlaceDomino(p);
			
		}
	}
}

//----------------------------------------------------------------------------------

void CPlayerComponent::UpdateCursorPointer()
{

		
	auxDebug->DrawSphere(GetPositionFromPointer(), .1f, Col_Yellow, true);
	

}

//----------------------------------------------------------------------------------
#include "Cry3DEngine/ITimeOfDay.h"
Vec3 CPlayerComponent::GetPositionFromPointer()
{
	
	Vec3 curPos = m_placementCurrentGoalPosition;

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
	
	/*
	IPhysicalEntity** pSkips = new IPhysicalEntity*[4096];
	
	for (int i = 0; i < m_Dominoes.size()-1; i++)
	{
		if (m_Dominoes.empty())
			break;

		pSkips[i] = m_Dominoes[i]->GetPhysicalEntity();
		
	}
	
	*/
	int hits = 0;

	if (m_placementActive || m_isMoving)
		hits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_static | ent_terrain, rayFlags, &hit, 1);
	else
		hits = gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_all, rayFlags, &hit, 1);

	if (hits > 0)
	{
		if (IEntity* pEnt = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider))
			if(pEnt->GetComponent<CDominoComponent>())
			m_isHoveringEntity = true;
		else
			m_isHoveringEntity = false;

		curPos = hit.pt;
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
void CPlayerComponent::ShowGhostCursor()
{
}
void CPlayerComponent::CreateFirstGhost(Vec3 p)
{

	if (m_pGhostFirstDomino != nullptr)
		return;

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.vPosition = p;
	Vec3 dir = p - m_placementDesiredGoalPosition;
	dir.z = 0;
	spawnParams.qRotation = Quat::CreateRotationVDir(dir);

	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		pEntity->CreateComponentClass<CDominoComponent>();
		m_pGhostFirstDomino = pEntity;
		auto* pDominoMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Objects/dominoghost");

		pEntity->SetMaterial(pDominoMaterial);
	}

}
void CPlayerComponent::UpdateFirstGhost(float fTime)
{
	if (!m_pGhostFirstDomino)
		return;

	Vec3 v = m_pGhostFirstDomino->GetWorldPos() - m_placementCurrentGoalPosition;
	//v.z = 0;
	m_pGhostFirstDomino->SetRotation(Quat::CreateRotationVDir(v));
}

void CPlayerComponent::DestroyFirstGhost()
{
	if (m_pGhostFirstDomino == nullptr)
		return;

	
	gEnv->pEntitySystem->RemoveEntity(m_pGhostFirstDomino->GetId());
	m_pGhostFirstDomino = nullptr;
	
}
/*
void CPlayerComponent::CreateGhostCursor()
{

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

	spawnParams.vPosition = pos;
	Vec3 dir = pos - m_lastPlacedPosition;
	dir.z = 0;
	spawnParams.qRotation = Quat::CreateRotationVDir(dir);

	// Spawn the entity
	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		pEntity->CreateComponentClass<CDominoComponent>();
		//auto e = pEntity->CreateComponentClass<CDominoComponent>();

		//Vec3 m_position = e->GetEntity()->GetWorldPos();
	//	Quat m_rotation = e->GetEntity()->GetWorldRotation();
		m_Dominoes.append(pEntity);
	}

	m_lastPlacedPosition = pos;

}
*/
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

	m_pInputComponent->RegisterAction("player", "simulate", [this](int activationMode, float value)
		{
			if (activationMode == eAAM_OnPress)
			{
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
				Undo(History.size());
			}

		});
	m_pInputComponent->BindAction("player", "undo", eAID_KeyboardMouse, EKeyId::eKI_Z);


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
			if (activationMode == eAAM_OnRelease)
			{
				EndSimulation();
				m_isMoving = false;
				m_readyToSelect = false;
				m_readySelectDomino = nullptr;
				if (m_isMoving)
				{
					m_isMoving = false;
					m_readyToSelect = false;
					m_readySelectDomino = nullptr;
				}

				if (m_readyToSelect)
				{
					if (m_isHoveringEntity)
					{

						IEntity* SelectEntity();

					}
				}
				if (m_pGhostFirstDomino)
				{

					if (m_ActiveHistory->m_Dominoes.size() < 1)
					{

						Vec3 dir = GetPositionFromPointer() - m_pGhostFirstDomino->GetWorldPos();
						dir.z = 0;
						dir.normalize();

						PlaceDomino(m_firstPlacedPosition, Quat::CreateRotationVDir(dir));

					}

					DestroyFirstGhost();
				}

				if (m_placementActive)
					if (m_ActiveHistory)
						InsertHistorySet(m_ActiveHistory);

				m_ActiveHistory = nullptr;

				m_placementActive = false;
				m_firstPlaced = false;
			}

			if (m_isSimulating)
				return;

			if (activationMode == eAAM_OnPress)
			{
				m_readyToSelect = false;
				m_readySelectDomino = nullptr;
				if (m_isHoveringEntity)
				{
					m_readyToSelect = true;
					m_readySelectDomino = GetEntityFromPointer();
					m_placementActive = false;

				}
				else
				{
				
					m_SelectedEntities.clear();
					m_placementActive = true;
				}

				if (!m_ActiveHistory)
					m_ActiveHistory = new SHistorySet();

				m_ActiveHistory->m_index = History.size();

			}
		});

	m_pInputComponent->BindAction("player", "select", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);


	CryLog("Player: Successfully bound all controls");

}

void CPlayerComponent::UpdateCameraTargetGoal(float fTime)
{


	Vec3 origin = m_cameraDesiredGoalPosition + Vec3(0,0,100);
	Vec3 dir = Vec3(0, 0, -1) ;

	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(origin, dir * INFINITY, ent_all, rayFlags, &hit, 1);

	
	if (hits >= 1) {
		m_cameraDesiredGoalPosition = hit.pt;
	}
	else {
		m_cameraDesiredGoalPosition.z = 32;
	}
	
	m_cameraDesiredGoalPosition.z = 0;
	if (!GetTacticalCameraMovementInputDirection().IsZero()) {
		m_goalSpeed = m_currViewDistance / (m_maxTacticalDistance + m_minTacticalDistance);
			m_cameraDesiredGoalPosition += ((GetTacticalCameraMovementInputDirection() * m_goalSpeed * m_panSensitivity) * fTime);
	}
	
	Vec3 pos = Lerp(m_cameraCurrentGoalPosition, m_cameraDesiredGoalPosition, fTime  * m_goalTension);
	m_cameraCurrentGoalPosition = pos;
	m_desiredCameraTranform.SetRotation33(Matrix33(IDENTITY));
	m_desiredCameraTranform.SetTranslation(m_cameraCurrentGoalPosition);

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


