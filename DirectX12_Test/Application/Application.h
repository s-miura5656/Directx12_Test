#pragma once

#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <algorithm>
#include <memory>
#include <wrl.h>

class Dx12Wrapper;
//class PMDRenderer;
//class PMDActor;

//�V���O���g���N���X
class Application
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �萔
	const size_t pmdvertexsize = 38; // ���_�P������̃T�C�Y

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
	std::shared_ptr<Dx12Wrapper> _dx12;
	//	std::shared_ptr<PMDRenderer> _pmdRenderer;
	//	std::shared_ptr<PMDActor> _pmdActor;

	D3D12_VERTEX_BUFFER_VIEW vbView;
	D3D12_INDEX_BUFFER_VIEW ibView;

	ComPtr<ID3D12RootSignature> rootsignature;
	ID3D12PipelineState* _pipelinestate;

	SceneMatrix* mapMatrix;
	DirectX::XMMATRIX worldMat;
	DirectX::XMMATRIX viewMat;
	DirectX::XMMATRIX projMat;

	ID3D12DescriptorHeap* basicDescHeap;
	ID3D12DescriptorHeap* materialDescHeap;

	std::vector<Material> materials;
	std::vector<ComPtr<ID3D12Resource>> textureResources;
	std::vector<ComPtr<ID3D12Resource>> sphResources;
	std::vector<ComPtr<ID3D12Resource>> spaResources;
	std::vector<ComPtr<ID3D12Resource>> toonResources;
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
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	
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
	SIZE GetWindowSize() const;
	~Application();
};

