#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#include <tchar.h>
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

	RECT wrc = { 0,0, };

	return 0;
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



