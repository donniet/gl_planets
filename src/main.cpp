
#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <tuple>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::pair;
using std::tie;
using std::make_pair;

#include <boost/program_options.hpp>
using namespace boost::program_options;

void printInfoLog(GLuint obj) {
	int log_size = 0;
	int bytes_written = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &log_size);
	if (!log_size) return;
	char *infoLog = new char[log_size];
	glGetProgramInfoLog(obj, log_size, &bytes_written, infoLog);
	std::cerr << infoLog << std::endl;
	delete [] infoLog;
}



pair<bool,GLuint> read_n_compile_shader(string filename, GLenum shaderType) {
	GLuint hdlr;
	std::ifstream is(filename, std::ios::in|std::ios::binary|std::ios::ate);
	if (!is.is_open()) {
		std::cerr << "Unable to open file " << filename << std::endl;
		return make_pair(false,0);
	}
	long size = is.tellg();
	char *buffer = new char[size+1];
	is.seekg(0, std::ios::beg);
	is.read (buffer, size);
	is.close();
	buffer[size] = 0;

	hdlr = glCreateShader(shaderType);
	glShaderSource(hdlr, 1, (const GLchar**)&buffer, NULL);
	glCompileShader(hdlr);
	std::cerr << "info log for " << filename << std::endl;
	printInfoLog(hdlr);
	delete [] buffer;
	return make_pair(true,hdlr);
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


    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
		cerr << "could not initialize glfw\n";
        return -1;
	}

	if (!glewInit()) {
		cerr << "could not initialize glew\n";
		return -1;
	}

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "GL Planets", NULL, NULL);

	bool success = false;
	GLuint vertex;
	GLuint fragment;

	tie(success, vertex) = read_n_compile_shader(vertex_shader, GL_VERTEX_SHADER);
	if(!success) {
		cerr << "error reading and compiling shader: " << vertex_shader << endl;
		return -1;
	}
	tie(success, fragment) = read_n_compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
	if(!success) {
		cerr << "error reading and compiling shader: " << fragment_shader << endl;
		return -1;
	}

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;    
}