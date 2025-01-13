project(
  'disschat',
  'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++23',
  ]
)

# Add your top-level include directory
inc_dirs = include_directories('include')

#
# Dependencies
#

# 1) ImGui (for the client only)
imgui_dep = dependency('imgui', required : true)

# 2) Boost (Asio, Thread, etc.)
boost_dep = dependency('boost', modules : ['system', 'thread'], required : true)

# 3) Taskflow
#    If you need to treat your 'subprojects/taskflow' as a subproject,
#    you might do: `taskflow_dep = dependency('taskflow', fallback : ['taskflow', 'taskflow_dep'])`
#    If you have only the headers in 'subprojects/taskflow', you might do something like:
taskflow_dep = declare_dependency(
  include_directories: include_directories('subprojects/taskflow')
)

# 4) OpenGL + GLFW for ImGui rendering (client only)
opengl_dep = dependency('opengl', required : true)
glfw_dep   = dependency('glfw3', required : true, method : 'pkg-config')
# If pkg-config fails, you might do `compiler.find_library('glfw', dirs: ...)` as fallback

# 5) macOS frameworks (since you are on macOS)
frameworks = [
  '-framework', 'Cocoa',
  '-framework', 'QuartzCore',
  '-framework', 'OpenGL',
  '-framework', 'IOKit',
  '-framework', 'CoreVideo'
]
framework_dep = declare_dependency(link_args : frameworks)

# Put the client dependencies together
client_deps = [
  imgui_dep,
  boost_dep,
  taskflow_dep,
  opengl_dep,
  glfw_dep,
  framework_dep
]

# The server only needs Boost and Taskflow
server_deps = [
  boost_dep,
  taskflow_dep
]

#
# Build Targets
#

# ------------------ Server Executable ------------------
server_sources = [
  'src/server_main.cpp',
  'src/server.cpp'
]

server_exe = executable(
  'server',
  server_sources,
  include_directories : inc_dirs,
  dependencies : server_deps,
  install : true
)

# ------------------ Client Executable ------------------
client_sources = [
  'src/client_main.cpp',
  'src/client.cpp'
]

client_exe = executable(
  'client',
  client_sources,
  include_directories : inc_dirs,
  dependencies : client_deps,
  install : true
)

#
# Done!
#
message('Configured Meson project with a server and client executable.')
