#include "Engine/Engine/engine.h"
#include <iostream>

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

using namespace Neptune;

#if 1
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	return 0;
}
#else

int main()
{
	int a = EngineUtils::TestFunc(1, 4);
	std::cout << a << std::endl;
	return 0;
}

#endif