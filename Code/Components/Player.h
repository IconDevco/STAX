#pragma once

#include <array>
#include <numeric>

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include "DefaultComponents/Geometry/AdvancedAnimationComponent.h"
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>
#include "DefaultComponents/Physics/CharacterControllerComponent.h"
#include <CryAISystem/Components/IEntityNavigationComponent.h>


#include "CryInput/IInput.h"
#include "CryAction/IActionMapManager.h"


#include "CryCore/Containers/CryArray.h"
#include "PersistantDebug.h"
#include "DominoPlacer.h"
#include "DominoManager.h"
#include "DominoEditor.h"



////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////

class CPlayerComponent final : public IEntityComponent
{

	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	enum class EInputFlag : uint8
	{
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3
	};

	static constexpr EEntityAspects InputAspect = eEA_GameClientD;

	template<typename T, size_t SAMPLES_COUNT>
	class MovingAverage
	{
		static_assert(SAMPLES_COUNT > 0, "SAMPLES_COUNT shall be larger than zero!");

	public:

		MovingAverage()
			: m_values()
			, m_cursor(SAMPLES_COUNT)
			, m_accumulator()
		{
		}

		MovingAverage& Push(const T& value)
		{
			if (m_cursor == SAMPLES_COUNT)
			{
				m_values.fill(value);
				m_cursor = 0;
				m_accumulator = std::accumulate(m_values.begin(), m_values.end(), T(0));
			}
			else
			{
				m_accumulator -= m_values[m_cursor];
				m_values[m_cursor] = value;
				m_accumulator += m_values[m_cursor];
				m_cursor = (m_cursor + 1) % SAMPLES_COUNT;
			}

			return *this;
		}

		T Get() const
		{
			return m_accumulator / T(SAMPLES_COUNT);
		}

		void Reset()
		{
			m_cursor = SAMPLES_COUNT;
		}

	private:

		std::array<T, SAMPLES_COUNT> m_values;
		size_t m_cursor;

		T m_accumulator;
	};



public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() = default;

	// IEntityComponent
	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
	}

	void OnReadyForGameplayOnServer();
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0; }

	void Revive(const Matrix34& transform);




protected:
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);

	void BindInputs();

	void InitializeLocalPlayer();

protected:
	/// Cryengine Shit ///

	struct RemoteReviveParams
	{
		void SerializeWith(TSerialize ser)
		{
			ser.Value("pos", position, 'wrld');
			ser.Value("rot", rotation, 'ori0');
		}

		Vec3 position;
		Quat rotation;
	};
	bool RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel);

	/// ~Cryengine Shit ///

protected:
	IPersistantDebug* debug;

	float waterLevel = 16;
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;

	CEnumFlags<EInputFlag> m_inputFlags;
	Vec2 m_mouseDeltaRotation;

	MovingAverage<Vec2, 10> m_mouseDeltaSmoothingFilter;

	Quat m_lookOrientation = IDENTITY; //!< Should translate to head orientation in the future
	float m_horizontalAngularVelocity = 0;
	MovingAverage<float, 10> m_averagedHorizontalAngularVelocity;

	const char* m_playerName = "Moose";

	////////////////////// MODE /////////////////////////////

protected:


	DynArray<IEntity*> m_JustPlacedDominoes;
	IEntity* m_pFirstPlacedDomino = nullptr;


	void BeginSimulation();
	void EndSimulation();
	void ResetDominoes();

	bool m_isSimulating = false;

	int m_historyStep = 0;

	int m_undoSteps = 0;
	//void Undo(int stepToRemove);

	//void RestartHistory(int undoSteps);

	float m_debugTextOffset = 0;
	void UpdateDebug(float fTime);
	bool m_readyToSelect = false;
	IEntity* m_readySelectDomino = nullptr;



	bool m_isDominoPhysicsEnabled = false;

	//void DisableDominoPhysics();
	//void EnableDominoPhysics();

	IRenderAuxGeom* auxDebug;

	void AddForceToDomino(IEntity* dom, float force);

	enum EToolMode {
		eTM_Editing, //moving, scaling, coloring
		eTM_Placing, //only placing. placing off a domino continues from that line
		eTM_Simulating //clickable and physics enabled
	};

	EToolMode m_activeToolMode = eTM_Editing;
	int m_activeIndex = 0;

	bool moveSingleton = false;
	IEntity* m_lastHoveredDomino = nullptr;
	void ChangeToolMode(int index);

	bool m_isPlacing = false;

	Vec3 m_nextPlacePosition = Vec3(0);

	bool m_isPlacingFromExistingDomino = false;

	bool m_isShiftPressed = false;
	bool m_isAltPressed = false;
	bool m_isCtrlPressed = false;


	float m_scaleModifier = 0.2f;

	Vec3 m_clickPosition = Vec3(0);
	IEntity* m_halfSelectDomino;// = false;

	bool m_isHoveringEntity()
	{
		return GetDominoFromPointer() ? true : false;
	}

	Vec3 m_smoothGoalPosition = Vec3(0);
	Vec3 m_smoothCurrentPosition = Vec3(0);

	IEntity* m_pCursorGhostDomino = nullptr;

	Vec3 m_lastFrameMovePosition;

	Vec3 m_dragStartPosition = Vec3(0);
	bool m_isDragging = false;
	float m_dragThreshold = 0.1f;

	DynArray<IEntity*> m_SelectedDominoes;

	bool m_isMoving = false;
	Vec3 m_tempMoveDir = Vec3(0);

	void UpdateMoveDomino(Vec3 pos, float fTime);

	DynArray<IEntity*> m_Dominoes;



	void MouseDown();
	void MouseUp();



	//SIMULATION

//------------------------------------------------------
//----------------------SNAPSHOT------------------------
//------------------------------------------------------

	enum ESnapshotType
	{
		remove,
		add,
		move,
		paint,
		eST_MAX
	};

	struct Snapshot {
	public:
		DynArray<IEntity*> m_Dominoes;
		DynArray<Vec3> m_positions;
		DynArray<bool> m_deleted;

		ESnapshotType m_snapshotType = eST_MAX;

	};

	

	DynArray<Snapshot*> m_snapshots;
	
	// visible index
	int activeIndex = -1;

	void TakeSnapshot(DynArray<IEntity*>& entities, ESnapshotType snapshotType);

	void Undo(int activeSnapshot);
	void Redo(int nextActiveSnapshot);

	DynArray<IEntity*> m_EntitiesMarkedForDeletion;



	//------------------------------------------------------
	//----------------------PLACEMENT-----------------------
	//------------------------------------------------------
public:
	void UpgradeDominoToDouble(IEntity* pDomino);

	void PlaceDomino(Vec3 pos);

	void CreateOriginGhost(Vec3 origin);
	void UpdateOriginGhost(float fTime);
	void DestroyOriginGhost();

	void CreateCursorGhost(Vec3 origin);
	void UpdateCursorGhost(float fTime);
	void DestroyCursorGhost();

	void UpdateActivePlacementPosition(Vec3 o, float fTime);

protected:
	IEntity* m_pOriginGhostDomino = nullptr;

	float m_placementDistance = .15f;

	int m_totalPlacedDominoes = 0;
	Vec3 m_lastPlacedPosition = Vec3(0);
	//Vec3 m_firstPlacedPosition = Vec3(0);
	Vec3 m_firstPlacedPosition = Vec3(0);
	bool m_firstPlaced = false;
	Quat m_lockedRotation = IDENTITY;
	//Vec3 m_lastPlacedPosition = Vec3(0);
	bool m_placementActive = false;

	int m_dominoesJustPlaced = 0;
	DynArray<IEntity*> m_pDominoesJustPlaced;;

	bool m_isDrawingShape = false;

	void DrawLine(Vec3 start);

	Vec3 lerp(const Vec3& start, const Vec3& end, float t)
	{
		Vec3 result;
		result.x = start.x + t * (end.x - start.x);
		result.y = start.y + t * (end.y - start.y);
		result.z = start.z + t * (end.z - start.z);
		return result;
	}
	/*
	std::vector<Vec3> createPointsAlongLine(const Vec3& start, const Vec3& end, float distance) {
		float lineLength = std::sqrt(
			std::pow(end.x - start.x, 2) +
			std::pow(end.y - start.y, 2) +
			std::pow(end.z - start.z, 2)
		);

		int numPoints = static_cast<int>(lineLength / distance) + 1;

		std::vector<Vec3> points;
		points.reserve(numPoints);

		for (int i = 0; i < numPoints; ++i) {
			float t = static_cast<float>(i) / (numPoints - 1);  // Calculate interpolation parameter
			Vec3 point = lerp(start, end, t);  // Interpolate point
			points.push_back(point);
		}

		return points;
		}
		*/

	

//------------------------------------------------------
//------------------CAMERA & MOVEMENT-------------------
//------------------------------------------------------
public:
	void UpdateZoom(float fTime);

	void UpdateCameraTargetGoal(float fTime);

	void UpdateTacticalViewDirection(float frameTime);

	void UpdateCamera(float frameTime);

	Vec3 GetTacticalCameraMovementInputDirection();


protected:
	bool m_lookActive;
	const float m_rotationSensitivity = 0.002f;
	float m_scrollY;
	float m_scrollSpeedMultiplier = 0.2f;

	float m_desiredViewDistance = 2;
	float m_currViewDistance = 2;

	float m_zoomTension = 0.3f;//1.7f;

	float m_minViewDistance = .2f;

	float m_minTacticalDistance = .2f;
	float m_maxTacticalDistance = 12;

	Vec3 m_cameraCurrentPosition;
	Vec3 m_cameraGoalPosition;

	float m_goalTension = 2.f;
	float m_goalSpeed = 2.f;

	Matrix34 m_desiredCameraTranform;

	float m_scrollSensitivity;
	float m_panSensitivity = 70;

	Vec3 m_moveGoalPosition = Vec3(0);
	Vec3 m_moveCurrentPosition = Vec3(0);



//------------------------------------------------------
//-----------------------PAINTING------------------------
//------------------------------------------------------




//------------------------------------------------------
//-----------------------EDITING------------------------
//------------------------------------------------------
public:
		void DeselectDomino(IEntity* pDomino);
		void DeselectAllDominoes();
		void RemoveDomino(IEntity* pDomino);
		//void DeleteDomino(IEntity* pDomino) { RemoveDomino(pDomino); }
		
		//This will hide dominoes and restore them if undone
		void DeleteDominoes(DynArray<IEntity*> pDominoes);// 
		void UndeleteDominoes(DynArray<IEntity*> pDominoes);// 
	
		void DestroyDomino(IEntity* pDomino) { RemoveDomino(pDomino); }

		IEntity* SelectDomino(IEntity* pDomino);

		void DrawMarquee();
		void BeginMarquee();

		void EndSelection();

		bool IsPointWithinMarquee(const Vec3& pt, const Vec2& start, const Vec2& end)
		{
			float minX = std::min(start.x, end.x);
			float maxX = std::max(start.x, end.x);
			float minY = std::min(start.y, end.y);
			float maxY = std::max(start.y, end.y);

			return (pt.x >= minX && pt.x <= maxX &&
				pt.y >= minY && pt.y <= maxY);
		}


		bool m_isMarquee;
		Vec2 m_marqueeStart;
		Vec2 m_marqueeEnd;

		Vec3 m_marqueeOrigin = Vec3(0);
		Vec2 GetCursorScreenPosition();

		void CopySelection();
		
		void MoveSelection();

		Vec2 debugScreenPos;

//------------------------------------------------------
//-----------------------CURSOR-------------------------
//------------------------------------------------------
public:
	void BeginSmoothPosition(Vec3 origin);
	void UpdateSmoothPoisition(float fTime);
	Vec3 GetSmoothPosition();

	void ShowCursor();
	void HideCursor();



	Vec3 GetPositionFromPointer(bool ignoreDominoes);

	IEntity* GetDominoFromPointer();


	void DebugCursorElements(float fTime);

protected:
	bool m_mouseDown = false;


	//MANAGEMENT


};

