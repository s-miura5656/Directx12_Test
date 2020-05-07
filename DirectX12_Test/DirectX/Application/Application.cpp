//ウィンドウ表示＆DirectX初期化
#include "Application.h"
#include "../Application/Application.h"
#include "../Dx12Wrapper/Dx12Wrapper.h"
#include "../PMD/PMDActor.h"
#include "../PMD/PMDRenderer.h"

#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")

using namespace DirectX;
using namespace std;

// 面倒だけど書かなあかんやつ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) // ウィンドウが破棄されたら呼ばれます
	{
		PostQuitMessage(0); // OSに対して「もうこのアプリは終わるんや」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 規定の処理を行う
}

///@brief コンソール画面にフォーマット付き文字列を表示
///@param format フォーマット(%dとか%fとかの)
///@param 可変長引数
///@remarksこの関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	DebugOutputFormatString("Show window test.");

	HINSTANCE hInst = GetModuleHandle(nullptr);
	
	// ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	windowClass.lpszClassName = _T("DirectXTest"); // アプリケーションクラス名(適当でいいです)
	windowClass.hInstance = GetModuleHandle(0); // ハンドルの取得
	RegisterClassEx(&windowClass); // アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, window_width, window_height }; // ウィンドウサイズを決める
	
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // ウィンドウのサイズはちょっと面倒なので関数を使って補正する
	
	// ウィンドウオブジェクトの生成
	hwnd = CreateWindow(windowClass.lpszClassName, // クラス名指定
		   _T("DX12_TEST"), // タイトルバーの文字
		   WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウです
		   CW_USEDEFAULT, // 表示X座標はOSにお任せします
		   CW_USEDEFAULT, // 表示Y座標はOSにお任せします
		   wrc.right - wrc.left, // ウィンドウ幅
		   wrc.bottom - wrc.top, // ウィンドウ高
		   nullptr, // 親ウィンドウハンドル
		   nullptr, // メニューハンドル
		   windowClass.hInstance, // 呼び出しアプリケーションハンドル
		   nullptr); // 追加パラメータ
}

bool Application::Init()
{
	CreateGameWindow(_hwnd, _windowClass);
	
	_dx12.reset(new Dx12Wrapper(_hwnd));
	
	_pmdRenderer.reset(new PMDRenderer(_dx12));

	_pmdActor.reset(new PMDActor(_pmdRenderer, "Content/Model/初音ミク.pmd"));
	_pmdActor->LoadVMDFile("Content/Motion/pose.vmd", "pose");
	
	ShowWindow(_hwnd, SW_SHOW); // ウィンドウ表示

	return true;
}

void Application::Run()
{
	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		_dx12->BeginDraw();

		_pmdRenderer->Update();

		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState().Get());
		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature().Get());

		_dx12->SetScene();

		_pmdActor->Update();
		_pmdActor->Draw();

		_dx12->EndDraw();

		// フリップ
		_dx12->SwapChain()->Present(1, 0);
	}
}

void Application::Terminate()
{
	// もうクラス使わないから登録解除
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

Application& Application::Instance() {
	static Application instance;
	return instance;
}
