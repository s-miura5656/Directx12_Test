#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <map>
#include <unordered_map>
#include <DirectXTex.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <memory>
#include <tchar.h>


class Dx12Wrapper
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	SIZE windowSize;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceVal;
	ID3D12DescriptorHeap* rtvHeaps;
	ID3D12DescriptorHeap* dsvHeap;
	// �o�b�N�o�b�t�@
	std::vector<ID3D12Resource*> _backBuffers;
	// �r���[�|�[�g
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	// �V�U�[��`
	std::unique_ptr<D3D12_RECT> _scissorrect;
	//�t�@�C�����p�X�ƃ��\�[�X�̃}�b�v�e�[�u��
	std::map<std::string, ID3D12Resource*> _resourceTable;
	//�e�N�X�`���e�[�u��
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> _textureTable;
	// DXGI�܂�菉����
	HRESULT InitializeDXGIDevice();
	// �R�}���h�܂�菉����
	HRESULT InitializeCommand();
	// �X���b�v�`�F�C���̐���
	HRESULT CreateSwapChain(const HWND& hwnd);
	// �����_�[�^�[�Q�b�g�r���[����
	HRESULT CreateRTV();
	// �e�N�X�`�����[�_�e�[�u���̍쐬
	void CreateTextureLoaderTable();
	// �f�v�X�o�b�t�@�쐬
	HRESULT CreateDepthBuffer();
	// �e�N�X�`���t�@�C���̓ǂݍ���
	ID3D12Resource* LoadTextureFromFile(const char* texpath);


	using LoadLambda_t = std::function<
		HRESULT(const std::wstring & path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

	std::map<std::string, LoadLambda_t> _loadLambdaTable;

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void BeginDraw();
	void EndDraw();

	///�e�N�X�`���p�X����K�v�ȃe�N�X�`���o�b�t�@�ւ̃|�C���^��Ԃ�
	///@param texpath �e�N�X�`���t�@�C���p�X
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> SwapChain();

};

