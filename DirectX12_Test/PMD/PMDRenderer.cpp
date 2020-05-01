#include "PMDRenderer.h"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "../Dx12Wrapper/Dx12Wrapper.h"
#include <string>
#include <algorithm>



using namespace std;
using namespace DirectX;

ID3D12Resource* PMDRenderer::CreateDefaultTexture(size_t width, size_t height)
{
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	
	ID3D12Resource* buff = nullptr;
	
	auto result = _dx12->Device()->CreateCommittedResource(
				  &texHeapProp,
				  D3D12_HEAP_FLAG_NONE,//特に指定なし
				  &resDesc,
				  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				  nullptr,
				  IID_PPV_ARGS(&buff)
	);

	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return nullptr;
	}
	
	return buff;
}

ID3D12Resource* PMDRenderer::CreateWhiteTexture()
{
	ID3D12Resource* whiteBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 4 * 4);
	
	std::fill(data.begin(), data.end(), 0xff);

	auto result = whiteBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
	
	assert(SUCCEEDED(result));
	
	return whiteBuff;
}

ID3D12Resource* PMDRenderer::CreateBlackTexture()
{
	ID3D12Resource* blackBuff = CreateDefaultTexture(4, 4);
	
	std::vector<unsigned char> data(4 * 4 * 4);
	
	std::fill(data.begin(), data.end(), 0x00);

	auto result = blackBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
	
	assert(SUCCEEDED(result));
	
	return blackBuff;
}

ID3D12Resource* PMDRenderer::CreateGrayGradationTexture()
{
	ID3D12Resource* gradBuff = CreateDefaultTexture(4, 256);
	
	//上が白くて下が黒いテクスチャデータを作成
	std::vector<unsigned int> data(4 * 256);
	
	auto it = data.begin();
	
	unsigned int c = 0xff;
	
	for (; it != data.end(); it += 4) 
	{
		auto col = (c << 0xff) | (c << 16) | (c << 8) | c;
		
		std::fill(it, it + 4, col);
		
		--c;
	}

	auto result = gradBuff->WriteToSubresource(0, nullptr, data.data(), 4 * sizeof(unsigned int), sizeof(unsigned int) * data.size());
	
	assert(SUCCEEDED(result));
	
	return gradBuff;
}

bool PMDRenderer::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
{
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA(" ファイルが見当たりません ");
		}
		else
		{
			std::string errstr;
			errstr.resize(error->GetBufferSize());
			std::copy_n((char*)error->GetBufferPointer(),
				error->GetBufferSize(),
				errstr.begin());
			errstr += "\n";
			::OutputDebugStringA(errstr.c_str());
		}
		return false;
	}
	else
	{
		return true;
	}
}

HRESULT PMDRenderer::CreateGraphicsPipelineForPMD()
{
	ComPtr<ID3DBlob> _vsBlob = nullptr;
	ComPtr<ID3DBlob> _psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3DCompileFromFile(L"Shader/BaseVertexShader.hlsl",
									 nullptr,
									 D3D_COMPILE_STANDARD_FILE_INCLUDE,
									 "BaseVS", "vs_5_0",
									 D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
									 0, &_vsBlob, &errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get())) 
	{
		assert(0);
		return result;
	}


	result = D3DCompileFromFile(L"Shader/BasePixelShader.hlsl",
								nullptr,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								"BasePS", "ps_5_0",
								D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								0, &_psBlob, &errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

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

	gpipeline.pRootSignature = rootSignature.Get();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(_vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(_psBlob.Get());
	
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	
	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	
	gpipeline.DepthStencilState.DepthEnable = true; // 深度バッファーを使う
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込む
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 小さいほうを採用
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.StencilEnable = false;
	
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	result = _dx12->Device()->CreateGraphicsPipelineState(&gpipeline, 
			 IID_PPV_ARGS(pipelinestate.ReleaseAndGetAddressOf()));

	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
	}

	return result;
}

HRESULT PMDRenderer::CreateRootSignature()
{
	// レンジ
	CD3DX12_DESCRIPTOR_RANGE descTblRanges[4] = {};//テクスチャと定数の 3 つ
	
	descTblRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // 定数[b0](ビュープロジェクション用)
	descTblRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // 定数[b1](ワールド、ボーン用)
	descTblRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2); // 定数[b2](マテリアル用)
	descTblRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // テクスチャ４つ(基本と Sph と Spa とトゥーン)
	
	// ルートパラメーター
	CD3DX12_ROOT_PARAMETER rootParams[3] = {};
	
	rootParams[0].InitAsDescriptorTable(1, &descTblRanges[0]); // ビュープロジェクション変換
	rootParams[1].InitAsDescriptorTable(1, &descTblRanges[1]); // ワールド・ボーン変換
	rootParams[2].InitAsDescriptorTable(2, &descTblRanges[2]); // マテリアル周り
	
	// サンプラー
	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	
	samplerDescs[0].Init(0);
	samplerDescs[1].Init(1, D3D12_FILTER_ANISOTROPIC,
						    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
						    D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	
	rootSignatureDesc.Init(3, rootParams, 2, samplerDescs, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	
	auto result = D3D12SerializeRootSignature(
				  &rootSignatureDesc,
				  D3D_ROOT_SIGNATURE_VERSION_1,
				  rootSigBlob.ReleaseAndGetAddressOf(),
				  errorBlob.ReleaseAndGetAddressOf());
	
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}
	
	result = _dx12->Device()->CreateRootSignature(
			 0,
			 rootSigBlob->GetBufferPointer(),
			 rootSigBlob->GetBufferSize(),
			 IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()));
	
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}
	
	return result;
}

PMDRenderer::PMDRenderer(std::shared_ptr<Dx12Wrapper> dx12):_dx12(dx12)
{
	assert(SUCCEEDED(CreateRootSignature()));
	assert(SUCCEEDED(CreateGraphicsPipelineForPMD()));
	whiteTex = CreateWhiteTexture();
	blackTex = CreateBlackTexture();
	gradTex = CreateGrayGradationTexture();
}

PMDRenderer::~PMDRenderer()
{
}


void PMDRenderer::Update() 
{
	
}
void PMDRenderer::Draw() 
{
	
}

void PMDRenderer::SetCmdList()
{
	_dx12->CommandList()->SetPipelineState(pipelinestate.Get());
	_dx12->CommandList()->SetGraphicsRootSignature(rootSignature.Get());
}

PMDRenderer::ComPtr<ID3D12PipelineState> PMDRenderer::GetPipelineState() { return pipelinestate; }

PMDRenderer::ComPtr<ID3D12RootSignature> PMDRenderer::GetRootSignature() { return rootSignature; }

