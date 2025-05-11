wavefront_sources = [
  'main.c',
  'wf-application.c',
  'wf-window.c',
  'wf-player.c',
  'wf-waveform.c',
  'wf-seek-bar.c',
]

wavefront_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('gstreamer-1.0'),
  dependency('gstreamer-play-1.0'),
  cc.find_library('m', required: true),
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
