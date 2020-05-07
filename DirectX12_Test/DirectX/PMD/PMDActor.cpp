#include "PMDActor.h"
#include "PMDRenderer.h"
#include "../Dx12Wrapper/Dx12Wrapper.h"
#include <d3dx12.h>
#include <sstream>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

#pragma comment(lib,"winmm.lib")

namespace 
{
// ���f���̃p�X�ƃe�N�X�`���̃p�X���獇���p�X�𓾂�
// @param modelPath �A�v���P�[�V�������猩�� pmd ���f���̃p�X
// @param texPath PMD ���f�����猩���e�N�X�`���̃p�X
// @return �A�v���P�[�V�������猩���e�N�X�`���̃p�X
	string GetTexturePathFromModelAndTexPath(
		const string& modelPath,
		const char* texPath)
	{
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);

		return folderPath + texPath;
	}

// string ( �}���`�o�C�g������ ) ���� wstring ( ���C�h������ )�@�𓾂�
// @param str �}���`�o�C�g������
// @return �ϊ����ꂽ���C�h������
	wstring GetWideStringFromString(const string& str)
	{
		// �Ăяo������ ( �����񐔂𓾂� )
		auto num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			nullptr,
			0);

		wstring wstr;
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

// �t�@�C��������g���q���擾����
// @param path �Ώۂ̃p�X������
// @param �g���q
	string GetExtension(const string& path)
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

// �e�N�X�`���̃p�X���Z�p���[�^�[�����ŕ�������
// @param path �Ώۂ̃p�X������
// @param splitter ��؂蕶��
// @return �����O��̕�����y�A
	std::pair<std::string, std::string> SplitFileName(
		const std::string& path, const char splitter = '*')
	{
		int idx = path.find(splitter);
		std::pair<std::string, std::string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}
}

void* PMDActor::Transform::operator new(size_t size) 
{
	return _aligned_malloc(size, 16);
}

// @param ���f���p�X
HRESULT PMDActor::LoadPMDFile(const char* path)
{
	HRESULT result = S_OK;

	// PMD�w�b�_�[�\����
	struct PMDheader
	{
		float version;		 // �� : 00 00 80 3F = 1.00
		char model_name[20]; // ���f����
		char comment[256];	 // ���f���R�����g
	};

	char signature[3] = {}; // �V�O�l�`��
	PMDheader pmdheader = {};
	std::string strModelPath = path;

	auto fp = fopen(strModelPath.c_str(), "rb");

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	// PMD���_�\����
	struct PMDVertex
	{
		XMFLOAT3 pos;			  // ���_���W	    : 12�o�C�g
		XMFLOAT3 normal;		  // �@���x�N�g��	: 12�o�C�g
		XMFLOAT2 uv;			  // uv ���W		:  8�o�C�g
		unsigned short boneNo[2]; // �{�[���ԍ�		:  4�o�C�g
		unsigned char boneWeight; // �{�[���e���x	:  1�o�C�g
		unsigned char edgeFlg;	  // �֊s���t���O	:  1�o�C�g
	};

#pragma pack(1) // �������� 1 �o�C�g�p�b�L���O�ƂȂ�A�A���C�����g�͔������Ȃ�
	// PMD �}�e���A���\����
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;		 // �f�B�t���[�Y�F
		float alpha;			 // �f�B�t���[�Y��
		float specularity;		 // �X�y�L�����̋���
		XMFLOAT3 specular;		 // �X�y�L�����F
		XMFLOAT3 ambient;		 // �A���r�G���g�F
		unsigned char toonIdx;	 // �g�D�[���ԍ�
		unsigned char edgeFlg;	 // �}�e���A�����Ƃ̗֊s���t���O

		// ���� : ������ 2 �o�C�g�̃p�f�B���O������ 

		unsigned int indicesNum; // �q�̃}�e���A�������蓖�Ă���C���f�b�N�X�� 
		char texFilePath[20];	 //  �e�N�X�`���t�@�C���p�X + ��
	};
#pragma pack()

	fread(&vertNum, sizeof(vertNum), 1, fp);

	vertices.resize(vertNum * pmdvertexsize); // �o�b�t�@�[�̊m��

	fread(vertices.data(), vertices.size(), 1, fp);

	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	indices.resize(indicesNum);

	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	fread(&materialNum, sizeof(materialNum), 1, fp);

	materials.resize(materialNum);
	textureResources.resize(materialNum);
	sphResources.resize(materialNum);
	spaResources.resize(materialNum);
	toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);

	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	for (size_t i = 0; i < pmdMaterials.size(); ++i)
	{
		materials[i].indicesNum = pmdMaterials[i].indicesNum;
		materials[i].material.diffuse = pmdMaterials[i].diffuse;
		materials[i].material.alpha = pmdMaterials[i].alpha;
		materials[i].material.specular = pmdMaterials[i].specular;
		materials[i].material.specularity = pmdMaterials[i].specularity;
		materials[i].material.ambient = pmdMaterials[i].ambient;
		materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		// �g�D�[�����\�[�X�̓ǂݍ���
		char toonFilePath[32];

		char toonFileName[16];

		sprintf(toonFilePath, "Content/Toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);

		toonResources[i] = _dx12->GetTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0)
		{
			textureResources[i] = nullptr;
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";

		if (count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);
			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph") {
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa") {
					spaFileName = namepair.second;
				}
			}
		}
		else {
			if (GetExtension(pmdMaterials[i].texFilePath) == "sph") {
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (GetExtension(pmdMaterials[i].texFilePath) == "spa") {
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else {
				texFileName = pmdMaterials[i].texFilePath;
			}
		}

		if (texFileName != "")
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			textureResources[i] = _dx12->GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			sphResources[i] = _dx12->GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			spaResources[i] = _dx12->GetTextureByPath(spaFilePath.c_str());
		}
	}

#pragma pack(1)
	// �ǂݍ��ݗp�{�[���\����
	struct PMDBone
	{
		char boneName[20];		 // �{�[����
		unsigned short parentNo; // �e�{�[���ԍ�
		unsigned short nextNo;	 // ��[�̃{�[���ԍ�
		unsigned char type;		 // �{�[�����
		unsigned short ikBoneNo; // IK �{�[���ԍ�
		XMFLOAT3 pos;			 // �{�[���̊���W�_
	};
#pragma pack()

	unsigned short boneNum = 0;

	fread(&boneNum, sizeof(boneNum), 1, fp);

	std::vector<PMDBone> pmdBones(boneNum);

	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	// �C���f�b�N�X�Ɩ��O�̑Ή��֌W�\�z�̂��߂Ɍ�Ŏg��
	std::vector<std::string> boneNames(pmdBones.size());

	// �{�[���m�[�h�}�b�v�����
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}

	// �e�q�֌W���\�z����
	for (auto& pb : pmdBones)
	{
		// �e�C���f�b�N�X�ԍ��̃`�F�b�N ( ���肦�Ȃ��ԍ��Ȃ��΂� )
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	_boneMatrices.resize(pmdBones.size());


	

	// �{�[�������ׂď�����
	std::fill(_boneMatrices.begin(), 
			  _boneMatrices.end(), 
		      XMMatrixIdentity());



	fclose(fp);

	CreateVertexBuffer();

	CreateIndexBuffer();

	CreateMaterialBuffer();

	return result;
}

HRESULT PMDActor::CreateVertexBuffer()
{
	auto result = _dx12->Device()->CreateCommittedResource(
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

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = vertices.size();
	vbView.StrideInBytes = pmdvertexsize;

	return result;
}

HRESULT PMDActor::CreateIndexBuffer()
{
	auto result = _dx12->Device()->CreateCommittedResource(
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

	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	return result;
}

HRESULT PMDActor::CreateMaterialBuffer()
{
	/* �}�e���A���o�b�t�@ --------------------------------------*/
	auto materialBuffSize = sizeof(MaterialForHlsl);

	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	auto result = _dx12->Device()->CreateCommittedResource(
				  &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				  D3D12_HEAP_FLAG_NONE,
				  &CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum),
				  D3D12_RESOURCE_STATE_GENERIC_READ,
				  nullptr,
				  IID_PPV_ARGS(materialBuff.ReleaseAndGetAddressOf()));

	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	char* mapMaterial = nullptr;

	result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	for (auto& m : materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material; // �f�[�^�R�s�[
		mapMaterial += materialBuffSize;
	}

	materialBuff->Unmap(0, nullptr);

	return S_OK;


	
}

HRESULT PMDActor::CreateTransformView()
{
	//GPU�o�b�t�@�쐬
	auto buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size());

	buffSize = (buffSize + 0xff) & ~0xff;
	
	auto result = _dx12->Device()->CreateCommittedResource(
				  &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				  D3D12_HEAP_FLAG_NONE,
				  &CD3DX12_RESOURCE_DESC::Buffer(buffSize),
				  D3D12_RESOURCE_STATE_GENERIC_READ,
				  nullptr,
				  IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	//�}�b�v�ƃR�s�[
	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	*_mappedMatrices = _transform.world;

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	//�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	
	transformDescHeapDesc.NumDescriptors = 1;//�Ƃ肠�������[���h�ЂƂ�
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//�f�X�N���v�^�q�[�v���
	
	result = _dx12->Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));//����
	
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;
	
	_dx12->Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};

	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = materialNum * 5;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = _dx12->Device()->CreateDescriptorHeap(
				  &matDescHeapDesc, IID_PPV_ARGS(&materialDescHeap));

	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};

	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize; // �}�e���A���� 256 �A���C�����g�T�C�Y

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// �擪���L�^
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(materialDescHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = _dx12->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < materials.size(); ++i)
	{
		//�}�e���A���Œ�o�b�t�@�r���[
		_dx12->Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += incSize;

		matCBVDesc.BufferLocation += materialBuffSize;

		if (textureResources[i] == nullptr) {
			srvDesc.Format = _renderer->whiteTex->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(_renderer->whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = textureResources[i]->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset(incSize);

		if (sphResources[i] == nullptr) {
			srvDesc.Format = _renderer->whiteTex->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(_renderer->whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = sphResources[i]->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (spaResources[i] == nullptr) {
			srvDesc.Format = _renderer->blackTex->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(_renderer->blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = spaResources[i]->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;


		if (toonResources[i] == nullptr) {
			srvDesc.Format = _renderer->gradTex->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(_renderer->gradTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = toonResources[i]->GetDesc().Format;
			_dx12->Device()->CreateShaderResourceView(toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
	/*------------------------------------------------------------*/
	return S_OK;
}

void PMDActor::RecursiveMatrixMultipy(BoneNode* node, XMMATRIX& mat)
{
	_boneMatrices[node->boneIdx] *= mat;

	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultipy(cnode, _boneMatrices[node->boneIdx]);
	}
}

void PMDActor::MotionUpdate()
{
	elapsedTime = timeGetTime() - startTime;

	unsigned int frameNo = 30 * (elapsedTime / 1000.f);

	//�s����N���A(���ĂȂ��ƑO�t���[���̃|�[�Y���d�ˊ|������ă��f��������)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//���[�V�����f�[�^�X�V
	for (auto& bonemotion : _motiondata) 
	{
		auto node = _boneNodeTable[bonemotion.first];
		
		//���v������̂�T��
		auto motions = bonemotion.second;

		auto rit = find_if(motions.rbegin(), motions.rend(), [frameNo](const KeyFrame& keyframe) 
		{ return keyframe.frameNo <= frameNo; });

		//���v������̂��Ȃ���Δ�΂�
		if (rit == motions.rend())
		{
			continue;
		}
		
		XMMATRIX rotation;
		
		auto it = rit.base();
		
		if (it != motions.end()) 
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) 
				   / static_cast<float>(it->frameNo - rit->frameNo);
			
			rotation = XMMatrixRotationQuaternion(rit->quaternion)
					 * (1 - t)
					 + XMMatrixRotationQuaternion(it->quaternion)
					 * t;
		}
		else 
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) 
			     * rotation 
			     * XMMatrixTranslation(pos.x, pos.y, pos.z);
		
		_boneMatrices[node.boneIdx] = mat;
	}
	
	RecursiveMatrixMultipy(&_boneNodeTable["�Z���^�["], _transform.world);

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

PMDActor::PMDActor(std::shared_ptr<PMDRenderer> renderer, const char* path):
	_renderer(renderer),
	_dx12(renderer->_dx12),
	angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(path);
	CreateTransformView();
	CreateMaterialBuffer();
	CreateMaterialAndTextureView();

//	auto armNode = _boneNodeTable["���r"];
//
//	auto& armPos = armNode.startPos;
//
//	auto armMat = XMMatrixTranslation(-armPos.x, -armPos.y, -armPos.z)   // 1 �{�[����_�����_�֖߂��悤�ɕ��s�ړ�
//				* XMMatrixRotationZ(XM_PIDIV2)  						 // 2 �X�O����]
//				* XMMatrixTranslation(armPos.x, armPos.y, armPos.z);	 // 3 �{�[�������̃{�[����_�֖߂��悤�ɕ��s�ړ�
//
//	auto elbowNode = _boneNodeTable["���Ђ�"];
//
//	auto& elbowPos = elbowNode.startPos;
//
//	auto elbowMat = XMMatrixTranslation(-elbowPos.x, -elbowPos.y, -elbowPos.z)   // 1 �{�[����_�����_�֖߂��悤�ɕ��s�ړ�
//				  * XMMatrixRotationZ(-XM_PIDIV2)  								 // 2 �X�O����]
//				  * XMMatrixTranslation(elbowPos.x, elbowPos.y, elbowPos.z);	 // 3 �{�[�������̃{�[����_�֖߂��悤�ɕ��s�ړ�
//
//	_boneMatrices[armNode.boneIdx] = armMat;
//	_boneMatrices[elbowNode.boneIdx] = elbowMat;
//
//	RecursiveMatrixMultipy(&_boneNodeTable["�Z���^�["], _transform.world);
//
//	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

PMDActor::~PMDActor()
{
}

void PMDActor::Update()
{
	MotionUpdate();
//	angle += 0.03f;
//	_mappedMatrices[0] = XMMatrixRotationY(angle);
}

void PMDActor::Draw()
{
	_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_dx12->CommandList()->IASetVertexBuffers(0, 1, &vbView);   // ���_�o�b�t�@
	_dx12->CommandList()->IASetIndexBuffer(&ibView);		   // �C���f�b�N�X�o�b�t�@

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
	_dx12->CommandList()->SetDescriptorHeaps(1, transheaps);
	_dx12->CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { materialDescHeap.Get() };
	_dx12->CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = _dx12->Device()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;

	for (auto& m : materials)
	{
		_dx12->CommandList()->SetGraphicsRootDescriptorTable(2, materialH);

		_dx12->CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);

		// �q�[�v�|�C���^�[�ƃC���f�b�N�X�����ɐi�߂�
		materialH.ptr += cbvsrvIncSize;

		idxOffset += m.indicesNum;
	}
}

void PMDActor::LoadVMDFile(const char* filepath, const char* name)
{
	auto fp = fopen(filepath, "rb");

	fseek(fp, 50, SEEK_SET);

	unsigned int motionDataNum = 0;

	fread(&motionDataNum, sizeof(motionDataNum), 1, fp);

	std::vector<VMDMotionData> vmdMotionData(motionDataNum);

	for (auto& motion : vmdMotionData)
	{
		fread(motion.boneName, sizeof(motion.boneName), 1, fp);
		fread(&motion.frameNo,
			sizeof(motion.frameNo)
			+ sizeof(motion.location)
			+ sizeof(motion.quaternion)
			+ sizeof(motion.bezier), 1, fp);
	}

	for (auto& vmdMotion : vmdMotionData)
	{
		XMVECTOR vector_quaternion = XMLoadFloat4(&vmdMotion.quaternion);
		_motiondata[vmdMotion.boneName].emplace_back(KeyFrame(vmdMotion.frameNo, vector_quaternion));
	}

	for (auto& bonemotion : _motiondata)
	{
		auto node = _boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
				 * XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)
				 * XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}

	RecursiveMatrixMultipy(&_boneNodeTable["�Z���^�["], _transform.world);
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::PlayAnimation()
{
	startTime = timeGetTime();
}
