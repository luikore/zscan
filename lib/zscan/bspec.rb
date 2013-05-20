# generated by rake gen
class ZScan
  class BSpec
    def _append_expect expect_i, pack_format, opts
      if opts.is_a?(Hash)
        expect = opts[:expect]
        if expect.is_a?(Integer) and opts.size == 1
          ZScan::BSpec._append self, expect_i
          ZScan::BSpec._append_expect self, [expect].pack(pack_format)
        else
          raise ArgumentError, "only :expect option allowed, but got #{opts.inspect}"
        end
      end
    end

    def initialize &p
      instance_eval &p
    end

    
    def int8 opts=nil
      _append_expect 1, 'c', opts
      ZScan::BSpec._append self, 5
    end
    
    def uint8 opts=nil
      _append_expect 1, 'C', opts
      ZScan::BSpec._append self, 6
    end
    
    def int16 opts=nil
      _append_expect 2, (BSpec.big_endian? ? 's>' : 's<'), opts
      ZScan::BSpec._append self, 7
    end
    
    def int16_swap opts=nil
      _append_expect 2, (BSpec.big_endian? ? 's<' : 's>'), opts
      ZScan::BSpec._append self, 8
    end
    
    def int32 opts=nil
      _append_expect 3, (BSpec.big_endian? ? 'l>' : 'l<'), opts
      ZScan::BSpec._append self, 9
    end
    
    def int32_swap opts=nil
      _append_expect 3, (BSpec.big_endian? ? 'l<' : 'l>'), opts
      ZScan::BSpec._append self, 10
    end
    
    def int64 opts=nil
      _append_expect 4, (BSpec.big_endian? ? 'q>' : 'q<'), opts
      ZScan::BSpec._append self, 11
    end
    
    def int64_swap opts=nil
      _append_expect 4, (BSpec.big_endian? ? 'q<' : 'q>'), opts
      ZScan::BSpec._append self, 12
    end
    
    def uint16 opts=nil
      _append_expect 2, (BSpec.big_endian? ? 'S>' : 'S<'), opts
      ZScan::BSpec._append self, 13
    end
    
    def uint16_swap opts=nil
      _append_expect 2, (BSpec.big_endian? ? 'S<' : 'S>'), opts
      ZScan::BSpec._append self, 14
    end
    
    def uint32 opts=nil
      _append_expect 3, (BSpec.big_endian? ? 'L>' : 'L<'), opts
      ZScan::BSpec._append self, 15
    end
    
    def uint32_swap opts=nil
      _append_expect 3, (BSpec.big_endian? ? 'L<' : 'L>'), opts
      ZScan::BSpec._append self, 16
    end
    
    def uint64 opts=nil
      _append_expect 4, (BSpec.big_endian? ? 'Q>' : 'Q<'), opts
      ZScan::BSpec._append self, 17
    end
    
    def uint64_swap opts=nil
      _append_expect 4, (BSpec.big_endian? ? 'Q<' : 'Q>'), opts
      ZScan::BSpec._append self, 18
    end
    

    
    def single
      ZScan::BSpec._append self, 19
    end
    
    def single_swap
      ZScan::BSpec._append self, 20
    end
    
    def double
      ZScan::BSpec._append self, 21
    end
    
    def double_swap
      ZScan::BSpec._append self, 22
    end
    

    if ZScan::BSpec.big_endian?
      
      alias int16_be int16
      alias int16_le int16_swap
      
      alias int32_be int32
      alias int32_le int32_swap
      
      alias int64_be int64
      alias int64_le int64_swap
      
      alias uint16_be uint16
      alias uint16_le uint16_swap
      
      alias uint32_be uint32
      alias uint32_le uint32_swap
      
      alias uint64_be uint64
      alias uint64_le uint64_swap
      
      alias single_be single
      alias single_le single_swap
      
      alias double_be double
      alias double_le double_swap
      
    else
      
      alias int16_le int16
      alias int16_be int16_swap
      
      alias int32_le int32
      alias int32_be int32_swap
      
      alias int64_le int64
      alias int64_be int64_swap
      
      alias uint16_le uint16
      alias uint16_be uint16_swap
      
      alias uint32_le uint32
      alias uint32_be uint32_swap
      
      alias uint64_le uint64
      alias uint64_be uint64_swap
      
      alias single_le single
      alias single_be single_swap
      
      alias double_le double
      alias double_be double_swap
      
    end
    alias byte uint8
    alias float single
    alias float_le single_le
    alias float_be single_be
  end
end