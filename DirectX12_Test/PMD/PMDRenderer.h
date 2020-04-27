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
	// �V�F�[�_�[���ɓn���ׂ̊�{�I�ȍs��̃f�[�^
	struct SceneMatrix
	{
		DirectX::XMMATRIX world;	// ���[���h�s��
		DirectX::XMMATRIX view;		// �r���[�s��
		DirectX::XMMATRIX proj;		// �v���W�F�N�V�����s��
		DirectX::XMFLOAT3 eye;      // ���_���W
	};


	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// �ϐ��錾
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

	// �V�F�[�_�[�ǂݍ��ݐ��������s���m�F
	bool CheckShaderCompileResult(HRESULT result, ComPtr<ID3DBlob> error = nullptr);
	// �V�F�[�_�[�̃Z�b�g
	HRESULT SetShader();
	// �p�C�v���C���̐���
	HRESULT CreateGraphicsPipelineForPMD();
	// �萔�o�b�t�@�쐬
	HRESULT CreateConstantBuffer();// �萔�o�b�t�@�쐬

	
public:
	PMDRenderer(std::shared_ptr<Dx12Wrapper> dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	ComPtr<ID3D12PipelineState> GetPipelineState();
	ComPtr<ID3D12RootSignature> GetRootSignature();
};

