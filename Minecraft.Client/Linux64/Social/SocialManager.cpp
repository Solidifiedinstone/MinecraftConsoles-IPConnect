#include "stdafx.h"
#include "SocialManager.h"

CSocialManager* CSocialManager::m_pInstance = nullptr;

CSocialManager::CSocialManager()
{
	m_dwSocialPostingCapability = 0;
	m_dwCurrRequestUser = 0;
	m_eCurrState = eStateUnitialised;
	memset(&m_Overlapped, 0, sizeof(m_Overlapped));
	memset(&m_PostPreviewImage, 0, sizeof(m_PostPreviewImage));
	m_pMainImageBuffer = nullptr;
	m_dwMainImageBufferSize = 0;
	memset(m_wchTitleA, 0, sizeof(m_wchTitleA));
	memset(m_wchCaptionA, 0, sizeof(m_wchCaptionA));
	memset(m_wchDescA, 0, sizeof(m_wchDescA));
}

CSocialManager* CSocialManager::Instance()
{
	if (m_pInstance == nullptr)
	{
		m_pInstance = new CSocialManager();
	}
	return m_pInstance;
}

void CSocialManager::Initialise()
{
	m_eCurrState = eStateReady;
}

void CSocialManager::Tick()
{
	// No-op on Linux
}

bool CSocialManager::RefreshPostingCapability()
{
	return false;
}

bool CSocialManager::IsTitleAllowedToPostAnything()
{
	return false;
}

bool CSocialManager::IsTitleAllowedToPostImages()
{
	return false;
}

bool CSocialManager::IsTitleAllowedToPostLinks()
{
	return false;
}

bool CSocialManager::AreAllUsersAllowedToPostImages()
{
	return false;
}

bool CSocialManager::PostLinkToSocialNetwork(ESocialNetwork eSocialNetwork, DWORD dwUserIndex, bool bUsingKinect)
{
	return false;
}

bool CSocialManager::PostImageToSocialNetwork(ESocialNetwork eSocialNetwork, DWORD dwUserIndex, bool bUsingKinect)
{
	return false;
}


void CSocialManager::DestroyMainPostImage()
{
	if (m_pMainImageBuffer)
	{
		delete[] m_pMainImageBuffer;
		m_pMainImageBuffer = nullptr;
		m_dwMainImageBufferSize = 0;
	}
}

void CSocialManager::DestroyPreviewPostImage()
{
	// No-op on Linux
}
