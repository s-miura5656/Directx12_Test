#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <string>
#include <memory>

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

	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	// PMD �t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);
	// ���_�o�b�t�@�̍쐬
	HRESULT CreateVertexBuffer();
	// �C���f�b�N�X�o�b�t�@�쐬
	HRESULT CreateIndexBuffer();
	// �}�e���A���o�b�t�@�쐬
	HRESULT CreateMaterialBuffer();

public:
	PMDActor(std::shared_ptr<Dx12Wrapper> dx12, const char* path);
	~PMDActor();
	
	void Update();
	void Draw();

};

