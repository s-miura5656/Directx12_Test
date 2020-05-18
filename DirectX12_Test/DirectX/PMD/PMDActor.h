#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <string>
#include <memory>
#include <unordered_map>
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
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1, p2;
		KeyFrame(unsigned int fno, DirectX::XMVECTOR& q, DirectX::XMFLOAT3 ofst, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
			: frameNo(fno), quaternion(q), offset(ofst), p1(ip1), p2(ip2) {}
	};


//	struct BoneNode
//	{
//		int boneIdx;					 // �{�[���C���f�b�N�X
//		DirectX::XMFLOAT3 startPos;	     // �{�[����_ ( ��]�̒��S )
//		DirectX::XMFLOAT3 endPos;	     // �{�[����[�_ ( ���ۂ̃X�L�j���O�ɂ͗��p���Ȃ� )
//		std::vector<BoneNode*> children; // �q�m�[�h
//	};

	struct BoneNode {
		uint32_t boneIdx;                // �{�[���C���f�b�N�X
		uint32_t boneType;               // �{�[�����
		uint32_t ikParentBone;           // IK�e�{�[��
		DirectX::XMFLOAT3 startPos;      // �{�[����_(��]���S)
		std::vector<BoneNode*> children; // �q�m�[�h
	};

	struct PMDIK 
	{
		uint16_t boneIdx;
		uint16_t targetIdx;
		uint16_t iterations;
		float limit;
		std::vector<uint16_t> nodeIdx;
	};

	std::vector<PMDIK> pmdIkData;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �萔�錾
	const size_t pmdvertexsize = 38; // ���_�P������̃T�C�Y
	const float epsilon = 0.0005f;   // �덷�͈͓̔����ǂ����Ɏg�p����萔

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
	std::unordered_map <std::string, std::vector<KeyFrame>> _motiondata;
	std::vector<std::string> _boneNameArray;
	std::vector<BoneNode*> boneNodeAddressArray;

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;
	ComPtr<ID3D12Resource> _transformMat = nullptr;//���W�ϊ��s��(���̓��[���h�̂�)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//���W�ϊ��q�[�v

	float angle;
	DWORD startTime;
	DWORD elapsedTime;
	unsigned int duration;

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
	// ���[�V�����Đ�
	void MotionUpdate();

	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
	
	void IkDebug(std::vector<PMDIK> pmd_Ik_Data);

	// CCDIK �ɂ��{�[������������
	// @param ik �Ώ� IK �I�u�W�F�N�g
	void SolveCCDIK(const PMDIK& ik);

	// �]���藝 IK �ɂ��{�[������������
	// @param ik �Ώ� IK �I�u�W�F�N�g
	void SolveCosineIK(const PMDIK& ik);

	// LookAt �s��ɂ��{�[������������
	// @param ik �Ώ� IK �I�u�W�F�N�g
	void SolveLookAt(const PMDIK& ik);

	void IKSolve();

	std::vector<uint32_t> _kneeIdxes;

public:
	PMDActor(std::shared_ptr<PMDRenderer> renderer, const char* path);
	~PMDActor();
	
	void Update();
	void Draw();

	void LoadVMDFile(const char* filepath, const char* name);
	void PlayAnimation();
};

