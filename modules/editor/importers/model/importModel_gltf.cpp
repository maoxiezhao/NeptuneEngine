#include "importModel.h"
#include "modelTool.h"
#include "editor\editor.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_USE_RAPIDJSON
#include "loader\tiny_gltf.h"

namespace VulkanTest
{
namespace Editor
{	
	static const bool transformToLH = true;

	namespace
	{
		bool FileExists(const std::string& name, void*)
		{
			auto& fs = Engine::Instance->GetFileSystem();
			return fs.FileExists(name.c_str());
		}

		std::string ExpandFilePath(const std::string& filepath, void*)
		{
#ifdef _WIN32
			DWORD len = ExpandEnvironmentStringsA(filepath.c_str(), NULL, 0);
			char* str = new char[len];
			ExpandEnvironmentStringsA(filepath.c_str(), str, len);

			std::string s(str);
			delete[] str;
			return s;
#else
			ASSERT(false);
#endif
		}

		bool ReadWholeFile(std::vector<unsigned char>* out, std::string* err, const std::string& filepath, void*) 
		{
			auto& fs = Engine::Instance->GetFileSystem();
			OutputMemoryStream mem;
			if (!fs.LoadContext(filepath.c_str(), mem))
				return false;

			out->resize(mem.Size());
			memcpy(out->data(), mem.Data(), mem.Size());
			return true;
		}

		bool WriteWholeFile(std::string* err, const std::string& filepath, const std::vector<unsigned char>& contents, void*) 
		{
			ASSERT(false);
			return false;
		}
	}

	struct LoadState
	{
		tinygltf::Model gltfModel;
	};

	void LoadNode(int nodeIndex, ModelImporter::ImportNode& parent, LoadState& state)
	{
		if (nodeIndex < 0)
			return;

		ModelImporter::ImportNode& newNode = parent.children.emplace();
		auto& node = state.gltfModel.nodes[nodeIndex];
		newNode.name = node.name;

		if (node.mesh >= 0)
		{
			if (node.skin >= 0)
			{
				ASSERT(false);
			}
			else
			{
				// Mesh instance
				newNode.meshIndex = node.mesh;
			}
		}

		Transform& transform = newNode.transform;
		if (!node.scale.empty())
			transform.scale = F32x3((F32)node.scale[0], (F32)node.scale[1], (F32)node.scale[2]);
		if (!node.rotation.empty())
			transform.rotation = F32x4((F32)node.rotation[0], (F32)node.rotation[1], (F32)node.rotation[2], (F32)node.rotation[3]);
		if (!node.translation.empty())
			transform.translation = F32x3((F32)node.translation[0], (F32)node.translation[1], (F32)node.translation[2]);
		if (!node.matrix.empty())
		{
			transform.world._11 = (F32)node.matrix[0];
			transform.world._12 = (F32)node.matrix[1];
			transform.world._13 = (F32)node.matrix[2];
			transform.world._14 = (F32)node.matrix[3];
			transform.world._21 = (F32)node.matrix[4];
			transform.world._22 = (F32)node.matrix[5];
			transform.world._23 = (F32)node.matrix[6];
			transform.world._24 = (F32)node.matrix[7];
			transform.world._31 = (F32)node.matrix[8];
			transform.world._32 = (F32)node.matrix[9];
			transform.world._33 = (F32)node.matrix[10];
			transform.world._34 = (F32)node.matrix[11];
			transform.world._41 = (F32)node.matrix[12];
			transform.world._42 = (F32)node.matrix[13];
			transform.world._43 = (F32)node.matrix[14];
			transform.world._44 = (F32)node.matrix[15];
			transform.Apply();
		}
		transform.UpdateTransform();

		if (!node.children.empty())
		{
			for (int child : node.children)
				LoadNode(child, newNode, state);
		}
	}

	bool ImportModelDataGLTF(const char* path, ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg)
	{
		PROFILE_FUNCTION();

		FileSystem& fs = Engine::Instance->GetFileSystem();
		OutputMemoryStream mem;
		if (!fs.LoadContext(path, mem))
			return false;

		tinygltf::TinyGLTF loader;
		tinygltf::Model gltfModel;

		tinygltf::FsCallbacks callbacks;
		callbacks.ReadWholeFile = ReadWholeFile;
		callbacks.WriteWholeFile = WriteWholeFile;
		callbacks.FileExists = FileExists;
		callbacks.ExpandFilePath = ExpandFilePath;
		loader.SetFsCallbacks(callbacks);

		PathInfo pathInfo(path);		
		char ext[5] = {};
		CopyString(Span(ext), pathInfo.extension);
		MakeLowercase(Span(ext), ext);

		std::string errMsg;
		std::string warnMsg;
		bool ret = false;
		if (EqualString(ext, "gltf"))
		{
			ret = loader.LoadASCIIFromString(
				&gltfModel, 
				&errMsg, 
				&warnMsg,
				reinterpret_cast<const char*>(mem.Data()),
				static_cast<unsigned int>(mem.Size()), 
				pathInfo.dir);
		}
		else
		{
			ret = loader.LoadBinaryFromMemory(
				&gltfModel,
				&errMsg, 
				&warnMsg,
				mem.Data(),
				static_cast<unsigned int>(mem.Size()), 
				pathInfo.dir);
		}

		if (!ret)
		{
			Logger::Error("Failed to import gltf, error:%s", errMsg.c_str());
			return false;
		}

		// Gather materials
		auto& materials = modelData.materials;
		for (auto& x : gltfModel.materials)
		{
			ModelImporter::ImportMaterial& mat = materials.emplace();
			mat.name = x.name;
			mat.color = F32x4(1.0f);
			mat.metallic = 1.0f;
			mat.roughness = 1.0f;
			mat.doubleSided = x.doubleSided;

			// metallic-roughness workflow:
			auto baseColorFactor = x.values.find("baseColorFactor");
			if (baseColorFactor != x.values.end())
			{
				mat.color.x = F32(baseColorFactor->second.ColorFactor()[0]);
				mat.color.y = F32(baseColorFactor->second.ColorFactor()[1]);
				mat.color.z = F32(baseColorFactor->second.ColorFactor()[2]);
				mat.color.w = F32(baseColorFactor->second.ColorFactor()[3]);
			}

			// Roughness
			auto roughnessFactor = x.values.find("roughnessFactor");
			if (roughnessFactor != x.values.end())
			{
				mat.roughness = F32(roughnessFactor->second.Factor());
			}

			// Metallic
			auto metallicFactor = x.values.find("metallicFactor");
			if (metallicFactor != x.values.end())
			{
				mat.metallic = F32(metallicFactor->second.Factor());
			}

			// AlphaMode
			auto alphaMode = x.additionalValues.find("alphaMode");
			if (alphaMode != x.additionalValues.end())
			{
				if (alphaMode->second.string_value.compare("BLEND") == 0)
					mat.blendMode = BLENDMODE_ALPHA;
			
				if (alphaMode->second.string_value.compare("MASK") == 0)
					mat.alphaRef = 0.5f;
			}

			// AlphaCutoff
			auto alphaCutoff = x.additionalValues.find("alphaCutoff");
			if (alphaCutoff != x.additionalValues.end())
				mat.alphaRef = 1 - F32(alphaCutoff->second.Factor());
		
			// Gather texture
			auto GatherTexture = [&mat, &fs, &x, &modelData, &gltfModel](Texture::TextureType type) {

				String textureName;
				switch (type)
				{
				case Texture::DIFFUSE:
					textureName = "baseColorTexture";
					break;
				case Texture::NORMAL:
					textureName = "normalTexture";
					break;
				case Texture::SURFACEMAP:
					textureName = "metallicRoughnessTexture";
					break;
				default:
					break;
				}

				tinygltf::Texture* texture = nullptr;
				auto textureIt = x.values.find(textureName.c_str());
				if (textureIt != x.values.end())
					texture = &gltfModel.textures[textureIt->second.TextureIndex()];
				if (texture == nullptr)
					return;

				int imgSource = texture->source;
				if (texture->extensions.count("KHR_texture_basisu"))
					imgSource = texture->extensions["KHR_texture_basisu"].Get("source").Get<int>();

				auto texname = gltfModel.images[imgSource].uri;
				if (texname.empty())
					return;

				ModelImporter::ImportTexture& tex = mat.textures[(U32)type];
				tex.type = type;
				tex.path = texname;
				tex.isValid = fs.FileExists(tex.path.c_str());
				tex.import = true;

				modelData.textures.push_back(tex);
			};
			GatherTexture(Texture::DIFFUSE);
			GatherTexture(Texture::NORMAL);
			GatherTexture(Texture::SURFACEMAP);
		}

		// Gather meshes
		auto& meshes = modelData.meshes;
		auto& lods = modelData.lods;
		meshes.reserve((U32)gltfModel.meshes.size());
		for (auto& x : gltfModel.meshes)
		{
			auto& mesh = meshes.emplace();
			mesh.name = x.name;

			auto& aabb = mesh.aabb;
			for (auto& prim : x.primitives)
			{
				ASSERT(prim.indices >= 0);

				// Fill indices:
				const tinygltf::Accessor& accessor = gltfModel.accessors[prim.indices];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				size_t indexCount = accessor.count;
				size_t indexOffset = mesh.indices.size();
				mesh.indices.resize(indexOffset + indexCount);

				auto& subset = mesh.subsets.emplace();
				subset.uniqueIndexOffset = indexOffset;
				subset.uniqueIndexCount = indexCount;
				subset.materialIndex = prim.material;

				// Indices data
				U32 vertexOffset = mesh.vertexPositions.size();
				const U8* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
				int stride = accessor.ByteStride(bufferView);
				if (stride == 1)
				{
					for (size_t i = 0; i < indexCount; i += 3)
					{
						mesh.indices[indexOffset + i + 0] = vertexOffset + data[i + 0];
						mesh.indices[indexOffset + i + 1] = vertexOffset + data[i + 1];
						mesh.indices[indexOffset + i + 2] = vertexOffset + data[i + 2];
					}
				}
				else if (stride == 2)
				{
					for (size_t i = 0; i < indexCount; i += 3)
					{
						mesh.indices[indexOffset + i + 0] = vertexOffset + ((U16*)data)[i + 0];
						mesh.indices[indexOffset + i + 1] = vertexOffset + ((U16*)data)[i + 1];
						mesh.indices[indexOffset + i + 2] = vertexOffset + ((U16*)data)[i + 2];
					}
				}
				else if (stride == 4)
				{
					for (size_t i = 0; i < indexCount; i += 3)
					{
						mesh.indices[indexOffset + i + 0] = vertexOffset + ((U32*)data)[i + 0];
						mesh.indices[indexOffset + i + 1] = vertexOffset + ((U32*)data)[i + 1];
						mesh.indices[indexOffset + i + 2] = vertexOffset + ((U32*)data)[i + 2];
					}
				}
				else
				{
					ASSERT_MSG(false, "Unsupported index stride!");
				}

				// Vertex attributes
				for (auto& attr : prim.attributes)
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[attr.second];
					const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

					int stride = accessor.ByteStride(bufferView);
					size_t vertexCount = accessor.count;
					const U8* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

					String attrName = attr.first;
					if (attrName == "POSITION")
					{
						mesh.vertexPositions.resize(vertexOffset + vertexCount);
						for (size_t i = 0; i < vertexCount; ++i)
						{
							F32x3 pos = ((F32x3*)data)[i];
							mesh.vertexPositions[vertexOffset + i] = pos;
							aabb.min = Min(aabb.min, pos);
							aabb.max = Max(aabb.max, pos);
						}

						if (accessor.sparse.isSparse)
						{
							ASSERT_MSG(false, "Unsupported sparse vertex storage!");
							return false;
						}
					}
					else if (attrName == "NORMAL")
					{
						mesh.vertexNormals.resize(vertexOffset + vertexCount);
						for (size_t i = 0; i < vertexCount; ++i)
							mesh.vertexNormals[vertexOffset + i] = ((F32x3*)data)[i];

						if (accessor.sparse.isSparse)
						{
							ASSERT_MSG(false, "Unsupported sparse vertex storage!");
							return false;
						}
					}
					else if (attrName == "TANGENT")
					{
						mesh.vertexTangents.resize(vertexOffset + vertexCount);
						for (size_t i = 0; i < vertexCount; ++i)
							mesh.vertexTangents[vertexOffset + i] = ((F32x4*)data)[i];
					}
					else if (attrName == "TEXCOORD_0")
					{
						mesh.vertexUvset_0.resize(vertexOffset + vertexCount);
						if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
						{
							for (size_t i = 0; i < vertexCount; ++i)
								mesh.vertexUvset_0[vertexOffset + i] = ((F32x2*)data)[i];
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
						{
							for (size_t i = 0; i < vertexCount; ++i)
							{
								const uint8_t& s = *(uint8_t*)((size_t)data + i * stride + 0);
								const uint8_t& t = *(uint8_t*)((size_t)data + i * stride + 1);

								mesh.vertexUvset_0[vertexOffset + i].x = s / 255.0f;
								mesh.vertexUvset_0[vertexOffset + i].y = t / 255.0f;
							}
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
						{
							for (size_t i = 0; i < vertexCount; ++i)
							{
								const uint16_t& s = *(uint16_t*)((size_t)data + i * stride + 0 * sizeof(uint16_t));
								const uint16_t& t = *(uint16_t*)((size_t)data + i * stride + 1 * sizeof(uint16_t));

								mesh.vertexUvset_0[vertexOffset + i].x = s / 65535.0f;
								mesh.vertexUvset_0[vertexOffset + i].y = t / 65535.0f;
							}
						}
					}
				}
			}

			// Detect mesh lod
			mesh.lod = ModelTool::DetectLodIndex(mesh.name.c_str());

			if (lods.size() <= mesh.lod)
				lods.resize(mesh.lod + 1);
			lods[mesh.lod].meshes.push_back(&mesh);
		}

		// Load transform hierarchy
		modelData.root.name = pathInfo.basename;

		LoadState loadState = {};
		loadState.gltfModel = gltfModel;
		const tinygltf::Scene& gltfScene = gltfModel.scenes[std::max(0, gltfModel.defaultScene)];
		for (size_t i = 0; i < gltfScene.nodes.size(); i++)
		{
			LoadNode(gltfScene.nodes[i], modelData.root, loadState);
		}

		return true;
	}
}
}