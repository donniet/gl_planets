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
#include <stb/stb_image_resize.h>
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
#include <initializer_list>
#include <algorithm>
#include <vector>
#include <array>
#include <sstream>

// for debugging
#include <iostream>

#define LOG(msg) { std::cout << __FILE__ << ":" << __LINE__ << " " << msg << std::endl; }


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
using std::map;
using std::tuple;
using std::initializer_list;
using std::bind;
using std::fill;
using std::stringstream;

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

		if(data_ == nullptr) {
            std::cerr << "could not open file: '" << path << "';\n";
            return;
        }

        // // check the texture for a power of 2
        // if(!is_power_of_two(width_) || !is_power_of_two(height_)) {
        //     std::cerr << "file '" << path << "' is not a power of 2\n";
        //     return;
        // }

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

    static bool is_power_of_two(int x) {
        return (x != 0) && ((x & (x - 1)) == 0);
    }

	bool is_valid() const { return data_ != nullptr; }
	operator bool() const { return is_valid(); }
	unsigned char * data() const { return data_; }
	int width() const { return width_; } 
	int height() const { return height_; }

	operator GLuint() const { return texture_id; }
};

class TextureArray {
    GLuint texture_id_;
    vector<string> paths_;
    vector<unsigned char *> data_;
    int width_;
    int height_;
    int channels_;

    void init() {
        // load all the images
        size_t count = paths_.size();

        vector<int>             widths(count);
        vector<int>             heights(count);
        vector<int>             channels(count);
        vector<unsigned char *> temp(count);

        for(int i = 0; i < count; i++) {
            temp[i] = stbi_load(paths_[i].c_str(), &widths[i], &heights[i], &channels[i], 3);
        }

        auto max_width = *std::max_element(widths.begin(), widths.end());
        auto max_height = *std::max_element(heights.begin(), heights.end());

        // next power of 2
        int siz = 1;
        int levels = 0; // eventually set mipmap levels
        for(; siz < max_width && siz < max_height; siz <<= 1, levels++) { }



        data_.resize(count);

        for(int i = 0; i < count; i++) {
            data_[i] = new unsigned char[max_width * max_height * 3];
            stbir_resize_uint8(temp[i], widths[i], heights[i], 0, 
                               data_[i], siz, siz, 0, 3);
        }

        // LOG("glGenTextures")
        glGenTextures(1, &texture_id_);
        // LOG("glBindTexure")
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture_id_);
        // LOG("glTexStorage3D")
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, levels, GL_RGB8, siz, siz, count);
        for(int i = 0; i < count; i++) {
            // LOG("glTexSubImage3D")
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, siz, siz, 1, GL_RGB, GL_UNSIGNED_BYTE, data_[i]);
        }
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        

        for(int i = 0; i < count; i++) {
            stbi_image_free(temp[i]);
            temp[i] = nullptr;
        }

    }

public:
    operator GLuint() const { return texture_id_; }
    TextureArray(vector<string> const & paths)
        : paths_(paths)
    { init(); }

    template<size_t LEN>
    TextureArray(std::array<string, LEN> const & paths)
        : paths_(paths.begin(), paths.end())
    { init(); }

    ~TextureArray()
    {
        for(unsigned char *& dat : data_) {
            delete [] dat;
            dat = nullptr;
        }
    }
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
	static pair<Shader,bool> shader_from_shader_file(
        string file_name, GLenum shader_type, 
        vector<string> const & extensions = {}, 
        vector<pair<string,string>> defines = {}) 
    {
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

        stringstream extenstr;
        for(auto const & ext : extensions) {
            extenstr << "#extension " << ext << " : enable\n";
        }
        stringstream defstr;
        for(auto const & def : defines) {
            defstr << "#define " << def.first << " " << def.second << "\n";
        }

        auto exts = extenstr.str();
        auto defs = defstr.str();

        GLchar const * sources[] = { exts.c_str(), defs.c_str(), sh };

		hdlr = glCreateShader(shader_type);
		glShaderSource(hdlr, 3, sources, NULL);
		glCompileShader(hdlr);
		glGetShaderiv(hdlr, GL_COMPILE_STATUS, &status);
		if(status != GL_TRUE) {
            GLint log_size;
		    glGetShaderiv(hdlr, GL_INFO_LOG_LENGTH, &log_size);
            GLchar * log = new GLchar[log_size];
            glGetShaderInfoLog(hdlr, log_size, &log_size, log);

            std::cerr << "info log:\n" << log << std::endl;
            delete [] log;

			return {Shader(hdlr), false};
		}

		return make_pair(Shader(hdlr), true);
	}
public:
	Shader(GLuint shader) : shader_(shader) {}
    Shader(Shader const & rhs) : shader_(rhs.shader_) {}
    Shader(Shader && rhs) : shader_(rhs.shader_) {}
    Shader& operator=(Shader const & rhs) {
        shader_ = rhs.shader_;
        return *this;
    }
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
		glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &log_size);

		if (log_size <= 0) {
			return "";
		}

		std::unique_ptr<char[]> buffer(new char [log_size]);
		
		glGetShaderInfoLog(shader_, log_size, &bytes_written, buffer.get());
		return string(buffer.get());
	}

	static pair<Shader,bool> vertex_from_shader_file(
        string file_path, 
        vector<string> const & extensions = {}, 
        vector<pair<string,string>> defines = {}) 
    {
		return shader_from_shader_file(file_path, GL_VERTEX_SHADER, extensions, defines);
	}
	static pair<Shader,bool> fragment_from_shader_file(
        string file_path, 
        vector<string> const & extensions = {}, 
        vector<pair<string,string>> defines = {}) 
    {
		return shader_from_shader_file(file_path, GL_FRAGMENT_SHADER, extensions, defines);
	}		
};

class Program;


class programParameters {
private:
	Program const & program_;
	vector<function<void()>> param_setters_;
	GLuint geometry_count_;
	GLuint texture_count_;
	vector<GLuint> texture_ids_;
public:
	programParameters(Program const & p);

	template<typename T>
	programParameters & operator()(string const & name, T const & dat);

	void draw_arrays_triangle_fan();
};


class Program {
private:
	GLuint program_;

    Shader vertex_, fragment_;
public:
	Program(GLuint program, Shader const & vertex, Shader const & fragment) 
        : program_(program), vertex_(vertex), fragment_(fragment)
    { }
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
    string vertex_info_log() const {
        return vertex_.info_log();
    }
    string fragment_info_log() const {
        return fragment_.info_log();
    }

	static pair<Program,bool> from_shader_files(
        string vertex_path, string fragment_path, 
        vector<string> const & extensions = {}, 
        vector<pair<string,string>> defines = {}) 
    {
		bool success;
        Shader vertex, fragment;

		tie(vertex, success) = Shader::vertex_from_shader_file(vertex_path, extensions, defines);
		if(!success) { return make_pair(Program(), false); }

		tie(fragment, success) = Shader::fragment_from_shader_file(fragment_path, extensions, defines); 
		if(!success) { return make_pair(Program(), false); }

		GLuint prog = glCreateProgram();
		glAttachShader(prog, vertex);
		glAttachShader(prog, fragment);
		glLinkProgram(prog);

	    Program ret(prog, vertex, fragment);

		if(!ret.linker_status()) {
			return make_pair(ret, false);
		}

		return make_pair(ret, true);
	}

	programParameters make_drawer() { 
		return programParameters(*this); 
	}
};

programParameters::programParameters(Program const & p) : 
	program_(p), geometry_count_(0), texture_count_(0) 
{ 
	GLint maxTextureUnits;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
	texture_ids_.resize(maxTextureUnits);
	fill(texture_ids_.begin(), texture_ids_.end(), 0);
}


template<>
programParameters & programParameters::operator()(string const & name, TextureArray const & dat) {
    GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	GLuint texture_id = texture_count_++;
	texture_ids_[location] = texture_id;

	param_setters_.push_back([&dat, location, texture_id]() { 
		glActiveTexture(GL_TEXTURE0 + texture_id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, dat);
        // glBindTextures(0, dat.size(), dat.textures());
		glUniform1i(location, texture_id);
	});

	return *this;
}


template<>
programParameters & programParameters::operator()(string const & name, float const & dat) 
{
    GLint location = glGetUniformLocation(program_, name.c_str());
    if(location < 0) return *this;

    param_setters_.push_back([&dat, location]() {
        glUniform1f(location, dat);
    });
    return *this;
}


template<>
programParameters & programParameters::operator()<>(string const & name, Texture const & dat) 
{
	GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	GLuint texture_id = texture_count_++;
	texture_ids_[location] = texture_id;

	param_setters_.push_back([&dat, location, texture_id]() { 
		glActiveTexture(GL_TEXTURE0 + texture_id);
		glBindTexture(GL_TEXTURE_2D, dat);
		glUniform1i(location, texture_id);
	});
	return *this;
}


void programParameters::draw_arrays_triangle_fan() {
	glUseProgram(program_);
    
	for(auto const & p : param_setters_)
		p();

	glDrawArrays(GL_TRIANGLE_FAN, 0, geometry_count_);
}

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
	friend class programParameters;
private:
	data_type const & data_;
public:
	UniformMatrix(data_type const & data) : data_(data) {}

	void operator()(GLint location) {
		glUniformMatrix4fv(location, 1, GL_FALSE, &data_[0][0]);
	}
	void setup_parameter(GLint location) const {
		glUniformMatrix4fv(location, 1, GL_FALSE, &data_[0][0]);
	}
};


template<>
programParameters & programParameters::operator()<>(string const & name, UniformMatrix<float,4> const & dat) 
{
	GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	param_setters_.push_back(bind(&UniformMatrix<float,4>::setup_parameter, dat, location));
	return *this;
}

template<typename T, size_t siz> class Uniform;
template<> class Uniform<float,3> {
	typedef glm::vec3 data_type;
	friend class programParameters;
private:
	data_type const & data_;
public:
	Uniform(data_type const & data) : data_(data) {}

	void operator()(GLint location) {
		glUniform3fv(location, 1, &data_[0]);
	}
	void setup_parameter(GLint location) const {
		glUniform3fv(location, 1, &data_[0]);
	}
};

template<>
programParameters & programParameters::operator()<>(string const & name, Uniform<float,3> const & dat) 
{
	GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	param_setters_.push_back(bind(&Uniform<float,3>::setup_parameter, &dat, location));
	return *this;
}


template<typename T, size_t siz> class UniformArray;
template<> class UniformArray<float, 3> {
    typedef glm::vec3 data_type;
    friend class programParameters;
private:
    data_type const * data_;
    size_t len_;
public:
    UniformArray(data_type const * data, size_t len) 
        : data_(data), len_(len)
    { }

    void setup_parameter(GLint location) const {
        glUniform3fv(location, len_, &(*data_)[0]);
    }
};

template<>
programParameters & programParameters::operator()<>(string const & name, UniformArray<float,3> const & dat) 
{
	GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	param_setters_.push_back(bind(&UniformArray<float,3>::setup_parameter, &dat, location));
	return *this;
}

template<> class UniformArray<float, 1> {
    typedef float data_type;
    friend class programParameters;
private:
    data_type const * data_;
    size_t len_;
public:
    UniformArray(data_type const * data, size_t len) 
        : data_(data), len_(len)
    { }

    void setup_parameter(GLint location) const {
        glUniform1fv(location, len_, data_);
    }
};

template<>
programParameters & programParameters::operator()<>(string const & name, UniformArray<float,1> const & dat) 
{
	GLint location = glGetUniformLocation(program_, name.c_str());
	if(location < 0) return *this;

	param_setters_.push_back(bind(&UniformArray<float,1>::setup_parameter, &dat, location));
	return *this;
}

template<typename T> struct gl_type {};
template<> struct gl_type<float> { static const GLenum value = GL_FLOAT; };

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
	static constexpr size_t width = siz;

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
	size_t geometry_count() const {
		return count_ / siz;
	}
	GLuint size() const {
		return count_ * sizeof(T);
	}
	GLenum type() const {
		return gl_type<T>::value;
	}
	const void * data() const {
		return data_;
	}

	void setup_parameter(GLint location) const {
		glBindBuffer(GL_ARRAY_BUFFER, buffer_);
		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, width, type(), GL_FALSE, 0, 0);
	}
};


template<>
programParameters & programParameters::operator()<>(string const & name, ArrayBuffer<float,2> const & dat) 
{
	GLint location = glGetAttribLocation(program_, name.c_str());
	if(location < 0) return *this;

	geometry_count_ = dat.geometry_count();
	param_setters_.push_back(bind(&ArrayBuffer<float,2>::setup_parameter, &dat, location));
	return *this;
}






#endif