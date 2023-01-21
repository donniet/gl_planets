#ifndef __GL_HPP__
#define __GL_HPP__

#define GLEW_STATIC
#include <GL/glew.h>

#define GLFW_INCLUDE_ES
#include <GLFW/glfw3.h>

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi

#include <stb/stb_image.h>
#include <string>
#include <tuple>
#include <utility>
#include <memory>
#include <fstream>
#include <map>
#include <exception>
#include <sstream>
#include <functional>
#include <vector>

// for debugging
#include <iostream>

using std::string;
using std::pair;
using std::make_pair;
using std::shared_ptr;
using std::unique_ptr;
using std::ifstream;
using std::map;
using std::tie;
using std::vector;
using std::function;

class Texture {
private:
	string path_;
	int width_;
	int height_;
	int channels_;
	unsigned char * data_;
	GLuint texture_id;

public:
	Texture(string path, int desired_channels = 3) : 
		path_(path), texture_id(0) 
	{
		data_ = stbi_load(path_.c_str(), &width_, &height_, &channels_, desired_channels);

		if(data_ == nullptr) return;

		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		// TODO: only apply these packing rules when the width/height of the texture demand it
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, data_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture(Texture && rhs) 
		: path_(rhs.path_), width_(rhs.width_), height_(rhs.height_), 
		  channels_(rhs.channels_), data_(rhs.data_)
	{
		rhs.data_ = nullptr;
	}
	Texture(Texture const & rhs) = delete;

	~Texture() {
		if(is_valid()) {
			stbi_image_free(data_);
			data_ = nullptr;
		}
	}

	bool is_valid() const { return data_ != nullptr; }
	operator bool() const { return is_valid(); }
	unsigned char * data() const { return data_; }
	int width() const { return width_; } 
	int height() const { return height_; }

	operator GLuint() const { return texture_id; }
};


class Shader {
private:
	GLuint shader_;	
private:
	static pair<shared_ptr<char[]>, bool> read_file(string file_path) {
		using std::ifstream, std::ios, std::move;

		ifstream is(file_path, ios::in|ios::binary|ios::ate);
		if (!is.is_open()) {
			return make_pair(shared_ptr<char[]>(), false);
		}
		long size = is.tellg();

		shared_ptr<char[]> buffer(new char[size+1]);

		is.seekg(0, ios::beg);
		is.read (buffer.get(), size);
		is.close();
		buffer[size] = 0;

		return make_pair(move(buffer), true);
	}
	static pair<Shader,bool> shader_from_shader_file(string file_name, GLenum shader_type) {
		shared_ptr<char[]> buffer;
		bool success;

		tie(buffer, success) = read_file(file_name);
		if(!success) {
			return make_pair(Shader(), false);
		}

		// std::cout << "// shader:\n" << buffer << std::endl;

		char const * sh = buffer.get();
		GLuint hdlr;
		GLint status;

		hdlr = glCreateShader(shader_type);
		glShaderSource(hdlr, 1, (const GLchar*const*)&sh, NULL);
		glCompileShader(hdlr);
		glGetShaderiv(hdlr, GL_COMPILE_STATUS, &status);
		if(status != GL_TRUE) {
			return make_pair(Shader(hdlr), false);
		}

		return make_pair(Shader(hdlr), true);
	}
public:
	Shader(GLuint shader) : shader_(shader) {}
	operator GLuint() const { return shader_; }

	Shader() : shader_(0) {}

	bool valid() const { return shader_ != 0; }
	bool compile_status() const {
		if(!valid()) return false;

		GLint status;
		glGetShaderiv(shader_, GL_COMPILE_STATUS, &status);
		return status == GL_TRUE;
	}
	string info_log() const {
		if(!valid()) return "invalid shader";

		int log_size = 0;
		int bytes_written = 0;
		glGetProgramiv(shader_, GL_INFO_LOG_LENGTH, &log_size);

		if (log_size <= 0) {
			return "";
		}

		std::unique_ptr<char[]> buffer(new char [log_size]);
		
		glGetProgramInfoLog(shader_, log_size, &bytes_written, buffer.get());
		return string(buffer.get());
	}

	static pair<Shader,bool> vertex_from_shader_file(string file_path) {
		return shader_from_shader_file(file_path, GL_VERTEX_SHADER);
	}
	static pair<Shader,bool> fragment_from_shader_file(string file_path) {
		return shader_from_shader_file(file_path, GL_FRAGMENT_SHADER);
	}		
};


template<typename T> struct Param 
{
	string name;
	T & value; 
};

template<typename T> Param<T> make_param(string name, T & value) {
	return Param<T>{name, value};
}


template<typename ... Ts>
struct draw_arrays_helper;

template<typename ... Ts>
struct drawer;

class Program {
private:
	GLuint program_;
	map<string,GLint> locations_;
public:
	Program(GLuint program) : program_(program) {}
	operator GLuint() const { return program_; }
	Program() : program_(0) {} 

	bool valid() const { 
		return program_ != 0;
	}
	bool linker_status() const {
		if(!valid()) return false;

		GLint linker_status;
		glGetProgramiv(program_, GL_LINK_STATUS, &linker_status);

		return linker_status == GL_TRUE;
	}

	static pair<Program,bool> from_shader_files(string vertex_path, string fragment_path) {
		Shader vertex, fragment;
		bool success;

		tie(vertex, success) = Shader::vertex_from_shader_file(vertex_path);
		if(!success) { return make_pair(Program(), false); }

		tie(fragment, success) = Shader::fragment_from_shader_file(fragment_path); 
		if(!success) { return make_pair(Program(), false); }

		GLuint prog = glCreateProgram();
		glAttachShader(prog, vertex);
		glAttachShader(prog, fragment);
		glLinkProgram(prog);

		Program ret(prog);

		if(!ret.linker_status()) {
			return make_pair(ret, false);
		}

		return make_pair(ret, true);
	}

	template<typename ... Ts>
	drawer<Ts...> drawer(Param<Ts> ... params);

	template<typename ... Ts>
	void draw_arrays_triangle_fan(Param<Ts> ... params);
};

std::ostream & operator<<(std::ostream & os, glm::mat4 const & mat) {
	std::cout << "[\n";
	for(int i = 0; i < 16; i++) {
		std::cout << mat[i/4][i%4];
		if(i % 4 == 3) std::cout << std::endl;
		else std::cout << ", ";
	}
	std::cout << "]" << std::endl;
	return os;
}

template<typename T, size_t siz> class UniformMatrix;
template<> class UniformMatrix<float,4> {
	typedef glm::mat4 data_type;
private:
	data_type const & data_;
public:
	UniformMatrix(data_type const & data) : data_(data) {}

	void operator()(GLint location) {
		glUniformMatrix4fv(location, 1, GL_FALSE, &data_[0][0]);
	}
};

template<typename T, size_t siz> class Uniform;
template<> class Uniform<float,3> {
	typedef glm::vec3 data_type;
private:
	data_type const & data_;
public:
	Uniform(data_type const & data) : data_(data) {}

	void operator()(GLint location) {
		glUniform3fv(location, 1, &data_[0]);
	}
};

template<typename T, size_t siz>
class ArrayBuffer {
	friend class Program;

	GLuint buffer_;
	size_t count_;
	void const * data_;
protected:
public:
	operator GLuint() const { return buffer_; }

	typedef T value_type;
	static constexpr size_t size = siz;

	template<size_t length>
	ArrayBuffer(T (&data)[length]) : 
		ArrayBuffer(length, data) 
	{ }
	ArrayBuffer(size_t count, T const * first) 
		: count_(count), data_(first)
	{
		glGenBuffers(1, &buffer_);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_);
		// ensure size parameter is bytes
		glBufferData(GL_ARRAY_BUFFER, count_ * sizeof(T), static_cast<void const *>(data_), GL_STATIC_DRAW);
	}
	ArrayBuffer(ArrayBuffer && rhs) 
		: buffer_(rhs.buffer_), count_(rhs.count_), data_(rhs.data_)
	{
		rhs.buffer_ = 0;
		rhs.count_ = 0;
		rhs.data_ = nullptr;
	}
	ArrayBuffer(ArrayBuffer const & rhs) = delete;
	~ArrayBuffer() {
		if(buffer_ == 0) return;

		glDeleteBuffers(1, &buffer_);
	}
	size_t vertex_count() const {
		return count_ / siz;
	}
};

template<typename T> struct gl_type {};
template<> struct gl_type<float> { static const GLenum value = GL_FLOAT; };


template<typename T, size_t siz, typename ... Ts>
struct draw_arrays_helper<ArrayBuffer<T,siz>, Ts...> 
	: public draw_arrays_helper<Ts...> 
{
	GLuint loc_;
	string name_;
	ArrayBuffer<T,siz> & buffer_;

	typedef draw_arrays_helper<Ts...> Base;

	draw_arrays_helper(Program & prog, Param<ArrayBuffer<T,siz>> param, Param<Ts> ... params) :
		draw_arrays_helper<Ts...>(prog, params...), 
		loc_(Base::get_attrib_location(param.name)),
		name_(param.name),
		buffer_(param.value)
	{ }

	inline void draw_arrays(GLenum type) {
		glEnableVertexAttribArray(loc_);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_);
		glVertexAttribPointer(loc_, siz, gl_type<T>::value, GL_FALSE, 0, nullptr);
		// set the vertex count in the leaf class
		Base::set_vertex_count(buffer_.vertex_count());

		Base::draw_arrays(type);
	}

	~draw_arrays_helper() {
		glDisableVertexAttribArray(loc_);
	}
};

template<typename T, size_t siz, typename ... Ts>
struct draw_arrays_helper<UniformMatrix<T,siz>, Ts...> 
	: public draw_arrays_helper<Ts...> 
{
	typedef draw_arrays_helper<Ts...> Base;

	GLint uniform_location_;
	string name_;
	UniformMatrix<T,siz> & uniform_matrix_;


	draw_arrays_helper(Program & prog, Param<UniformMatrix<T,siz>> param, Param<Ts> ... params) :
		draw_arrays_helper<Ts...>(prog, params...), 
		uniform_location_(Base::get_uniform_location(param.name)),
		name_(param.name),
		uniform_matrix_(param.value)
	{ 
	}

	inline void draw_arrays(GLenum type) {
		uniform_matrix_(uniform_location_);

		Base::draw_arrays(type);
	}
};


template<typename T, size_t siz, typename ... Ts>
struct draw_arrays_helper<Uniform<T,siz>, Ts...> 
	: public draw_arrays_helper<Ts...> 
{
	typedef draw_arrays_helper<Ts...> Base;

	GLint uniform_location_;
	string name_;
	Uniform<T,siz> & uniform_;

	draw_arrays_helper(Program & prog, Param<Uniform<T,siz>> param, Param<Ts> ... params) :
		draw_arrays_helper<Ts...>(prog, params...), 
		uniform_location_(Base::get_uniform_location(param.name)),
		name_(param.name),
		uniform_(param.value)
	{ }

	inline void draw_arrays(GLenum type) {
		uniform_(uniform_location_);

		Base::draw_arrays(type);
	}
};

template<typename ... Ts>
struct draw_arrays_helper<Texture, Ts...> 
	: public draw_arrays_helper<Ts...> 
{
	typedef draw_arrays_helper<Ts...> Base;
	GLint texture_location_;
	size_t texture_label_;
	string name_;
	Texture & texture_;


	draw_arrays_helper(Program & prog, Param<Texture> param, Param<Ts> ... params) :
		draw_arrays_helper<Ts...>(prog, params...), 
		texture_location_(Base::get_uniform_location(param.name)),
		texture_label_(Base::allocate_texture()),
		name_(param.name),
		texture_(param.value)
	{ }

	inline void draw_arrays(GLenum type) {
		glActiveTexture(GL_TEXTURE0 + texture_label_);
		glBindTexture(GL_TEXTURE_2D, texture_ );
		glUniform1i(texture_location_, texture_label_);

		Base::draw_arrays(type);
	}
};

template<> struct draw_arrays_helper<> {
	Program & prog_;
	size_t vertex_count_;
	size_t texture_count_;

	GLuint get_attrib_location(string name) const {
		auto loc = glGetAttribLocation(prog_, name.c_str());

		return loc;
	};
	GLuint get_uniform_location(string name) const {
		auto loc = glGetUniformLocation(prog_, name.c_str());
		
		return loc;
	}
	inline void begin_draw() {
		glUseProgram(prog_);
	}
	inline void draw_arrays(GLenum type) {
		glDrawArrays(type, 0, vertex_count_);
	}
	size_t allocate_texture() { return texture_count_++; }
	void set_vertex_count(size_t count) { vertex_count_ = count; }

	draw_arrays_helper(Program & prog) 
		: prog_(prog), vertex_count_(0), texture_count_(0)
	{ }
};


template<typename ... Ts>
struct drawer : public draw_arrays_helper<Ts...> { 
	typedef draw_arrays_helper<Ts...> Base;

	void draw_arrays_triangle_fan() {
		Base::begin_draw();
		Base::draw_arrays(GL_TRIANGLE_FAN);
	}
};

template<typename ... Ts>
drawer<Ts...> Program::drawer(Param<Ts> ... params) {
	return drawer<Ts...>(params...);
}

template<typename ... Ts>
void Program::draw_arrays_triangle_fan(Param<Ts> ... params)
{
	draw_arrays_helper<Ts...> helper(*this, params...);
	helper.begin_draw();
	helper.draw_arrays(GL_TRIANGLE_FAN);
}





#endif