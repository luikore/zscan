require "mkmf"
require "fileutils"

if RbConfig::MAKEFILE_CONFIG['CC'] !~ /clang/
  $CFLAGS << ' -std=c99 -Wno-declaration-after-statement -Wno-strict-aliasing'
end

create_makefile 'zscan'

makefile = File.read 'Makefile'

if RUBY_VERSION >= '2.6'
  FileUtils.cp "pack/internal-26.h", "pack/internal.h"
  FileUtils.cp "pack/pack-26.c", "pack/pack.c"
elsif RUBY_VERSION > '2.4'
  FileUtils.cp "pack/internal-25.h", "pack/internal.h"
  FileUtils.cp "pack/pack-25.c", "pack/pack.c"
else
  FileUtils.cp "pack/internal-23.h", "pack/internal.h"
  FileUtils.cp "pack/pack-23.c", "pack/pack.c"
end

lines = makefile.lines.map do |line|
  if line.start_with?('ORIG_SRCS =')
    line.sub /$/, " pack/pack.c"
  elsif line.start_with?('OBJS =')
    line.sub /$/, " pack/pack.o"
  else
    line
  end
end
headers = Dir.glob('**/*.h').join ' '
File.open 'Makefile', 'w' do |f|
  f.puts lines
  f.puts "\n$(OBJS): #{headers}"
end
