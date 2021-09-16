#pragma once

#include "app.h"

class TestApp : public App
{
public:
	TestApp() = default;
	~TestApp() = default;

	void InitializeImpl()override;
	void Render()override;
	void UninitializeImpl()override;
};