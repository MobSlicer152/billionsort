add_rules(
	"mode.debug",
	"mode.release",
	"plugin.vsxmake.autoupdate"
)

set_project("sort")
set_version("0.0.0", { build = "%Y%m%d%H%M%S" })

set_languages("gnuxx20")
set_warnings("all")

add_includedirs("$(scriptdir)")
add_defines("_CRT_SECURE_NO_WARNINGS")

if is_mode("debug") then
	add_defines("DEBUG")
else
	add_defines("RELEASE")
end

add_requires("libsdl3")

target("sort")
	set_kind("binary")
	add_files("sort.cpp")
	add_packages("libsdl3")
target_end()

