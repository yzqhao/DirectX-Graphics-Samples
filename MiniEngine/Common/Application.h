#pragma once

#include "GameCore.h"

class Application : public GameCore::IGameApp
{
public:

	Application()
	{
	}

	virtual void Startup(void) final override;
	virtual void RenderScene(void) final override;

	virtual bool WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:

	virtual void _DoStartup(void) = 0;
	virtual void _DoRenderScene(void) = 0;

};