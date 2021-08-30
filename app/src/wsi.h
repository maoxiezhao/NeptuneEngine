#pragma once

#include "common.h"

class Platform;

class WSI
{
public:
	bool Initialize();
	void Uninitialize();
	void RunFrame();
	void SetPlatform(Platform* platform);

private:
	Platform* mPlatform = nullptr;
};