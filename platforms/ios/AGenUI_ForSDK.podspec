Pod::Spec.new do |s|
  s.name             = 'AGenUI'
  s.version          = '1.1.0'
  s.summary          = 'A Native Renderer for A2UI.'
  s.description      = 'A Native Renderer for A2UI.'
  s.homepage         = 'https://genui.amap.com'
  s.author           = 'AGenUI'
  s.license          = { :type => 'Apache License, Version 2.0', :file => 'LICENSE' }
  s.source           = { :git => 'https://github.com/AGenUI/AGenUI.git', :tag => s.version.to_s }
  s.swift_version         = '5.0'
  s.ios.deployment_target = '13.0'

  # Force static framework build
  s.static_framework = true

  # C++ dependency config - core/ directory in monorepo is linked via prepare_command hard-link tree; symlinks are not recognized by the project
  s.prepare_command = <<-CMD.gsub(/^\s{4}/, '')
    set -e
    if [ ! -d "vendor/yoga" ]; then
      mkdir -p vendor
      curl -L https://github.com/facebook/yoga/archive/refs/tags/v2.0.0.tar.gz | tar xz -C vendor
      mv vendor/yoga-2.0.0 vendor/yoga
    fi
    ENGINE_REL="../../core"
    if [ ! -d "$ENGINE_REL" ]; then
      echo "[AGenUI.podspec] ERROR: core directory not found: $(pwd)/$ENGINE_REL" >&2
      exit 1
    fi
    rm -rf core
    if cp -al "$ENGINE_REL" core 2>/dev/null; then
      echo "[AGenUI.podspec] Created hard-link tree core <-> $ENGINE_REL (shared inode)"
    else
      cp -R "$ENGINE_REL" core
      echo "[AGenUI.podspec] WARN: cp -al failed, fell back to regular copy"
    fi
  CMD

  s.source_files = 'AGenUI/Classes/**/*'
  
  # Only expose pure Objective-C headers to Swift, exclude C++ headers
  s.public_header_files = 'AGenUI/Classes/**/*.h'
     
  # C++ dependency config - fetch from remote git repository
  s.subspec 'core' do |cpp|
    # C++ source files - exclude headers from source_files to avoid umbrella header inclusion
    cpp.source_files = [
      'core/src/**/*.{cpp,cc,c}',
      'vendor/yoga/yoga/**/*.{cpp,cc,c}'
    ]
      
    # Only preserve header paths, not as public headers
    cpp.preserve_paths = 'core/**/*'
      
    # Header search paths - use correct CocoaPods paths
    # Must include src directory and subdirectories, as C++ code uses relative paths like #include "core/subscriber.h"
    # Only keep subspec-specific settings; Debug/Release optimization settings live in s.pod_target_xcconfig to avoid duplication that triggers CocoaPods merge conflicts
    cpp.xcconfig = {
      'HEADER_SEARCH_PATHS' => '"${PODS_TARGET_SRCROOT}/core/include" "${PODS_TARGET_SRCROOT}/core/src" "${PODS_TARGET_SRCROOT}/core/src/**" "${PODS_TARGET_SRCROOT}/vendor/yoga"',
      'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
      'CLANG_CXX_LIBRARY' => 'libc++',
      'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) COCOAPODS=1',

      # Disable C++ RTTI (Run-Time Type Information)
      'GCC_ENABLE_CPP_RTTI' => 'NO'
    }
    
    # Exclude JNI-related files (Android only)
    cpp.exclude_files = [
      'core/src/jni/**/*',
      'core/src/third_party/ik/**/*',
      'core/src/third_party/sax/**/*',
      'core/src/third_party/Html.cpp',
      'core/src/third_party/key_define.cpp'
    ]
  end

  # Build configuration specific compile config
  s.pod_target_xcconfig = {
    # Mark ForSDK build for conditional compilation to retain full components
    'SWIFT_ACTIVE_COMPILATION_CONDITIONS' => '$(inherited) AGENUI_SDK_BUILD',
    # Debug config: no optimization, preserve debug info, no strip
    'GCC_OPTIMIZATION_LEVEL[config=Debug]' => '0',
    'SWIFT_OPTIMIZATION_LEVEL[config=Debug]' => '-Onone',
    'SWIFT_COMPILATION_MODE[config=Debug]' => 'singlefile',
    'LLVM_LTO[config=Debug]' => 'YES',
    'DEAD_CODE_STRIPPING[config=Debug]' => 'YES',
    'STRIP_INSTALLED_PRODUCT[config=Debug]' => 'NO',
    'STRIP_STYLE[config=Debug]' => 'debugging',
    'GCC_SYMBOLS_PRIVATE_EXTERN[config=Debug]' => 'NO',
    'DEPLOYMENT_POSTPROCESSING[config=Debug]' => 'NO',

    # Release config: binary size optimization
    'GCC_OPTIMIZATION_LEVEL[config=Release]' => 'z',
    'SWIFT_OPTIMIZATION_LEVEL[config=Release]' => '-Osize',
    'SWIFT_COMPILATION_MODE[config=Release]' => 'wholemodule',
    'LLVM_LTO[config=Release]' => 'YES',
    'DEAD_CODE_STRIPPING[config=Release]' => 'YES',
    'STRIP_INSTALLED_PRODUCT[config=Release]' => 'YES',
    'STRIP_STYLE[config=Release]' => 'all',
    'GCC_SYMBOLS_PRIVATE_EXTERN[config=Release]' => 'YES',
    'DEPLOYMENT_POSTPROCESSING[config=Release]' => 'YES'
  }
  
  # Include C++ subspec by default
  s.default_subspecs = 'core'
  
  # Resource config - include the entire bundle directly
  s.resources = 'AGenUI/Assets/AGenUI.bundle'

end
