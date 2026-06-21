project('mate-paint-pro', 'cpp',
  version: '1.0.0',
  default_options: [
    'cpp_std=c++11',
    'datadir=share',
  ]
)

i18n = import('i18n')

gtk_dep = dependency('gtk+-3.0')

icon_install_dir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())

sources = files(
  'src/main.cpp',
  'src/utils.cpp',
  'src/selection.cpp',
  'src/drawing.cpp',
  'src/rendering.cpp',
  'src/events.cpp',
  'src/layers.cpp',
  'src/undo.cpp',
  'src/filters.cpp',
  'src/file_io.cpp',
  'src/image_ops.cpp',
  'src/text_tool.cpp',
  'src/palette.cpp',
  'src/ui_widgets.cpp',
  'src/menus.cpp',
)

executable('mate-paint-pro',
  sources,
  dependencies: gtk_dep,
  include_directories: include_directories('src'),
  cpp_args: [
    '-DICON_INSTALL_DIR="' + icon_install_dir + '"',
    '-DGETTEXT_PACKAGE="mate-paint"',
    '-DLOCALEDIR="' + join_paths(get_option('prefix'), get_option('localedir')) + '"',
  ],
  install: true
)

install_subdir('data', install_dir: icon_install_dir)

install_data('mate-paint-pro.desktop',
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'applications')
)

install_subdir('help',
  install_dir: join_paths(get_option('datadir'))
)

subdir('po')
