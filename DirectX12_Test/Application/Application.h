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

//シングルトンクラス
class Application
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 定数
	const size_t pmdvertexsize = 38; // 頂点１つ当たりのサイズ

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

	// シェーダー側に渡す為の基本的な行列のデータ
	struct SceneMatrix
	{
		DirectX::XMMATRIX world;	   // ワールド行列
		DirectX::XMMATRIX view;	   // ビュー行列
		DirectX::XMMATRIX proj;	   // プロジェクション行列
		DirectX::XMFLOAT3 eye;      // 視点座標
	};

	// 変数宣言 /////////////////////////////////////////////////////////////////////////////
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
	unsigned int vertNum; // 頂点数
	unsigned int indicesNum;
	unsigned int materialNum;
	ComPtr<ID3D12Resource> vertBuff;
	ComPtr<ID3D12Resource> idxBuff;
	ComPtr<ID3DBlob> _vsBlob;
	ComPtr<ID3DBlob> _psBlob;
	ComPtr<ID3DBlob> errorBlob;
	// 関数宣言 ////////////////////////////////////////////////////////////////////////////
	//ゲーム用ウィンドウの生成
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	
	// PMD ファイルのロード
	HRESULT LoadPMDFile();
	// 頂点バッファの作成
	HRESULT CreateVertexBuffer();
	// インデックスバッファ作成
	HRESULT CreateIndexBuffer();
	// マテリアルバッファ作成
	HRESULT CreateMaterialBuffer();
	// シェーダー読み込み成功か失敗か確認
	bool CheckShaderCompileResult(HRESULT result, ComPtr<ID3DBlob> error = nullptr);
	// シェーダーのセット
	HRESULT SetShader();
	// パイプラインの生成
	HRESULT CreateGraphicsPipelineForPMD();
	// 定数バッファ作成
	HRESULT CreateConstantBuffer();

	//↓シングルトンのためにコンストラクタをprivateに
	//さらにコピーと代入を禁止に
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	//Applicationのシングルトンインスタンスを得る
	static Application& Instance();

	///初期化
	bool Init();

	///ループ起動
	void Run();

	///後処理
	void Terminate();
	SIZE GetWindowSize() const;
	~Application();
};

