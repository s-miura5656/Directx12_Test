#include "PMDActor.h"
#include "PMDRenderer.h"
#include "../Dx12Wrapper/Dx12Wrapper.h"
#include <d3dx12.h>
#include <sstream>
#include <array>

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


	// z 軸を特定の方向に向ける行列を返す
	// @ lookat 向かせたい方向のベクトル
	// @ up 上ベクトル
	// @ right 右ベクトル
	DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& lookat, DirectX::XMFLOAT3& up, DirectX::XMFLOAT3& right)
	{
		// 向かせたい方向
		DirectX::XMVECTOR vz = lookat;

		// (向かせたい方向を向かせたときの) 仮の y 軸ベクトル
		DirectX::XMVECTOR vy = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));

		// (向かせたい方向を向かせたときの) y 軸
//	XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vz, vx));
		DirectX::XMVECTOR vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));

		vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));

		// LookAt と Up が同じ方向を向いていたら right を基準にして作り直す
		if (std::abs(DirectX::XMVector3Dot(vy, vz).m128_f32[0] == 1.f))
		{
			// 仮の X 方向を定義
			vx = DirectX::XMVector3Normalize(XMLoadFloat3(&right));

			// (向かせたい方向を向かせたときの) y 軸
			vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));

			vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));
		}

		DirectX::XMMATRIX ret = DirectX::XMMatrixIdentity();

		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;

		return ret;
	}

	// z 軸を特定の方向に向ける行列を返す
	// @ origin 特定のベクトル
	// @ lookat 向かせたい方向のベクトル
	// @ up 上ベクトル
	// @ right 右ベクトル
	DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, DirectX::XMFLOAT3& up, DirectX::XMFLOAT3& right)
	{
		return DirectX::XMMatrixTranspose(LookAtMatrix(origin, up, right) * LookAtMatrix(lookat, up, right));
	}

	//ボーン種別
	enum class BoneType 
	{
		Rotation,      // 回転
		RotAndMove,    // 回転＆移動
		IK,            // IK
		Undefined,     // 未定義
		IKChild,       // IK影響ボーン
		RotationChild, // 回転影響ボーン
		IKDestination, // IK接続先
		Invisible      // 見えないボーン
	};
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
		DirectX::XMFLOAT3 pos;			  // 頂点座標	    : 12バイト
		DirectX::XMFLOAT3 normal;		  // 法線ベクトル	: 12バイト
		DirectX::XMFLOAT2 uv;			  // uv 座標		:  8バイト
		unsigned short boneNo[2]; // ボーン番号		:  4バイト
		unsigned char boneWeight; // ボーン影響度	:  1バイト
		unsigned char edgeFlg;	  // 輪郭線フラグ	:  1バイト
	};

#pragma pack(1) // ここから 1 バイトパッキングとなり、アライメントは発生しない
	// PMD マテリアル構造体
	struct PMDMaterial
	{
		DirectX::XMFLOAT3 diffuse;  // ディフューズ色
		float alpha;			    // ディフューズα
		float specularity;		    // スペキュラの強さ
		DirectX::XMFLOAT3 specular;	// スペキュラ色
		DirectX::XMFLOAT3 ambient;	// アンビエント色
		unsigned char toonIdx;	    // トゥーン番号
		unsigned char edgeFlg;	    // マテリアルごとの輪郭線フラグ

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

	unsigned short boneNum = 0;

	fread(&boneNum, sizeof(boneNum), 1, fp);

#pragma pack(1)
	// 読み込み用ボーン構造体
	struct PMDBone
	{
		char boneName[20];		 // ボーン名
		unsigned short parentNo; // 親ボーン番号
		unsigned short nextNo;	 // 先端のボーン番号
		unsigned char type;		 // ボーン種別
		unsigned short ikBoneNo; // IK ボーン番号
		DirectX::XMFLOAT3 pos;	 // ボーンの基準座標点
	};
#pragma pack()

	std::vector<PMDBone> pmdBones(boneNum);

	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	uint16_t ikNum = 0;

	fread(&ikNum, sizeof(ikNum), 1, fp);

	pmdIkData.resize(ikNum);

	for (auto& ik : pmdIkData)
	{
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);
		
		uint8_t chainLen = 0;
		
		fread(&chainLen, sizeof(chainLen), 1, fp);
		
		ik.nodeIdx.resize(chainLen);
		
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		
		fread(&ik.limit, sizeof(ik.limit), 1, fp);

		if (chainLen == 0)
		{
			continue;
		}

		fread(ik.nodeIdx.data(), sizeof(ik.nodeIdx[0]), chainLen, fp);
	}

	fclose(fp);

	// インデックスと名前の対応関係構築のために後で使う
	std::vector<std::string> boneNames(pmdBones.size());

	_boneNameArray.resize(pmdBones.size());
	boneNodeAddressArray.resize(pmdBones.size());

	// ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		_boneNameArray[idx] = pb.boneName;
		boneNodeAddressArray[idx] = &node;
		string boneName = pb.boneName;
		if (boneName.find("ひざ") != std::string::npos) {
			_kneeIdxes.emplace_back(idx);
		}
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
			  DirectX::XMMatrixIdentity());

	IkDebug(pmdIkData);


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
	auto buffSize = sizeof(DirectX::XMMATRIX) * (1 + _boneMatrices.size());

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

void PMDActor::RecursiveMatrixMultipy(PMDActor::BoneNode* node, DirectX::XMMATRIX& mat)
{
	_boneMatrices[node->boneIdx] *= mat;

	for (auto cnode : node->children)
	{
		RecursiveMatrixMultipy(cnode, _boneMatrices[node->boneIdx]);
	}
}

void PMDActor::MotionUpdate()
{
	elapsedTime = timeGetTime() - startTime;

	unsigned int frameNo = 30 * (elapsedTime / 1000.f);

	if (frameNo > duration)
	{
		startTime = timeGetTime();
		frameNo = 0;
	}

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), DirectX::XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : _motiondata) 
	{
		auto& boneName = bonemotion.first;
		auto itBoneNode = _boneNodeTable.find(boneName);
		if (itBoneNode == _boneNodeTable.end()) {
			continue;
		}

		auto node = itBoneNode->second;
//		auto node = _boneNodeTable[bonemotion.first];
		
		//合致するものを探す
		auto keyframes = bonemotion.second;

		auto rit = find_if(keyframes.rbegin(), keyframes.rend(),
			[frameNo](const KeyFrame& keyframe) { return keyframe.frameNo <= frameNo; });

		//合致するものがなければ飛ばす
		if (rit == keyframes.rend())
		{
			continue;
		}
		
		DirectX::XMMATRIX rotation = XMMatrixIdentity();
		DirectX::XMVECTOR offset = XMLoadFloat3(&rit->offset);

		auto it = rit.base();
		
		if (it != keyframes.end()) 
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) 
				   / static_cast<float>(it->frameNo - rit->frameNo);
			
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);

			rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionSlerp(rit->quaternion, it->quaternion, t));

			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else 
		{
			rotation = DirectX::XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			     * rotation 
			     * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		
		_boneMatrices[node.boneIdx] = mat * XMMatrixTranslationFromVector(offset);
	}
	
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], _transform.world);

	IKSolve(frameNo);

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
	//ターゲット
	auto targetBoneNode = boneNodeAddressArray[ik.boneIdx];
	auto targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	auto parentMat = _boneMatrices[boneNodeAddressArray[ik.boneIdx]->ikParentBone];
	DirectX::XMVECTOR det;
	auto invParentMat = DirectX::XMMatrixInverse(&det, parentMat);
	auto targetNextPos = DirectX::XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);


	//まずはIKの間にあるボーンの座標を入れておく(逆順注意)
	std::vector<DirectX::XMVECTOR> bonePositions;
	//末端ノード
	auto endPos = XMLoadFloat3(&boneNodeAddressArray[ik.targetIdx]->startPos);
	//中間ノード(ルートを含む)
	for (auto& cidx : ik.nodeIdx) 
	{
		bonePositions.push_back(XMLoadFloat3(&boneNodeAddressArray[cidx]->startPos));
	}

	std::vector<DirectX::XMMATRIX> mats(bonePositions.size());
	fill(mats.begin(), mats.end(), DirectX::XMMatrixIdentity());
	//ちょっとよくわからないが、PMDエディタの6.8°が0.03になっており、これは180で割っただけの値である。
	//つまりこれをラジアンとして使用するにはXM_PIを乗算しなければならない…と思われる。
	auto ikLimit = ik.limit * DirectX::XM_PI;
	//ikに設定されている試行回数だけ繰り返す
	for (int c = 0; c < ik.iterations; ++c) {
		//ターゲットと末端がほぼ一致したら抜ける
		if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
			break;
		}
		//それぞれのボーンを遡りながら角度制限に引っ掛からないように曲げていく
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx) {
			const auto& pos = bonePositions[bidx];

			//まず現在のノードから末端までと、現在のノードからターゲットまでのベクトルを作る
			auto vecToEnd = DirectX::XMVectorSubtract(endPos, pos);
			auto vecToTarget = DirectX::XMVectorSubtract(targetNextPos, pos);
			vecToEnd = DirectX::XMVector3Normalize(vecToEnd);
			vecToTarget = DirectX::XMVector3Normalize(vecToTarget);

			//ほぼ同じベクトルになってしまった場合は外積できないため次のボーンに引き渡す
			if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon) {
				continue;
			}
			//外積計算および角度計算
			auto cross = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vecToEnd, vecToTarget));
			float angle = DirectX::XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];
			angle = min(angle, ikLimit);//回転限界補正
			DirectX::XMMATRIX rot = DirectX::XMMatrixRotationAxis(cross, angle);//回転行列
			//posを中心に回転
			auto mat = DirectX::XMMatrixTranslationFromVector(-pos) *
					   rot *
					   DirectX::XMMatrixTranslationFromVector(pos);
			
			mats[bidx] *= mat;//回転行列を保持しておく(乗算で回転重ね掛けを作っておく)
			
							  //対象となる点をすべて回転させる(現在の点から見て末端側を回転)
			for (auto idx = bidx - 1; idx >= 0; --idx) {//自分を回転させる必要はない
				bonePositions[idx] = DirectX::XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = DirectX::XMVector3Transform(endPos, mat);
			//もし正解に近くなってたらループを抜ける
			if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
				break;
			}
		}
	}
	int idx = 0;
	for (auto& cidx : ik.nodeIdx) {
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	auto node = boneNodeAddressArray[ik.nodeIdx.back()];
	RecursiveMatrixMultipy(node, parentMat);
}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
	// IK 構成点を保存
	std::vector<DirectX::XMVECTOR> positions;
	// IK のそれぞれのボーン間の距離を保存
	std::array<float, 2> edgeLens;

	//ターゲット(末端ボーンではなく、末端ボーンが近づく目標ボーンの座標を取得)
	auto& targetNode = boneNodeAddressArray[ik.boneIdx];
	auto targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[ik.boneIdx]);

	//IKチェーンが逆順なので、逆に並ぶようにしている
	//末端ボーン
	auto endNode = boneNodeAddressArray[ik.targetIdx];
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));
	//中間及びルートボーン
	for (auto& chainBoneIdx : ik.nodeIdx) {
		auto boneNode = boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	//ちょっと分かりづらいと思ったので逆にしておきます。そうでもない人はそのまま
	//計算してもらって構わないです。
	reverse(positions.begin(), positions.end());

	//元の長さを測っておく
	edgeLens[0] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	//ルートボーン座標変換(逆順になっているため使用するインデックスに注意)
	positions[0] = DirectX::XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdx[1]]);
	//真ん中はどうせ自動計算されるので計算しない
	//先端ボーン
	positions[2] = DirectX::XMVector3Transform(positions[2], _boneMatrices[ik.boneIdx]);//ホンマはik.targetIdxだが…！？

	//ルートから先端へのベクトルを作っておく
	auto linearVec = DirectX::XMVectorSubtract(positions[2], positions[0]);
	float A = DirectX::XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = DirectX::XMVector3Normalize(linearVec);

	//ルートから真ん中への角度計算
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));

	//真ん中からターゲットへの角度計算
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	//「軸」を求める
	//もし真ん中が「ひざ」であった場合には強制的にX軸とする。
	DirectX::XMVECTOR axis;
	if (find(_kneeIdxes.begin(), _kneeIdxes.end(), ik.nodeIdx[0]) == _kneeIdxes.end()) {
		auto vm = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(positions[2], positions[0]));
		auto vt = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetPos, positions[0]));
		axis = DirectX::XMVector3Cross(vt, vm);
	}
	else {
		auto right = DirectX::XMFLOAT3(1, 0, 0);
		axis = DirectX::XMLoadFloat3(&right);
	}

	//注意点…IKチェーンは根っこに向かってから数えられるため1が根っこに近い
	auto mat1 = DirectX::XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= DirectX::XMMatrixRotationAxis(axis, theta1);
	mat1 *= DirectX::XMMatrixTranslationFromVector(positions[0]);


	auto mat2 = DirectX::XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= DirectX::XMMatrixRotationAxis(axis, theta2 - DirectX::XM_PI);
	mat2 *= DirectX::XMMatrixTranslationFromVector(positions[1]);

	_boneMatrices[ik.nodeIdx[1]] *= mat1;
	_boneMatrices[ik.nodeIdx[0]] = mat2 * _boneMatrices[ik.nodeIdx[1]];
	_boneMatrices[ik.targetIdx] = _boneMatrices[ik.nodeIdx[0]];//直前の影響を受ける
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
	// この関数に来た時点でノードは１つしかなく、
	// チェーンに入っているノード番号は IK ルートノードのものなので、
	// このルートノードから末端に向かうベクトルを考える
	auto rootNode = boneNodeAddressArray[ik.nodeIdx[0]];

	auto targetNode = boneNodeAddressArray[ik.boneIdx];

	auto rpos1 = DirectX::XMLoadFloat3(&rootNode->startPos);

	auto tpos1 = DirectX::XMLoadFloat3(&targetNode->startPos);

	auto rpos2 = DirectX::XMVector3TransformCoord(rpos1, _boneMatrices[ik.nodeIdx[0]]);

	auto tpos2 = DirectX::XMVector3TransformCoord(tpos1, _boneMatrices[ik.boneIdx]);

	auto originVec = DirectX::XMVectorSubtract(tpos1, rpos1);

	auto targetVec = DirectX::XMVectorSubtract(tpos2, rpos2);

	originVec = DirectX::XMVector3Normalize(originVec);

	targetVec = DirectX::XMVector3Normalize(targetVec);

	auto up = DirectX::XMFLOAT3(0, 1.f, 0);

	auto right = DirectX::XMFLOAT3(1.f, 0, 0);

	_boneMatrices[ik.nodeIdx[0]] = LookAtMatrix(originVec, targetVec, up, right);
}

void PMDActor::IKSolve(int frameNo)
{
	auto it = find_if(ikEnableData.rbegin(), ikEnableData.rend(), 
			  [frameNo](const VMDIKEnable& ikenable) 
			  {return ikenable.frameNo <= frameNo;  });

	for (auto& ik : pmdIkData)
	{
		if (it != ikEnableData.rend()) 
		{
			auto ikEnableIt = it->ikEnableTable.find(_boneNameArray[ik.boneIdx]);
			
			if (ikEnableIt != it->ikEnableTable.end()) 
			{
				if (!ikEnableIt->second) continue;
			}
		}
		auto childrenNodesCount = ik.nodeIdx.size();

		switch (childrenNodesCount)
		{
		case 0: // 間のボーン数が 0 (ありえない)
			assert(0);
			continue;
		case 1: // 間のボーン数が 1 の時は LookAt
			SolveLookAt(ik);
			break;
		case 2: // 間のボーン数が 2 の時は 余弦定理 IK
			SolveCosineIK(ik);
			break;
		default: // 間のボーン数が 3 以上の時は CCD-IK
			SolveCCDIK(ik);
			break;
		}
	}
}



PMDActor::PMDActor(std::shared_ptr<PMDRenderer> renderer, const char* path):
	_renderer(renderer),
	_dx12(renderer->_dx12),
	angle(0.0f)
{
	_transform.world = DirectX::XMMatrixIdentity();
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

	unsigned int keyframeNum = 0;

	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame 
	{
		char boneName[15];            // ボーン名
		unsigned int frameNo;         // フレーム番号(読込時は現在のフレーム位置を0とした相対位置)
		DirectX::XMFLOAT3 location;	  // 位置
		DirectX::XMFLOAT4 quaternion; // Quaternion // 回転
		unsigned char bezier[64];     // [4][4][4]  ベジェ補完パラメータ
	};

	std::vector<VMDKeyFrame> keyframes(keyframeNum);

	for (auto& keyframe : keyframes)
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo)
			+ sizeof(keyframe.location)
			+ sizeof(keyframe.quaternion)
			+ sizeof(keyframe.bezier), 1, fp);
	}

#pragma pack(1)
	// 表情データ
	struct VMDmorph
	{
		char name[15];
		uint32_t frameNo;
		float weight;
	};
#pragma pack()

	uint32_t morphCount = 0;

	fread(&morphCount, sizeof(morphCount), 1, fp);

	std::vector<VMDmorph> morphs(morphCount);

	fread(morphs.data(), sizeof(VMDmorph), morphCount, fp);

#pragma pack(1)
	// カメラ
	struct VMDCamera 
	{
		uint32_t frameNo;
		float distance;
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 eulerAngle;
		uint8_t Interpolation[24];
		uint32_t fov;
		uint8_t persFlg;
	};
#pragma pack()

	uint32_t vmdCameraCount = 0;

	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);

	// ライト照明データ
	struct VMDLight
	{
		uint32_t frameNo;
		DirectX::XMFLOAT3 rgb;
		DirectX::XMFLOAT3 vec;
	};

	uint32_t vmdLightCount = 0;

	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);

	std::vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);

#pragma pack(1)
	// セルフ影データ
	struct VMDSelfShadow
	{
		uint32_t frameNo;
		uint8_t mode;
		float distance;
	};
#pragma pack()

	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);

	std::vector<VMDSelfShadow> selfShadowData;
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp);

	uint32_t ikSwitchCount = 0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);

	ikEnableData.resize(ikSwitchCount);

	for (auto& ikEnable : ikEnableData) 
	{
		//キーフレーム情報なのでまずはフレーム番号読み込み
		fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp);
		
		//次に可視フラグがありますがこれは使用しないので1バイトシークでも構いません
		uint8_t visibleFlg = 0;
		
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);
		
		//対象ボーン数読み込み
		uint32_t ikBoneCount = 0;
		
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);
		
		//ループしつつ名前とON/OFF情報を取得
		for (int i = 0; i < ikBoneCount; ++i) 
		{
			char ikBoneName[20];
			
			fread(ikBoneName, _countof(ikBoneName), 1, fp);
			
			uint8_t flg = 0;
			
			fread(&flg, sizeof(flg), 1, fp);
			
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}

	fclose(fp);

	for (auto& f : keyframes)
	{
		DirectX::XMVECTOR vector_quaternion = XMLoadFloat4(&f.quaternion);
		auto unko2 = DirectX::XMFLOAT2((float)f.bezier[3] / 127.0f, (float)f.bezier[7] / 127.0f);
		auto unko3 = DirectX::XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f);

		_motiondata[f.boneName].emplace_back(KeyFrame(f.frameNo, 
													  vector_quaternion,
													  f.location,
													  unko2,
													  unko3));

		duration = std::max<unsigned int>(duration, f.frameNo);
	}

	for (auto& motion : _motiondata)
	{
		sort(motion.second.begin(), motion.second.end(), 
			[](const KeyFrame& lval, const KeyFrame& rval) 
			{ return lval.frameNo <= rval.frameNo; });
	}

	for (auto& bonemotion : _motiondata)
	{
		auto itBoneNode = _boneNodeTable.find(bonemotion.first);
		
		if (itBoneNode == _boneNodeTable.end())
		{
			continue;
		}
		
		auto& node = itBoneNode->second;

		auto& pos = node.startPos;
		
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
				 * DirectX::XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)
				 * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		
		_boneMatrices[node.boneIdx] = mat;
	}

	auto identity = XMMatrixIdentity();
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], identity);
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::PlayAnimation()
{
	startTime = timeGetTime();
}

float PMDActor::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
	{
		return x;
	}

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x; // t^3 の係数
	const float k1 = 3 * b.x - 6 * a.x;     // t^2 の係数
	const float k2 = 3 * a.x;				// t の係数

	

	// t を近似値で求める
	for (int i = 0; i < n; i++)
	{
		// f(t) を求める
		auto ft = (k0 * t * t * t) + (k1 * t * t) + (k2 * t) - x;

		// もし結果が 0 に近い ( 誤差の範囲内 ) なら打ち切る
		if (ft <= epsilon && ft >= -epsilon)
		{
			break;
		}

		t -= ft / 2; // 刻む
	}

	// 求めたい t はすでに求めているので y を計算する
	auto r = 1 - t;

	return (t * t * t) + (3 * t * t * r * b.y) + (3 * t * r * r * a.y);
}

void PMDActor::IkDebug(std::vector<PMDIK> pmd_Ik_Data)
{
	auto getNameFromIdx = [&](uint16_t idx)->string
	{
		auto it = find_if(_boneNodeTable.begin(), _boneNodeTable.end(),
			[idx](const pair<string, BoneNode>& obj) { return obj.second.boneIdx == idx; });

		if (it != _boneNodeTable.end())
		{
			return it->first;
		}
		else
		{
			return "";
		}
	};

	for (auto& ik : pmd_Ik_Data)
	{
		std::ostringstream oss;

		oss << "IKボーン番号=" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << endl;

		for (auto& node : ik.nodeIdx)
		{
			oss << "\tノードボーン=" << node << ":" << getNameFromIdx(node) << endl;
		}

		OutputDebugStringA(oss.str().c_str());
	}
}

