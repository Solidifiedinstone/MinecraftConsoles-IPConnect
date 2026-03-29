#include "stdafx.h"
#include "Linux64_UIController.h"

ConsoleUIController ui;

void ConsoleUIController::init(void *dev, void *ctx, void *pRenderTargetView, void *pDepthStencilView, S32 w, S32 h)
{
}

void ConsoleUIController::render()
{
}

void ConsoleUIController::beginIggyCustomDraw4J(IggyCustomDrawCallbackRegion *region, CustomDrawData *customDrawRegion)
{
}

CustomDrawData *ConsoleUIController::setupCustomDraw(UIScene *scene, IggyCustomDrawCallbackRegion *region)
{
	return nullptr;
}

CustomDrawData *ConsoleUIController::calculateCustomDraw(IggyCustomDrawCallbackRegion *region)
{
	return nullptr;
}

void ConsoleUIController::endCustomDraw(IggyCustomDrawCallbackRegion *region)
{
}

void ConsoleUIController::setTileOrigin(S32 xPos, S32 yPos)
{
}

GDrawTexture *ConsoleUIController::getSubstitutionTexture(int textureId)
{
	return nullptr;
}

void ConsoleUIController::destroySubstitutionTexture(void *destroyCallBackData, GDrawTexture *handle)
{
}

void ConsoleUIController::shutdown()
{
}
