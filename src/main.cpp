

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


struct Transform {
    glm::mat4 m;
    glm::mat4 mInv;
};


class Orbit {
private:
    float radius_;
    glm::mat4 translate_;

public:
    Orbit(float radius) : radius_(radius) {
        translate_ = glm::translate(glm::identity<glm::mat4>(), glm::vec3(radius, 0.0f, 0.0f));    
    }

    glm::mat4 operator()(float distance) const {
        return glm::rotate(translate_, glm::radians(distance / radius_), glm::vec3(0.0f, 0.0f, 1.0f));
    }
};


#if 0


class Orbit {
public:
    float orbital_radius; /* km */
    float orbital_speed; /* km/s */
    float rotation_period; /* s */
    float axial_tilt; /* radians */

    
public:
    glm::mat4 transform(float t) {
        float orbital_argument = t * orbital_speed / orbital_radius;
        float rotation_argument = t / rotation_period;

        glm::mat4 ret = glm::identity<glm::mat4>();

        // planet rotation
        ret = glm::rotate(ret, rotation_argument, glm::vec3(0, 1, 0));
        // axial tilt
        ret = glm::rotate(ret, axial_tilt, glm::vec3(0, 0, 1));
        // planet translation
        ret = glm::translate(ret, glm::vec3(orbital_radius, 0, 0));
        // orbital rotation
        ret = glm::rotate(ret, orbital_argument, glm::vec3(0, 1, 0));

        return ret;
    }
};

class Scene {
private:
    float t; // seconds
    float fov; // radians

    glm::vec3 sun_position;
    glm::vec3 jupiter_position;
    float jupiter_rotation;
    float jupiter_rotation_period;
    float jupiter_radius;
    float jupiter_orbital_radius;
    float jupiter_orbital_velocity;
    glm::vec3 io_position;
    float io_radius;
    float io_orbital_radius;

    glm::vec3 camera_position;
    float camera_orbit_radius;
    
public:
    Scene() 
        : t(0.0f), fov(0.0f), sun_position(0.0f, 0.0f, 0.0f), 
          jupiter_position(0.0f, 0.0f, 0.0f), jupiter_radius(69911 /* km */), 
          io_position(0.0f, 0.0f, 0.0f), io_radius(1821 /* km */), io_orbital_radius(0.0f), 
          camera_position(0.0f, 0.0f, 0.0f), camera_orbit_radius(0.0f),
          jupiter_rotation(0.0f), jupiter_rotation_period(2. * M_PI / 10. * 60. * 60.)
    { }

    void set_time(float t) 
    {
        this->t = t;
        
        jupiter_transform = Orbit{jupiter_orbital_radius, jupiter_orbital_velocity, }
        update_sun_position();
        update_jupiter_position();
        update_io_position();
        update_camera_position();
    }

    void update_sun_position() {
        // sun stays at origin
    }

    void update_jupiter_position()
    {
        jupiter_position = Orbit{jupiter_orbital_radius, jupiter_orbital_velocity}.position(t);
        jupiter_rotation = jupiter_rotation_speed * t;

    }
};

#endif


int main(int ac, char * av[]) {
	string vertex_shader = "../shaders/sphere.vert";
	string fragment_shader = "../shaders/sphere.frag";
	string texture_path = "../img/io-2.jpg";
	string starfield_path = "../img/TychoSkymapII.t3_04096x02048.jpg";
    string dem_path = "../img/io_dem_4096x2048.png";
    string normal_path = "../img/io_normal_4096x2048.jpg";
	float fieldOfView = 75., near = 45., far = 1000.;

	options_description desc("options");
	desc.add_options()
		("help,h", "prints this message")
		("vertex,v", value(&vertex_shader), "vertex shader path")
		("fragment,f", value(&fragment_shader), "fragment shader path")
		("fov", value(&fieldOfView), "field of view in degrees")
		("texture,t", value(&texture_path), "path to texture of planet")
		("starfield,s", value(&starfield_path), "path to starfield spheremap")
        ("dem_path", value(&dem_path), "path to DEM")
        ("normal_path", value(&normal_path), "path to normal map")
	;
	variables_map vm;
	store(parse_command_line(ac, av, desc), vm);

	if(vm.count("help")) {
		cout << desc << "\n";
		return 0;
	}

    notify(vm);


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


	// Texture io_texture(texture_path);
	Texture star_texture(starfield_path);
    Texture dem_texture(dem_path);
    // Texture normal_texture(normal_path);

	// if(!io_texture) {
	// 	cerr << "unable to load texture '" << texture_path << "'\n";
	// 	return -1;
	// }
	if(!star_texture) {
		cerr << "unable to load starfield '" << starfield_path << "'\n";
		return -1;
	}

	Program program;
	bool success;
	tie(program, success) = Program::from_shader_files(vertex_shader, fragment_shader, {
        "GL_EXT_texture_array"
    }, {
        { "PLANETS", "2" }
    });
	if(!success) {
		std::cerr << "error making program" << std::endl;
        std::cerr << "vertex log: " << program.vertex_info_log() << std::endl;
        std::cerr << "fragment log: " << program.fragment_info_log() << std::endl;
        return -1;
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


    auto camera_orbit = Orbit(10. /* km */);
    auto io_orbit = Orbit(1821. /* km */);
    auto jupiter_orbit = Orbit(740000. /* km */);



	glm::mat4 mv;
	glm::vec3 camera;
	glm::vec3 sun;
    glm::vec3 position[] = { glm::vec3(50,0,0), glm::vec3(0,0,0) };
    float radius[] = { 35., 5. }; // km
    std::array<string,2> texture_paths = { "../img/20180511_jupiter_map_css_plus_juno_bj.jpg", texture_path };
    std::array<string,2> norm_paths = { "../img/io_normal_4096x2048.jpg", "../img/io_normal_4096x2048.jpg" };

	ArrayBuffer<float,2> corners_buffer(corners);
	UniformMatrix<float,4> inverse_transform(mv);
	Uniform<float,3> camera_position(camera);
	Uniform<float,3> sun_position(sun);
    UniformArray<float,3> planet_position(position, 2);
    UniformArray<float,1> planet_radius(radius, 2);
    TextureArray planet_textures(texture_paths);
    TextureArray planet_normals(norm_paths);
	
	auto drawer = program.make_drawer()
		("camera", camera_position )
		("texture", planet_textures )
        ("norm", planet_normals )
        ("dem", dem_texture )
		("starfield", star_texture )
		("sun", sun_position )
		("inv", inverse_transform )
		("corner", corners_buffer )
        ("radius", planet_radius )
        ("position", planet_position )
	;

	while (!glfwWindowShouldClose(window))
	{
		/* Process window events */
		glfwPollEvents();
		
		/* Clear the framebuffer to black */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 m = glm::identity<glm::mat4>();
		glm::mat4 view = glm::identity<glm::mat4>();

		view = glm::translate(view, glm::vec3(0, 0, -10.));
		view = glm::rotate(view, (float)time_now / (float)400., glm::vec3(0, 0.5, 0));
        // view = glm::rotate(view, (float)time_now / (float)65., glm::vec3(0, 0, 1));

		mv = projection * view * m;
		mv = glm::inverse(mv);
		
		glm::mat4 inv = glm::inverse(view);
		camera = inv * glm::vec4(0, 0, 0, 1);

		// cout << "camera: " << camera.x << " " << camera.y << " " << camera.z << " " << camera.w << endl;

		sun = glm::vec3(
			glm::cos((float)time_now / (float)60.), 
			0.,
			-glm::sin((float)time_now / (float)60.));

		drawer.draw_arrays_triangle_fan();
		
	
		/* Display framebuffer */
		glfwSwapBuffers(window);

		
		/* Update fps counter */
		time_now = glfwGetTime();
		// printf("%.1fms\n", (time_now - time_of_last_swap) * 1.0e3);
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
