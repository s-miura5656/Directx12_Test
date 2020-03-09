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

// @brief �R���\�[����ʂɃt�H�[�}�b�g�t���������\��
// @param format �t�H�[�}�b�g (%d �Ƃ� %f �Ƃ���)
// @param �ϒ�����
// @remarks ���̊֐��̓f�o�b�N�p�ł��B�f�o�b�N���ɂ������삵�܂���B

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
	// �E�B���h�E���j�����ꂽ��Ă΂��
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); // OS �ɑ΂��āu�������̃A�v���͏I���v�Ɠ`����
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam); // ����̏������s��
}

ID3D12Device* _dev			= nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

HRESULT D3D12CreateDevice(
		IUnknown* pAdapter,
		D3D_FEATURE_LEVEL MinimumFeatureLevel, // �Œ���K�v�ȃt�B�[�`���[���x��
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
	w.lpfnWndProc   = (WNDPROC)WindowProcedure; // �R�[���o�b�N�֐��̎w��
	w.lpszClassName = _T("DX12Sample");         // �A�v���P�[�V�����N���X��
	w.hInstance     = GetModuleHandle(nullptr); // �n���h���̎擾

	RegisterClassEx(&w); // �A�v���P�[�V�����N���X(�E�B���h�E�N���X�̎w���OS�ɓ`����)

	float window_width  = 1280.0f;
	float window_height = 720.0f;

	RECT wrc = { 0, 0, window_width, window_height }; // �E�B���h�E�̃T�C�Y�����߂�

	// �֐����g���ăE�B���h�E�̃T�C�Y��␳����
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// �E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow(w.lpszClassName,      // �N���X���w��
							 _T("DX12_TEST"),      // �^�C�g���o�[�̕���
							 WS_OVERLAPPEDWINDOW,  // �^�C�g���o�[�Ƌ��E���̂���E�B���h�E
							 CW_USEDEFAULT,		   // �\�� x ���W�� OS �ɂ��܂���
							 CW_USEDEFAULT,		   // �\�� y ���W�� OS �ɂ��܂���
							 wrc.right - wrc.left, // �E�B���h�E��
							 wrc.bottom - wrc.top, // �E�B���h�E��
							 nullptr,			   // �e�E�B���h�E�n���h��
							 nullptr,			   // ���j���[�n���h��
							 w.hInstance,		   // �Ăяo���A�v���P�[�V�����n���h��
							 nullptr);			   // �ǉ��p�����[�^�[

	// �E�B���h�E�\��
	ShowWindow(hwnd, SW_SHOW);

	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));

	// �A�_�v�^�[�̗񋓗p
	std::vector <IDXGIAdapter*> adapters;

	// �����ɓ���̖��O�����A�_�v�^�[�I�u�W�F�N�g������
	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // �A�_�v�^�[�̐����I�u�W�F�N�g���擾

		std::wstring strDesc = adesc.Description;

		// �T�������A�_�v�^�[�̖��O���m�F
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3D �f�o�C�X�̏�����
	D3D_FEATURE_LEVEL featurelevel;
	
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

	for(auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featurelevel = lv;
			break; // �����\�ȃo�[�W���������������烋�[�v�𔲂���
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

		// �A�v���P�[�V�������I���Ƃ��� message �� WM_QUIT �ɂȂ�
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	// �����N���X�͎g��Ȃ��̂œo�^��������
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}






