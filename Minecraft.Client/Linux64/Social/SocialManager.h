#pragma once

#define MAX_SOCIALPOST_CAPTION 60
#define MAX_SOCIALPOST_DESC 100

enum ESocialNetwork
{
	eFacebook = 0,
	eNumSocialNetworks
};

class CSocialManager
{
private:
	CSocialManager();
	static CSocialManager* m_pInstance;
	DWORD m_dwSocialPostingCapability;
	DWORD m_dwCurrRequestUser;
	enum EState { eStateUnitialised = 0, eStateReady };
	EState m_eCurrState;
	XOVERLAPPED m_Overlapped;
	XSOCIAL_PREVIEWIMAGE m_PostPreviewImage;
	unsigned char* m_pMainImageBuffer;
	DWORD m_dwMainImageBufferSize;
	void DestroyMainPostImage();
	void DestroyPreviewPostImage();
	WCHAR m_wchTitleA[MAX_SOCIALPOST_CAPTION+1];
	WCHAR m_wchCaptionA[MAX_SOCIALPOST_CAPTION+1];
	WCHAR m_wchDescA[MAX_SOCIALPOST_DESC+1];
public:
	static CSocialManager* Instance();
	void Initialise();
	void Tick();
	bool RefreshPostingCapability();
	bool IsTitleAllowedToPostAnything();
	bool IsTitleAllowedToPostImages();
	bool IsTitleAllowedToPostLinks();
	bool AreAllUsersAllowedToPostImages();
	bool PostLinkToSocialNetwork(ESocialNetwork, DWORD, bool);
	bool PostImageToSocialNetwork(ESocialNetwork, DWORD, bool);
	void SetSocialPostText(LPCWSTR, LPCWSTR, LPCWSTR);
};
