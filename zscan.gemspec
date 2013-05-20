Gem::Specification.new do |s|
  s.name = "zscan"
  s.version = "2.0" # version mapped from zscan.rb, don't change here
  s.author = "Zete Lui"
  s.homepage = "https://github.com/luikore/zscan"
  s.platform = Gem::Platform::RUBY
  s.summary = "improved string scanner"
  s.description = "improved string scanner, respects anchors and lookbehinds, supports codepoint positioning"
  s.required_ruby_version = ">=1.9.2"
  s.licenses = ['BSD']

  s.files = Dir.glob('{rakefile,zscan.gemspec,readme.md,**/*.{rb,h,c,inc}},ext/pack/COPYING*')
  s.require_paths = ["lib"]
  s.extensions = ["ext/extconf.rb"]
  s.rubygems_version = '1.8.24'
  s.has_rdoc = false
end
