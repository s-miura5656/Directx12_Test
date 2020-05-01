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

	// �ϐ��錾
	std::shared_ptr<Dx12Wrapper> _dx12;
	
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelinestate;

	// PMD�p���ʃe�N�X�`��(���A���A�O���C�X�P�[���O���f�[�V����)
	ComPtr<ID3D12Resource> whiteTex = nullptr;
	ComPtr<ID3D12Resource> blackTex = nullptr;
	ComPtr<ID3D12Resource> gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	// �V�F�[�_�[�ǂݍ��ݐ��������s���m�F
	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);
	// �p�C�v���C���̐���
	HRESULT CreateGraphicsPipelineForPMD();
	// ���[�g�V�O�l�`��������
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

