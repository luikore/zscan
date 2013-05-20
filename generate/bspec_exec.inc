__attribute__((__noinline__))
static VALUE bspec_exec(void** ip, char* s, VALUE a) {
  static void* opcodes[] = { <%= @opcode_list.join ', ' %> };
  if (ip == NULL) {
    return (VALUE)opcodes;
  }
  goto **(ip++);
BS_RET:
  return a;
<% [8, 16, 32, 64].each do |len| %>
BS_EXPECT<%= len %>:
  {
    char* expect = (char*)(ip++);
    if (strncmp(s, expect, <%= len / 8 %>)) {
      return Qnil;
    }
    goto **(ip++);
  }
<% end %>
<% @conv_insns.each do |ins| %>
BS_<%= ins %>:
  {
    <%= extract ins %>;
    rb_ary_push(a, <%= convert ins %>);
    s += <%= incr ins %>;
    goto **(ip++);
  }
<% end %>
}
