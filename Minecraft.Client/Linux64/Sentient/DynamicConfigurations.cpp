#include "stdafx.h"
#include "DynamicConfigurations.h"

// Static member definitions
MinecraftDynamicConfigurations::Dynamic_Config_Trial_Data MinecraftDynamicConfigurations::trialData;
bool MinecraftDynamicConfigurations::s_bFirstUpdateStarted = false;
bool MinecraftDynamicConfigurations::s_bUpdatedConfigs[MinecraftDynamicConfigurations::eDynamic_Config_Max] = { false };
MinecraftDynamicConfigurations::EDynamic_Configs MinecraftDynamicConfigurations::s_eCurrentConfig = MinecraftDynamicConfigurations::eDynamic_Config_Trial;
size_t MinecraftDynamicConfigurations::s_currentConfigSize = 0;
size_t MinecraftDynamicConfigurations::s_dataWrittenSize = 0;
byte* MinecraftDynamicConfigurations::s_dataWritten = nullptr;

void MinecraftDynamicConfigurations::Tick()
{
	// No-op on Linux
}

DWORD MinecraftDynamicConfigurations::GetTrialTime()
{
	return DYNAMIC_CONFIG_DEFAULT_TRIAL_TIME;
}

void MinecraftDynamicConfigurations::UpdateAllConfigurations()
{
	// No-op on Linux
}

void MinecraftDynamicConfigurations::UpdateNextConfiguration()
{
	// No-op on Linux
}

void MinecraftDynamicConfigurations::UpdateConfiguration(EDynamic_Configs id)
{
	// No-op on Linux
}

void MinecraftDynamicConfigurations::GetSizeCompletedCallback(HRESULT taskResult, void *userCallbackData)
{
	// No-op on Linux
}

void MinecraftDynamicConfigurations::GetDataCompletedCallback(HRESULT taskResult, void *userCallbackData)
{
	// No-op on Linux
}
