#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdint.h>
#include <glm/glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

constexpr bool SmoothNormals = true;

struct Material {
    glm::vec4 diffuse{1};
};

struct Vertex {
    glm::vec3 position{0};
    glm::vec3 normal{0};
    int32_t materialId{-1};
};

bool CreateDirectoryRecursive(std::string const & dirName, std::error_code & err) {
    err.clear();
    if (!std::filesystem::create_directories(dirName, err)) {
        if (std::filesystem::exists(dirName)) {
            err.clear();
            return true;    
        }
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> getFiles(const std::string& folderPath) {
    std::vector<std::filesystem::path> files;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
                std::cout << "Found file " << entry.path() << std::endl;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return {};
    }
    std::cout << "Found " << files.size() << " files" << std::endl;
    return files;
}

void embedVertices(const std::vector<Vertex>& vertices, const std::vector<Material>& materials, const std::filesystem::path& file) {
    std::error_code err;
    if (!CreateDirectoryRecursive(file.parent_path().string(), err)) {
        // Report the error:
        std::cout << "Failed to create directory for" << file << ": " << err.message() << std::endl;
    }
    std::ofstream out(file);
    if (!out.is_open()) {
        std::cerr << "Failed to open file " << file << std::endl;
        return;
    }

    out << "#pragma once\n\n";
    out << "#include <stdint.h>\n";
    out << "#include \"../rendering/Vertex.h\"\n\n";
    // materials
    out << "constexpr uint32_t " << file.stem().string() << "_materialCount = " << materials.size() << ";\n";
    out << "const Material " << file.stem().string() << "_materials[] = {";
    std::size_t counter = 0;
    for (const auto& material : materials) {
        if (counter++ % 50 == 0) out << "\n   ";
        out << std::format("{{{{{},{},{},{}}}}},",
            material.diffuse.r, material.diffuse.g, material.diffuse.b, material.diffuse.a
        );
    }
    out << "\n};\n";
    
    // vertices
    out << "constexpr uint32_t " << file.stem().string() << "_vertexCount = " << vertices.size() << ";\n";
    out << "const Vertex " << file.stem().string() << "_vertices[] = {";
    counter = 0;
    for (const auto& vertex : vertices) {
        if (counter++ % 50 == 0) out << "\n   ";
        out << std::format("{{{{{},{},{}}},{{{},{},{}}},{}}},",
            vertex.position.x, vertex.position.y, vertex.position.z,
            vertex.normal.x, vertex.normal.y, vertex.normal.z,
            vertex.materialId
        );
    }
    out << "\n};\n";
    out.close();
    std::cout << "  Embedded vertices in " << file << std::endl;
}

void dumpVertices(const std::vector<Vertex>& vertices, const std::vector<Material>& materials, const std::filesystem::path& file) {
    std::cout << "TODO: dump vertices" << std::endl;
}

std::pair<std::vector<Vertex>, std::vector<Material>> loadModel(const std::filesystem::path& file) {
    // parse file
    std::cout << "File: " << file << std::endl;
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    if (!reader.ParseFromFile(file.string(), config)) {
        if (!reader.Error().empty()) {
            std::cerr << "  Error: " << reader.Error() << std::endl;
        }
        return {};
    }
    if (!reader.Warning().empty()) {
        std::cerr << "  Warning: " << reader.Warning() << std::endl;
    }

    const auto& attrib = reader.GetAttrib();
    std::cout << "  Vertex count: " << attrib.vertices.size() << std::endl;
    std::cout << "  Normal count: " << attrib.normals.size() << std::endl;
    std::cout << "  Material count: " << reader.GetMaterials().size() << std::endl;

    std::cout << "  Loading vertices ..." << std::endl;
    std::vector<Vertex> vertices;
    for (const auto& shape : reader.GetShapes()) {
        auto& mesh = shape.mesh;
        std::size_t faceId = 0;
        for (const auto& index : mesh.indices) {
            Vertex vertex;
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (index.normal_index != -1) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            int32_t materialId = faceId++ / 3;
            if (materialId < mesh.material_ids.size()) vertex.materialId = mesh.material_ids[materialId];
            vertices.push_back(vertex);
        }
    }
    if (attrib.normals.empty()) {
        std::cout << "  No normals found, creating from vertices ..." << std::endl;

        for (const auto& shape : reader.GetShapes()) {
            auto& mesh = shape.mesh;
            std::vector<glm::vec3> normals(mesh.indices.size());
            for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
                glm::vec3 pos1 {
                    attrib.vertices[3 * mesh.indices[i + 0].vertex_index + 0],
                    attrib.vertices[3 * mesh.indices[i + 0].vertex_index + 1],
                    attrib.vertices[3 * mesh.indices[i + 0].vertex_index + 2]
                };
                glm::vec3 pos2 {
                    attrib.vertices[3 * mesh.indices[i + 1].vertex_index + 0],
                    attrib.vertices[3 * mesh.indices[i + 1].vertex_index + 1],
                    attrib.vertices[3 * mesh.indices[i + 1].vertex_index + 2]
                };
                glm::vec3 pos3 {
                    attrib.vertices[3 * mesh.indices[i + 2].vertex_index + 0],
                    attrib.vertices[3 * mesh.indices[i + 2].vertex_index + 1],
                    attrib.vertices[3 * mesh.indices[i + 2].vertex_index + 2]
                };
                glm::vec3 normal = glm::normalize(glm::cross(pos2 - pos1, pos3 - pos1));
                for (uint32_t j = 0; j < 3; j++) {
                    if (SmoothNormals) normals[mesh.indices[i + j].vertex_index] += normal;
                    else normals[i + j] = normal;
                }
            }
            for (uint32_t i = 0; i < mesh.indices.size(); i++) {
                if (SmoothNormals) vertices[i].normal = glm::normalize(normals[mesh.indices[i].vertex_index]);
                else vertices[i].normal = normals[i];
            }
        }
    }
    std::cout << "  Loading materials ..." << std::endl;
    std::vector<Material> materials;
    for (const auto& material : reader.GetMaterials()) {
        materials.push_back({
            .diffuse = glm::vec4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1)
        });
    }

    std::cout << "  Loaded " << vertices.size() << " vertices" << std::endl;
    return {std::move(vertices), std::move(materials)};
}

int main() {
    std::string inFolder = "models";
    std::string outFolder = "src/meshes";

    auto files = getFiles(inFolder);
    std::cout << std::endl;
    for (const auto& file : files) {
        auto&& [vertices, materials] = loadModel(file);
        if (vertices.empty()) {
            std::cerr << "Failed to load model " << file << std::endl;
            continue;
        }
        auto outPath = std::filesystem::path(outFolder) / file.filename();
        outPath.replace_extension(".h");
        embedVertices(vertices, materials, outPath);
    }
    return 0;
}