-- registers a new command-line option.

newoption 
{
   trigger     = "renderer",
   value       = "API",
   description = "renderer graphics api",
   allowed = 
   {
      { "dx11",    "DirectX 11" },
      { "dx12",    "DirectX 12" },
      { "vulkan",  "Vulkan" }
   }
}

newoption 
{
   trigger     = "sdk_version",
   value       = "version",
   description = "operating system SDK",
}

newoption 
{
   trigger     = "net_lib",
   value       = "dir",
   description = "net io library",
   allowed = 
   {
      { "asio",   "asio" },
      { "zmq",    "zeromq" },
   }
}

newoption 
{
   trigger     = "platform_dir",
   value       = "dir",
   description = "platform specifc src folder",
}

newoption 
{
   trigger     = "env_dir",
   value       = "dir",
   description = "specify location of engine in relation to project"
}

newoption 
{
	trigger = "dynamic_plugins",
	description = "Plugins are dynamic libraries."
}

newoption 
{
	trigger = "no_editor",
	description = "do not build editor."
}

newoption 
{
	trigger = "build_tests",
	description = "build tests projects."
}

newoption 
{
	trigger = "build_app",
	description = "build default app."
}

newoption 
{
   trigger     = "work_dir",
   value       = "dir",
   description = "specify location of engine work"
}