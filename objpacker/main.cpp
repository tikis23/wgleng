#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <stdint.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

constexpr bool SmoothNormals = false;

struct Material {
    glm::vec4 diffuse{1};
};

struct Vertex {
    glm::vec3 position{0};
    glm::vec3 normal{0};
    int32_t materialId{-1};

	bool operator==(const Vertex& other) const {
		float epsilon = 0.0001f;
		return 
            glm::abs(position.x - other.position.x) < epsilon &&
            glm::abs(normal.x - other.normal.x) < epsilon &&
			materialId == other.materialId;
	}
};
namespace std {
    template<>
    struct hash<Vertex> {
        std::size_t operator()(const Vertex& v) const {
            std::size_t seed = 0;
            seed ^= std::hash<glm::vec3>{}(v.position) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<glm::vec3>{}(v.normal) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<int32_t>{}(v.materialId) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

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

void embedVertices(const std::vector<Vertex>& vertices, const std::vector<Material>& materials,
                    const std::vector<uint32_t>& indices, const std::filesystem::path& file) {
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
    out << "#include <wgleng/rendering/Vertex.h>\n\n";
    // materials
    out << "constexpr uint32_t " << file.stem().string() << "_materialCount = " << materials.size() << ";\n";
    out << "const Material " << file.stem().string() << "_materials[] = {";
    std::size_t counter = 0;
    for (const auto& material : materials) {
        if (counter++ % 150 == 0) out << "\n   ";
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
        if (counter++ % 150 == 0) out << "\n   ";
        out << std::format("{{{{{},{},{}}},{{{},{},{}}},{}}},",
            vertex.position.x, vertex.position.y, vertex.position.z,
            vertex.normal.x, vertex.normal.y, vertex.normal.z,
            vertex.materialId
        );
    }
    out << "\n};\n";

    // indices
    out << "constexpr uint32_t " << file.stem().string() << "_indexCount = " << indices.size() << ";\n";
    out << "const uint32_t " << file.stem().string() << "_indices[] = {";
    counter = 0;
    for (const auto& index : indices) {
        if (counter++ % 1000 == 0) out << "\n   ";
		out << std::format("{},", index);
    }
    out << "\n};\n";

    out.close();
    std::cout << "  Embedded vertices in " << file << std::endl;
}

void dumpVertices(const std::vector<Vertex>& vertices, const std::vector<Material>& materials,
        const std::vector<uint32_t>& indices, const std::filesystem::path& file) {
    std::cout << "TODO: dump vertices" << std::endl;
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> generateIndices(const std::vector<Vertex>& vertices) {
    std::vector<uint32_t> indices;
    indices.reserve(vertices.size());
    std::vector<Vertex> newVertices;
    newVertices.reserve(vertices.size());
	std::unordered_map<Vertex, uint32_t> vertexMap;
	for (uint32_t i = 0; i < vertices.size(); i++) {
		auto it = vertexMap.find(vertices[i]);
		if (it == vertexMap.end()) {
			vertexMap[vertices[i]] = newVertices.size();
			indices.push_back(newVertices.size());
			newVertices.push_back(vertices[i]);
		}
		else {
			indices.push_back(it->second);
		}
	}
	return { std::move(newVertices), std::move(indices) };
}

void generateFlatNormals(std::vector<Vertex>& vertices) {
    glm::vec3 prevNormal{0, 1, 0};
    for (uint32_t i = 0; i < vertices.size(); i += 3) {
		const glm::vec3& pos1 = vertices[i + 0].position;
		const glm::vec3& pos2 = vertices[i + 1].position;
		const glm::vec3& pos3 = vertices[i + 2].position;
        glm::vec3 normal = glm::normalize(glm::cross(pos2 - pos1, pos3 - pos1));
        if (glm::any(glm::isnan(normal))) normal = prevNormal;
		prevNormal = normal;
        for (uint32_t j = 0; j < 3; j++) vertices[i + j].normal = normal;
    }
}
void generateSmoothNormals(std::vector<Vertex>& vertices) {
	for (auto& vertex : vertices) vertex.normal = glm::vec3(0);
	auto [newVertices, indices] = generateIndices(vertices);
    glm::vec3 prevNormal{0, 1, 0};
    for (uint32_t i = 0; i < indices.size(); i += 3) {
		const glm::vec3& pos1 = newVertices[indices[i + 0]].position;
		const glm::vec3& pos2 = newVertices[indices[i + 1]].position;
		const glm::vec3& pos3 = newVertices[indices[i + 2]].position;
        glm::vec3 normal = glm::normalize(glm::cross(pos2 - pos1, pos3 - pos1));
        if (glm::any(glm::isnan(normal))) normal = prevNormal;
		prevNormal = normal;
        for (uint32_t j = 0; j < 3; j++) newVertices[indices[i + j]].normal += normal;
    }
    vertices.clear();
	for (auto i : indices) {
		newVertices[i].normal = glm::normalize(newVertices[i].normal);
		vertices.push_back(newVertices[i]);
    }
}

std::tuple<std::vector<Vertex>, std::vector<Material>, std::vector<uint32_t>> loadModel(const std::filesystem::path& file) {
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
    std::cout << "  Shape count: " << reader.GetShapes().size() << std::endl;
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
            //if (index.normal_index != -1) {
            //    vertex.normal = {
            //        attrib.normals[3 * index.normal_index + 0],
            //        attrib.normals[3 * index.normal_index + 1],
            //        attrib.normals[3 * index.normal_index + 2]
            //    };
            //}
            int32_t materialId = faceId++ / 3;
            if (materialId < mesh.material_ids.size()) vertex.materialId = mesh.material_ids[materialId];
            vertices.push_back(vertex);
        }
    }

    std::cout << "  Creating normals ..." << std::endl;
	if (SmoothNormals) generateSmoothNormals(vertices);
	else generateFlatNormals(vertices);

    std::cout << "  Creating indices ..." << std::endl;
	auto [newVertices, indices] = generateIndices(vertices);
	vertices = std::move(newVertices);

    std::cout << "  Loading materials ..." << std::endl;
    std::vector<Material> materials;
    for (const auto& material : reader.GetMaterials()) {
        materials.push_back({
            .diffuse = glm::vec4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1)
        });
    }

    std::cout << "  Loaded " << vertices.size() << " vertices, " << indices.size() << " indices." << std::endl;
    return {std::move(vertices), std::move(materials), std::move(indices)};
}

int main() {
    std::string inFolder = "models";
    std::string outFolder = "src/meshes";

    auto files = getFiles(inFolder);
    std::cout << std::endl;
    for (const auto& file : files) {
		if (file.extension() != ".obj") continue;

        auto&& [vertices, materials, indices] = loadModel(file);
        if (vertices.empty()) {
            std::cerr << "Failed to load model " << file << std::endl;
            continue;
        }
        auto outPath = std::filesystem::path(outFolder) / file.filename();
        outPath.replace_extension(".h");
        embedVertices(vertices, materials, indices, outPath);
    }
    return 0;
}
