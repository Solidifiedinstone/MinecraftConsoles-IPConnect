// =============================================================================
// Linux64_UIStubs.cpp
//
// Stub / no-op implementations for all UIController methods and other UI
// symbols that are referenced by the game code but whose real
// implementations live in Common/UI/ source files excluded from the Linux
// build.
// =============================================================================

#include "stdafx.h"

// Pull in the UIController class definition (includes IUIController, UIEnums,
// UIGroup, etc.)
#include "Common/UI/UIController.h"
#include "Common/UI/UIStructs.h"
#include "Common/Consoles_App.h"
#include "Common/Audio/SoundEngine.h"

// Forward declarations for types used only in signatures
class UIAbstractBitmapFont;
class UIBitmapFont;
class UITTFFont;
class UIComponent_DebugUIConsole;
class UIComponent_DebugUIMarketingGuide;
class UIControl;
class UIScene;
class Tutorial;

// =============================================================================
// UIController static members
// =============================================================================

int64_t UIController::iggyAllocCount = 0;
CRITICAL_SECTION UIController::ms_reloadSkinCS;
bool UIController::ms_bReloadSkinCSInitialised = false;
DWORD UIController::m_dwTrialTimerLimitSecs = 0;

// =============================================================================
// UIController constructor / destructor
// =============================================================================

UIController::UIController()
{
    m_uiDebugConsole = nullptr;
    m_uiDebugMarketingGuide = nullptr;
    m_mcBitmapFont = nullptr;
    m_mcTTFFont = nullptr;
    m_moj7 = nullptr;
    m_moj11 = nullptr;
    m_eCurrentFont = eFont_NotLoaded;
    m_eTargetFont = eFont_NotLoaded;
    m_bCleanupOnReload = false;
    m_defaultBuffer = nullptr;
    m_tempBuffer = nullptr;

    InitializeCriticalSection(&m_navigationLock);
    InitializeCriticalSection(&m_Allocatorlock);
    InitializeCriticalSection(&m_registeredCallbackScenesCS);

    for (unsigned int i = 0; i < eUIGroup_COUNT; ++i)
    {
        m_groups[i] = nullptr;
        m_bCloseAllScenes[i] = false;
    }
    for (unsigned int i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        m_bMenuDisplayed[i] = false;
        m_bMenuToBeClosed[i] = false;
        m_iCountDown[i] = 0;
    }

    m_winUserIndex = 0;
    m_mouseDraggingSliderScene = eUIScene_COUNT;
    m_mouseDraggingSliderId = -1;
    m_mouseClickConsumedByScene = false;
    m_bMouseHoverHorizontalList = false;
    m_lastHoverMouseX = -1;
    m_lastHoverMouseY = -1;
    m_bSystemUIShowing = false;
    m_reloadSkinThread = nullptr;
    m_navigateToHomeOnReload = false;
    m_accumulatedTicks = 0;
    m_lastUiSfx = 0;
    m_iPressStartQuadrantsMask = 0;

    if (!ms_bReloadSkinCSInitialised)
    {
        InitializeCriticalSection(&ms_reloadSkinCS);
        ms_bReloadSkinCSInitialised = true;
    }
}

// =============================================================================
// UIController – font helpers (private / protected)
// =============================================================================

UIController::EFont UIController::getFontForLanguage(int language)
{
    return eFont_Bitmap;
}

UITTFFont *UIController::createFont(EFont fontLanguage)
{
    return nullptr;
}

void UIController::SetupFont() {}
bool UIController::PendingFontChange() { return false; }
bool UIController::UsingBitmapFont() { return true; }
void UIController::setCleanupOnReload() { m_bCleanupOnReload = true; }
void UIController::updateCurrentFont() {}

// =============================================================================
// UIController – ticking
// =============================================================================

void UIController::tick() {}

// =============================================================================
// UIController – skin loading
// =============================================================================

void UIController::loadSkins() {}

IggyLibrary UIController::loadSkin(const wstring & /*skinPath*/, const wstring & /*skinName*/)
{
    IggyLibrary lib = {};
    return lib;
}

void UIController::ReloadSkin() {}
void UIController::StartReloadSkinThread() {}
bool UIController::IsReloadingSkin() { return false; }
bool UIController::IsExpectingOrReloadingSkin() { return false; }
void UIController::CleanUpSkinReload() {}

int UIController::reloadSkinThreadProc(void * /*lpParam*/) { return 0; }

byteArray UIController::getMovieData(const wstring & /*filename*/)
{
    return byteArray();
}

// =============================================================================
// UIController – input
// =============================================================================

void UIController::tickInput() {}
void UIController::handleInput() {}
void UIController::handleKeyPress(unsigned int /*iPad*/, unsigned int /*key*/) {}

rrbool RADLINK UIController::ExternalFunctionCallback(void * /*user_callback_data*/,
                                                       Iggy * /*player*/,
                                                       IggyExternalFunctionCallUTF16 * /*call*/)
{
    return 0;
}

// =============================================================================
// UIController – rendering
// =============================================================================

void UIController::renderScenes() {}

void UIController::getRenderDimensions(C4JRender::eViewportType /*viewport*/, S32 &width, S32 &height)
{
    width = 1920;
    height = 1080;
}

void UIController::setupRenderPosition(C4JRender::eViewportType /*viewport*/) {}
void UIController::setupRenderPosition(S32 /*xOrigin*/, S32 /*yOrigin*/) {}

void UIController::SetSysUIShowing(bool /*bVal*/) {}
void UIController::SetSystemUIShowing(LPVOID /*lpParam*/, bool /*bVal*/) {}

void UIController::setupCustomDrawGameState() {}
void UIController::endCustomDrawGameState() {}
void UIController::setupCustomDrawMatrices(UIScene * /*scene*/, CustomDrawData * /*customDrawRegion*/) {}
void UIController::setupCustomDrawGameStateAndMatrices(UIScene * /*scene*/, CustomDrawData * /*customDrawRegion*/) {}
void UIController::endCustomDrawMatrices() {}
void UIController::endCustomDrawGameStateAndMatrices() {}

void RADLINK UIController::CustomDrawCallback(void * /*user_callback_data*/,
                                               Iggy * /*player*/,
                                               IggyCustomDrawCallbackRegion * /*Region*/) {}

GDrawTexture *RADLINK UIController::TextureSubstitutionCreateCallback(void * /*user_callback_data*/,
                                                                       IggyUTF16 * /*texture_name*/,
                                                                       S32 * /*width*/,
                                                                       S32 * /*height*/,
                                                                       void ** /*destroy_callback_data*/)
{
    return nullptr;
}

void RADLINK UIController::TextureSubstitutionDestroyCallback(void * /*user_callback_data*/,
                                                               void * /*destroy_callback_data*/,
                                                               GDrawTexture * /*handle*/) {}

void UIController::registerSubstitutionTexture(const wstring & /*textureName*/, PBYTE /*pbData*/, DWORD /*dwLength*/) {}
void UIController::unregisterSubstitutionTexture(const wstring & /*textureName*/, bool /*deleteData*/) {}

// =============================================================================
// UIController – navigation
// =============================================================================

bool UIController::NavigateToScene(int /*iPad*/, EUIScene /*scene*/, void * /*initData*/,
                                    EUILayer /*layer*/, EUIGroup /*group*/)
{
    return false;
}

bool UIController::NavigateBack(int /*iPad*/, bool /*forceUsePad*/, EUIScene /*eScene*/, EUILayer /*eLayer*/)
{
    return false;
}

void UIController::NavigateToHomeMenu() {}

UIScene *UIController::GetTopScene(int /*iPad*/, EUILayer /*layer*/, EUIGroup /*group*/)
{
    return nullptr;
}

size_t UIController::RegisterForCallbackId(UIScene * /*scene*/) { return 0; }
void UIController::UnregisterCallbackId(size_t /*id*/) {}
UIScene *UIController::GetSceneFromCallbackId(size_t /*id*/) { return nullptr; }
void UIController::EnterCallbackIdCriticalSection() {}
void UIController::LeaveCallbackIdCriticalSection() {}

void UIController::CloseAllPlayersScenes() {}
void UIController::CloseUIScenes(int /*iPad*/, bool /*forceIPad*/) {}

void UIController::setFullscreenMenuDisplayed(bool /*displayed*/) {}

// =============================================================================
// UIController – menu state queries
// =============================================================================

bool UIController::IsPauseMenuDisplayed(int /*iPad*/) { return false; }
bool UIController::IsContainerMenuDisplayed(int /*iPad*/) { return false; }
bool UIController::IsIgnorePlayerJoinMenuDisplayed(int /*iPad*/) { return false; }
bool UIController::IsIgnoreAutosaveMenuDisplayed(int /*iPad*/) { return false; }
void UIController::SetIgnoreAutosaveMenuDisplayed(int /*iPad*/, bool /*displayed*/) {}
bool UIController::IsSceneInStack(int /*iPad*/, EUIScene /*eScene*/) { return false; }
bool UIController::GetMenuDisplayed(int /*iPad*/) { return false; }
void UIController::SetMenuDisplayed(int /*iPad*/, bool /*bVal*/) {}
void UIController::CheckMenuDisplayed() {}

void UIController::AnimateKeyPress(int /*iPad*/, int /*iAction*/, bool /*bRepeat*/,
                                    bool /*bPressed*/, bool /*bReleased*/) {}
void UIController::OverrideSFX(int /*iPad*/, int /*iAction*/, bool /*bVal*/) {}

// =============================================================================
// UIController – tooltips
// =============================================================================

void UIController::SetTooltipText(unsigned int /*iPad*/, unsigned int /*tooltip*/, int /*iTextID*/) {}
void UIController::SetEnableTooltips(unsigned int /*iPad*/, BOOL /*bVal*/) {}
void UIController::ShowTooltip(unsigned int /*iPad*/, unsigned int /*tooltip*/, bool /*show*/) {}
void UIController::SetTooltips(unsigned int /*iPad*/, int /*iA*/, int /*iB*/, int /*iX*/,
                                int /*iY*/, int /*iLT*/, int /*iRT*/, int /*iLB*/,
                                int /*iRB*/, int /*iLS*/, int /*iRS*/, int /*iBack*/,
                                bool /*forceUpdate*/) {}
void UIController::EnableTooltip(unsigned int /*iPad*/, unsigned int /*tooltip*/, bool /*enable*/) {}
void UIController::RefreshTooltips(unsigned int /*iPad*/) {}

// =============================================================================
// UIController – sound
// =============================================================================

void UIController::PlayUISFX(ESoundEffect /*eSound*/) {}

// =============================================================================
// UIController – gamertag / selected item
// =============================================================================

void UIController::DisplayGamertag(unsigned int /*iPad*/, bool /*show*/) {}
void UIController::SetSelectedItem(unsigned int /*iPad*/, const wstring & /*name*/) {}
void UIController::UpdateSelectedItemPos(unsigned int /*iPad*/) {}

// =============================================================================
// UIController – DLC / TMS / inventory
// =============================================================================

void UIController::HandleDLCMountingComplete() {}
void UIController::HandleDLCInstalled(int /*iPad*/) {}
#ifdef _XBOX_ONE
void UIController::HandleDLCLicenseChange() {}
#endif
void UIController::HandleTMSDLCFileRetrieved(int /*iPad*/) {}
void UIController::HandleTMSBanFileRetrieved(int /*iPad*/) {}
void UIController::HandleInventoryUpdated(int /*iPad*/) {}
void UIController::HandleGameTick() {}

// =============================================================================
// UIController – tutorials
// =============================================================================

void UIController::SetTutorial(int /*iPad*/, Tutorial * /*tutorial*/) {}
void UIController::SetTutorialDescription(int /*iPad*/, TutorialPopupInfo * /*info*/) {}
void UIController::RemoveInteractSceneReference(int /*iPad*/, UIScene * /*scene*/) {}
void UIController::SetTutorialVisible(int /*iPad*/, bool /*visible*/) {}
bool UIController::IsTutorialVisible(int /*iPad*/) { return false; }

// =============================================================================
// UIController – player base / quadrant
// =============================================================================

void UIController::UpdatePlayerBasePositions() {}
void UIController::SetEmptyQuadrantLogo(int /*iSection*/) {}
void UIController::HideAllGameUIElements() {}
void UIController::ShowOtherPlayersBaseScene(unsigned int /*iPad*/, bool /*show*/) {}

// =============================================================================
// UIController – trial timer
// =============================================================================

void UIController::ShowTrialTimer(bool /*show*/) {}
void UIController::SetTrialTimerLimitSecs(unsigned int /*uiSeconds*/) {}
void UIController::UpdateTrialTimer(unsigned int /*iPad*/) {}
void UIController::ReduceTrialTimerValue() {}

// =============================================================================
// UIController – autosave / saving
// =============================================================================

void UIController::ShowAutosaveCountdownTimer(bool /*show*/) {}
void UIController::UpdateAutosaveCountdownTimer(unsigned int /*uiSeconds*/) {}
void UIController::ShowSavingMessage(unsigned int /*iPad*/, C4JStorage::ESavingMessage /*eVal*/) {}

// =============================================================================
// UIController – press start / display name
// =============================================================================

void UIController::ShowPlayerDisplayname(bool /*show*/) {}
bool UIController::PressStartPlaying(unsigned int /*iPad*/) { return false; }
void UIController::ShowPressStart(unsigned int /*iPad*/) {}
void UIController::HidePressStart() {}
void UIController::ClearPressStart() {}

// =============================================================================
// UIController – message boxes
// =============================================================================

C4JStorage::EMessageResult UIController::RequestAlertMessage(UINT /*uiTitle*/, UINT /*uiText*/,
    UINT * /*uiOptionA*/, UINT /*uiOptionC*/, DWORD /*dwPad*/,
    int (*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
    LPVOID /*lpParam*/, WCHAR * /*pwchFormatString*/)
{
    return (C4JStorage::EMessageResult)0;
}

C4JStorage::EMessageResult UIController::RequestErrorMessage(UINT /*uiTitle*/, UINT /*uiText*/,
    UINT * /*uiOptionA*/, UINT /*uiOptionC*/, DWORD /*dwPad*/,
    int (*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
    LPVOID /*lpParam*/, WCHAR * /*pwchFormatString*/)
{
    return (C4JStorage::EMessageResult)0;
}

C4JStorage::EMessageResult UIController::RequestMessageBox(UINT /*uiTitle*/, UINT /*uiText*/,
    UINT * /*uiOptionA*/, UINT /*uiOptionC*/, DWORD /*dwPad*/,
    int (*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
    LPVOID /*lpParam*/, WCHAR * /*pwchFormatString*/,
    DWORD /*dwFocusButton*/, bool /*bIsError*/)
{
    return (C4JStorage::EMessageResult)0;
}

C4JStorage::EMessageResult UIController::RequestUGCMessageBox(UINT /*title*/, UINT /*message*/,
    int /*iPad*/,
    int (*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
    LPVOID /*lpParam*/)
{
    return (C4JStorage::EMessageResult)0;
}

C4JStorage::EMessageResult UIController::RequestContentRestrictedMessageBox(UINT /*title*/, UINT /*message*/,
    int /*iPad*/,
    int (*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
    LPVOID /*lpParam*/)
{
    return (C4JStorage::EMessageResult)0;
}

// =============================================================================
// UIController – user index
// =============================================================================

void UIController::SetWinUserIndex(unsigned int iPad) { m_winUserIndex = iPad; }
unsigned int UIController::GetWinUserIndex() { return m_winUserIndex; }

// =============================================================================
// UIController – debug console
// =============================================================================

void UIController::ShowUIDebugConsole(bool /*show*/) {}
void UIController::ShowUIDebugMarketingGuide(bool /*show*/) {}
void UIController::logDebugString(const string & /*text*/) {}

// =============================================================================
// UIController – find scene
// =============================================================================

UIScene *UIController::FindScene(EUIScene /*sceneType*/) { return nullptr; }

// =============================================================================
// UIController – font caching
// =============================================================================

void UIController::setFontCachingCalculationBuffer(int /*length*/) {}

// =============================================================================
// UIController – pre/post init
// =============================================================================

void UIController::preInit(S32 /*width*/, S32 /*height*/) {}
void UIController::postInit() {}

// =============================================================================
// MemSect stub
// =============================================================================

void MemSect(int) {}

// =============================================================================
// ATG::XMLParser stubs — already provided in Extrax64Stubs.cpp, do not
// duplicate here.
// =============================================================================

// =============================================================================
// UIScene_SettingsGraphicsMenu::DistanceToLevel  /  LevelToDistance
// =============================================================================

#include "Common/UI/UIScene_SettingsGraphicsMenu.h"

int UIScene_SettingsGraphicsMenu::DistanceToLevel(int dist) { return 0; }
int UIScene_SettingsGraphicsMenu::LevelToDistance(int dist) { return 0; }

// =============================================================================
// SoundEngine path statics — only the three that are missing for _LINUX64.
// m_szStreamFileA and m_activeSounds are already defined in SoundEngine.cpp.
// =============================================================================

char SoundEngine::m_szSoundPath[] = {"Windows64Media//Sound//"};
char SoundEngine::m_szMusicPath[] = {"music//"};
char SoundEngine::m_szRedistName[] = {"redist64"};

// =============================================================================
// CMinecraftApp::GetTPConfigVal
// =============================================================================

int CMinecraftApp::GetTPConfigVal(WCHAR * /*pwchDataFile*/)
{
    return -1;
}

// =============================================================================
// Item ID stubs (static const int members, ODR-use definitions)
// The values are already provided in the class definition (Item.h);
// these out-of-class definitions satisfy the linker when addresses are taken.
// =============================================================================

#include "../../Minecraft.World/Item.h"

const int Item::apple_Id;
const int Item::arrow_Id;
const int Item::beef_cooked_Id;
const int Item::beef_raw_Id;
const int Item::book_Id;
const int Item::boots_chain_Id;
const int Item::boots_diamond_Id;
const int Item::boots_iron_Id;
const int Item::boots_leather_Id;
const int Item::bread_Id;
const int Item::chestplate_chain_Id;
const int Item::chestplate_diamond_Id;
const int Item::chestplate_iron_Id;
const int Item::chestplate_leather_Id;
const int Item::chicken_cooked_Id;
const int Item::chicken_raw_Id;
const int Item::clock_Id;
const int Item::coal_Id;
const int Item::compass_Id;
const int Item::cookie_Id;
const int Item::diamond_Id;
const int Item::enderPearl_Id;
const int Item::expBottle_Id;
const int Item::eyeOfEnder_Id;
const int Item::fish_cooked_Id;
const int Item::flintAndSteel_Id;
const int Item::goldIngot_Id;
const int Item::hatchet_diamond_Id;
const int Item::hatchet_iron_Id;
const int Item::helmet_chain_Id;
const int Item::helmet_diamond_Id;
const int Item::helmet_iron_Id;
const int Item::helmet_leather_Id;
const int Item::hoe_diamond_Id;
const int Item::hoe_iron_Id;
const int Item::ironIngot_Id;
const int Item::leggings_chain_Id;
const int Item::leggings_diamond_Id;
const int Item::leggings_iron_Id;
const int Item::leggings_leather_Id;
const int Item::melon_Id;
const int Item::paper_Id;
const int Item::pickAxe_diamond_Id;
const int Item::pickAxe_iron_Id;
const int Item::porkChop_cooked_Id;
const int Item::porkChop_raw_Id;
const int Item::redStone_Id;
const int Item::rotten_flesh_Id;
const int Item::saddle_Id;
const int Item::seeds_melon_Id;
const int Item::seeds_pumpkin_Id;
const int Item::seeds_wheat_Id;
const int Item::shears_Id;
const int Item::shovel_diamond_Id;
const int Item::shovel_iron_Id;
const int Item::sword_diamond_Id;
const int Item::sword_iron_Id;
const int Item::wheat_Id;

// =============================================================================
// Tile ID stubs (static const int members, ODR-use definitions)
// =============================================================================

#include "../../Minecraft.World/Tile.h"

const int Tile::bookshelf_Id;
const int Tile::glass_Id;
const int Tile::glowstone_Id;
const int Tile::wool_Id;

// =============================================================================
// IUIScene stubs
// =============================================================================

/* Forward-declare IUIScene classes to avoid UI header dependencies */
class IUIScene_CraftingMenu { public: static bool isItemSelected(int); };
class IUIScene_CreativeMenu { public: static void staticCtor(); };

bool IUIScene_CraftingMenu::isItemSelected(int) { return false; }
void IUIScene_CreativeMenu::staticCtor() {}

/* Forward-declare */
class Merchant;
class IUIScene_PauseMenu { public: static int ExitWorldThreadProc(void*); static void _ExitWorld(LPVOID); static int SaveWorldThreadProc(void*); };
class IUIScene_TradingMenu { public: shared_ptr<Merchant> getMerchant(); };

int IUIScene_PauseMenu::ExitWorldThreadProc(void*) { return 0; }
void IUIScene_PauseMenu::_ExitWorld(LPVOID) {}
int IUIScene_PauseMenu::SaveWorldThreadProc(void*) { return 0; }
shared_ptr<Merchant> IUIScene_TradingMenu::getMerchant() { return nullptr; }

// =============================================================================
// LeaderboardManager
// =============================================================================

#include "Common/Leaderboards/LeaderboardManager.h"

LeaderboardManager *LeaderboardManager::m_instance = nullptr;

// =============================================================================
// NetworkPlayerXbox stubs
// =============================================================================

#include "Xbox/Network/NetworkPlayerXbox.h"

NetworkPlayerXbox::NetworkPlayerXbox(IQNetPlayer *qnetPlayer)
    : m_qnetPlayer(qnetPlayer), m_pSocket(nullptr), m_lastChunkPacketTime(0) {}

IQNetPlayer *NetworkPlayerXbox::GetQNetPlayer() { return m_qnetPlayer; }
unsigned char NetworkPlayerXbox::GetSmallId() { return m_qnetPlayer ? m_qnetPlayer->GetSmallId() : 0; }
void NetworkPlayerXbox::SendData(INetworkPlayer*, const void*, int, bool, bool) {}
bool NetworkPlayerXbox::IsSameSystem(INetworkPlayer*) { return false; }
int NetworkPlayerXbox::GetOutstandingAckCount() { return 0; }
int NetworkPlayerXbox::GetSendQueueSizeBytes(INetworkPlayer*, bool) { return 0; }
int NetworkPlayerXbox::GetSendQueueSizeMessages(INetworkPlayer*, bool) { return 0; }
int NetworkPlayerXbox::GetCurrentRtt() { return 0; }
bool NetworkPlayerXbox::IsHost() { return m_qnetPlayer ? m_qnetPlayer->IsHost() : false; }
bool NetworkPlayerXbox::IsGuest() { return m_qnetPlayer ? m_qnetPlayer->IsGuest() : false; }
bool NetworkPlayerXbox::IsLocal() { return m_qnetPlayer ? m_qnetPlayer->IsLocal() : true; }
int NetworkPlayerXbox::GetSessionIndex() { return 0; }
bool NetworkPlayerXbox::IsTalking() { return false; }
bool NetworkPlayerXbox::IsMutedByLocalUser(int) { return false; }
bool NetworkPlayerXbox::HasVoice() { return false; }
bool NetworkPlayerXbox::HasCamera() { return false; }
int NetworkPlayerXbox::GetUserIndex() { return 0; }
void NetworkPlayerXbox::SetSocket(Socket* s) { m_pSocket = s; }
Socket* NetworkPlayerXbox::GetSocket() { return m_pSocket; }
const wchar_t* NetworkPlayerXbox::GetOnlineName() { return m_qnetPlayer ? m_qnetPlayer->GetGamertag() : L"Player"; }
wstring NetworkPlayerXbox::GetDisplayName() { return GetOnlineName(); }
PlayerUID NetworkPlayerXbox::GetUID() { return m_qnetPlayer ? m_qnetPlayer->GetXuid() : 0; }
void NetworkPlayerXbox::SentChunkPacket() {}
int NetworkPlayerXbox::GetTimeSinceLastChunkPacket_ms() { return 0; }

// =============================================================================
// PostProcesser stubs
// =============================================================================

#include "Common/PostProcesser.h"

PostProcesser::PostProcesser() {}
PostProcesser::~PostProcesser() {}
void PostProcesser::Apply() const {}
void PostProcesser::ApplyFromCopied() const {}
void PostProcesser::CopyBackbuffer() {}
void PostProcesser::ResetViewport() {}
void PostProcesser::SetGamma(float) {}
void PostProcesser::SetViewport(const D3D11_VIEWPORT &) {}

// =============================================================================
// RemoveEntitiesPacket::MAX_PER_PACKET  (static const int, ODR-use definition)
// =============================================================================

#include "../../Minecraft.World/RemoveEntitiesPacket.h"

const int RemoveEntitiesPacket::MAX_PER_PACKET;

// =============================================================================
// SFontData::Codepoints  (static array, sized FONTSIZE = 23*20 = 460)
// =============================================================================

#include "Common/UI/UIFontData.h"

unsigned short SFontData::Codepoints[SFontData::FONTSIZE] = {};
SFontData SFontData::Mojangles_7 = {};
SFontData SFontData::Mojangles_11 = {};

// =============================================================================
// ShutdownManager stubs
// Per PS3 header: HasFinished / HasStarted return void, ShouldRun returns bool.
// =============================================================================

#include "PS3/PS3Extras/ShutdownManager.h"

void ShutdownManager::HasFinished(ShutdownManager::EThreadId) {}
void ShutdownManager::HasStarted(ShutdownManager::EThreadId) {}
void ShutdownManager::HasStarted(ShutdownManager::EThreadId, C4JThread::EventArray *) {}
bool ShutdownManager::ShouldRun(ShutdownManager::EThreadId) { return true; }

// =============================================================================
// UIScene_FullscreenProgress
// =============================================================================

/* Forward-declare to avoid pulling in full UI headers */
class UIScene_FullscreenProgress { public: static void SetWasCancelled(bool); };
void UIScene_FullscreenProgress::SetWasCancelled(bool) {}
