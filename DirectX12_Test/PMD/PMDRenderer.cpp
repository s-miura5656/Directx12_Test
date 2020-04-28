#include "PMDRenderer.h"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "../Dx12Wrapper/Dx12Wrapper.h"
#include <string>
#include <algorithm>



using namespace std;
using namespace DirectX;

bool PMDRenderer::CheckShaderCompileResult(HRESULT result, ComPtr<ID3DBlob> error)
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

HRESULT PMDRenderer::SetShader()
{
	HRESULT result;

	result = D3DCompileFromFile(L"Shader/BaseVertexShader.hlsl",
								nullptr,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								"BaseVS", "vs_5_0",
								D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								0, _vsBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());

	CheckShaderCompileResult(result, errorBlob);

	result = D3DCompileFromFile(L"Shader/BasePixelShader.hlsl",
								nullptr,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								"BasePS", "ps_5_0",
								D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								0, _psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());

	CheckShaderCompileResult(result, errorBlob);

	return result;
}

HRESULT PMDRenderer::CreateGraphicsPipelineForPMD()
{
	HRESULT result = S_OK;
	SetShader();

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

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	CD3DX12_DESCRIPTOR_RANGE descTblRange[3] = {};//テクスチャと定数の２つ

	descTblRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descTblRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	descTblRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

	CD3DX12_ROOT_PARAMETER rootparam[2] = {};

	rootparam[0].InitAsDescriptorTable(1, &descTblRange[0]);
	rootparam[1].InitAsDescriptorTable(2, &descTblRange[1]);

	rootSignatureDesc.pParameters = rootparam;// ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = 2;          // ルートパラメータ数

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc[2] = {};

	samplerDesc[0].Init(0);
	samplerDesc[1].Init(1, D3D12_FILTER_ANISOTROPIC, 
						   D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
						   D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	rootSignatureDesc.pStaticSamplers = samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 2;

	ComPtr<ID3DBlob> rootSigBlob = nullptr;

	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	result = _dx12->Device()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature));

	gpipeline.pRootSignature = rootsignature.Get();


	result = _dx12->Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelinestate));

	return result;
}

HRESULT PMDRenderer::CreateConstantBuffer()
{
	HRESULT result = S_OK;

	DXGI_SWAP_CHAIN_DESC1 desc = {};

	result = _dx12->SwapChain()->GetDesc1(&desc);

	worldMat = XMMatrixIdentity();

	XMFLOAT3 eye(0, 15, -15);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);

	viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	projMat = XMMatrixPerspectiveFovLH(
			  XM_PIDIV4, // 画角 45°
			  static_cast<float>(desc.Width) / static_cast<float>(desc.Height), // アスペクト比
			  1.0f,    // 近いほう
			  100.0f); // 遠いほう

	ID3D12Resource* constBuff = nullptr;

	result = _dx12->Device()->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&constBuff));

	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);

	mapMatrix->world = worldMat;
	mapMatrix->view = viewMat;
	mapMatrix->proj = projMat;
	mapMatrix->eye = eye;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dx12->Device()->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;

	_dx12->Device()->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	return result;
}

PMDRenderer::PMDRenderer(std::shared_ptr<Dx12Wrapper> dx12):_dx12(dx12)
{
	CreateGraphicsPipelineForPMD();
	CreateConstantBuffer();
}

PMDRenderer::~PMDRenderer()
{
}


void
PMDRenderer::Update() 
{
	angle += 0.01f;
}
void
PMDRenderer::Draw() 
{
	worldMat = XMMatrixRotationY(angle);
	mapMatrix->world = worldMat;
	mapMatrix->view = viewMat;
	mapMatrix->proj = projMat;
	_dx12->CommandList()->SetPipelineState(pipelinestate.Get());
	_dx12->CommandList()->SetGraphicsRootSignature(rootsignature.Get());
	_dx12->CommandList()->SetDescriptorHeaps(1, basicDescHeap.GetAddressOf());
	_dx12->CommandList()->SetGraphicsRootDescriptorTable(
		0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
}

PMDRenderer::ComPtr<ID3D12PipelineState> PMDRenderer::GetPipelineState() { return pipelinestate; }

PMDRenderer::ComPtr<ID3D12RootSignature> PMDRenderer::GetRootSignature() { return rootsignature; }

