#include "Mesh.h"

#include <vector>

MeshImpl::MeshImpl(std::string_view name)
	: m_name(name) {
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
}
MeshImpl::~MeshImpl() {
	if (m_vao == 0) return;
	glDeleteBuffers(1, &m_ebo);
	glDeleteBuffers(1, &m_vbo);
	glDeleteVertexArrays(1, &m_vao);
}

void MeshImpl::Load(std::span<const Vertex> vertices, std::span<const Material> materials,
	std::span<const uint32_t> indices, bool createAabb, bool wireframe) {
	// materials
	std::vector<uint32_t> mappedMaterials(materials.size());
	for (std::size_t i = 0; i < mappedMaterials.size(); i++) {
		const auto& material = materials[i];
		// if material already exists, use that id
		const auto& existingMaterials = MeshRegistry::GetMaterials();
		bool found = false;
		for (std::size_t j = 1; j < existingMaterials.size(); j++) {
			if (glm::length(existingMaterials[j].diffuse - material.diffuse) < 0.004f) {
				mappedMaterials[i] = j;
				found = true;
				break;
			}
		}
		if (!found) {
			MeshRegistry::CreateMaterial(material);
			mappedMaterials[i] = existingMaterials.size() - 1;
		}
	}

	// vertices
	std::vector<Vertex> verts(vertices.size());
	for (std::size_t i = 0; i < verts.size(); i++) {
		verts[i] = vertices[i];
		if (vertices[i].materialId < 0) {
			verts[i].materialId = 0;
		}
		else {
			verts[i].materialId = mappedMaterials[vertices[i].materialId];
		}
	}

	// aabb
	if (createAabb) {
		m_aabbMin = glm::vec3(FLT_MAX);
		m_aabbMax = glm::vec3(-FLT_MAX);
		for (const auto& vert : verts) {
			m_aabbMin = glm::min(m_aabbMin, vert.position);
			m_aabbMax = glm::max(m_aabbMax, vert.position);
		}
	}

	// wireframe
	std::vector<uint32_t> wireframeIndices;
	if (wireframe) {
		wireframeIndices.reserve(indices.size() * 2);
		for (std::size_t i = 0; i < indices.size(); i += 3) {
			wireframeIndices.push_back(indices[i]);
			wireframeIndices.push_back(indices[i + 1]);
			wireframeIndices.push_back(indices[i + 1]);
			wireframeIndices.push_back(indices[i + 2]);
			wireframeIndices.push_back(indices[i + 2]);
			wireframeIndices.push_back(indices[i]);
		}
		indices = wireframeIndices;
	}

	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex), (void*)offsetof(Vertex, materialId));

	m_drawCount = indices.size();
}
void MeshImpl::Unload() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
	m_drawCount = 0;
}

Mesh MeshRegistry::Create(std::string_view name) {
	const auto it = m_meshes.emplace(std::string(name), name);
	return &(it.first->second);
}
Mesh MeshRegistry::Get(std::string_view name) {
	const auto it = m_meshes.find(std::string(name));
	if (it != m_meshes.end()) return &(it->second);
	return nullptr;
}
void MeshRegistry::Destroy(std::string_view name) {
	m_meshes.erase(std::string(name));
}
void MeshRegistry::CreateMaterial(const Material& material) {
	m_materials.push_back(material);
}
const std::vector<Material>& MeshRegistry::GetMaterials() {
	return m_materials;
}
void MeshRegistry::Clear() {
	m_meshes.clear();
	m_materials.clear();
}
