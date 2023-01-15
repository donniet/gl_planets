
#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <tuple>
#include <memory>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::pair;
using std::tie;
using std::make_pair;

#include <boost/program_options.hpp>
using namespace boost::program_options;

string infoLog(GLuint obj) {
	int log_size = 0;
	int bytes_written = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &log_size);

	if (!log_size) return string();

	std::unique_ptr<char[]> buffer(new char [log_size]);
	
	glGetProgramInfoLog(obj, log_size, &bytes_written, buffer.get());
	// std::cerr << infoLog << std::endl;
	return string(buffer.get());
}



pair<bool,GLuint> read_n_compile_shader(string filename, GLenum shaderType) {
	GLuint hdlr;
	GLint status;

	std::ifstream is(filename, std::ios::in|std::ios::binary|std::ios::ate);
	if (!is.is_open()) {
		std::cerr << "Unable to open file " << filename << std::endl;
		return make_pair(false,0);
	}
	long size = is.tellg();

	std::unique_ptr<char[]> buffer(new char[size+1]);

	is.seekg(0, std::ios::beg);
	is.read (buffer.get(), size);
	is.close();
	buffer[size] = 0;

	hdlr = glCreateShader(shaderType);
	glShaderSource(hdlr, 1, (const GLchar**)&buffer, NULL);
	glCompileShader(hdlr);
	glGetShaderiv(hdlr, GL_COMPILE_STATUS, &status);
	if(status != GL_TRUE) {
		cerr << "could not compile shader.\n";
		std::cerr << "info log for " << filename << "\n" <<
			infoLog(hdlr) << "\n";

		return make_pair(false,hdlr);
	}

	return make_pair(true,hdlr);
}

pair<bool,GLuint> program_from_shader_files(string vertex_path, string fragment_path) {
	bool success;
	GLuint vertex;
	GLuint fragment;

	tie(success, vertex) = read_n_compile_shader(vertex_path, GL_VERTEX_SHADER);
	if(!success) {
		cerr << "error reading and compiling shader: " << vertex_path << endl;
		return make_pair(false, 0);
	}
	tie(success, fragment) = read_n_compile_shader(fragment_path, GL_FRAGMENT_SHADER);
	if(!success) {
		cerr << "error reading and compiling shader: " << fragment_path << endl;
		return make_pair(false, 0);
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vertex);
	glAttachShader(prog, fragment);
	glLinkProgram(prog);
	GLint linker_status;
	glGetProgramiv(prog, GL_LINK_STATUS, &linker_status);

	if(linker_status != GL_TRUE) {
		cerr << "error linking program: \n" << infoLog(prog) << "\n";
		return make_pair(false, prog);
	}

	return make_pair(true, prog);
}


int main(int ac, char * av[]) {
	string vertex_shader = "../shaders/sphere_vert.glsl";
	string fragment_shader = "../shaders/sphere_frag.glsl";

	options_description desc("options");
	desc.add_options()
		("help,h", "prints this message")
		("vertex,v", value(&vertex_shader), "vertex shader path")
		("fragment,f", value(&fragment_shader), "fragment shader path")
	;
	variables_map vm;
	store(parse_command_line(ac, av, desc), vm);

	if(vm.count("help")) {
		cout << desc << "\n";
		return 0;
	}

	glutInit(&ac, av);
	glutCreateWindow("GL PLanets");
	GLenum ret;

	if ((ret = glewInit()) != GLEW_OK) {
		cerr << "could not initialize glew: " << glewGetErrorString(ret) << "\n";
		return -1;
	}

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

	GLuint prog;
	bool success;
	tie(success, prog) = program_from_shader_files(vertex_shader, fragment_shader);
	if(!success) return -1;

	glutMainLoop();
	

    return 0;    
}