#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <string>
#include <memory>
#include <map>


class Dx12Wrapper;
class PMDRenderer;
class PMDActor
{
	friend PMDRenderer;
private:
	std::shared_ptr<PMDRenderer> _renderer;
	std::shared_ptr<Dx12Wrapper> _dx12;

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

	struct Transform {
		//�����Ɏ����Ă�XMMATRIX�����o��16�o�C�g�A���C�����g�ł��邽��
		//Transform��new����ۂɂ�16�o�C�g���E�Ɋm�ۂ���
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	struct BoneNode
	{
		int boneIdx;					 // �{�[���C���f�b�N�X
		DirectX::XMFLOAT3 startPos;				 // �{�[����_ ( ��]�̒��S )
		DirectX::XMFLOAT3 endPos;				 // �{�[����[�_ ( ���ۂ̃X�L�j���O�ɂ͗��p���Ȃ� )
		std::vector<BoneNode*> children; // �q�m�[�h
	};

	struct VMDMotionData
	{
		char boneName[15];            // �{�[����
		unsigned int frameNo;         // �t���[���ԍ�(�Ǎ����͌��݂̃t���[���ʒu��0�Ƃ������Έʒu)
		DirectX::XMFLOAT3 location;	  // �ʒu
		DirectX::XMFLOAT4 quaternion; // Quaternion // ��]
		unsigned char bezier[64];     // [4][4][4]  �x�W�F�⊮�p�����[�^
	};

	struct KeyFrame
	{
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		KeyFrame(unsigned int fno, DirectX::XMVECTOR& q)
			: frameNo(fno), quaternion(q) {}
	};

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �萔�錾
	const size_t pmdvertexsize = 38; // ���_�P������̃T�C�Y

	// �ϐ��錾
	D3D12_VERTEX_BUFFER_VIEW vbView;
	D3D12_INDEX_BUFFER_VIEW ibView;

	ComPtr<ID3D12DescriptorHeap> materialDescHeap;

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
	ComPtr<ID3D12Resource> materialBuff;

	std::vector<DirectX::XMMATRIX> _boneMatrices;
	std::map<std::string, BoneNode> _boneNodeTable;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;
	ComPtr<ID3D12Resource> _transformMat = nullptr;//���W�ϊ��s��(���̓��[���h�̂�)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//���W�ϊ��q�[�v

	float angle;

	// PMD �t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);
	// ���_�o�b�t�@�̍쐬
	HRESULT CreateVertexBuffer();
	// �C���f�b�N�X�o�b�t�@�쐬
	HRESULT CreateIndexBuffer();
	// �}�e���A���o�b�t�@�쐬
	HRESULT CreateMaterialBuffer();
	// ���W�ϊ��p�r���[�̐���
	HRESULT CreateTransformView();
	// �}�e���A�����e�N�X�`���̃r���[���쐬
	HRESULT CreateMaterialAndTextureView();
	// �ċA�֐�
	void RecursiveMatrixMultipy(BoneNode* node, DirectX::XMMATRIX& mat);

public:
	PMDActor(std::shared_ptr<PMDRenderer> renderer, const char* path);
	~PMDActor();
	
	void Update();
	void Draw();

	void LoadVMDFile(const char* filepath, const char* name);
};

