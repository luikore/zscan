Gem::Specification.new do |s|
  s.name = "zscan"
  s.version = "0.5"
  s.author = "Zete Lui"
  s.homepage = "https://github.com/luikore/zscan"
  s.platform = Gem::Platform::RUBY
  s.summary = "improved string scanner"
  s.description = "improved string scanner"
  s.required_ruby_version = ">=1.9.2"

  s.files = %w"readme.md lib/zscan.rb ext/zscan.c ext/extconf.rb zscan.gemspec"
  s.require_paths = ["lib"]
  s.extensions = ["ext/extconf.rb"]
  s.rubygems_version = '1.8.24'
  s.has_rdoc = false
end
