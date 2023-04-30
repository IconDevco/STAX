#include "StdAfx.h"
#include "SnapshotManager.h"

void CSnapshotManager::Initialize()
{
}

void CSnapshotManager::AddSnapshot()
{
    if (currentIndex < currentIndices)
    {
        CutSnapshots();

        UpdateSnapshotStates();
        currentIndices = currentIndex;
    }

    currentIndex += 1;

}

void CSnapshotManager::UpdateSnapshotStates()
{
    for(SSnapshot* snapshot : m_Snapshots)
    {
       
        CheckActivity(snapshot);
    }

}

void CSnapshotManager::UndoSnapshot()
{
}

void CSnapshotManager::UndoSnapshot(int index)
{
}

void CSnapshotManager::MarkSnapshot()
{
    for (SSnapshot* snapshot : m_Snapshots)
    {

        CryLog("Scanning: " + snapshot->cacheIndex);
        if (snapshot->cacheIndex > currentSelectedIndex)
        {
            CryLog("Marking: " + snapshot->cacheIndex);
            snapshot->markedForDeletion = true;
        }

    }
}

void CSnapshotManager::CheckActivity(SSnapshot* snapshot)
{
 
        //snapshot.transform.parent = this.transform;
        //snapshot.SetActive(false);
        if (currentSelectedIndex == snapshot->cacheIndex)
            SetSnapshot();
        if (snapshot->markedForDeletion) 
            RemoveFromList();
    
}

void CSnapshotManager::SetSnapshot()
{

}
