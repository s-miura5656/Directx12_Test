#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDActor;

class PMDRenderer
{
	friend PMDActor;
private:
	
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 変数宣言
	std::shared_ptr<Dx12Wrapper> _dx12;
	
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelinestate;

	// PMD用共通テクスチャ(白、黒、グレイスケールグラデーション)
	ComPtr<ID3D12Resource> whiteTex = nullptr;
	ComPtr<ID3D12Resource> blackTex = nullptr;
	ComPtr<ID3D12Resource> gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	// シェーダー読み込み成功か失敗か確認
	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);
	// パイプラインの生成
	HRESULT CreateGraphicsPipelineForPMD();
	// ルートシグネチャ初期化
	HRESULT CreateRootSignature();
public:
	PMDRenderer(std::shared_ptr<Dx12Wrapper> dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	void SetCmdList();
	ComPtr<ID3D12PipelineState> GetPipelineState();
	ComPtr<ID3D12RootSignature> GetRootSignature();
};

