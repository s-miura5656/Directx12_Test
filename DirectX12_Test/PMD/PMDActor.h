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

	// シェーダー側に投げられるマテリアルデータ
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	// それ以外のマテリアルデータ
	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// 全体をまとめるデータ
	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 定数宣言
	const size_t pmdvertexsize = 38; // 頂点１つ当たりのサイズ

	// 変数宣言
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
	unsigned int vertNum; // 頂点数
	unsigned int indicesNum;
	unsigned int materialNum;
	ComPtr<ID3D12Resource> vertBuff;
	ComPtr<ID3D12Resource> idxBuff;

	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	// PMD ファイルのロード
	HRESULT LoadPMDFile(const char* path);
	// 頂点バッファの作成
	HRESULT CreateVertexBuffer();
	// インデックスバッファ作成
	HRESULT CreateIndexBuffer();
	// マテリアルバッファ作成
	HRESULT CreateMaterialBuffer();

public:
	PMDActor(std::shared_ptr<Dx12Wrapper> dx12, const char* path);
	~PMDActor();
	
	void Update();
	void Draw();

};

