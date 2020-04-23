#pragma once
#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <algorithm>
#include <memory>
#include <wrl.h>

//class Dx12Wrapper;
//class PMDRenderer;
//class PMDActor;

//�V���O���g���N���X
class Application
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �V�F�[�_�[���ɓ�������}�e���A���f�[�^
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	// ����ȊO�̃}�e���A���f�[�^
	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// �S�̂��܂Ƃ߂�f�[�^
	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	// �V�F�[�_�[���ɓn���ׂ̊�{�I�ȍs��̃f�[�^
	struct SceneMatrix
	{
		DirectX::XMMATRIX world;	   // ���[���h�s��
		DirectX::XMMATRIX view;	   // �r���[�s��
		DirectX::XMMATRIX proj;	   // �v���W�F�N�V�����s��
		DirectX::XMFLOAT3 eye;      // ���_���W
	};

	// �ϐ��錾 /////////////////////////////////////////////////////////////////////////////
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	//	std::shared_ptr<Dx12Wrapper> _dx12;
	//	std::shared_ptr<PMDRenderer> _pmdRenderer;
	//	std::shared_ptr<PMDActor> _pmdActor;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;

	using LoadLambda_t = std::function<
		HRESULT(const std::wstring & path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

	std::map<std::string, LoadLambda_t> loadLambdaTable;

	//�t�@�C�����p�X�ƃ��\�[�X�̃}�b�v�e�[�u��
	std::map<std::string, ID3D12Resource*> _resourceTable;

	std::vector<ID3D12Resource*> _backBuffers;
	ID3D12DescriptorHeap* rtvHeaps;
	ID3D12DescriptorHeap* dsvHeap;

	D3D12_VERTEX_BUFFER_VIEW vbView;
	D3D12_INDEX_BUFFER_VIEW ibView;


	ID3D12Fence* _fence;
	UINT64 _fenceVal;

	ID3D12RootSignature* rootsignature;
	ID3D12PipelineState* _pipelinestate;


	SceneMatrix* mapMatrix;
	DirectX::XMMATRIX worldMat;
	DirectX::XMMATRIX viewMat;
	DirectX::XMMATRIX projMat;

	ID3D12DescriptorHeap* basicDescHeap;
	ID3D12DescriptorHeap* materialDescHeap;

	std::vector<Material> materials;
	std::vector<ID3D12Resource*> textureResources;
	std::vector<ID3D12Resource*> sphResources;
	std::vector<ID3D12Resource*> spaResources;
	std::vector<ID3D12Resource*> toonResources;
	std::vector<unsigned char> vertices;
	std::vector<unsigned short> indices;
	unsigned int vertNum; // ���_��
	unsigned int indicesNum;
	unsigned int materialNum;
	ComPtr<ID3D12Resource> vertBuff;
	ComPtr<ID3D12Resource> idxBuff;
	ComPtr<ID3DBlob> _vsBlob;
	ComPtr<ID3DBlob> _psBlob;
	ComPtr<ID3DBlob> errorBlob;
	// �֐��錾 ////////////////////////////////////////////////////////////////////////////
	//�Q�[���p�E�B���h�E�̐���
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);
	ID3D12Resource* LoadTextureFromFile(const char* texpath);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

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
	// PMD �t�@�C���̃��[�h
	HRESULT LoadPMDFile();
	// ���_�o�b�t�@�̍쐬
	HRESULT CreateVertexBuffer();
	// �C���f�b�N�X�o�b�t�@�쐬
	HRESULT CreateIndexBuffer();
	// �}�e���A���o�b�t�@�쐬
	HRESULT CreateMaterialBuffer();
	// �V�F�[�_�[�ǂݍ��ݐ��������s���m�F
	bool CheckShaderCompileResult(HRESULT result, ComPtr<ID3DBlob> error = nullptr);
	// �V�F�[�_�[�̃Z�b�g
	HRESULT SetShader();
	// �p�C�v���C���̐���
	HRESULT CreateGraphicsPipelineForPMD();
	// �萔�o�b�t�@�쐬
	HRESULT CreateConstantBuffer();

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
	SIZE GetWindowSize()const;
	~Application();
};

