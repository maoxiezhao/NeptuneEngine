#include "Engine/Engine/engine.h"
#include <iostream>

#define WIN32_LEAN_AND_MEAN             // �� Windows ͷ�ļ����ų�����ʹ�õ�����
// Windows ͷ�ļ�
#include <windows.h>
// C ����ʱͷ�ļ�
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