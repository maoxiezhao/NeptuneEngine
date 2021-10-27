#pragma once

#include "app.h"

class TestApp : public App
{
public:
	TestApp() = default;
	~TestApp() = default;

	void Setup()override;
	void Render()override;
	void InitializeImpl()override;
	void UninitializeImpl()override;
};