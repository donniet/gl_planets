cmake_minimum_required(VERSION 3.14)

set(BOOST_VERSION 1.71.0)

project("gl_planets" VERSION 0.1 LANGUAGES C CXX)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(GLUT REQUIRED)
find_package(Boost ${BOOST_VERSION} COMPONENTS program_options REQUIRED )

add_executable(gl_planets src/main.cpp)

target_include_directories(gl_planets PRIVATE ${Boost_INCLUDE_DIR} ${GLUT_INCLUDE_DIRS})
target_link_libraries(gl_planets GLEW OpenGL::GL ${Boost_LIBRARIES} ${GLUT_LIBRARIES})

target_compile_features(gl_planets PRIVATE cxx_std_17)


