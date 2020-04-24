#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <map>
#include <unordered_map>
#include <DirectXTex.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <memory>
#include <tchar.h>


class Dx12Wrapper
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	SIZE windowSize;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceVal;
	ID3D12DescriptorHeap* rtvHeaps;
	ID3D12DescriptorHeap* dsvHeap;
	// バックバッファ
	std::vector<ID3D12Resource*> _backBuffers;
	// ビューポート
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	// シザー矩形
	std::unique_ptr<D3D12_RECT> _scissorrect;
	//ファイル名パスとリソースのマップテーブル
	std::map<std::string, ID3D12Resource*> _resourceTable;
	//テクスチャテーブル
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> _textureTable;
	// DXGIまわり初期化
	HRESULT InitializeDXGIDevice();
	// コマンドまわり初期化
	HRESULT InitializeCommand();
	// スワップチェインの生成
	HRESULT CreateSwapChain(const HWND& hwnd);
	// レンダーターゲットビュー生成
	HRESULT CreateRTV();
	// テクスチャローダテーブルの作成
	void CreateTextureLoaderTable();
	// デプスバッファ作成
	HRESULT CreateDepthBuffer();
	// テクスチャファイルの読み込み
	ID3D12Resource* LoadTextureFromFile(const char* texpath);


	using LoadLambda_t = std::function<
		HRESULT(const std::wstring & path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

	std::map<std::string, LoadLambda_t> _loadLambdaTable;

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void BeginDraw();
	void EndDraw();

	///テクスチャパスから必要なテクスチャバッファへのポインタを返す
	///@param texpath テクスチャファイルパス
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> SwapChain();

};

