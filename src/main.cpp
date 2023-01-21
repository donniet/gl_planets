

#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <tuple>
#include <memory>
#include <map>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::pair;
using std::tie;
using std::make_pair;
using std::unique_ptr;
using std::shared_ptr;
using std::map;

#include "gl.hpp"

#include <boost/program_options.hpp>
using namespace boost::program_options;

std::vector<std::string> GetFirstNMessages(GLuint numMsgs = 100)
{
	GLint maxMsgLen = 0;
	glGetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH, &maxMsgLen);

	std::vector<GLchar> msgData(numMsgs * maxMsgLen);
	std::vector<GLenum> sources(numMsgs);
	std::vector<GLenum> types(numMsgs);
	std::vector<GLenum> severities(numMsgs);
	std::vector<GLuint> ids(numMsgs);
	std::vector<GLsizei> lengths(numMsgs);

	GLuint numFound = glGetDebugMessageLog(numMsgs, msgData.size(), &sources[0], &types[0], &ids[0], &severities[0], &lengths[0], &msgData[0]);

	sources.resize(numFound);
	types.resize(numFound);
	severities.resize(numFound);
	ids.resize(numFound);
	lengths.resize(numFound);

	std::vector<std::string> messages;
	messages.reserve(numFound);

	std::vector<GLchar>::iterator currPos = msgData.begin();
	for(size_t msg = 0; msg < lengths.size(); ++msg)
	{
		messages.push_back(std::string(currPos, currPos + lengths[msg] - 1));
		currPos = currPos + lengths[msg];
	}

	return messages;
}

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
	// cout << "shader source:\n" << buffer.get() << endl;

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
void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}



int main(int ac, char * av[]) {
	string vertex_shader = "../shaders/sphere_vert.glsl";
	string fragment_shader = "../shaders/sphere_frag.glsl";
	string texture_path = "../img/marsb.jpg";
	string starfield_path = "../img/TychoSkymapII.t3_04096x02048.jpg";
	float fieldOfView = 60., near = 1., far = 10.;

	options_description desc("options");
	desc.add_options()
		("help,h", "prints this message")
		("vertex,v", value(&vertex_shader), "vertex shader path")
		("fragment,f", value(&fragment_shader), "fragment shader path")
		("fov", value(&fieldOfView), "field of view in degrees")
		("texture,t", value(&texture_path), "path to texture of planet")
		("starfield,s", value(&starfield_path), "path to starfield spheremap")
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


	Texture io_texture(texture_path);
	Texture star_texture(starfield_path);

	if(!io_texture) {
		cerr << "unable to load texture '" << texture_path << "'\n";
		return -1;
	}
	if(!star_texture) {
		cerr << "unable to load starfield '" << starfield_path << "'\n";
		return -1;
	}

	GLuint prog;
	bool success;
	tie(success, prog) = program_from_shader_files(vertex_shader, fragment_shader);
	if(!success) return -1;


	Program program;
	tie(program, success) = Program::from_shader_files(vertex_shader, fragment_shader);
	if(!success) {
		std::cerr << "error making program" << std::endl;
	}

	// During init, enable debug output
	glEnable( GL_DEBUG_OUTPUT );
	// glDebugMessageControl(GL_DEBUG_SOURCE_API​, 0​, GL_DEBUG_SEVERITY_NOTIFICATION​, 0​, nullptr​, GL_TRUE​);
	glDebugMessageCallback( MessageCallback, 0 );

	auto corner_location = glGetAttribLocation(prog, "corner");
	auto inv_location = glGetUniformLocation(prog, "inv");
	auto camera_location = glGetUniformLocation(prog, "camera");
	auto texture_location = glGetUniformLocation(prog, "texture");
	auto starfield_location = glGetUniformLocation(prog, "starfield");
	auto sun_location = glGetUniformLocation(prog, "sun");

	std::cout << "inv_location: " << inv_location << std::endl;
	std::cout << "corner_location: " << corner_location << std::endl;
	std::cout << "camera_location: " << camera_location << std::endl;
	std::cout << "texture_location: " << texture_location << std::endl;
	std::cout << "starfield_location: " << starfield_location << std::endl;
	std::cout << "sun_location: " << sun_location << std::endl;

	// handle resize
	glm::mat4 projection = glm::perspective(fieldOfView, (float)mode->width / (float)mode->height, near, far);

	GLuint texture_id, starfield_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, io_texture.width(), io_texture.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, io_texture.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);


	glGenTextures(1, &starfield_id);
	glBindTexture(GL_TEXTURE_2D, starfield_id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, star_texture.width(), star_texture.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, star_texture.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);


	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glfwSwapBuffers(window);


    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

	time_of_first_swap = glfwGetTime();
	n_frames = 0;
	time_of_last_swap = time_of_first_swap;

	// setup buffers
	GLuint corners_buffer_old;	

	float corners[] = {
		-1, -1,
		1, -1,
		1, 1,
		-1, 1
	};

	glGenBuffers(1, &corners_buffer_old);
	glBindBuffer(GL_ARRAY_BUFFER, corners_buffer_old);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

	ArrayBuffer<float,2> corners_buffer(sizeof(corners), corners);
		

	while (!glfwWindowShouldClose(window))
	{
		/* Process window events */
		glfwPollEvents();
		
		/* Clear the framebuffer to black */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 m = glm::identity<glm::mat4>();
		glm::mat4 view = glm::identity<glm::mat4>();

		view = glm::translate(view, glm::vec3(0, 0, -2));
		view = glm::rotate(view, (float)time_now / (float)120., glm::vec3(0, 1, 0));

		glm::mat4 mv = projection * view * m;
		mv = glm::inverse(mv);


		glm::vec4 camera(0, 0, 0, 1);
		glm::mat4 inv = glm::inverse(view);
		camera = inv * camera;

		// cout << "camera: " << camera.x << " " << camera.y << " " << camera.z << " " << camera.w << endl;

		glm::vec3 sun = glm::vec3(
			glm::cos((float)time_now / (float)600.), 
			0.,
			-glm::sin((float)time_now / (float)600.));

#if 0
		glUseProgram(prog);
		
		glUniformMatrix4fv(inv_location, 1, GL_FALSE, &mv[0][0]);
		glUniform3fv(camera_location, 1, &camera[0]);
		glUniform3fv(sun_location, 1, &sun[0]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glUniform1i(texture_location, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, starfield_id);
		glUniform1i(starfield_location, 1);

		glEnableVertexAttribArray(corner_location);
		glBindBuffer(GL_ARRAY_BUFFER, corners_buffer_old);
		glVertexAttribPointer(corner_location, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		

		glDisableVertexAttribArray(corner_location);
#else
		UniformMatrix<float,4> inverse_transform(mv);
		Uniform<float,3> camera_position(camera);
		Uniform<float,3> sun_position(sun);


		program.draw_arrays_triangle_fan(
			make_param("camera", camera_position ),
			make_param("texture", io_texture ),
			make_param("starfield", star_texture ),
			make_param("sun", sun_position ),
			make_param("inv", inverse_transform ),
			make_param("corner", corners_buffer )
		);

#endif		
		/* Display framebuffer */
		glfwSwapBuffers(window);
		
		std::vector<string> messages = GetFirstNMessages();
		for(string s : messages) {
			std::cout << s << std::endl;
		}

		
		/* Update fps counter */
		time_now = glfwGetTime();
		printf("%.1fms\n", (time_now - time_of_last_swap) * 1.0e3);
		time_of_last_swap = time_now;
		++n_frames;

		// break;	
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