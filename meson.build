project('ota', 'c')

ota_src = ['ota.c', 'teenyhttp.c', 'crypto.c', 'utils.c', 'manifest.c']
repo_src = ['repo.c', 'crypto.c', 'utils.c', 'manifest.c']
keygen_src = ['keygen.c', 'crypto.c', 'utils.c']

incs = include_directories(['json-glib-macros'])
  
cc = meson.get_compiler('c')

deps = [dependency('glib-2.0'),
        dependency('gio-unix-2.0'),
        dependency('json-glib-1.0'),
        dependency('nettle'),
        dependency('hogweed'),
        cc.find_library('gmp')]

add_global_arguments('-ggdb', language : 'c')
add_global_arguments('-Werror=implicit-function-declaration', language : 'c')

executable('ota', ota_src, include_directories : incs, dependencies : deps)
executable('repo', repo_src, include_directories : incs, dependencies : deps)
executable('keygen', keygen_src, include_directories : incs, dependencies : deps)