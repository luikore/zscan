require "mkmf"
require "fileutils"

if RUBY_PLATFORM !~ /linux/
  Dir.chdir File.dirname(__FILE__) do
    FileUtils.cp 'fmemopen/fmemopen.*', '.'
  end
end

create_makefile 'zscan'
