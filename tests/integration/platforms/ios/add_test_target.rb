#!/usr/bin/env ruby
# encoding: utf-8
# tests/integration/platforms/ios/add_test_target.rb
# Add AGenUITests unit test target to Playground.xcodeproj
# and add Swift source files and test_fixtures resources from AGenUITests/

Encoding.default_external = Encoding::UTF_8
Encoding.default_internal = Encoding::UTF_8

require 'xcodeproj'
require 'pathname'

# __dir__ = tests/integration/platforms/ios/, go back 4 levels to AGenUI root
PLAYGROUND_DIR = File.expand_path('../../../../playground/ios/Playground', __dir__)
PROJECT_PATH = File.join(PLAYGROUND_DIR, 'Playground.xcodeproj')
TEST_DIR = File.join(PLAYGROUND_DIR, 'AGenUITests')

project = Xcodeproj::Project.open(PROJECT_PATH)

# Check if AGenUITests target already exists
existing = project.targets.find { |t| t.name == 'AGenUITests' }
if existing
  puts "AGenUITests target already exists, skipping creation"
  exit 0
end

puts "Creating AGenUITests target..."

# Get main app target
app_target = project.targets.find { |t| t.name == 'Playground' }
abort "Playground target not found" unless app_target

# Create unit test bundle target
test_target = project.new_target(:unit_test_bundle, 'AGenUITests', :ios, '13.0', nil, :swift)
test_target.add_dependency(app_target)

# Set build settings
test_target.build_configurations.each do |config|
  config.build_settings['BUNDLE_LOADER'] = '$(TEST_HOST)'
  config.build_settings['TEST_HOST'] = '$(BUILT_PRODUCTS_DIR)/Playground.app/Playground'
  config.build_settings['PRODUCT_BUNDLE_IDENTIFIER'] = 'com.amap.agenui.AGenUITests'
  config.build_settings['PRODUCT_NAME'] = '$(TARGET_NAME)'
  config.build_settings['PRODUCT_MODULE_NAME'] = 'AGenUITests'
  config.build_settings['SWIFT_VERSION'] = '5.0'
  config.build_settings['CODE_SIGN_STYLE'] = 'Automatic'
  config.build_settings['DEVELOPMENT_TEAM'] = 'DKTPR2P6VZ'
  config.build_settings['INFOPLIST_FILE'] = ''
  config.build_settings['GENERATE_INFOPLIST_FILE'] = 'YES'
  config.build_settings['LD_RUNPATH_SEARCH_PATHS'] = [
    '$(inherited)',
    '@executable_path/Frameworks',
    '@loader_path/Frameworks'
  ]
  # Use AGenUITests Pods xcconfig
  pods_config_name = "Pods-AGenUITests.#{config.name.downcase}.xcconfig"
  pods_config_path = "Target Support Files/Pods-AGenUITests/#{pods_config_name}"
  config_ref = project.files.find { |f| f.path == pods_config_path }
  config.base_configuration_reference = config_ref if config_ref
end

# Create AGenUITests group (under project root)
test_group = project.main_group.new_group('AGenUITests', 'AGenUITests')

# Recursively add Swift source files
def add_files_recursive(group, dir, target, project)
  Dir.entries(dir).sort.each do |entry|
    next if entry.start_with?('.')
    full_path = File.join(dir, entry)
    if File.directory?(full_path)
      sub_group = group.new_group(entry, entry)
      add_files_recursive(sub_group, full_path, target, project)
    elsif entry.end_with?('.swift')
      file_ref = group.new_reference(entry)
      target.source_build_phase.add_file_reference(file_ref)
    end
  end
end

add_files_recursive(test_group, TEST_DIR, test_target, project)

# Add test_fixtures directory as folder reference (resource)
test_fixtures_path = File.join(TEST_DIR, 'test_fixtures')
if File.directory?(test_fixtures_path)
  fixtures_ref = test_group.new_reference('test_fixtures', :group)
  fixtures_ref.last_known_file_type = 'folder'
  target_resources = test_target.resources_build_phase
  target_resources.add_file_reference(fixtures_ref)
end

project.save

puts "AGenUITests target created successfully"
puts "targets: #{project.targets.map(&:name).join(', ')}"
