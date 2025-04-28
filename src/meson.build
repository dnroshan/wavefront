wavefront_sources = [
  'main.c',
  'wf-application.c',
  'wf-window.c',
  'wf-player.c',
]

wavefront_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('gstreamer-1.0'),
]

wavefront_sources += gnome.compile_resources('wavefront-resources',
  'resources/wavefront.gresource.xml',
  source_dir: 'resources',
  c_name: 'wavefront'
)

executable('wavefront', wavefront_sources,
  dependencies: wavefront_deps,
       install: true,
)
