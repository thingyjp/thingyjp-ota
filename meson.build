project('ota', 'c')

ota_src = ['ota.c', 'crypto.c', 'utils.c', 'manifest.c', 'mtd.c']
stamp_src = ['stamp.c', 'manifest.c', 'utils.c']
repo_src = ['repo.c', 'crypto.c', 'utils.c', 'manifest.c']
keygen_src = ['keygen.c', 'crypto.c', 'utils.c']

incs = include_directories(['json-glib-macros'])
  
cc = meson.get_compiler('c')

add_global_arguments('-ggdb', language : 'c')
add_global_arguments('-Werror=implicit-function-declaration', language : 'c')

if get_option('host')
 host_deps = [dependency('glib-2.0'),
        dependency('gio-unix-2.0'),
        dependency('json-glib-1.0'),
        dependency('nettle'),
        dependency('hogweed'),
        cc.find_library('gmp')]
 executable('ota_stamp', stamp_src, include_directories : incs, dependencies : host_deps, install : true)
 executable('ota_repo', repo_src, include_directories : incs, dependencies : host_deps, install : true)
 executable('ota_keygen', keygen_src, include_directories : incs, dependencies : host_deps, install : true)
else

 thingymcconfig_dep = dependency('libthingymcconfig_client_glib', required : false)
 if not thingymcconfig_dep.found()
  thingymcconfig = subproject('thingymcconfig')
  thingymcconfig_dep = thingymcconfig.get_variable('thingymcconfig_dep')
 endif  

 teenynet_dep = dependency('libteenynet', required : false)
 if not teenynet_dep.found()
  teenynet = subproject('teenynet')
  teenynet_dep = teenynet.get_variable('teenynet_dep')
 endif  

 target_deps = [thingymcconfig_dep,
                teenynet_dep,
                dependency('glib-2.0'),
                dependency('gio-unix-2.0'),
                dependency('json-glib-1.0'),
                dependency('nettle'),
                dependency('hogweed'),
                cc.find_library('gmp')]
 executable('ota', ota_src, include_directories : incs, dependencies : target_deps, install : true, install_dir : 'sbin')
endif