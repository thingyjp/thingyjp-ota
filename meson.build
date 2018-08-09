project('ota', 'c')

ota_src = ['ota.c', 'teenyhttp.c', 'crypto.c', 'utils.c', 'manifest.c', 'mtd.c']
stamp_src = ['stamp.c', 'manifest.c', 'utils.c']
repo_src = ['repo.c', 'crypto.c', 'utils.c', 'manifest.c']
keygen_src = ['keygen.c', 'crypto.c', 'utils.c']

incs = include_directories(['json-glib-macros'])
  
cc = meson.get_compiler('c')

deps = [dependency('libthingymcconfig_client_glib'),
        dependency('glib-2.0'),
        dependency('gio-unix-2.0'),
        dependency('json-glib-1.0'),
        dependency('nettle'),
        dependency('hogweed'),
        cc.find_library('gmp')]

add_global_arguments('-ggdb', language : 'c')
add_global_arguments('-Werror=implicit-function-declaration', language : 'c')

if get_option('host')
 executable('ota_stamp', stamp_src, include_directories : incs, dependencies : deps, install : true)
 executable('ota_repo', repo_src, include_directories : incs, dependencies : deps, install : true)
 executable('ota_keygen', keygen_src, include_directories : incs, dependencies : deps, install : true)
else
 executable('ota', ota_src, include_directories : incs, dependencies : deps, install : true, install_dir : 'sbin')
endif