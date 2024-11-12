#include "ShaderProgram.h"

#include <stdio.h>

void ShaderProgram::CheckCompileErrors(const unsigned int shader, const int type) {
    int success;
    char infoLog[1024];
    if (type == 1) {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            printf("ERROR: SHADER_COMPILATION_ERROR in '%s':\n%s\n", m_shaderName.c_str(), infoLog);
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            printf("ERROR: PROGRAM_LINKING_ERROR in '%s':\n%s\n", m_shaderName.c_str(), infoLog);
        }
    }
}

void ShaderProgram::CreateShader(GLuint program, const GLchar *source, GLenum type) {
    std::string src(source);
    for (const auto& [name, value] : m_shaderConstans) {
        size_t pos = 0;
        while ((pos = src.find(name, pos)) != std::string::npos) {
            src.replace(pos, name.length(), value);
            pos += value.length();
        }
    }

    source = src.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    CheckCompileErrors(shader, 1);
    glAttachShader(program, shader);
    glDeleteShader(shader);
}

ShaderProgram::ShaderProgram(std::string_view shaderName) 
    : m_shaderName{shaderName} {}

ShaderProgram::~ShaderProgram() {
    if (m_program) glDeleteProgram(m_program);
}

void ShaderProgram::SetConstant(std::string_view name, std::string_view value) {
    m_shaderConstans["<<" + std::string(name) + ">>"] = std::string(value);
}

void ShaderProgram::Load(const GLchar *vertexSource, const GLchar *fragmentSource) {
    if (m_program) glDeleteProgram(m_program);
    m_program = glCreateProgram();

    if (vertexSource) CreateShader(m_program, vertexSource, GL_VERTEX_SHADER);
    if (fragmentSource) CreateShader(m_program, fragmentSource, GL_FRAGMENT_SHADER);
    glLinkProgram(m_program);
    CheckCompileErrors(m_program, 0);
}

void ShaderProgram::Use() {
    glUseProgram(m_program);
}

void ShaderProgram::AddUniformBufferBinding(std::string_view name, GLuint bindingIndex) {
    GLuint blockIndex = glGetUniformBlockIndex(m_program, name.data());
    if (blockIndex == GL_INVALID_INDEX) {
        printf("ERROR: SHADER_UNIFORM_BLOCK_NOT_FOUND in '%s':\n%s\n", m_shaderName.c_str(), name.data());
        return;
    }
    glUniformBlockBinding(m_program, blockIndex, bindingIndex);   
}

void ShaderProgram::SetTexture(std::string_view name, GLenum target, GLuint index, GLuint texture) {
    GLuint location;
    auto it = m_uniformLocations.find(name.data());
    if (it != m_uniformLocations.end()) {
        location = it->second;
    } else {
        location = glGetUniformLocation(m_program, name.data());
        m_uniformLocations[name.data()] = location;
    }

    glUniform1i(location, index);
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(target, texture);
}
