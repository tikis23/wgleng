#pragma once

#include <GLES3/gl3.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Vertex.h"

class MeshImpl;
using Mesh = MeshImpl*;

class MeshImpl {
public:
	MeshImpl(std::string_view name);
	~MeshImpl();
	MeshImpl(const MeshImpl&) = delete;
	MeshImpl& operator=(const MeshImpl&) = delete;

	void Load(std::size_t vertexCount, const Vertex* vertices, std::size_t materialCount,
		const Material* materials, bool createAabb = true, const glm::mat4& model = glm::mat4(1));

	// returns {aabb_min, aabb_max} if createAabb was true, otherwise {0, 0}
	std::pair<glm::vec3, glm::vec3> GetAabb() const { return { m_aabbMin, m_aabbMax }; }

	void Unload();
	const std::string& GetName() const { return m_name; }
	GLuint GetVAO() const { return m_vao; }
	std::size_t GetDrawCount() const { return m_drawCount; }

private:
	std::string m_name;
	GLuint m_vao;
	GLuint m_vbo;
	std::size_t m_drawCount;
	glm::vec3 m_aabbMin{0};
	glm::vec3 m_aabbMax{0};
};

class MeshRegistry {
public:
	MeshRegistry() = delete;
	~MeshRegistry() = delete;

	static Mesh Create(std::string_view name);
	static Mesh Get(std::string_view name);
	static void Destroy(std::string_view name);

	static void CreateMaterial(const Material& material);
	static const std::vector<Material>& GetMaterials();
	// TODO: delete materials. Requires changing all meshes that use the material

	static void Clear();

private:
	static inline std::unordered_map<std::string, MeshImpl> m_meshes;
	static inline std::vector<Material> m_materials = {
		{{1, 0, 1, 1}} // default material
	};
};
