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
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1, p2;
		KeyFrame(unsigned int fno, DirectX::XMVECTOR& q, DirectX::XMFLOAT3 ofst, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
			: frameNo(fno), quaternion(q), offset(ofst), p1(ip1), p2(ip2) {}
	};


//	struct BoneNode
//	{
//		int boneIdx;					 // ボーンインデックス
//		DirectX::XMFLOAT3 startPos;	     // ボーン基準点 ( 回転の中心 )
//		DirectX::XMFLOAT3 endPos;	     // ボーン先端点 ( 実際のスキニングには利用しない )
//		std::vector<BoneNode*> children; // 子ノード
//	};

	struct BoneNode {
		uint32_t boneIdx;                // ボーンインデックス
		uint32_t boneType;               // ボーン種別
		uint32_t ikParentBone;           // IK親ボーン
		DirectX::XMFLOAT3 startPos;      // ボーン基準点(回転中心)
		std::vector<BoneNode*> children; // 子ノード
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

	// 定数宣言
	const size_t pmdvertexsize = 38; // 頂点１つ当たりのサイズ
	const float epsilon = 0.0005f;   // 誤差の範囲内かどうかに使用する定数

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
	std::unordered_map <std::string, std::vector<KeyFrame>> _motiondata;
	std::vector<std::string> _boneNameArray;
	std::vector<BoneNode*> boneNodeAddressArray;

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;
	ComPtr<ID3D12Resource> _transformMat = nullptr;//座標変換行列(今はワールドのみ)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//座標変換ヒープ

	float angle;
	DWORD startTime;
	DWORD elapsedTime;
	unsigned int duration;

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
	// モーション再生
	void MotionUpdate();

	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
	
	void IkDebug(std::vector<PMDIK> pmd_Ik_Data);

	// CCDIK によりボーン方向を解決
	// @param ik 対象 IK オブジェクト
	void SolveCCDIK(const PMDIK& ik);

	// 余弦定理 IK によりボーン方向を解決
	// @param ik 対象 IK オブジェクト
	void SolveCosineIK(const PMDIK& ik);

	// LookAt 行列によりボーン方向を解決
	// @param ik 対象 IK オブジェクト
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

