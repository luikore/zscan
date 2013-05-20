require "erb"

class Generate
  def self.files
    %w[ext/bspec_exec.inc ext/bspec_init.inc lib/zscan/bspec.rb]
  end

  def self.generate file
    new(file).render
  end

  def initialize file
    case file
    when 'ext/bspec_exec.inc'
      generate_exec
    when 'ext/bspec_init.inc'
      generate_init
    when 'lib/zscan/bspec.rb'
      generate_rb
    else
      raise "unkown file: #{file}"
    end
    @file = file
  end

  def render
    ERB.new(File.read File.join(__dir__, File.basename(@file))).result binding
  end

  def generate_init
    @incrs = insns.map{|ins| incr ins}.join ', '
    @insns_size = insns.size
  end

  def generate_exec
    @opcode_list = insns.map{|ins| "&&BS_#{ins}" }
    @conv_insns = insns.select{|ins| ins !~ /RET|EXPECT/ }
  end

  def generate_rb
    groups = insns.each_with_index.to_a.group_by{|x|
      case x.first
      when /INT/
        :int
      when /RET|EXPECT/
        :misc
      else
        :float
      end
    }
    @int_ins = groups[:int]
    @float_ins = groups[:float]
    @alias_ins = swap_types.map &:downcase
  end

  def swap_types
    %w[INT16 INT32 INT64 UINT16 UINT32 UINT64 SINGLE DOUBLE]
  end

  def insns
    ['RET', 'EXPECT8', 'EXPECT16', 'EXPECT32', 'EXPECT64', 'INT8', 'UINT8', *swap_types.flat_map{|ty| [ty, "#{ty}_SWAP"] }]
  end

  def to_pack_format ins
    raise "bad" if ins !~ /INT/
    base = {
      '8' => 'c',
      '16' => 's',
      '32' => 'l',
      '64' => 'q'
    }[ins[/\d+/]]
    if ins.start_with?('U')
      base.upcase!
    end

    if ins[/\d+/] == '8'
      return "'#{base}'"
    end

    if ins.end_with?('SWAP')
      format = base + '<'
      swap_format = base + '>'
    else
      format = base + '>'
      swap_format = base + '<'
    end
    "(BSpec.big_endian? ? '#{format}' : '#{swap_format}')"
  end

  def to_expect_i ins
    bits = ins[/\d+/]
    insns.index "EXPECT#{bits}"
  end

  # following methods used in C-code gen

  def incr ins
    case ins
    when /INT(\d+)/; $1.to_i / 8
    when /SINGLE/; 4
    when /DOUBLE/; 8
    when /RET|EXPECT/; 0
    else; raise 'bad'
    end
  end

  def c_type ins
    case ins
    when /(U?INT\d+)/; "#{$1.downcase}_t"
    when /SINGLE/; 'float'
    when /DOUBLE/; 'double'
    else; raise 'bad'
    end
  end

  def extract ins
    type = c_type ins
    len = incr(ins) * 8
    r = "((uint#{len}_t*)s)[0]"
    if ins.end_with?('SWAP')
      r = "swap#{len}(#{r})"
    end
    "uint#{len}_t r = #{r}"
  end

  def convert ins
    case ins
    when /(U)?INT64|UINT32/
      if ins.start_with?('U')
        "UINT64toNUM(r)"
      else
        "INT64toNUM(CAST(r, int64_t))"
      end
    when /INT32/
      "INT2NUM(CAST(r, int32_t))"
    when /INT(16|8)/
      "INT2FIX(CAST(r, #{c_type ins}))"
    when /SINGLE/
      "DBL2NUM((double)CAST(r, float))"
    when /DOUBLE/
      "DBL2NUM(CAST(r, double))"
    else
      raise 'bad'
    end
  end

end
