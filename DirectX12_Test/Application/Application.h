#pragma once

#include <Windows.h>
#include <tchar.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <wrl.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;

//�V���O���g���N���X
class Application
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �ϐ��錾 /////////////////////////////////////////////////////////////////////////////
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	std::shared_ptr<PMDActor> _pmdActor;

	// �֐��錾 ////////////////////////////////////////////////////////////////////////////
	//�Q�[���p�E�B���h�E�̐���
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);
	
	//���V���O���g���̂��߂ɃR���X�g���N�^��private��
	//����ɃR�s�[�Ƒ�����֎~��
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	//Application�̃V���O���g���C���X�^���X�𓾂�
	static Application& Instance();

	///������
	bool Init();

	///���[�v�N��
	void Run();

	///�㏈��
	void Terminate();
	SIZE GetWindowSize() const;
	~Application();
};

