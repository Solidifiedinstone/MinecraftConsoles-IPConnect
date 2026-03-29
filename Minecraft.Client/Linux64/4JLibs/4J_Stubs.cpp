/*
 * 4J_Stubs.cpp
 *
 * Stub / no-op implementations for all 4J library classes used by
 * the Linux 64-bit port of Minecraft Console Edition.
 *
 * Every public method declared in the four 4J headers is implemented
 * here so that the game links without pulling in platform-specific
 * libraries.  All functions are intentionally minimal: they return safe
 * default values and perform no real work.
 */

#include "stdafx.h"

#include <cstring>
#include <string>

/* ====================================================================
 * Global singleton instances
 * ==================================================================== */

C4JRender  RenderManager;
C_4JInput  InputManager;
C4JStorage StorageManager;
C_4JProfile ProfileManager;

/* ********************************************************************
 *
 *  C4JRender  --  Render Manager stubs
 *
 * ********************************************************************/

// --- Core -----------------------------------------------------------

void C4JRender::Initialise(void * /*pDevice*/, void * /*pSwapChain*/) {}
void C4JRender::InitialiseContext() {}
void C4JRender::Tick() {}
void C4JRender::UpdateGamma(unsigned short /*usGamma*/) {}
void C4JRender::StartFrame() {}
void C4JRender::Present() {}
void C4JRender::Clear(int /*flags*/, D3D11_RECT * /*pRect*/) {}
void C4JRender::SetClearColour(const float /*colourRGBA*/[4]) {}
void C4JRender::DoScreenGrabOnNextPresent() {}
bool C4JRender::IsWidescreen() { return true; }
bool C4JRender::IsHiDef()      { return true; }

void C4JRender::CaptureThumbnail(ImageFileBuffer * /*pngOut*/) {}
void C4JRender::CaptureScreen(ImageFileBuffer * /*jpgOut*/, XSOCIAL_PREVIEWIMAGE * /*previewOut*/) {}

void C4JRender::BeginConditionalSurvey(int /*identifier*/) {}
void C4JRender::EndConditionalSurvey() {}
void C4JRender::BeginConditionalRendering(int /*identifier*/) {}
void C4JRender::EndConditionalRendering() {}

// --- Matrix stack ---------------------------------------------------

void C4JRender::MatrixMode(int /*type*/) {}
void C4JRender::MatrixSetIdentity() {}
void C4JRender::MatrixTranslate(float /*x*/, float /*y*/, float /*z*/) {}
void C4JRender::MatrixRotate(float /*angle*/, float /*x*/, float /*y*/, float /*z*/) {}
void C4JRender::MatrixScale(float /*x*/, float /*y*/, float /*z*/) {}
void C4JRender::MatrixPerspective(float /*fovy*/, float /*aspect*/, float /*zNear*/, float /*zFar*/) {}
void C4JRender::MatrixOrthogonal(float /*left*/, float /*right*/, float /*bottom*/, float /*top*/, float /*zNear*/, float /*zFar*/) {}
void C4JRender::MatrixPop() {}
void C4JRender::MatrixPush() {}
void C4JRender::MatrixMult(float * /*mat*/) {}

const float *C4JRender::MatrixGet(int /*type*/)
{
    // 4x4 identity matrix (column-major)
    static float ident[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return ident;
}

void C4JRender::Set_matrixDirty() {}

// --- Vertex data ----------------------------------------------------

void C4JRender::DrawVertices(ePrimitiveType /*PrimitiveType*/, int /*count*/, void * /*dataIn*/,
                             eVertexType /*vType*/, C4JRender::ePixelShaderType /*psType*/) {}

void C4JRender::DrawVertexBuffer(ePrimitiveType /*PrimitiveType*/, int /*count*/, void * /*buffer*/,
                                 C4JRender::eVertexType /*vType*/, C4JRender::ePixelShaderType /*psType*/) {}

// --- Command buffers ------------------------------------------------

void C4JRender::CBuffLockStaticCreations() {}
int  C4JRender::CBuffCreate(int /*count*/) { return 0; }
void C4JRender::CBuffDelete(int /*first*/, int /*count*/) {}
void C4JRender::CBuffStart(int /*index*/, bool /*full*/) {}
void C4JRender::CBuffClear(int /*index*/) {}
int  C4JRender::CBuffSize(int /*index*/) { return 0; }
void C4JRender::CBuffEnd() {}
bool C4JRender::CBuffCall(int /*index*/, bool /*full*/) { return false; }
void C4JRender::CBuffTick() {}
void C4JRender::CBuffDeferredModeStart() {}
void C4JRender::CBuffDeferredModeEnd() {}

// --- Textures -------------------------------------------------------

int C4JRender::TextureCreate()
{
    static int s_nextTextureId = 1;
    return s_nextTextureId++;
}

void C4JRender::TextureFree(int /*idx*/) {}
void C4JRender::TextureBind(int /*idx*/) {}
void C4JRender::TextureBindVertex(int /*idx*/) {}
void C4JRender::TextureSetTextureLevels(int /*levels*/) {}
int  C4JRender::TextureGetTextureLevels() { return 0; }
void C4JRender::TextureData(int /*width*/, int /*height*/, void * /*data*/, int /*level*/, eTextureFormat /*format*/) {}
void C4JRender::TextureDataUpdate(int /*xoffset*/, int /*yoffset*/, int /*width*/, int /*height*/, void * /*data*/, int /*level*/) {}
void C4JRender::TextureSetParam(int /*param*/, int /*value*/) {}
void C4JRender::TextureDynamicUpdateStart() {}
void C4JRender::TextureDynamicUpdateEnd() {}

HRESULT C4JRender::LoadTextureData(const char * /*szFilename*/, D3DXIMAGE_INFO *pSrcInfo, int ** /*ppDataOut*/)
{
    if (pSrcInfo) { memset(pSrcInfo, 0, sizeof(D3DXIMAGE_INFO)); }
    return S_OK;
}

HRESULT C4JRender::LoadTextureData(BYTE * /*pbData*/, DWORD /*dwBytes*/, D3DXIMAGE_INFO *pSrcInfo, int ** /*ppDataOut*/)
{
    if (pSrcInfo) { memset(pSrcInfo, 0, sizeof(D3DXIMAGE_INFO)); }
    return S_OK;
}

HRESULT C4JRender::SaveTextureData(const char * /*szFilename*/, D3DXIMAGE_INFO * /*pSrcInfo*/, int * /*ppDataOut*/)
{
    return S_OK;
}

HRESULT C4JRender::SaveTextureDataToMemory(void * /*pOutput*/, int /*outputCapacity*/, int *outputLength, int /*width*/, int /*height*/, int * /*ppDataIn*/)
{
    if (outputLength) *outputLength = 0;
    return S_OK;
}

void  C4JRender::TextureGetStats() {}
void *C4JRender::TextureGetTexture(int /*idx*/) { return nullptr; }

// --- State control --------------------------------------------------

void C4JRender::StateSetColour(float /*r*/, float /*g*/, float /*b*/, float /*a*/) {}
void C4JRender::StateSetDepthMask(bool /*enable*/) {}
void C4JRender::StateSetBlendEnable(bool /*enable*/) {}
void C4JRender::StateSetBlendFunc(int /*src*/, int /*dst*/) {}
void C4JRender::StateSetBlendFactor(unsigned int /*colour*/) {}
void C4JRender::StateSetAlphaFunc(int /*func*/, float /*param*/) {}
void C4JRender::StateSetDepthFunc(int /*func*/) {}
void C4JRender::StateSetFaceCull(bool /*enable*/) {}
void C4JRender::StateSetFaceCullCW(bool /*enable*/) {}
void C4JRender::StateSetLineWidth(float /*width*/) {}
void C4JRender::StateSetWriteEnable(bool /*red*/, bool /*green*/, bool /*blue*/, bool /*alpha*/) {}
void C4JRender::StateSetDepthTestEnable(bool /*enable*/) {}
void C4JRender::StateSetAlphaTestEnable(bool /*enable*/) {}
void C4JRender::StateSetDepthSlopeAndBias(float /*slope*/, float /*bias*/) {}
void C4JRender::StateSetFogEnable(bool /*enable*/) {}
void C4JRender::StateSetFogMode(int /*mode*/) {}
void C4JRender::StateSetFogNearDistance(float /*dist*/) {}
void C4JRender::StateSetFogFarDistance(float /*dist*/) {}
void C4JRender::StateSetFogDensity(float /*density*/) {}
void C4JRender::StateSetFogColour(float /*red*/, float /*green*/, float /*blue*/) {}
void C4JRender::StateSetLightingEnable(bool /*enable*/) {}
void C4JRender::StateSetVertexTextureUV(float /*u*/, float /*v*/) {}
void C4JRender::StateSetLightColour(int /*light*/, float /*red*/, float /*green*/, float /*blue*/) {}
void C4JRender::StateSetLightAmbientColour(float /*red*/, float /*green*/, float /*blue*/) {}
void C4JRender::StateSetLightDirection(int /*light*/, float /*x*/, float /*y*/, float /*z*/) {}
void C4JRender::StateSetLightEnable(int /*light*/, bool /*enable*/) {}
void C4JRender::StateSetViewport(eViewportType /*viewportType*/) {}
void C4JRender::StateSetEnableViewportClipPlanes(bool /*enable*/) {}
void C4JRender::StateSetTexGenCol(int /*col*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/, bool /*eyeSpace*/) {}
void C4JRender::StateSetStencil(int /*Function*/, uint8_t /*stencil_ref*/, uint8_t /*stencil_func_mask*/, uint8_t /*stencil_write_mask*/) {}
void C4JRender::StateSetForceLOD(int /*LOD*/) {}

// --- Event tracking -------------------------------------------------

void C4JRender::BeginEvent(LPCWSTR /*eventName*/) {}
void C4JRender::EndEvent() {}

// --- PLM event handling ---------------------------------------------

void C4JRender::Suspend() {}
bool C4JRender::Suspended() { return false; }
void C4JRender::Resume() {}


/* ********************************************************************
 *
 *  C_4JInput  --  Input Manager stubs
 *
 * ********************************************************************/

void C_4JInput::Initialise(int /*iInputStateC*/, unsigned char /*ucMapC*/, unsigned char /*ucActionC*/, unsigned char /*ucMenuActionC*/) {}
void C_4JInput::Tick() {}
void C_4JInput::SetDeadzoneAndMovementRange(unsigned int /*uiDeadzone*/, unsigned int /*uiMovementRangeMax*/) {}

void         C_4JInput::SetGameJoypadMaps(unsigned char /*ucMap*/, unsigned char /*ucAction*/, unsigned int /*uiActionVal*/) {}
unsigned int C_4JInput::GetGameJoypadMaps(unsigned char /*ucMap*/, unsigned char /*ucAction*/) { return 0; }

void          C_4JInput::SetJoypadMapVal(int /*iPad*/, unsigned char /*ucMap*/) {}
unsigned char C_4JInput::GetJoypadMapVal(int /*iPad*/) { return 0; }

void C_4JInput::SetJoypadSensitivity(int /*iPad*/, float /*fSensitivity*/) {}

unsigned int C_4JInput::GetValue(int /*iPad*/, unsigned char /*ucAction*/, bool /*bRepeat*/) { return 0; }

bool C_4JInput::ButtonPressed(int /*iPad*/, unsigned char /*ucAction*/)  { return false; }
bool C_4JInput::ButtonReleased(int /*iPad*/, unsigned char /*ucAction*/) { return false; }
bool C_4JInput::ButtonDown(int /*iPad*/, unsigned char /*ucAction*/)     { return false; }

void C_4JInput::SetJoypadStickAxisMap(int /*iPad*/, unsigned int /*uiFrom*/, unsigned int /*uiTo*/) {}
void C_4JInput::SetJoypadStickTriggerMap(int /*iPad*/, unsigned int /*uiFrom*/, unsigned int /*uiTo*/) {}

void C_4JInput::SetKeyRepeatRate(float /*fRepeatDelaySecs*/, float /*fRepeatRateSecs*/) {}
void C_4JInput::SetDebugSequence(const char * /*chSequenceA*/, int(*/*Func*/)(LPVOID), LPVOID /*lpParam*/) {}

FLOAT C_4JInput::GetIdleSeconds(int /*iPad*/) { return 0.0f; }

bool C_4JInput::IsPadConnected(int iPad)
{
    // Pad 0 is always "connected" so the game has a primary controller
    return (iPad == 0);
}

float         C_4JInput::GetJoypadStick_LX(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_LY(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_RX(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_RY(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
unsigned char C_4JInput::GetJoypadLTrigger(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0; }
unsigned char C_4JInput::GetJoypadRTrigger(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0; }

void C_4JInput::SetMenuDisplayed(int /*iPad*/, bool /*bVal*/) {}

EKeyboardResult C_4JInput::RequestKeyboard(LPCWSTR /*Title*/, LPCWSTR /*Text*/, DWORD /*dwPad*/, UINT /*uiMaxChars*/,
                                           int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/, C_4JInput::EKeyboardMode /*eMode*/)
{
    return EKeyboard_Cancelled;
}

void C_4JInput::GetText(uint16_t * /*UTF16String*/) {}

bool C_4JInput::VerifyStrings(WCHAR ** /*pwStringA*/, int /*iStringC*/,
                              int(*/*Func*/)(LPVOID, STRING_VERIFY_RESPONSE *), LPVOID /*lpParam*/)
{
    return true;
}

void C_4JInput::CancelQueuedVerifyStrings(int(*/*Func*/)(LPVOID, STRING_VERIFY_RESPONSE *), LPVOID /*lpParam*/) {}
void C_4JInput::CancelAllVerifyInProgress() {}


/* ********************************************************************
 *
 *  C4JStorage  --  Storage Manager stubs
 *
 * ********************************************************************/

C4JStorage::C4JStorage()
    : m_pStringTable(nullptr)
{
}

void C4JStorage::Tick() {}

// --- Messages -------------------------------------------------------

C4JStorage::EMessageResult C4JStorage::RequestMessageBox(UINT /*uiTitle*/, UINT /*uiText*/, UINT * /*uiOptionA*/, UINT /*uiOptionC*/,
                                                         DWORD /*dwPad*/, int(*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
                                                         LPVOID /*lpParam*/, C4JStringTable * /*pStringTable*/,
                                                         WCHAR * /*pwchFormatString*/, DWORD /*dwFocusButton*/)
{
    return EMessage_ResultAccept;
}

C4JStorage::EMessageResult C4JStorage::GetMessageBoxResult() { return EMessage_ResultAccept; }

// --- Save device ----------------------------------------------------

bool C4JStorage::SetSaveDevice(int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/, bool /*bForceResetOfSaveDevice*/) { return true; }

// --- Save game ------------------------------------------------------

void C4JStorage::Init(unsigned int /*uiSaveVersion*/, LPCWSTR /*pwchDefaultSaveName*/, char * /*pszSavePackName*/,
                      int /*iMinimumSaveSize*/, int(*/*Func*/)(LPVOID, const ESavingMessage, int), LPVOID /*lpParam*/, LPCSTR /*szGroupID*/) {}

void C4JStorage::ResetSaveData() {}
void C4JStorage::SetDefaultSaveNameForKeyboardDisplay(LPCWSTR /*pwchDefaultSaveName*/) {}
void C4JStorage::SetSaveTitle(LPCWSTR /*pwchDefaultSaveName*/) {}
bool C4JStorage::GetSaveUniqueNumber(INT * /*piVal*/) { return false; }
bool C4JStorage::GetSaveUniqueFilename(char * /*pszName*/) { return false; }
void C4JStorage::SetSaveUniqueFilename(char * /*szFilename*/) {}
void C4JStorage::SetState(ESaveGameControlState /*eControlState*/, int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/) {}
void C4JStorage::SetSaveDisabled(bool /*bDisable*/) {}
bool C4JStorage::GetSaveDisabled() { return false; }
unsigned int C4JStorage::GetSaveSize() { return 0; }
void C4JStorage::GetSaveData(void * /*pvData*/, unsigned int * /*puiBytes*/) {}
PVOID C4JStorage::AllocateSaveData(unsigned int /*uiBytes*/) { return nullptr; }
void C4JStorage::SetSaveImages(PBYTE /*pbThumbnail*/, DWORD /*dwThumbnailBytes*/, PBYTE /*pbImage*/, DWORD /*dwImageBytes*/,
                               PBYTE /*pbTextData*/, DWORD /*dwTextDataBytes*/) {}

C4JStorage::ESaveGameState C4JStorage::SaveSaveData(int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

void C4JStorage::CopySaveDataToNewSave(PBYTE /*pbThumbnail*/, DWORD /*cbThumbnail*/, WCHAR * /*wchNewName*/,
                                       int (*/*Func*/)(LPVOID lpParam, bool), LPVOID /*lpParam*/) {}

void C4JStorage::SetSaveDeviceSelected(unsigned int /*uiPad*/, bool /*bSelected*/) {}
bool C4JStorage::GetSaveDeviceSelected(unsigned int /*iPad*/) { return false; }

C4JStorage::ESaveGameState C4JStorage::DoesSaveExist(bool *pbExists)
{
    if (pbExists) *pbExists = false;
    return ESaveGame_Idle;
}

bool C4JStorage::EnoughSpaceForAMinSaveGame() { return true; }

void C4JStorage::SetSaveMessageVPosition(float /*fY*/) {}

C4JStorage::ESaveGameState C4JStorage::GetSavesInfo(int /*iPad*/, int (*/*Func*/)(LPVOID lpParam, SAVE_DETAILS *pSaveDetails, const bool),
                                                    LPVOID /*lpParam*/, char * /*pszSavePackName*/)
{
    return ESaveGame_Idle;
}

PSAVE_DETAILS C4JStorage::ReturnSavesInfo() { return nullptr; }
void C4JStorage::ClearSavesInfo() {}

C4JStorage::ESaveGameState C4JStorage::LoadSaveDataThumbnail(PSAVE_INFO /*pSaveInfo*/,
                                                             int(*/*Func*/)(LPVOID lpParam, PBYTE pbThumbnail, DWORD dwThumbnailBytes),
                                                             LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

void C4JStorage::GetSaveCacheFileInfo(DWORD /*dwFile*/, XCONTENT_DATA & /*xContentData*/) {}
void C4JStorage::GetSaveCacheFileInfo(DWORD /*dwFile*/, PBYTE * /*ppbImageData*/, DWORD * /*pdwImageBytes*/) {}

C4JStorage::ESaveGameState C4JStorage::LoadSaveData(PSAVE_INFO /*pSaveInfo*/,
                                                    int(*/*Func*/)(LPVOID lpParam, const bool, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

C4JStorage::ESaveGameState C4JStorage::DeleteSaveData(PSAVE_INFO /*pSaveInfo*/,
                                                      int(*/*Func*/)(LPVOID lpParam, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

// --- DLC ------------------------------------------------------------

void C4JStorage::RegisterMarketplaceCountsCallback(int (*/*Func*/)(LPVOID lpParam, C4JStorage::DLC_TMS_DETAILS *, int), LPVOID /*lpParam*/) {}
void C4JStorage::SetDLCPackageRoot(char * /*pszDLCRoot*/) {}

C4JStorage::EDLCStatus C4JStorage::GetDLCOffers(int /*iPad*/, int(*/*Func*/)(LPVOID, int, DWORD, int), LPVOID /*lpParam*/, DWORD /*dwOfferTypesBitmask*/)
{
    return EDLC_Idle;
}

DWORD C4JStorage::CancelGetDLCOffers() { return 0; }
void  C4JStorage::ClearDLCOffers() {}

XMARKETPLACE_CONTENTOFFER_INFO& C4JStorage::GetOffer(DWORD /*dw*/)
{
    static XMARKETPLACE_CONTENTOFFER_INFO s_dummyOffer;
    static bool s_init = false;
    if (!s_init) { memset(&s_dummyOffer, 0, sizeof(s_dummyOffer)); s_init = true; }
    return s_dummyOffer;
}

int C4JStorage::GetOfferCount() { return 0; }

DWORD C4JStorage::InstallOffer(int /*iOfferIDC*/, uint64_t * /*ullOfferIDA*/, int(*/*Func*/)(LPVOID, int, int), LPVOID /*lpParam*/, bool /*bTrial*/)
{
    return 0;
}

DWORD C4JStorage::GetAvailableDLCCount(int /*iPad*/) { return 0; }

C4JStorage::EDLCStatus C4JStorage::GetInstalledDLC(int /*iPad*/, int(*/*Func*/)(LPVOID, int, int), LPVOID /*lpParam*/)
{
    return EDLC_Idle;
}

XCONTENT_DATA& C4JStorage::GetDLC(DWORD /*dw*/)
{
    static XCONTENT_DATA s_dummyDLC;
    static bool s_init = false;
    if (!s_init) { memset(&s_dummyDLC, 0, sizeof(s_dummyDLC)); s_init = true; }
    return s_dummyDLC;
}

DWORD C4JStorage::MountInstalledDLC(int /*iPad*/, DWORD /*dwDLC*/, int(*/*Func*/)(LPVOID, int, DWORD, DWORD), LPVOID /*lpParam*/, LPCSTR /*szMountDrive*/)
{
    return 0;
}

DWORD C4JStorage::UnmountInstalledDLC(LPCSTR /*szMountDrive*/) { return 0; }

void C4JStorage::GetMountedDLCFileList(const char * /*szMountDrive*/, std::vector<std::string>& /*fileList*/) {}

std::string C4JStorage::GetMountedPath(std::string /*szMount*/)
{
    return std::string();
}

// --- TMS / Global title storage -------------------------------------

C4JStorage::ETMSStatus C4JStorage::ReadTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, C4JStorage::eTMS_FileType /*eFileType*/,
                                               WCHAR * /*pwchFilename*/, BYTE ** /*ppBuffer*/, DWORD * /*pdwBufferSize*/,
                                               int(*/*Func*/)(LPVOID, WCHAR *, int, bool, int), LPVOID /*lpParam*/, int /*iAction*/)
{
    return ETMSStatus_Idle;
}

bool C4JStorage::WriteTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, WCHAR * /*pwchFilename*/, BYTE * /*pBuffer*/, DWORD /*dwBufferSize*/)
{
    return false;
}

bool C4JStorage::DeleteTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, WCHAR * /*pwchFilename*/)
{
    return false;
}

void C4JStorage::StoreTMSPathName(WCHAR * /*pwchName*/) {}

// --- TMS++ ----------------------------------------------------------

C4JStorage::ETMSStatus C4JStorage::TMSPP_ReadFile(int /*iPad*/, C4JStorage::eGlobalStorage /*eStorageFacility*/,
                                                  C4JStorage::eTMS_FILETYPEVAL /*eFileTypeVal*/, LPCSTR /*szFilename*/,
                                                  int(*/*Func*/)(LPVOID, int, int, PTMSPP_FILEDATA, LPCSTR), LPVOID /*lpParam*/, int /*iUserData*/)
{
    return ETMSStatus_Idle;
}

// --- CRC ------------------------------------------------------------

unsigned int C4JStorage::CRC(unsigned char * /*buf*/, int /*len*/) { return 0; }


/* ********************************************************************
 *
 *  C_4JProfile  --  Profile Manager stubs
 *
 * ********************************************************************/

void C_4JProfile::Initialise(DWORD /*dwTitleID*/, DWORD /*dwOfferID*/, unsigned short /*usProfileVersion*/,
                             UINT /*uiProfileValuesC*/, UINT /*uiProfileSettingsC*/, DWORD * /*pdwProfileSettingsA*/,
                             int /*iGameDefinedDataSizeX4*/, unsigned int * /*puiGameDefinedDataChangedBitmask*/) {}

void C_4JProfile::SetTrialTextStringTable(CXuiStringTable * /*pStringTable*/, int /*iAccept*/, int /*iReject*/) {}
void C_4JProfile::SetTrialAwardText(eAwardType /*AwardType*/, int /*iTitle*/, int /*iText*/) {}

int  C_4JProfile::GetLockedProfile() { return 0; }
void C_4JProfile::SetLockedProfile(int /*iProf*/) {}

bool C_4JProfile::IsSignedIn(int iQuadrant)
{
    // Quadrant / pad 0 is always "signed in" so the game can proceed
    return (iQuadrant == 0);
}

bool C_4JProfile::IsSignedInLive(int /*iProf*/) { return false; }
bool C_4JProfile::IsGuest(int /*iQuadrant*/)    { return false; }

UINT C_4JProfile::RequestSignInUI(bool /*bFromInvite*/, bool /*bLocalGame*/, bool /*bNoGuestsAllowed*/,
                                  bool /*bMultiplayerSignIn*/, bool /*bAddUser*/,
                                  int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

UINT C_4JProfile::DisplayOfflineProfile(int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

UINT C_4JProfile::RequestConvertOfflineToGuestUI(int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

void C_4JProfile::SetPrimaryPlayerChanged(bool /*bVal*/) {}
bool C_4JProfile::QuerySigninStatus() { return true; }

void C_4JProfile::GetXUID(int /*iPad*/, PlayerUID *pXuid, bool /*bOnlineXuid*/)
{
    if (pXuid) { memset(pXuid, 0, sizeof(PlayerUID)); }
}

BOOL C_4JProfile::AreXUIDSEqual(PlayerUID xuid1, PlayerUID xuid2)
{
    return (xuid1 == xuid2) ? TRUE : FALSE;
}

BOOL C_4JProfile::XUIDIsGuest(PlayerUID /*xuid*/) { return FALSE; }

bool C_4JProfile::AllowedToPlayMultiplayer(int /*iProf*/) { return true; }

bool C_4JProfile::GetChatAndContentRestrictions(int /*iPad*/, bool *pbChatRestricted, bool *pbContentRestricted, int *piAge)
{
    if (pbChatRestricted)    *pbChatRestricted    = false;
    if (pbContentRestricted) *pbContentRestricted = false;
    if (piAge)               *piAge               = 18;
    return false;
}

void C_4JProfile::StartTrialGame() {}

void C_4JProfile::AllowedPlayerCreatedContent(int /*iPad*/, bool /*thisQuadrantOnly*/, BOOL *allAllowed, BOOL *friendsAllowed)
{
    if (allAllowed)     *allAllowed     = TRUE;
    if (friendsAllowed) *friendsAllowed = TRUE;
}

BOOL C_4JProfile::CanViewPlayerCreatedContent(int /*iPad*/, bool /*thisQuadrantOnly*/, PPlayerUID /*pXuids*/, DWORD /*dwXuidCount*/)
{
    return TRUE;
}

void C_4JProfile::ShowProfileCard(int /*iPad*/, PlayerUID /*targetUid*/) {}

bool C_4JProfile::GetProfileAvatar(int /*iPad*/, int(*/*Func*/)(LPVOID lpParam, PBYTE pbThumbnail, DWORD dwThumbnailBytes), LPVOID /*lpParam*/)
{
    return false;
}

void C_4JProfile::CancelProfileAvatarRequest() {}

// --- SYS ------------------------------------------------------------

int  C_4JProfile::GetPrimaryPad() { return 0; }
void C_4JProfile::SetPrimaryPad(int /*iPad*/) {}

char *C_4JProfile::GetGamertag(int /*iPad*/)
{
    static char s_gamertag[] = "Player";
    return s_gamertag;
}

wstring C_4JProfile::GetDisplayName(int /*iPad*/)
{
    return L"Player";
}

bool C_4JProfile::IsFullVersion() { return true; }

void C_4JProfile::SetSignInChangeCallback(void (*/*Func*/)(LPVOID, bool, unsigned int), LPVOID /*lpParam*/) {}
void C_4JProfile::SetNotificationsCallback(void (*/*Func*/)(LPVOID, DWORD, unsigned int), LPVOID /*lpParam*/) {}

bool C_4JProfile::RegionIsNorthAmerica() { return false; }
bool C_4JProfile::LocaleIsUSorCanada()   { return false; }

HRESULT C_4JProfile::GetLiveConnectionStatus() { return E_FAIL; }

bool C_4JProfile::IsSystemUIDisplayed() { return false; }

void C_4JProfile::SetProfileReadErrorCallback(void (*/*Func*/)(LPVOID), LPVOID /*lpParam*/) {}

// --- Profile data ---------------------------------------------------

int C_4JProfile::SetDefaultOptionsCallback(int(*/*Func*/)(LPVOID, PROFILESETTINGS *, const int iPad), LPVOID /*lpParam*/) { return 0; }
int C_4JProfile::SetOldProfileVersionCallback(int(*/*Func*/)(LPVOID, unsigned char *, const unsigned short, const int), LPVOID /*lpParam*/) { return 0; }

C_4JProfile::PROFILESETTINGS *C_4JProfile::GetDashboardProfileSettings(int /*iPad*/) { return nullptr; }

void C_4JProfile::WriteToProfile(int /*iQuadrant*/, bool /*bGameDefinedDataChanged*/, bool /*bOverride5MinuteLimitOnProfileWrites*/) {}
void C_4JProfile::ForceQueuedProfileWrites(int /*iPad*/) {}
void *C_4JProfile::GetGameDefinedProfileData(int /*iQuadrant*/) { return nullptr; }
void C_4JProfile::ResetProfileProcessState() {}
void C_4JProfile::Tick() {}

// --- Achievements & Awards ------------------------------------------

void C_4JProfile::RegisterAward(int /*iAwardNumber*/, int /*iGamerconfigID*/, eAwardType /*eType*/, bool /*bLeaderboardAffected*/,
                                CXuiStringTable * /*pStringTable*/, int /*iTitleStr*/, int /*iTextStr*/, int /*iAcceptStr*/,
                                char * /*pszThemeName*/, unsigned int /*uiThemeSize*/) {}

int       C_4JProfile::GetAwardId(int /*iAwardNumber*/) { return 0; }
eAwardType C_4JProfile::GetAwardType(int /*iAwardNumber*/) { return eAwardType_Achievement; }
bool      C_4JProfile::CanBeAwarded(int /*iQuadrant*/, int /*iAwardNumber*/) { return true; }
void      C_4JProfile::Award(int /*iQuadrant*/, int /*iAwardNumber*/, bool /*bForce*/) {}
bool      C_4JProfile::IsAwardsFlagSet(int /*iQuadrant*/, int /*iAward*/) { return false; }

// --- Rich Presence --------------------------------------------------

void C_4JProfile::RichPresenceInit(int /*iPresenceCount*/, int /*iContextCount*/) {}
void C_4JProfile::RegisterRichPresenceContext(int /*iGameConfigContextID*/) {}
void C_4JProfile::SetRichPresenceContextValue(int /*iPad*/, int /*iContextID*/, int /*iVal*/) {}
void C_4JProfile::SetCurrentGameActivity(int /*iPad*/, int /*iNewPresence*/, bool /*bSetOthersToIdle*/) {}

// --- Purchase -------------------------------------------------------

void C_4JProfile::DisplayFullVersionPurchase(bool /*bRequired*/, int /*iQuadrant*/, int /*iUpsellParam*/) {}
void C_4JProfile::SetUpsellCallback(void (*/*Func*/)(LPVOID lpParam, eUpsellType type, eUpsellResponse response, int iUserData), LPVOID /*lpParam*/) {}

// --- Debug ----------------------------------------------------------

void C_4JProfile::SetDebugFullOverride(bool /*bVal*/) {}
