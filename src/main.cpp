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

	if (!log_size) {
		cout << "no log found\n";
		return "";
	}

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
	cout << "shader source:\n" << buffer.get() << endl;

	GLchar const * sh = buffer.get();

	hdlr = glCreateShader(shaderType);
	glShaderSource(hdlr, 1, (const GLchar*const*)&sh, NULL);
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
		cerr << "error reading and compiling vertex shader: " << vertex_path << endl;
		return make_pair(false, 0);
	}
	tie(success, fragment) = read_n_compile_shader(fragment_path, GL_FRAGMENT_SHADER);
	if(!success) {
		cerr << "error reading and compiling fragment shader: " << fragment_path << endl;
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

void error_callback(int error, const char * desc) {
	cerr << "ERROR: " << desc << "\n";
}

int main(int ac, char * av[]) {
	string vertex_shader = "../shaders/sphere_vert.glsl";
	string fragment_shader = "../shaders/sphere_frag.glsl";
	float fieldOfView = 60., near = 1., far = 10.;

	options_description desc("options");
	desc.add_options()
		("help,h", "prints this message")
		("vertex,v", value(&vertex_shader), "vertex shader path")
		("fragment,f", value(&fragment_shader), "fragment shader path")
		("fov", value(&fieldOfView), "field of view in degrees")
	;
	variables_map vm;
	store(parse_command_line(ac, av, desc), vm);

	if(vm.count("help")) {
		cout << desc << "\n";
		return 0;
	}

	// convert to radians
	fieldOfView *= glm::pi<float>() / 180.;

	size_t n_frames;
	double time_of_first_swap;
	double time_of_last_swap;
	double time_now;


	if (!glfwInit())
	{
		fprintf(stderr, "Error: Failed to init GLFW\n");
		return -1;
	}
	glfwSetErrorCallback(error_callback);


	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

	auto monitor = glfwGetPrimaryMonitor();
	auto mode = glfwGetVideoMode(monitor);
	
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	auto window = glfwCreateWindow(mode->width,
	                          mode->height,
	                          "GL Planets",
	                          monitor,
	                          NULL);
	
	if (!window)
	{
		fprintf(stderr, "Error: Failed to create window\n");
		glfwTerminate();
		return -1;
	}

	printf("%dx%d @ %dHz\n", mode->width,
	                         mode->height,
	                         mode->refreshRate);
	
	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
  	glewInit();

	printf("OpenGL info:\n"
	       "\tVendor   = \"%s\"\n"
	       "\tRenderer = \"%s\"\n"
	       "\tVersion  = \"%s\"\n",
	       glGetString(GL_VENDOR),
	       glGetString(GL_RENDERER),
	       glGetString(GL_VERSION));

	glfwSwapInterval(1);



	GLuint prog;
	bool success;
	tie(success, prog) = program_from_shader_files(vertex_shader, fragment_shader);
	if(!success) return -1;

	auto corner_location = glGetAttribLocation(prog, "corner");
	auto inv_location = glGetUniformLocation(prog, "inv");
	auto camera_location = glGetUniformLocation(prog, "camera");
	auto texture_location = glGetUniformLocation(prog, "texture");
	auto sun_location = glGetUniformLocation(prog, "sun");

	// handle resize
	glm::mat4 projection = glm::perspective(fieldOfView, (float)mode->width / (float)mode->height, near, far);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glfwSwapBuffers(window);


    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

	time_of_first_swap = glfwGetTime();
	n_frames = 0;
	time_of_last_swap = time_of_first_swap;

	// setup buffers
	GLuint corners_buffer;

	float corners[] = {
		-1, -1,
		1, -1,
		1, 1,
		-1, 1
	};

	glGenBuffers(1, &corners_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, corners_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);


	while (!glfwWindowShouldClose(window) &&
	       (glfwGetTime() - time_of_first_swap) < 10.0)
	{
		/* Process window events */
		glfwPollEvents();
		
		/* Clear the framebuffer to black */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 m = glm::identity<glm::mat4>();
		glm::mat4 view = glm::identity<glm::mat4>();

		view = glm::translate(view, glm::vec3(0, 0, -2));
		view = glm::rotate(view, (float)time_now / (float)1200., glm::vec3(0, 1, 0));

		glm::mat4 mv = projection;
		mv *= view;
		mv *= m;

		glm::vec4 camera(0, 0, 0, 1);
		glm::mat4 inv = glm::inverse(view);
		camera = inv * camera;

		glm::vec3 sun = glm::vec3(
			glm::cos((float)time_now / (float)6000.), 
			0.,
			-glm::sin((float)time_now / (float)6000.));


		glUseProgram(prog);
		glEnableVertexAttribArray(corner_location);
		glUniformMatrix4fv(inv_location, 1, GL_FALSE, &inv[0][0]);
		glUniform3fv(camera_location, 1, &camera[0]);
		glUniform3fv(sun_location, 1, &sun[0]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform1i(texture_location, 0);

		glBindBuffer(GL_ARRAY_BUFFER, corners_buffer);
		glVertexAttribPointer(corner_location, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glDisableVertexAttribArray(corner_location);


		
		/* Display framebuffer */
		glfwSwapBuffers(window);
		
		/* Update fps counter */
		time_now = glfwGetTime();
		//printf("%.1fms\n", (time_now - time_of_last_swap) * 1.0e3);
		time_of_last_swap = time_now;
		++n_frames;
	}

	printf("%zu frames in %gs = %.1fHz\n",
	    n_frames,
	    time_of_last_swap - time_of_first_swap,
	    (double)n_frames / (time_of_last_swap - time_of_first_swap));



	glfwMakeContextCurrent(NULL);
	
	glfwDestroyWindow(window);
	glfwTerminate();

	

    return 0;    
}