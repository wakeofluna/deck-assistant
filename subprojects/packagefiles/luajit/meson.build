project('luajit', 'c', version: '2.1')

inc = include_directories('src')

pymod = import('python')
python = pymod.find_installation('python3', required: true)

build_script = files('build.py')
run_command(
    [python, build_script],
    check: true,
)

cc = meson.get_compiler('cpp')
luajit_lib = cc.find_library('lua51', dirs: meson.current_source_dir() / 'src', static: true, required: true)

luajit_dep = declare_dependency(
    include_directories: inc,
    dependencies: luajit_lib,
).as_system('system')

meson.override_dependency('luajit-2.1', luajit_dep)
