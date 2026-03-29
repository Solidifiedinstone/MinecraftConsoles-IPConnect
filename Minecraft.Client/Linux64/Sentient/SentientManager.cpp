#include "stdafx.h"
#include "SentientManager.h"

CSentientManager SentientManager;

// CSentientManager stub implementations

HRESULT CSentientManager::Init()
{
	m_initialiseTime = 0.0f;
	m_lastHeartbeat = 0.0f;
	m_bFirstFlush = true;
	m_multiplayerInstanceID = 0;
	m_levelInstanceID = 0;
	memset(m_fLevelStartTime, 0, sizeof(m_fLevelStartTime));
	return S_OK;
}

HRESULT CSentientManager::Tick()
{
	return S_OK;
}

HRESULT CSentientManager::Flush()
{
	return S_OK;
}

BOOL CSentientManager::RecordPlayerSessionStart(DWORD dwUserId) { return FALSE; }
BOOL CSentientManager::RecordPlayerSessionExit(DWORD dwUserId, int exitStatus) { return FALSE; }
BOOL CSentientManager::RecordHeartBeat(DWORD dwUserId) { return FALSE; }
BOOL CSentientManager::RecordLevelStart(DWORD dwUserId, ESen_FriendOrMatch friendsOrMatch, ESen_CompeteOrCoop competeOrCoop, int difficulty, DWORD numberOfLocalPlayers, DWORD numberOfOnlinePlayers) { return FALSE; }
BOOL CSentientManager::RecordLevelExit(DWORD dwUserId, ESen_LevelExitStatus levelExitStatus) { return FALSE; }
BOOL CSentientManager::RecordLevelSaveOrCheckpoint(DWORD dwUserId, INT saveOrCheckPointID, INT saveSizeInBytes) { return FALSE; }
BOOL CSentientManager::RecordLevelResume(DWORD dwUserId, ESen_FriendOrMatch friendsOrMatch, ESen_CompeteOrCoop competeOrCoop, int difficulty, DWORD numberOfLocalPlayers, DWORD numberOfOnlinePlayers, INT saveOrCheckPointID) { return FALSE; }
BOOL CSentientManager::RecordPauseOrInactive(DWORD dwUserId) { return FALSE; }
BOOL CSentientManager::RecordUnpauseOrActive(DWORD dwUserId) { return FALSE; }
BOOL CSentientManager::RecordMenuShown(DWORD dwUserId, INT menuID, INT optionalMenuSubID) { return FALSE; }
BOOL CSentientManager::RecordAchievementUnlocked(DWORD dwUserId, INT achievementID, INT achievementGamerscore) { return FALSE; }
BOOL CSentientManager::RecordMediaShareUpload(DWORD dwUserId, ESen_MediaDestination mediaDestination, ESen_MediaType mediaType) { return FALSE; }
BOOL CSentientManager::RecordUpsellPresented(DWORD dwUserId, ESen_UpsellID upsellId, INT marketplaceOfferID) { return FALSE; }
BOOL CSentientManager::RecordUpsellResponded(DWORD dwUserId, ESen_UpsellID upsellId, INT marketplaceOfferID, ESen_UpsellOutcome upsellOutcome) { return FALSE; }
BOOL CSentientManager::RecordPlayerDiedOrFailed(DWORD dwUserId, INT lowResMapX, INT lowResMapY, INT lowResMapZ, INT mapID, INT playerWeaponID, INT enemyWeaponID, ETelemetryChallenges enemyTypeID) { return FALSE; }
BOOL CSentientManager::RecordEnemyKilledOrOvercome(DWORD dwUserId, INT lowResMapX, INT lowResMapY, INT lowResMapZ, INT mapID, INT playerWeaponID, INT enemyWeaponID, ETelemetryChallenges enemyTypeID) { return FALSE; }

BOOL CSentientManager::RecordSkinChanged(DWORD dwUserId, DWORD dwSkinId) { return FALSE; }
BOOL CSentientManager::RecordBanLevel(DWORD dwUserId) { return FALSE; }
BOOL CSentientManager::RecordUnBanLevel(DWORD dwUserId) { return FALSE; }

INT CSentientManager::GetMultiplayerInstanceID() { return 0; }
INT CSentientManager::GenerateMultiplayerInstanceId() { return ++m_multiplayerInstanceID; }
void CSentientManager::SetMultiplayerInstanceId(INT value) { m_multiplayerInstanceID = value; }

// Private helper stubs
INT CSentientManager::GetSecondsSinceInitialize() { return 0; }
INT CSentientManager::GetMode(DWORD dwUserId) { return 0; }
INT CSentientManager::GetSubMode(DWORD dwUserId) { return 0; }
INT CSentientManager::GetLevelId(DWORD dwUserId) { return 0; }
INT CSentientManager::GetSubLevelId(DWORD dwUserId) { return 0; }
INT CSentientManager::GetTitleBuildId() { return 0; }
INT CSentientManager::GetLevelInstanceID() { return 0; }
INT CSentientManager::GetSingleOrMultiplayer() { return 0; }
INT CSentientManager::GetDifficultyLevel(INT diff) { return 0; }
INT CSentientManager::GetLicense() { return 0; }
INT CSentientManager::GetDefaultGameControls() { return 0; }
INT CSentientManager::GetAudioSettings(DWORD dwUserId) { return 0; }
INT CSentientManager::GetLevelExitProgressStat1() { return 0; }
INT CSentientManager::GetLevelExitProgressStat2() { return 0; }

// SenStat function stubs

BOOL SenStatPlayerSessionStart(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT TitleBuildID, INT SkeletonDistanceInInches, INT EnrollmentType, INT NumberOfSkeletonsInView) { return FALSE; }
BOOL SenStatPlayerSessionExit(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID) { return FALSE; }
BOOL SenStatHeartBeat(DWORD dwUserID, INT SecondsSinceInitialize) { return FALSE; }
BOOL SenStatLevelStart(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT SingleOrMultiplayer, INT FriendsOrMatch, INT CompeteOrCoop, INT DifficultyLevel, INT NumberOfLocalPlayers, INT NumberOfOnlinePlayers, INT License, INT DefaultGameControls, INT AudioSettings, INT SkeletonDistanceInInches, INT NumberOfSkeletonsInView) { return FALSE; }
BOOL SenStatLevelExit(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT LevelExitStatus, INT LevelExitProgressStat1, INT LevelExitProgressStat2, INT LevelDurationInSeconds) { return FALSE; }
BOOL SenStatLevelSaveOrCheckpoint(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT LevelExitProgressStat1, INT LevelExitProgressStat2, INT LevelDurationInSeconds, INT SaveOrCheckPointID) { return FALSE; }
BOOL SenStatLevelResume(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT SingleOrMultiplayer, INT FriendsOrMatch, INT CompeteOrCoop, INT DifficultyLevel, INT NumberOfLocalPlayers, INT NumberOfOnlinePlayers, INT License, INT DefaultGameControls, INT SaveOrCheckPointID, INT AudioSettings, INT SkeletonDistanceInInches, INT NumberOfSkeletonsInView) { return FALSE; }
BOOL SenStatPauseOrInactive(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID) { return FALSE; }
BOOL SenStatUnpauseOrActive(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID) { return FALSE; }
BOOL SenStatMenuShown(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT MenuID, INT OptionalMenuSubID, INT LevelInstanceID, INT MultiplayerInstanceID) { return FALSE; }
BOOL SenStatAchievementUnlocked(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT AchievementID, INT AchievementGamerscore) { return FALSE; }
BOOL SenStatMediaShareUpload(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT MediaDestination, INT MediaType) { return FALSE; }
BOOL SenStatUpsellPresented(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT UpsellID, INT MarketplaceOfferID) { return FALSE; }
BOOL SenStatUpsellResponded(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT UpsellID, INT MarketplaceOfferID, INT UpsellOutcome) { return FALSE; }
BOOL SenStatPlayerDiedOrFailed(DWORD dwUserID, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT LowResMapX, INT LowResMapY, INT LowResMapZ, INT MapID, INT PlayerWeaponID, INT EnemyWeaponID, INT EnemyTypeID, INT SecondsSinceInitialize, INT CopyOfSecondsSinceInitialize) { return FALSE; }
BOOL SenStatEnemyKilledOrOvercome(DWORD dwUserID, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT LowResMapX, INT LowResMapY, INT LowResMapZ, INT MapID, INT PlayerWeaponID, INT EnemyWeaponID, INT EnemyTypeID, INT SecondsSinceInitialize, INT CopyOfSecondsSinceInitialize) { return FALSE; }
BOOL SenStatSkinChanged(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID, INT SkinID) { return FALSE; }
BOOL SenStatBanLevel(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID) { return FALSE; }
BOOL SenStatUnBanLevel(DWORD dwUserID, INT SecondsSinceInitialize, INT ModeID, INT OptionalSubModeID, INT LevelID, INT OptionalSubLevelID, INT LevelInstanceID, INT MultiplayerInstanceID) { return FALSE; }
