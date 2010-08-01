#include <ruby.h>

VALUE rb_cSet;

/*
 *  Document-class: Set
 *
 *  Set implements a collection of unordered values with no duplicates.
 *  This is a hybrid of Array's intuitive inter-operation facilities and
 *  Hash's fast lookup.
 *
 *  Example:
 *
 *    require 'set'
 *    s1 = Set.new [1, 2]                   # -> #<Set: {1, 2}>
 *    s2 = [1, 2].to_set                    # -> #<Set: {1, 2}>
 *    s1 == s2                              # -> true
 *    s1.add("foo")                         # -> #<Set: {1, 2, "foo"}>
 *    s1.merge([2, 6])                      # -> #<Set: {6, 1, 2, "foo"}>
 *    s1.subset? s2                         # -> false
 *    s2.subset? s1                         # -> true
 */

typedef struct {
    VALUE hash;
} Set;

static void
set_mark(void *ptr)
{
    Set *set = ptr;
    rb_gc_mark(set->hash);
}

#define set_free RUBY_TYPED_DEFAULT_FREE

static void
set_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
        const Set *set = ptr;
        size = sizeof(Set);
        /* TODO: check a way to calculate hash memsize */
        /* size += rb_hash_memsize(set->hash); */
    }
    return size;
}

static const rb_data_type_t set_data_type = {
    "set",
    {set_mark, set_free, set_memsize,},
};

#define GetSetPtr(obj, tobj) \
    TypedData_Get_Struct(obj, Set, &set_data_type, tobj)

static Set *
get_set_ptr(VALUE self)
{
    Set *set;
    GetSetPtr(self, set);
    if (!set->hash) {
       rb_raise(rb_eArgError, "uninitialized Set");
    }
    return set;
}

static VALUE
set_alloc(VALUE klass)
{
    Set *set;
    return TypedData_Make_Struct(klass, Set, &set_data_type, set);
}

/*TODO: implement*/
static void
set_initialize(Set *set)
{
    set->hash = rb_hash_new();
}

static VALUE
rb_set_initialize0(VALUE self)
{
    Set *set;
    GetSetPtr(self, set);

    set_initialize(set);

    return self;
}

static
do_with_enum()
{
    rb_intern(const char *name)
    if (rb_respond_to(self, rb_intern("each_entry")))
        /*FIXME: enum.each_entry(&block)*/
    else if (rb_respond_to(self, rb_intern("each")))
        /*FIXME: enum.each_entry(&block)*/
    else
        rb_raise(rb_eArgError, "value must be enumerable");
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new set.
 */
static VALUE
rb_set_initialize(int argc, VALUE *argv, VALUE self)
{
    Set *set;
    GetSetPtr(self, set);

    switch (argc) {
      case 0:
       set->hash = rb_hash_new();
       break;
      case 1:
       /* FIXME:
       if (block?)
           do_with_enum(enum) { |o| add(block[o]) }
       else
           merge(enum)
       end
       */
       break;
      default:
       rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
    }
    return self;
}

/*
* Creates a new set containing the given objects.
*/

static VALUE
rb_set_s_create(int argc, VALUE *argv, VALUE klass)
{
/* FIXME
    return set;
 */
}


void
Init_cset(void)
{
    rb_define_alloc_func(rb_cSet, set_alloc);
    rb_define_method(rb_cSet, "initialize", rb_set_initialize, 0);

    rb_provide("set.rb");
}
