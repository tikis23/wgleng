#include "Mesh.h"

#include <vector>

MeshImpl::MeshImpl(std::string_view name)
    : m_name(name) {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
}
MeshImpl::~MeshImpl() {
    if (m_vao == 0) return;
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void MeshImpl::Load(std::size_t vertexCount, const Vertex* vertices, std::size_t materialCount, const Material* materials, const glm::mat4& model) {
    // materials
    std::vector<uint32_t> mappedMaterials(materialCount);
    for (std::size_t i = 0; i < materialCount; i++) {
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
            mappedMaterials[i] =  existingMaterials.size() - 1;
        }
    }

    // vertices
    std::vector<Vertex> verts(vertexCount);
    for (std::size_t i = 0; i < vertexCount; i++) {
        verts[i] = vertices[i];
        if (model != glm::mat4(1)) {
            verts[i].position = model * glm::vec4(vertices[i].position, 1);
            verts[i].normal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(model))) * vertices[i].normal);
        }
        if (vertices[i].materialId < 0) {
            verts[i].materialId = 0;
        } else {
            verts[i].materialId = mappedMaterials[vertices[i].materialId];
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), verts.data(), GL_STATIC_DRAW);
    glBindVertexArray(m_vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex), (void*)offsetof(Vertex, materialId));

    m_drawCount = vertexCount;
}
void MeshImpl::Unload() {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    m_drawCount = 0;
}

Mesh MeshRegistry::Create(std::string_view name) {
    auto it = m_meshes.emplace(std::string(name), name);
    return &(it.first->second);
}
Mesh MeshRegistry::Get(std::string_view name) {
    auto it = m_meshes.find(std::string(name));
    if (it != m_meshes.end()) return &(it->second);
    return nullptr;
}
void MeshRegistry::Destroy(std::string_view name) {
    m_meshes.erase(std::string(name));
}
void MeshRegistry::CreateMaterial(const Material &material) {
    m_materials.push_back(material);
}
const std::vector<Material>& MeshRegistry::GetMaterials() {
    return m_materials;
}
void MeshRegistry::Clear() {
    m_meshes.clear();
    m_materials.clear();
}
