#pragma once

#include <string_view>
#include <GLES3/gl3.h>
#include <unordered_map>
#include <string>

class ShaderProgram {
public:
    ShaderProgram(std::string_view shaderName);
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;

    void SetConstant(std::string_view name, std::string_view value);
    void Load(const GLchar* vertexSource, const GLchar* fragmentSource);

    void Use() const;
    GLuint GetId() const { return m_program; }

    void AddUniformBufferBinding(std::string_view name, GLuint bindingIndex) const;

    void SetTexture(std::string_view name, GLenum target, GLuint index, GLuint texture);

private:
    bool CheckCompileErrors(const unsigned int shader, const int type) const;
    void CreateShader(GLuint program, const GLchar *source, GLenum type);
    GLuint m_program{0};
    std::string m_shaderName;
    std::unordered_map<std::string, GLuint> m_uniformLocations;
    std::unordered_map<std::string, std::string> m_shaderConstans;
};