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

	struct Transform {
		//内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
		//Transformをnewする際には16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	struct BoneNode
	{
		int boneIdx;					 // ボーンインデックス
		DirectX::XMFLOAT3 startPos;				 // ボーン基準点 ( 回転の中心 )
		DirectX::XMFLOAT3 endPos;				 // ボーン先端点 ( 実際のスキニングには利用しない )
		std::vector<BoneNode*> children; // 子ノード
	};

	struct VMDMotionData
	{
		char boneName[15];            // ボーン名
		unsigned int frameNo;         // フレーム番号(読込時は現在のフレーム位置を0とした相対位置)
		DirectX::XMFLOAT3 location;	  // 位置
		DirectX::XMFLOAT4 quaternion; // Quaternion // 回転
		unsigned char bezier[64];     // [4][4][4]  ベジェ補完パラメータ
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
	ComPtr<ID3D12Resource> materialBuff;

	std::vector<DirectX::XMMATRIX> _boneMatrices;
	std::map<std::string, BoneNode> _boneNodeTable;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;
	ComPtr<ID3D12Resource> _transformMat = nullptr;//座標変換行列(今はワールドのみ)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//座標変換ヒープ

	float angle;

	// PMD ファイルのロード
	HRESULT LoadPMDFile(const char* path);
	// 頂点バッファの作成
	HRESULT CreateVertexBuffer();
	// インデックスバッファ作成
	HRESULT CreateIndexBuffer();
	// マテリアルバッファ作成
	HRESULT CreateMaterialBuffer();
	// 座標変換用ビューの生成
	HRESULT CreateTransformView();
	// マテリアル＆テクスチャのビューを作成
	HRESULT CreateMaterialAndTextureView();
	// 再帰関数
	void RecursiveMatrixMultipy(BoneNode* node, DirectX::XMMATRIX& mat);

public:
	PMDActor(std::shared_ptr<PMDRenderer> renderer, const char* path);
	~PMDActor();
	
	void Update();
	void Draw();

	void LoadVMDFile(const char* filepath, const char* name);
};

