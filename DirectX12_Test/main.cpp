#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef _DEBUG
#include <iostream>
#endif 

using namespace std;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param format フォーマット (%d とか %f とかの)
// @param 可変長引数
// @remarks この関数はデバック用です。デバック時にしか動作しません。

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); // OS に対して「もうこのアプリは終わる」と伝える
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理を行う
}

ID3D12Device* _dev			= nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

HRESULT D3D12CreateDevice(
		IUnknown* pAdapter,
		D3D_FEATURE_LEVEL MinimumFeatureLevel, // 最低限必要なフィーチャーレベル
		REFIID riid,
		void** ppDevice
);

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	DebugOutputFormatString("Show window test.");
	getchar();

	WNDCLASSEX w = {};

	w.cbSize        = sizeof(WNDCLASSEX);
	w.lpfnWndProc   = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = _T("DX12Sample");         // アプリケーションクラス名
	w.hInstance     = GetModuleHandle(nullptr); // ハンドルの取得

	RegisterClassEx(&w); // アプリケーションクラス(ウィンドウクラスの指定をOSに伝える)

	float window_width  = 1280.0f;
	float window_height = 720.0f;

	RECT wrc = { 0, 0, window_width, window_height }; // ウィンドウのサイズを決める

	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName,      // クラス名指定
							 _T("DX12_TEST"),      // タイトルバーの文字
							 WS_OVERLAPPEDWINDOW,  // タイトルバーと境界線のあるウィンドウ
							 CW_USEDEFAULT,		   // 表示 x 座標は OS におまかせ
							 CW_USEDEFAULT,		   // 表示 y 座標は OS におまかせ
							 wrc.right - wrc.left, // ウィンドウ幅
							 wrc.bottom - wrc.top, // ウィンドウ高
							 nullptr,			   // 親ウィンドウハンドル
							 nullptr,			   // メニューハンドル
							 w.hInstance,		   // 呼び出しアプリケーションハンドル
							 nullptr);			   // 追加パラメーター

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));

	// アダプターの列挙用
	std::vector <IDXGIAdapter*> adapters;

	// ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクトを取得

		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3D デバイスの初期化
	D3D_FEATURE_LEVEL featurelevel;
	
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

	for(auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featurelevel = lv;
			break; // 生成可能なバージョンが見つかったらループを抜ける
		}
	}

	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときに message が WM_QUIT になる
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}






