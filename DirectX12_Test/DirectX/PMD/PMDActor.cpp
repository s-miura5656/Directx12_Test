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
// モデルのパスとテクスチャのパスから合成パスを得る
// @param modelPath アプリケーションから見た pmd モデルのパス
// @param texPath PMD モデルから見たテクスチャのパス
// @return アプリケーションから見たテクスチャのパス
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

// string ( マルチバイト文字列 ) から wstring ( ワイド文字列 )　を得る
// @param str マルチバイト文字列
// @return 変換されたワイド文字列
	wstring GetWideStringFromString(const string& str)
	{
		// 呼び出し一回目 ( 文字列数を得る )
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

// ファイル名から拡張子を取得する
// @param path 対象のパス文字列
// @param 拡張子
	string GetExtension(const string& path)
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
}

void* PMDActor::Transform::operator new(size_t size) 
{
	return _aligned_malloc(size, 16);
}

// @param モデルパス
HRESULT PMDActor::LoadPMDFile(const char* path)
{
	HRESULT result = S_OK;

	// PMDヘッダー構造体
	struct PMDheader
	{
		float version;		 // 例 : 00 00 80 3F = 1.00
		char model_name[20]; // モデル名
		char comment[256];	 // モデルコメント
	};

	char signature[3] = {}; // シグネチャ
	PMDheader pmdheader = {};
	std::string strModelPath = path;

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

	fread(&vertNum, sizeof(vertNum), 1, fp);

	vertices.resize(vertNum * pmdvertexsize); // バッファーの確保

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
		// トゥーンリソースの読み込み
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
	// 読み込み用ボーン構造体
	struct PMDBone
	{
		char boneName[20];		 // ボーン名
		unsigned short parentNo; // 親ボーン番号
		unsigned short nextNo;	 // 先端のボーン番号
		unsigned char type;		 // ボーン種別
		unsigned short ikBoneNo; // IK ボーン番号
		XMFLOAT3 pos;			 // ボーンの基準座標点
	};
#pragma pack()

	unsigned short boneNum = 0;

	fread(&boneNum, sizeof(boneNum), 1, fp);

	std::vector<PMDBone> pmdBones(boneNum);

	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	// インデックスと名前の対応関係構築のために後で使う
	std::vector<std::string> boneNames(pmdBones.size());

	// ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}

	// 親子関係を構築する
	for (auto& pb : pmdBones)
	{
		// 親インデックス番号のチェック ( ありえない番号なら飛ばす )
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	_boneMatrices.resize(pmdBones.size());


	

	// ボーンをすべて初期化
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
	/* マテリアルバッファ --------------------------------------*/
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
		*((MaterialForHlsl*)mapMaterial) = m.material; // データコピー
		mapMaterial += materialBuffSize;
	}

	materialBuff->Unmap(0, nullptr);

	return S_OK;


	
}

HRESULT PMDActor::CreateTransformView()
{
	//GPUバッファ作成
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

	//マップとコピー
	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	*_mappedMatrices = _transform.world;

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	//ビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	
	transformDescHeapDesc.NumDescriptors = 1;//とりあえずワールドひとつ
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別
	
	result = _dx12->Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));//生成
	
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
	matCBVDesc.SizeInBytes = materialBuffSize; // マテリアルの 256 アライメントサイズ

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// 先頭を記録
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(materialDescHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = _dx12->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < materials.size(); ++i)
	{
		//マテリアル固定バッファビュー
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

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : _motiondata) 
	{
		auto node = _boneNodeTable[bonemotion.first];
		
		//合致するものを探す
		auto motions = bonemotion.second;

		auto rit = find_if(motions.rbegin(), motions.rend(), [frameNo](const KeyFrame& keyframe) 
		{ return keyframe.frameNo <= frameNo; });

		//合致するものがなければ飛ばす
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
	
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], _transform.world);

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

//	auto armNode = _boneNodeTable["左腕"];
//
//	auto& armPos = armNode.startPos;
//
//	auto armMat = XMMatrixTranslation(-armPos.x, -armPos.y, -armPos.z)   // 1 ボーン基準点を原点へ戻すように平行移動
//				* XMMatrixRotationZ(XM_PIDIV2)  						 // 2 ９０°回転
//				* XMMatrixTranslation(armPos.x, armPos.y, armPos.z);	 // 3 ボーンを元のボーン基準点へ戻すように平行移動
//
//	auto elbowNode = _boneNodeTable["左ひじ"];
//
//	auto& elbowPos = elbowNode.startPos;
//
//	auto elbowMat = XMMatrixTranslation(-elbowPos.x, -elbowPos.y, -elbowPos.z)   // 1 ボーン基準点を原点へ戻すように平行移動
//				  * XMMatrixRotationZ(-XM_PIDIV2)  								 // 2 ９０°回転
//				  * XMMatrixTranslation(elbowPos.x, elbowPos.y, elbowPos.z);	 // 3 ボーンを元のボーン基準点へ戻すように平行移動
//
//	_boneMatrices[armNode.boneIdx] = armMat;
//	_boneMatrices[elbowNode.boneIdx] = elbowMat;
//
//	RecursiveMatrixMultipy(&_boneNodeTable["センター"], _transform.world);
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
	_dx12->CommandList()->IASetVertexBuffers(0, 1, &vbView);   // 頂点バッファ
	_dx12->CommandList()->IASetIndexBuffer(&ibView);		   // インデックスバッファ

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

		// ヒープポインターとインデックスを次に進める
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

	RecursiveMatrixMultipy(&_boneNodeTable["センター"], _transform.world);
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::PlayAnimation()
{
	startTime = timeGetTime();
}
