Pod::Spec.new do |s|
  s.name             = 'AGenUI'
  s.version          = '1.1.0'
  s.summary          = 'A Native Renderer for A2UI.'
  s.description      = 'A Native Renderer for A2UI - AGen UI SDK.'
  s.homepage         = 'https://genui.amap.com'
  s.author           = 'AGenUI'
  s.license          = { :type => 'Apache License, Version 2.0', :file => 'LICENSE' }

  # Binary distribution: download pre-built XCFramework + resource Bundle from GitHub Release.
  # The zip is produced by scripts/ios/pack-release.sh and uploaded as a GitHub Release asset
  # tagged AGenUI-<version>. Consumers get a ready-to-use binary — no C++ compilation required.
  s.source           = {
    :http => "https://github.com/AGenUI/AGenUI/releases/download/AGenUI-#{s.version}/AGenUI-#{s.version}-ios.zip",
    :type => :zip
  }

  s.swift_version         = '5.0'
  s.ios.deployment_target = '13.0'

  # Pre-built binary framework (static library, compiled with Release optimizations)
  s.vendored_frameworks = 'AGenUI.xcframework'

  # Resource bundle (included at zip root level by pack-release.sh)
  s.resources = 'AGenUI.bundle'

  # C++ runtime linking: the XCFramework contains compiled C++ code.
  # These settings ensure the consumer's linker resolves C++ standard library symbols correctly,
  # even in pure-Swift projects that have no C++ sources of their own.
  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'GCC_ENABLE_CPP_RTTI' => 'NO'
  }

end
