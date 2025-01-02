#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"


class ComputeShader
{
public:
	unsigned ID;
	ComputeShader(const char* computePath);
	~ComputeShader();
	void use();
	void setBool(const std::string& name, bool value) const;
	void setInt(const std::string& name, int value) const;
	void setFloat(const std::string& name, float value) const;
	void setMat4(const std::string& name, glm::mat4 mat) const;
	void setVec2(const std::string& name, glm::vec2 v) const;

private:

};

ComputeShader::ComputeShader(const char* computePath)
{
	std::string computeCode;
	std::ifstream cShaderFile;

	// Ensure ifstream objects can throw exceptions
	cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		// Open files
		cShaderFile.open(computePath);

		// Read file content into stream
		std::stringstream cShaderStream;
		cShaderStream << cShaderFile.rdbuf();
		cShaderFile.close();
		computeCode = cShaderStream.str();
	}
	catch (std::ifstream::failure e) {
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
	}

	const char* cShaderCode = computeCode.c_str();

	// Compile shaders
	unsigned compute;
	int success;
	char infoLog[512];

	// CS
	compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &cShaderCode, NULL);
	glCompileShader(compute);
	glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(compute, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n" << infoLog << std::endl;
	};

	// Shader program
	ID = glCreateProgram();
	glAttachShader(ID, compute);
	glLinkProgram(ID);
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}

	// Delete shaders since they're already linked to the program
	glDeleteShader(compute);
}

ComputeShader::~ComputeShader()
{
}

void ComputeShader::use() {
	glUseProgram(ID);
}

void ComputeShader::setBool(const std::string& name, bool value) const
{
	glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}
void ComputeShader::setInt(const std::string& name, int value) const
{
	glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}
void ComputeShader::setFloat(const std::string& name, float value) const
{
	glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void ComputeShader::setMat4(const std::string& name, glm::mat4 mat) const
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void ComputeShader::setVec2(const std::string& name, glm::vec2 v) const
{
	glUniform2f(glGetUniformLocation(ID, name.c_str()), v.x, v.y);
}

#endif // !COMPUTE_SHADER_H
