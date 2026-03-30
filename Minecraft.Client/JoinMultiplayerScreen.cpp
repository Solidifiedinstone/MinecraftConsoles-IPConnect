#include "stdafx.h"
#include "JoinMultiplayerScreen.h"
#include "Button.h"
#include "EditBox.h"
#include "Options.h"
#include "../Minecraft.World/net.minecraft.locale.h"
#include "Common/Network/SessionInfo.h"

JoinMultiplayerScreen::JoinMultiplayerScreen(Screen *lastScreen)
{
	ipEdit = NULL;
	this->lastScreen = lastScreen;
}

void JoinMultiplayerScreen::tick()
{
	ipEdit->tick();
}

void JoinMultiplayerScreen::init()
{
    Language *language = Language::getInstance();

    Keyboard::enableRepeatEvents(true);
    buttons.clear();
    buttons.push_back(new Button(0, width / 2 - 100, height / 4 + 24 * 4 + 12, language->getElement(L"multiplayer.connect")));
    buttons.push_back(new Button(1, width / 2 - 100, height / 4 + 24 * 5 + 12, language->getElement(L"gui.cancel")));
    wstring ip = replaceAll(minecraft->options->lastMpIp,L"_", L":");
    buttons[0]->active = ip.length() > 0;

    ipEdit = new EditBox(this, font, width / 2 - 100, height / 4 - 10 + 50 + 18, 200, 20, ip);
    ipEdit->inFocus = true;
    ipEdit->setMaxLength(128);

}

void JoinMultiplayerScreen::removed()
{
	Keyboard::enableRepeatEvents(false);
}

void JoinMultiplayerScreen::buttonClicked(Button *button)
{
    if (!button->active) return;
    if (button->id == 1)
	{
        minecraft->setScreen(lastScreen);
    }
	else if (button->id == 0)
	{
        wstring ip = trimString(ipEdit->getValue());

        minecraft->options->lastMpIp = replaceAll(ip,L":", L"_");
        minecraft->options->save();

        vector<wstring> parts = stringSplit(ip,L':');
        if (ip[0]==L'[')
		{
            int pos = (int)ip.find(L"]");
            if (pos != wstring::npos)
			{
                wstring path = ip.substr(1, pos);
                wstring port = trimString(ip.substr(pos + 1));
                if (port[0]==L':' && port.length() > 0)
				{
                    port = port.substr(1);
                    parts.clear();
					parts.push_back(path);
                    parts.push_back(port);
                }
				else
				{
					parts.clear();
                    parts.push_back(path);
                }
            }

        }
        if (parts.size() > 2)
		{
			parts.clear();
			parts.push_back(ip);
        }

		// Use proper network manager join flow
        {
            wstring host = parts[0];
            int port = parts.size() > 1 ? parseInt(parts[1], 25565) : 25565;
            char hostBuf[128] = {};
            wcstombs(hostBuf, host.c_str(), sizeof(hostBuf) - 1);

            FriendSessionInfo *session = new FriendSessionInfo();
            strncpy_s(session->data.hostIP, sizeof(session->data.hostIP), hostBuf, _TRUNCATE);
            session->data.hostPort = port;
            wcsncpy_s(session->data.hostName, XUSER_NAME_SIZE, L"Server", _TRUNCATE);
            session->data.isJoinable = true;
            session->data.isReadyToJoin = true;
            session->data.maxPlayers = MINECRAFT_NET_MAX_PLAYERS;

            ProfileManager.SetLockedProfile(0);
            DWORD dwLocalUsersMask = CGameNetworkManager::GetLocalPlayerMask(ProfileManager.GetPrimaryPad());
            minecraft->clearConnectionFailed();
            g_NetworkManager.JoinGame(session, dwLocalUsersMask);
        }
    }
}

int  JoinMultiplayerScreen::parseInt(const wstring& str, int def)
{
	return _fromString<int>(str);
}

void JoinMultiplayerScreen::keyPressed(wchar_t ch, int eventKey)
{
    ipEdit->keyPressed(ch, eventKey);

    if (ch == 13)
	{
        buttonClicked(buttons[0]);
    }
    buttons[0]->active = ipEdit->getValue().length() > 0;
}

void JoinMultiplayerScreen::mouseClicked(int x, int y, int buttonNum)
{
    Screen::mouseClicked(x, y, buttonNum);

    ipEdit->mouseClicked(x, y, buttonNum);
}

void JoinMultiplayerScreen::render(int xm, int ym, float a)
{
    Language *language = Language::getInstance();

    // fill(0, 0, width, height, 0x40000000);
    renderBackground();

    drawCenteredString(font, language->getElement(L"multiplayer.title"), width / 2, height / 4 - 60 + 20, 0xffffff);
    drawString(font, language->getElement(L"multiplayer.info1"), width / 2 - 140, height / 4 - 60 + 60 + 9 * 0, 0xa0a0a0);
    drawString(font, language->getElement(L"multiplayer.info2"), width / 2 - 140, height / 4 - 60 + 60 + 9 * 1, 0xa0a0a0);
    drawString(font, language->getElement(L"multiplayer.ipinfo"), width / 2 - 140, height / 4 - 60 + 60 + 9 * 4, 0xa0a0a0);

    ipEdit->render();

    Screen::render(xm, ym, a);

}