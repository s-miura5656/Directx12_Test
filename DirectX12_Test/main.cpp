#include "Application/Application.h"

#ifdef _DEBUG
int main() {
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif // _DEBUG

	auto& app = Application::Instance();
	if (!app.Init())
	{
		return -1;
	}

	app.Run();

	app.Terminate();

	return 0;
}