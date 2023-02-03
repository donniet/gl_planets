

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
	string texture_path = "../img/io-2.jpg";
	string starfield_path = "../img/TychoSkymapII.t3_04096x02048.jpg";
	float fieldOfView = 90., near = 1., far = 10.;

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

	GLint maxTextureUnits;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

	printf("OpenGL info:\n"
	       "\tVendor   = \"%s\"\n"
	       "\tRenderer = \"%s\"\n"
	       "\tVersion  = \"%s\"\n"
		   "\nMax Texture Units = %d\n",
	       glGetString(GL_VENDOR),
	       glGetString(GL_RENDERER),
	       glGetString(GL_VERSION),
		   maxTextureUnits);

	// return 0;

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

	Program program;
	bool success;
	tie(program, success) = Program::from_shader_files(vertex_shader, fragment_shader);
	if(!success) {
		std::cerr << "error making program" << std::endl;
	}

#ifdef DEBUG
	// During init, enable debug output
	glEnable( GL_DEBUG_OUTPUT );
	// glDebugMessageControl(GL_DEBUG_SOURCE_API​, 0​, GL_DEBUG_SEVERITY_NOTIFICATION​, 0​, nullptr​, GL_TRUE​);
	glDebugMessageCallback( MessageCallback, 0 );
#endif

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
	GLuint corners_buffer_old;	

	float corners[] = {
		-1, -1,
		1, -1,
		1, 1,
		-1, 1
	};

	glm::mat4 mv;
	glm::vec3 camera;
	glm::vec3 sun;

	ArrayBuffer<float,2> corners_buffer(corners);
	UniformMatrix<float,4> inverse_transform(mv);
	Uniform<float,3> camera_position(camera);
	Uniform<float,3> sun_position(sun);
	
	auto drawer = program.make_drawer()
		("camera", camera_position )
		("texture", io_texture )
		("starfield", star_texture )
		("sun", sun_position )
		("inv", inverse_transform )
		("corner", corners_buffer )
	;

	while (!glfwWindowShouldClose(window))
	{
		/* Process window events */
		glfwPollEvents();
		
		/* Clear the framebuffer to black */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 m = glm::identity<glm::mat4>();
		glm::mat4 view = glm::identity<glm::mat4>();

		view = glm::translate(view, glm::vec3(0, 0, -1.5));
		view = glm::rotate(view, (float)time_now / (float)43., glm::vec3(0, 0.5, 0.75));
        // view = glm::rotate(view, (float)time_now / (float)65., glm::vec3(0, 0, 1));

		mv = projection * view * m;
		mv = glm::inverse(mv);
		
		glm::mat4 inv = glm::inverse(view);
		camera = inv * glm::vec4(0, 0, 0, 1);

		// cout << "camera: " << camera.x << " " << camera.y << " " << camera.z << " " << camera.w << endl;

		sun = glm::vec3(
			glm::cos((float)time_now / (float)600.), 
			0.,
			-glm::sin((float)time_now / (float)600.));

		drawer.draw_arrays_triangle_fan();
		
	
		/* Display framebuffer */
		glfwSwapBuffers(window);

		
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