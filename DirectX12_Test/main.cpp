//ウィンドウ表示＆DirectX初期化
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


#ifdef _DEBUG
#include<iostream>
#endif // _DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")

using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

///@brief コンソール画面にフォーマット付き文字列を表示
///@param format フォーマット(%dとか%fとかの)
///@param 可変長引数
///@remarksこの関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

// モデルのパスとテクスチャのパスから合成パスを得る
// @param modelPath アプリケーションから見た pmd モデルのパス
// @param texPath PMD モデルから見たテクスチャのパス
// @return アプリケーションから見たテクスチャのパス
std::string GetTexturePathFromModelAndTexPath(
			const std::string& modelPath,
			const char* texPath)
{
	int pathIndex1 = modelPath.rfind('/');
	int pathIndex2 = modelPath.rfind('\\');
	auto pathIndex = max(pathIndex1, pathIndex2);
	auto folderPath = modelPath.substr(0, pathIndex + 1);
	
	return folderPath + texPath;
}

// std::string ( マルチバイト文字列 ) からstd::wstring ( ワイド文字列 )　を得る
// @param str マルチバイト文字列
// @return 変換されたワイド文字列
std::wstring GetWideStringFromString(const std::string& str)
{
	// 呼び出し一回目 ( 文字列数を得る )
	auto num1 = MultiByteToWideChar(
				CP_ACP,
				MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
				str.c_str(),
				-1,
				nullptr,
				0);

	std::wstring wstr;
	wstr.resize(num1);

	auto num2 = MultiByteToWideChar(
				CP_ACP,
				MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
				str.c_str(),
				-1,
				&wstr[0],
				num1);

	assert(num1 == num2);
	
	return wstr;
}

ID3D12Resource* LoadTextureFromFile(std::string& texPath)
{
	// WIC テクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto result = LoadFromWICFile(
				  GetWideStringFromString(texPath).c_str(),
				  WIC_FLAGS_NONE,
				  &metadata,
				  scratchImg);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	// WriteToSubresource で転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// バッファー作成
	ID3D12Resource* texBuff = nullptr;

	result = _dev->CreateCommittedResource(
			 &texHeapProp,
			 D3D12_HEAP_FLAG_NONE,
			 &resDesc,
			 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			 nullptr,
			 IID_PPV_ARGS(&texBuff));

	if (FAILED(result))
	{
		return nullptr;
	}

	result = texBuff->WriteToSubresource(
					  0,
					  nullptr,
					  img->pixels,
					  img->rowPitch,
					  img->slicePitch
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	return texBuff;
}

ID3D12Resource* CreateWhiteTexture()
{
	// WriteToSubresource で転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 4;
	resDesc.Height = 4;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// バッファー作成
	ID3D12Resource* whiteBuff = nullptr;

	auto result = _dev->CreateCommittedResource(
						&texHeapProp,
						D3D12_HEAP_FLAG_NONE,
						&resDesc,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						nullptr,
						IID_PPV_ARGS(&whiteBuff));

	if (FAILED(result))
	{
		return nullptr;
	}

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);

	result = whiteBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		data.size()
	);

	return whiteBuff;
}

// ファイル名から拡張子を取得する
// @param path 対象のパス文字列
// @param 拡張子
std::string GetExtension(const std::string& path)
{
	int idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

// テクスチャのパスをセパレーター文字で分離する
// @param path 対象のパス文字列
// @param splitter 区切り文字
// @return 分離前後の文字列ペア
std::pair<std::string, std::string> SplitFileName(
	const std::string& path, const char splitter = '*') 
{
	int idx = path.find(splitter);
	std::pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(idx + 1, path.length() - idx - 1);
	return ret;
}

// 面倒だけど書かなあかんやつ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) // ウィンドウが破棄されたら呼ばれます
	{
		PostQuitMessage(0); // OSに対して「もうこのアプリは終わるんや」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 規定の処理を行う
}

void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

#ifdef _DEBUG
int main() {
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif // _DEBUG
	DebugOutputFormatString("Show window test.");
	HINSTANCE hInst = GetModuleHandle(nullptr);
	// ウィンドウクラス生成＆登録
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = _T("DirectXTest"); // アプリケーションクラス名(適当でいいです)
	w.hInstance = GetModuleHandle(0); // ハンドルの取得
	RegisterClassEx(&w); // アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, window_width, window_height }; // ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // ウィンドウのサイズはちょっと面倒なので関数を使って補正する
	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName, // クラス名指定
		_T("DX12_TEST"), // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT, // 表示X座標はOSにお任せします
		CW_USEDEFAULT, // 表示Y座標はOSにお任せします
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		w.hInstance, // 呼び出しアプリケーションハンドル
		nullptr); // 追加パラメータ

#ifdef _DEBUG
	// デバッグレイヤーをオンに
	EnableDebugLayer();
#endif
	// DirectX12まわり初期化
	// フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	HRESULT result = S_OK;

	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory))))
	{
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory)))) {
			return -1;
		}
	}

	std::vector <IDXGIAdapter*> adapters;

	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};

		adpt->GetDesc(&adesc);

		std::wstring strDesc = adesc.Description;

		if (strDesc.find(L"NVIDIA") != std::string::npos) {

			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;

	for (auto l : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) {

			featureLevel = l;
			break;
		}
	}

	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // ここはコマンドリストと合わせてください
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue)); // コマンドキュー生成

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	result = _dxgiFactory->CreateSwapChainForHwnd(
						   _cmdQueue,
						   hwnd,
						   &swapchainDesc,
						   nullptr,
						   nullptr,
						   (IDXGISwapChain1**)&_swapchain);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i)
	{
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));

		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);

		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// 深度バッファの作成
	D3D12_RESOURCE_DESC depthResDesc = {};
	
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 二次元のテクスチャデータ
	depthResDesc.Width = window_width;							  // 幅と高さはレンダーターゲットと同じ
	depthResDesc.Height = window_height;						  // 同上
	depthResDesc.DepthOrArraySize = 1;							  // テクスチャ配列でも、3D テクスチャでもない
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;				  // 深度値書き込み用フォーマット
	depthResDesc.SampleDesc.Count = 1;							  // サンプルは 1 ピクセル当たり 1 つ
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // デプスステンシルとして使用

	// 深度値用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};

	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// このクリアバリューが重要な意味を持つ
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 深さ 1.0 ( 最大値 )でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; // 32 ビット float 値としてクリア

	ID3D12Resource* depthBuffer = nullptr;

	result = _dev->CreateCommittedResource(
				   &depthHeapProp,
				   D3D12_HEAP_FLAG_NONE,
				   &depthResDesc,
				   D3D12_RESOURCE_STATE_DEPTH_WRITE,
				   &depthClearValue,
				   IID_PPV_ARGS(&depthBuffer)
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};

	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	ID3D12DescriptorHeap* dsvHeap = nullptr;

	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	// 深度ビューの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 深度値に 32 ビット使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	_dev->CreateDepthStencilView(
		  depthBuffer,
		  &dsvDesc,
		  dsvHeap->GetCPUDescriptorHandleForHeapStart());



	ID3D12Fence* _fence = nullptr;

	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hwnd, SW_SHOW); // ウィンドウ表示

	// PMDヘッダー構造体
	struct PMDheader
	{
		float version;		 // 例 : 00 00 80 3F = 1.00
		char model_name[20]; // モデル名
		char comment[256];	 // モデルコメント
	};

	char signature[3] = {}; // シグネチャ
	PMDheader pmdheader = {};
	std::string strModelPath = "Content/Model/巡音ルカ.pmd";
	auto fp = fopen(strModelPath.c_str(), "rb");
	
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	// PMD頂点構造体
	struct PMDVertex 
	{
		XMFLOAT3 pos;			  // 頂点座標	    : 12バイト
		XMFLOAT3 normal;		  // 法線ベクトル	: 12バイト
		XMFLOAT2 uv;			  // uv 座標		:  8バイト
		unsigned short boneNo[2]; // ボーン番号		:  4バイト
		unsigned char boneWeight; // ボーン影響度	:  1バイト
		unsigned char edgeFlg;	  // 輪郭線フラグ	:  1バイト
	};

#pragma pack(1) // ここから 1 バイトパッキングとなり、アライメントは発生しない
	// PMD マテリアル構造体
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;		 // ディフューズ色
		float alpha;			 // ディフューズα
		float specularity;		 // スペキュラの強さ
		XMFLOAT3 specular;		 // スペキュラ色
		XMFLOAT3 ambient;		 // アンビエント色
		unsigned char toonIdx;	 // トゥーン番号
		unsigned char edgeFlg;	 // マテリアルごとの輪郭線フラグ

		// 注意 : ここに 2 バイトのパディングがある 

		unsigned int indicesNum; // 子のマテリアルが割り当てられるインデックス数 
		char texFilePath[20];	 //  テクスチャファイルパス + α
	};
#pragma pack()

	// シェーダー側に投げられるマテリアルデータ
	struct MaterialForHlsl
	{
		XMFLOAT3 diffuse;
		float alpha;
		XMFLOAT3 specular;
		float specularity;
		XMFLOAT3 ambient;
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

	constexpr size_t pmdvertexsize = 38; // 頂点１つ当たりのサイズ

	unsigned int vertNum; // 頂点数

	fread(&vertNum, sizeof(vertNum), 1, fp);

	std::vector<unsigned char> vertices(vertNum* pmdvertexsize); // バッファーの確保
	
	fread(vertices.data(), vertices.size(), 1, fp);

	std::vector<unsigned short> indices;

	unsigned int indicesNum;

	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	indices.resize(indicesNum);

	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	unsigned int materialNum;

	fread(&materialNum, sizeof(materialNum), 1, fp);

	std::vector<ID3D12Resource*> textureResources(materialNum);
	std::vector<ID3D12Resource*> sphResources(materialNum);
	std::vector<PMDMaterial> pmdMaterials(materialNum);
	
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	std::vector<Material> materials(pmdMaterials.size());

	for (size_t i = 0; i < pmdMaterials.size(); ++i)
	{
		materials[i].indicesNum		      = pmdMaterials[i].indicesNum;
		materials[i].material.diffuse     = pmdMaterials[i].diffuse;
		materials[i].material.alpha       = pmdMaterials[i].alpha;
		materials[i].material.specular    = pmdMaterials[i].specular;
		materials[i].material.specularity = pmdMaterials[i].specularity;
		materials[i].material.ambient	  = pmdMaterials[i].ambient;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		std::string texFileName = pmdMaterials[i].texFilePath;

		if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);
			if (GetExtension(namepair.first) == "sph" ||
				GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
			}
			else
			{
				texFileName = namepair.first;
			}
		}

		

		auto texFilePath = GetTexturePathFromModelAndTexPath(
						   strModelPath,
						   texFileName.c_str());

		textureResources[i] = LoadTextureFromFile(texFilePath);
	}

	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	/* 頂点バッファ -----------------------------------------------*/
	ID3D12Resource* vertBuff = nullptr;

	result = _dev->CreateCommittedResource(
				   &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				   D3D12_HEAP_FLAG_NONE,
				   &CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
				   D3D12_RESOURCE_STATE_GENERIC_READ,
				   nullptr,
				   IID_PPV_ARGS(&vertBuff)
	);

	unsigned char* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = vertices.size();
	vbView.StrideInBytes = pmdvertexsize;
	/*------------------------------------------------------------*/

	fclose(fp);

	/* インデックスバッファ --------------------------------------*/
	ID3D12Resource* idxBuff = nullptr;

	result = _dev->CreateCommittedResource(
				   &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				   D3D12_HEAP_FLAG_NONE,
				   &CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
				   D3D12_RESOURCE_STATE_GENERIC_READ,
				   nullptr,
				   IID_PPV_ARGS(&idxBuff)
	);

	unsigned short* mappedIndex = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIndex);
	std::copy(std::begin(indices), std::end(indices), mappedIndex);
	idxBuff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = {};

	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = indices.size() * sizeof(indices[0]);
	/*------------------------------------------------------------*/

	/* マテリアルバッファ --------------------------------------*/
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	ID3D12Resource* materialBuff = nullptr;
	
	result = _dev->CreateCommittedResource(
				   &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				   D3D12_HEAP_FLAG_NONE,
				   &CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum),
				   D3D12_RESOURCE_STATE_GENERIC_READ,
				   nullptr,
				   IID_PPV_ARGS(&materialBuff));

	char* mapMaterial = nullptr;

	result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	
	for (auto& m : materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material; // データコピー
		mapMaterial += materialBuffSize;
	}

	materialBuff->Unmap(0, nullptr);

	ID3D12DescriptorHeap* materialDescHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};

	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = materialNum * 3;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(
				   &matDescHeapDesc, IID_PPV_ARGS(&materialDescHeap));

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};

	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize; // マテリアルの 256 アライメントサイズ

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// 先頭を記録
	auto matDescHeapH = materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto whiteTex = CreateWhiteTexture();

	for (int i = 0; i < materialNum; ++i)
	{
		_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (textureResources[i] == nullptr)
		{
			srvDesc.Format = whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(
				  whiteTex, &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = textureResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(
				  textureResources[i],
				  &srvDesc,
				  matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (sphResources[i] == nullptr)
		{
			srvDesc.Format = whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(
				whiteTex, &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = sphResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(
				sphResources[i], &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
	/*------------------------------------------------------------*/

	

	/* シェーダ設定 ----------------------------------------------*/
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
								nullptr,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								"BasicVS", "vs_5_0",
								D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								0, &_vsBlob, &errorBlob);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA(" ファイルが見当たりません ");
			return 0;
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(),
						errorBlob->GetBufferSize(),
						errstr.begin());
			errstr += "\n";
			::OutputDebugStringA(errstr.c_str());
		}
	}

	result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
								nullptr,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								"BasicPS", "ps_5_0",
								D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								0, &_psBlob, &errorBlob);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA(" ファイルが見当たりません ");
			return 0;
		}
		else
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(),
				errorBlob->GetBufferSize(),
				errstr.begin());
			errstr += "\n";
			::OutputDebugStringA(errstr.c_str());
		}
	}
	/*------------------------------------------------------------*/

	

	/* 頂点レイアウト --------------------------------------------*/
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, 
		             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
					 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
					 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BONE_NO",  0, DXGI_FORMAT_R16G16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
					 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"WEIGHT",   0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
					 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
//		{"EDGEFLT",  0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
//					 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	/*------------------------------------------------------------*/

	/* グラフィックスパイプライン --------------------------------*/
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState.MultisampleEnable = false;
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;
	gpipeline.DepthStencilState.DepthEnable = true; // 深度バッファーを使う
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込む
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 小さいほうを採用
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	ID3D12RootSignature* rootsignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {};//テクスチャと定数の２つ
	
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[0].BaseShaderRegister = 0;
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 1;
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[2].NumDescriptors = 2;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].BaseShaderRegister = 0;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam[2] = {};
	
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 1;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 2;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSignatureDesc.pParameters = rootparam;// ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = 2;          // ルートパラメータ数

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;

	result = D3D12SerializeRootSignature(
			 &rootSignatureDesc,
			 D3D_ROOT_SIGNATURE_VERSION_1_0,
			 &rootSigBlob, &errorBlob);

	result = _dev->CreateRootSignature(
				   0,
				   rootSigBlob->GetBufferPointer(),
				   rootSigBlob->GetBufferSize(),
				   IID_PPV_ARGS(&rootsignature));

	rootSigBlob->Release();

	gpipeline.pRootSignature = rootsignature;

	ID3D12PipelineState* _pipelinestate = nullptr;

	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));
	/*------------------------------------------------------------*/

	struct TexRGBA
	{
		unsigned char R, G, B, A;
	};

	std::vector <TexRGBA> texturedata(256 * 256);

	for (auto& rgba : texturedata)
	{
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255;
	}

	

	/* 定数バッファー ----------------------------------------*/
	auto worldMat = XMMatrixRotationY(XM_PIDIV4);

	XMFLOAT3 eye(0, 10, -15);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);

	auto viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	auto projMat = XMMatrixPerspectiveFovLH(
				   XM_PIDIV2, // 画角 90°
				   static_cast<float>(window_width) / static_cast<float>(window_height), // アスペクト比
				   1.0f,    // 近いほう
				   100.0f); // 遠いほう

	auto WVP = worldMat * viewMat * projMat;
	/*------------------------------------------------------*/

	// シェーダー側に渡す為の基本的な行列のデータ
	struct MatricesData
	{
		XMMATRIX world;	   // モデル本体を回転させたり移動させたりする行列
		XMMATRIX viewproj; // ビューとプロジェクションの合成行列
	};

	ID3D12Resource* constBuff = nullptr;

	result = _dev->CreateCommittedResource(
				   &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				   D3D12_HEAP_FLAG_NONE,
				   &CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xff),
				   D3D12_RESOURCE_STATE_GENERIC_READ,
				   nullptr,
				   IID_PPV_ARGS(&constBuff));

	MatricesData* mapMatrix = nullptr;

	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);

	mapMatrix->world = worldMat;
	mapMatrix->viewproj = viewMat * projMat;

//	*mapMatrix = WVP;

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	
	MSG msg = {};
	float angle = 0.0f;
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		angle += 0.01f;
		worldMat = XMMatrixRotationY(angle);
		mapMatrix->world = worldMat;
		mapMatrix->viewproj = viewMat * projMat;

		// DirectX処理
		// バックバッファのインデックスを取得
		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx], 
									  D3D12_RESOURCE_STATE_PRESENT, 
									  D3D12_RESOURCE_STATE_RENDER_TARGET));

		_cmdList->SetPipelineState(_pipelinestate);

		// レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		// 画面クリア
		float clearColor[] = { 1.0f,1.0f,1.0f,1.0f }; // 白色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		/*-魔導書に書いてあるけど実装部分を書いてないところ ( 書かないと直角三角形にならない )-*/
		_cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, window_width, window_height));
		_cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0.0f, 0.0f, window_width, window_height));
		/*-------------------------------------------------------------------------------------*/

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);   // 頂点バッファ
		_cmdList->IASetIndexBuffer(&ibView);		   // インデックスバッファ

		_cmdList->SetGraphicsRootSignature(rootsignature);
		
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(
				  0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
		

		_cmdList->SetDescriptorHeaps(1, &materialDescHeap);

		auto materialH = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		unsigned int idxOffset = 0;

		auto cbvsrvIncSize = _dev->GetDescriptorHandleIncrementSize(
								   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3;

		for (auto& m : materials)
		{
			_cmdList->SetGraphicsRootDescriptorTable(1, materialH);

			_cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);

			// ヒープポインターとインデックスを次に進める
			materialH.ptr += cbvsrvIncSize;
		
			idxOffset += m.indicesNum;
		}

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx],
								      D3D12_RESOURCE_STATE_RENDER_TARGET,
								      D3D12_RESOURCE_STATE_PRESENT));

		// 命令のクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);
		// 待ち
		_cmdQueue->Signal(_fence, ++_fenceVal);

		if (_fence->GetCompletedValue() != _fenceVal)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);

			_fence->SetEventOnCompletion(_fenceVal, event);

			WaitForSingleObject(event, INFINITE);

			CloseHandle(event);
		}

		_cmdAllocator->Reset(); // キューをクリア
		_cmdList->Reset(_cmdAllocator, nullptr); // 再びコマンドリストをためる準備

		// フリップ
		_swapchain->Present(1, 0);
	}

	// もうクラス使わないから登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}