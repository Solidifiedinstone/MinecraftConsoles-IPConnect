// Linux64_Minecraft.cpp : Defines the entry point for the Linux build.
//

#include "stdafx.h"

#include <assert.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <climits>

#include "GameConfig/Minecraft.spa.h"
#include "../MinecraftServer.h"
#include "../LocalPlayer.h"
#include "../../Minecraft.World/ItemInstance.h"
#include "../../Minecraft.World/Language.h"
#include "../../Minecraft.World/StringHelpers.h"
#include "../../Minecraft.World/Level.h"
#include "../ClientConnection.h"
#include "../Minecraft.h"
#include "../User.h"
#include "../../Minecraft.World/Socket.h"
#include "../../Minecraft.World/ThreadName.h"
#include "../StatsCounter.h"
#include "../ConnectScreen.h"
#include "../Tesselator.h"
#include "../Options.h"
#include "Sentient/SentientManager.h"
#include "../../Minecraft.World/IntCache.h"
#include "../Textures.h"
#include "../Settings.h"
#include "../../Minecraft.World/compression.h"
#include "../GameRenderer.h"
#include "Network/PosixNetLayer.h"
#include "../../Minecraft.World/AABB.h"
#include "../../Minecraft.World/OldChunkStorage.h"
#include "../../Minecraft.World/net.minecraft.world.level.tile.h"

//-------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------

BOOL g_bWidescreen = TRUE;

int g_iScreenWidth = 1920;
int g_iScreenHeight = 1080;

float g_iAspectRatio = static_cast<float>(1920) / 1080;

char g_Win64Username[17] = { 0 };
wchar_t g_Win64UsernameW[17] = { 0 };

// Extern network globals (defined in PosixNetLayer)
extern bool g_Win64MultiplayerJoin;
extern int g_Win64MultiplayerPort;
extern char g_Win64MultiplayerIP[256];
extern bool g_Win64DedicatedServer;
extern int g_Win64DedicatedServerPort;
extern char g_Win64DedicatedServerBindIP[256];

//-------------------------------------------------------------------------------------
// Profile settings (matching Windows64 layout)
//-------------------------------------------------------------------------------------
#define NUM_PROFILE_VALUES   5
#define NUM_PROFILE_SETTINGS 4
DWORD dwProfileSettingsA[NUM_PROFILE_VALUES] =
{
	0, 0, 0, 0, 0  // No Xbox-specific profile setting IDs on Linux
};

//--------------------------------------------------------------------------------------
// Update the Aspect Ratio to support Any Aspect Ratio
//--------------------------------------------------------------------------------------
void UpdateAspectRatio(int width, int height)
{
	if (height > 0)
		g_iAspectRatio = static_cast<float>(width) / height;
}

//--------------------------------------------------------------------------------------
// Signal handler for graceful shutdown
//--------------------------------------------------------------------------------------
static void SignalHandler(int signum)
{
	app.m_bShutdown = true;
	MinecraftServer::HaltServer();
}

//--------------------------------------------------------------------------------------
// Parse command-line arguments
//--------------------------------------------------------------------------------------
struct Linux64LaunchOptions
{
	bool serverMode;
	bool fullscreen;
};

static Linux64LaunchOptions ParseCommandLine(int argc, char* argv[])
{
	Linux64LaunchOptions options = {};
	options.serverMode = false;
	options.fullscreen = false;

	g_Win64MultiplayerJoin = false;
	g_Win64MultiplayerPort = WIN64_NET_DEFAULT_PORT;
	g_Win64DedicatedServer = false;
	g_Win64DedicatedServerPort = WIN64_NET_DEFAULT_PORT;
	g_Win64DedicatedServerBindIP[0] = 0;

	for (int i = 1; i < argc; ++i)
	{
		if (strcasecmp(argv[i], "-server") == 0)
		{
			options.serverMode = true;
		}
		else if (strcasecmp(argv[i], "-fullscreen") == 0)
		{
			options.fullscreen = true;
		}
		else if (strcasecmp(argv[i], "-name") == 0 && (i + 1) < argc)
		{
			++i;
			strncpy(g_Win64Username, argv[i], sizeof(g_Win64Username) - 1);
			g_Win64Username[sizeof(g_Win64Username) - 1] = 0;
		}
		else if (strcasecmp(argv[i], "-ip") == 0 && (i + 1) < argc)
		{
			++i;
			if (options.serverMode)
			{
				strncpy(g_Win64DedicatedServerBindIP, argv[i], sizeof(g_Win64DedicatedServerBindIP) - 1);
				g_Win64DedicatedServerBindIP[sizeof(g_Win64DedicatedServerBindIP) - 1] = 0;
			}
			else
			{
				strncpy(g_Win64MultiplayerIP, argv[i], sizeof(g_Win64MultiplayerIP) - 1);
				g_Win64MultiplayerIP[sizeof(g_Win64MultiplayerIP) - 1] = 0;
				g_Win64MultiplayerJoin = true;
			}
		}
		else if (strcasecmp(argv[i], "-port") == 0 && (i + 1) < argc)
		{
			++i;
			char* endPtr = NULL;
			long port = strtol(argv[i], &endPtr, 10);
			if (endPtr != argv[i] && *endPtr == 0 && port > 0 && port <= 65535)
			{
				if (options.serverMode)
					g_Win64DedicatedServerPort = (int)port;
				else
					g_Win64MultiplayerPort = (int)port;
			}
		}
	}

	g_Win64DedicatedServer = options.serverMode;
	return options;
}

//--------------------------------------------------------------------------------------
// Load username from username.txt next to the executable
//--------------------------------------------------------------------------------------
static void LoadUsernameFromFile()
{
	char exePath[PATH_MAX] = {};
	ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
	if (len <= 0)
		return;
	exePath[len] = 0;

	// Find last slash
	char* lastSlash = strrchr(exePath, '/');
	if (lastSlash)
		*(lastSlash + 1) = '/0';

	char filePath[PATH_MAX] = {};
	snprintf(filePath, sizeof(filePath), "%susername.txt", exePath);

	FILE* f = fopen(filePath, "r");
	if (f)
	{
		char buf[128] = {};
		if (fgets(buf, sizeof(buf), f))
		{
			int buflen = (int)strlen(buf);
			while (buflen > 0 && (buf[buflen - 1] == '/n' || buf[buflen - 1] == '/r' || buf[buflen - 1] == ' '))
			{
				buf[--buflen] = '/0';
			}

			if (buflen > 0)
			{
				strncpy(g_Win64Username, buf, sizeof(g_Win64Username) - 1);
				g_Win64Username[sizeof(g_Win64Username) - 1] = 0;
			}
		}
		fclose(f);
	}
}

//--------------------------------------------------------------------------------------
// Convert narrow username to wide
//--------------------------------------------------------------------------------------
static void ConvertUsernameToWide()
{
	for (int i = 0; i < 16 && g_Win64Username[i]; ++i)
	{
		g_Win64UsernameW[i] = (wchar_t)(unsigned char)g_Win64Username[i];
	}
}

//--------------------------------------------------------------------------------------
// DefineActions - Set up joypad button mappings
//--------------------------------------------------------------------------------------
void DefineActions(void)
{
	// The app needs to define the actions required, and the possible mappings for these

	// Split into Menu actions, and in-game actions

	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_A,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_B,							_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_X,							_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_Y,							_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OK,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_CANCEL,						_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_UP,							_360_JOY_BUTTON_DPAD_UP | _360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_DOWN,						_360_JOY_BUTTON_DPAD_DOWN | _360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_LEFT,						_360_JOY_BUTTON_DPAD_LEFT | _360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_RIGHT,						_360_JOY_BUTTON_DPAD_RIGHT | _360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_PAGEUP,						_360_JOY_BUTTON_LT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_PAGEDOWN,					_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_RIGHT_SCROLL,				_360_JOY_BUTTON_RB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_LEFT_SCROLL,				_360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_PAUSEMENU,					_360_JOY_BUTTON_START);

	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_STICK_PRESS,				_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OTHER_STICK_PRESS,			_360_JOY_BUTTON_RTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OTHER_STICK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OTHER_STICK_DOWN,			_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OTHER_STICK_LEFT,			_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,ACTION_MENU_OTHER_STICK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);

	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_JUMP,					_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_FORWARD,				_360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_BACKWARD,				_360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LEFT,					_360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_RIGHT,					_360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LOOK_LEFT,				_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LOOK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LOOK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LOOK_DOWN,				_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_USE,					_360_JOY_BUTTON_LT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_ACTION,				_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_RIGHT_SCROLL,			_360_JOY_BUTTON_RB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_LEFT_SCROLL,			_360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_INVENTORY,				_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_PAUSEMENU,				_360_JOY_BUTTON_START);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_DROP,					_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_SNEAK_TOGGLE,			_360_JOY_BUTTON_RTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_CRAFTING,				_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_RENDER_THIRD_PERSON,	_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_GAME_INFO,				_360_JOY_BUTTON_BACK);

	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_DPAD_LEFT,				_360_JOY_BUTTON_DPAD_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_DPAD_RIGHT,			_360_JOY_BUTTON_DPAD_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_DPAD_UP,				_360_JOY_BUTTON_DPAD_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_0,MINECRAFT_ACTION_DPAD_DOWN,				_360_JOY_BUTTON_DPAD_DOWN);

	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_A,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_B,							_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_X,							_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_Y,							_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OK,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_CANCEL,						_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_UP,							_360_JOY_BUTTON_DPAD_UP | _360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_DOWN,						_360_JOY_BUTTON_DPAD_DOWN | _360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_LEFT,						_360_JOY_BUTTON_DPAD_LEFT | _360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_RIGHT,						_360_JOY_BUTTON_DPAD_RIGHT | _360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_PAGEUP,						_360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_PAGEDOWN,					_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_RIGHT_SCROLL,				_360_JOY_BUTTON_RB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_LEFT_SCROLL,				_360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_PAUSEMENU,					_360_JOY_BUTTON_START);

	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_STICK_PRESS,				_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OTHER_STICK_PRESS,			_360_JOY_BUTTON_RTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OTHER_STICK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OTHER_STICK_DOWN,			_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OTHER_STICK_LEFT,			_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,ACTION_MENU_OTHER_STICK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);

	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_JUMP,					_360_JOY_BUTTON_RB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_FORWARD,				_360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_BACKWARD,				_360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LEFT,					_360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_RIGHT,					_360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LOOK_LEFT,				_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LOOK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LOOK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LOOK_DOWN,				_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_USE,					_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_ACTION,				_360_JOY_BUTTON_LT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_RIGHT_SCROLL,			_360_JOY_BUTTON_DPAD_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_LEFT_SCROLL,			_360_JOY_BUTTON_DPAD_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_INVENTORY,				_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_PAUSEMENU,				_360_JOY_BUTTON_START);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_DROP,					_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_SNEAK_TOGGLE,			_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_CRAFTING,				_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_RENDER_THIRD_PERSON,	_360_JOY_BUTTON_RTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_GAME_INFO,				_360_JOY_BUTTON_BACK);

	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_DPAD_LEFT,				_360_JOY_BUTTON_DPAD_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_DPAD_RIGHT,			_360_JOY_BUTTON_DPAD_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_DPAD_UP,				_360_JOY_BUTTON_DPAD_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_1,MINECRAFT_ACTION_DPAD_DOWN,				_360_JOY_BUTTON_DPAD_DOWN);

	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_A,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_B,							_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_X,							_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_Y,							_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OK,							_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_CANCEL,						_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_UP,							_360_JOY_BUTTON_DPAD_UP | _360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_DOWN,						_360_JOY_BUTTON_DPAD_DOWN | _360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_LEFT,						_360_JOY_BUTTON_DPAD_LEFT | _360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_RIGHT,						_360_JOY_BUTTON_DPAD_RIGHT | _360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_PAGEUP,						_360_JOY_BUTTON_DPAD_UP | _360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_PAGEDOWN,					_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_RIGHT_SCROLL,				_360_JOY_BUTTON_RB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_LEFT_SCROLL,				_360_JOY_BUTTON_LB);

	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_JUMP,					_360_JOY_BUTTON_LT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_FORWARD,				_360_JOY_BUTTON_LSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_BACKWARD,				_360_JOY_BUTTON_LSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LEFT,					_360_JOY_BUTTON_LSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_RIGHT,					_360_JOY_BUTTON_LSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LOOK_LEFT,				_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LOOK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LOOK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LOOK_DOWN,				_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_USE,					_360_JOY_BUTTON_RT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_ACTION,				_360_JOY_BUTTON_A);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_RIGHT_SCROLL,			_360_JOY_BUTTON_DPAD_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_LEFT_SCROLL,			_360_JOY_BUTTON_DPAD_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_INVENTORY,				_360_JOY_BUTTON_Y);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_PAUSEMENU,				_360_JOY_BUTTON_START);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_DROP,					_360_JOY_BUTTON_B);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_SNEAK_TOGGLE,			_360_JOY_BUTTON_LB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_CRAFTING,				_360_JOY_BUTTON_X);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_RENDER_THIRD_PERSON,	_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_GAME_INFO,				_360_JOY_BUTTON_BACK);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_PAUSEMENU,					_360_JOY_BUTTON_START);

	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_STICK_PRESS,				_360_JOY_BUTTON_LTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OTHER_STICK_PRESS,			_360_JOY_BUTTON_RTHUMB);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OTHER_STICK_UP,				_360_JOY_BUTTON_RSTICK_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OTHER_STICK_DOWN,			_360_JOY_BUTTON_RSTICK_DOWN);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OTHER_STICK_LEFT,			_360_JOY_BUTTON_RSTICK_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,ACTION_MENU_OTHER_STICK_RIGHT,			_360_JOY_BUTTON_RSTICK_RIGHT);

	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_DPAD_LEFT,				_360_JOY_BUTTON_DPAD_LEFT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_DPAD_RIGHT,			_360_JOY_BUTTON_DPAD_RIGHT);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_DPAD_UP,				_360_JOY_BUTTON_DPAD_UP);
	InputManager.SetGameJoypadMaps(MAP_STYLE_2,MINECRAFT_ACTION_DPAD_DOWN,				_360_JOY_BUTTON_DPAD_DOWN);
}

//--------------------------------------------------------------------------------------
// Initialise the Minecraft runtime
//--------------------------------------------------------------------------------------
static Minecraft* InitialiseMinecraftRuntime()
{
	app.loadMediaArchive();
	app.loadStringTable();

	InputManager.Initialise(1, 3, MINECRAFT_ACTION_MAX, ACTION_MAX_MENU);
	DefineActions();
	InputManager.SetJoypadMapVal(0, 0);
	InputManager.SetKeyRepeatRate(0.3f, 0.2f);

	ProfileManager.Initialise(TITLEID_MINECRAFT,
		app.m_dwOfferID,
		PROFILE_VERSION_10,
		NUM_PROFILE_VALUES,
		NUM_PROFILE_SETTINGS,
		dwProfileSettingsA,
		app.GAME_DEFINED_PROFILE_DATA_BYTES * XUSER_MAX_COUNT,
		&app.uiGameDefinedDataChangedBitmask
	);
	ProfileManager.SetDefaultOptionsCallback(&CConsoleMinecraftApp::DefaultOptionsCallback, (LPVOID)&app);

	g_NetworkManager.Initialise();

	for (int i = 0; i < MINECRAFT_NET_MAX_PLAYERS; i++)
	{
		IQNet::m_player[i].m_smallId = (BYTE)i;
		IQNet::m_player[i].m_isRemote = false;
		IQNet::m_player[i].m_isHostPlayer = (i == 0);
		swprintf(IQNet::m_player[i].m_gamertag, 32, L"Player%d", i);
	}
	wcscpy(IQNet::m_player[0].m_gamertag, g_Win64UsernameW);

	WinsockNetLayer::Initialize();

	ProfileManager.SetDebugFullOverride(true);

	Tesselator::CreateNewThreadStorage(1024 * 1024);
	AABB::CreateNewThreadStorage();
	Vec3::CreateNewThreadStorage();
	IntCache::CreateNewThreadStorage();
	Compression::CreateNewThreadStorage();
	OldChunkStorage::CreateNewThreadStorage();
	Level::enableLightingCache();
	Tile::CreateNewThreadStorage();

	Minecraft::main();
	Minecraft* pMinecraft = Minecraft::GetInstance();
	if (pMinecraft == NULL)
		return NULL;

	app.InitGameSettings();
	app.InitialiseTips();

	return pMinecraft;
}

//--------------------------------------------------------------------------------------
// Main entry point
//--------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	// Set CWD to exe dir so asset paths resolve correctly
	{
		char exePath[PATH_MAX] = {};
		ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
		if (len > 0)
		{
			exePath[len] = 0;
			char* lastSlash = strrchr(exePath, '/');
			if (lastSlash)
			{
				*(lastSlash + 1) = '/0';
				chdir(exePath);
			}
		}
	}

	// Load username from file
	LoadUsernameFromFile();

	// Parse command-line arguments
	Linux64LaunchOptions launchOptions = ParseCommandLine(argc, argv);

	// If no username, fall back to "Player"
	if (g_Win64Username[0] == 0)
	{
		strncpy(g_Win64Username, "Player", sizeof(g_Win64Username) - 1);
		g_Win64Username[sizeof(g_Win64Username) - 1] = 0;
	}

	// Convert username to wide
	ConvertUsernameToWide();

	// Set up signal handlers for graceful shutdown
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);

	// Initialise the Minecraft runtime
	Minecraft* pMinecraft = InitialiseMinecraftRuntime();
	if (pMinecraft == NULL)
	{
		fprintf(stderr, "Failed to initialise the Minecraft runtime./n");
		return 1;
	}

	// Main game loop
	while (!app.m_bShutdown)
	{
		app.UpdateTime();

		InputManager.Tick();
		ProfileManager.Tick();
		StorageManager.Tick();
		RenderManager.Tick();

		g_NetworkManager.DoWork();

		// Render game graphics
		if (app.GetGameStarted())
		{
			pMinecraft->run_middle();
		}
		else
		{
			pMinecraft->soundEngine->tick(NULL, 0.0f);
			pMinecraft->textures->tick(true, false);
			IntCache::Reset();
			if (app.GetReallyChangingSessionType())
			{
				pMinecraft->tickAllConnections();
			}
		}

		pMinecraft->soundEngine->playMusicTick();

		ui.tick();
		ui.render();

		RenderManager.Present();

		ui.CheckMenuDisplayed();

		// Small sleep to avoid spinning the CPU
		usleep(1000); // 1ms
	}

	return 0;
}
