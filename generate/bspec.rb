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

    <% @int_ins.each do |ins, i| %>
    def <%= ins.downcase %> opts=nil
      _append_expect <%= to_expect_i ins %>, <%= to_pack_format ins %>, opts
      ZScan::BSpec._append self, <%= i %>
    end
    <% end %>

    <% @float_ins.each do |ins, i| %>
    def <%= ins.downcase %>
      ZScan::BSpec._append self, <%= i %>
    end
    <% end %>

    if ZScan::BSpec.big_endian?
      <% @alias_ins.map(&:downcase).each do |ins| %>
      alias <%= ins %>_be <%= ins %>
      alias <%= ins %>_le <%= ins %>_swap
      <% end %>
    else
      <% @alias_ins.map(&:downcase).each do |ins| %>
      alias <%= ins %>_le <%= ins %>
      alias <%= ins %>_be <%= ins %>_swap
      <% end %>
    end
    alias byte uint8
    alias float single
    alias float_le single_le
    alias float_be single_be
  end
end
