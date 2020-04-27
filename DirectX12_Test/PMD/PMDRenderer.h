#pragma once

#include <vector>
#include <wrl.h>
#include <memory>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>

class Dx12Wrapper;

class PMDRenderer
{
private:
	// シェーダー側に渡す為の基本的な行列のデータ
	struct SceneMatrix
	{
		DirectX::XMMATRIX world;	// ワールド行列
		DirectX::XMMATRIX view;		// ビュー行列
		DirectX::XMMATRIX proj;		// プロジェクション行列
		DirectX::XMFLOAT3 eye;      // 視点座標
	};


	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 変数宣言
	std::shared_ptr<Dx12Wrapper> _dx12;
	float angle = 0.0f;

	ComPtr<ID3DBlob> _vsBlob;
	ComPtr<ID3DBlob> _psBlob;
	ComPtr<ID3DBlob> errorBlob;
	ComPtr<ID3D12RootSignature> rootsignature;
	ComPtr<ID3D12PipelineState> pipelinestate;
	ComPtr<ID3D12DescriptorHeap> basicDescHeap;

	SceneMatrix* mapMatrix;
	DirectX::XMMATRIX worldMat;
	DirectX::XMMATRIX viewMat;
	DirectX::XMMATRIX projMat;

	// シェーダー読み込み成功か失敗か確認
	bool CheckShaderCompileResult(HRESULT result, ComPtr<ID3DBlob> error = nullptr);
	// シェーダーのセット
	HRESULT SetShader();
	// パイプラインの生成
	HRESULT CreateGraphicsPipelineForPMD();
	// 定数バッファ作成
	HRESULT CreateConstantBuffer();// 定数バッファ作成

	
public:
	PMDRenderer(std::shared_ptr<Dx12Wrapper> dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	ComPtr<ID3D12PipelineState> GetPipelineState();
	ComPtr<ID3D12RootSignature> GetRootSignature();
};

