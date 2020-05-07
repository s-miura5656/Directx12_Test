//�E�B���h�E�\����DirectX������
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

// �ʓ|�����Ǐ����Ȃ�������
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) // �E�B���h�E���j�����ꂽ��Ă΂�܂�
	{
		PostQuitMessage(0); // OS�ɑ΂��āu�������̃A�v���͏I�����v�Ɠ`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // �K��̏������s��
}

///@brief �R���\�[����ʂɃt�H�[�}�b�g�t���������\��
///@param format �t�H�[�}�b�g(%d�Ƃ�%f�Ƃ���)
///@param �ϒ�����
///@remarks���̊֐��̓f�o�b�O�p�ł��B�f�o�b�O���ɂ������삵�܂���
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
	
	// �E�B���h�E�N���X�������o�^
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // �R�[���o�b�N�֐��̎w��
	windowClass.lpszClassName = _T("DirectXTest"); // �A�v���P�[�V�����N���X��(�K���ł����ł�)
	windowClass.hInstance = GetModuleHandle(0); // �n���h���̎擾
	RegisterClassEx(&windowClass); // �A�v���P�[�V�����N���X(���������̍�邩���낵������OS�ɗ\������)

	RECT wrc = { 0,0, window_width, window_height }; // �E�B���h�E�T�C�Y�����߂�
	
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // �E�B���h�E�̃T�C�Y�͂�����Ɩʓ|�Ȃ̂Ŋ֐����g���ĕ␳����
	
	// �E�B���h�E�I�u�W�F�N�g�̐���
	hwnd = CreateWindow(windowClass.lpszClassName, // �N���X���w��
		   _T("DX12_TEST"), // �^�C�g���o�[�̕���
		   WS_OVERLAPPEDWINDOW, // �^�C�g���o�[�Ƌ��E��������E�B���h�E�ł�
		   CW_USEDEFAULT, // �\��X���W��OS�ɂ��C�����܂�
		   CW_USEDEFAULT, // �\��Y���W��OS�ɂ��C�����܂�
		   wrc.right - wrc.left, // �E�B���h�E��
		   wrc.bottom - wrc.top, // �E�B���h�E��
		   nullptr, // �e�E�B���h�E�n���h��
		   nullptr, // ���j���[�n���h��
		   windowClass.hInstance, // �Ăяo���A�v���P�[�V�����n���h��
		   nullptr); // �ǉ��p�����[�^
}

bool Application::Init()
{
	CreateGameWindow(_hwnd, _windowClass);
	
	_dx12.reset(new Dx12Wrapper(_hwnd));
	
	_pmdRenderer.reset(new PMDRenderer(_dx12));

	_pmdActor.reset(new PMDActor(_pmdRenderer, "Content/Model/�����~�N.pmd"));
	_pmdActor->LoadVMDFile("Content/Motion/pose.vmd", "pose");
	
	ShowWindow(_hwnd, SW_SHOW); // �E�B���h�E�\��

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
		// �����A�v���P�[�V�������I�����Ď���message��WM_QUIT�ɂȂ�
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

		// �t���b�v
		_dx12->SwapChain()->Present(1, 0);
	}
}

void Application::Terminate()
{
	// �����N���X�g��Ȃ�����o�^����
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
