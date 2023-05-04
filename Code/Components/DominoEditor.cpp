#include "StdAfx.h"
#include "DominoEditor.h"
#include "Domino.h"

#include <CryInput/IHardwareMouse.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>


void CDominoEditor::Initialize()
{
	m_SelectedDominoes.begin();
	m_SelectedDominoes.resize(9999);
}

void CDominoEditor::DeselectDomino(IEntity* pDomino)
{
	m_SelectedDominoes.erase(pDomino->GetComponent<CDominoComponent>()->m_Index);
	pDomino->GetComponent<CDominoComponent>()->m_isSelected = false;
}

void CDominoEditor::DeselectAllDominoes()
{


}

void CDominoEditor::RemoveDomino(IEntity* pDomino)
{
	gEnv->pEntitySystem->RemoveEntity(pDomino->GetId());
	//m_Dominoes.shrink_to_fit();

}

IEntity* CDominoEditor::SelectDomino(IEntity* pDomino)
{
	m_SelectedDominoes.emplace(pDomino->GetComponent<CDominoComponent>()->m_Index, pDomino);
	pDomino->GetComponent<CDominoComponent>()->m_isSelected = true;
	return pDomino;

}
