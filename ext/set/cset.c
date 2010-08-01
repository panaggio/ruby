#include <ruby.h>

VALUE rb_cSet;

/*
 *  Document-class: Set
 *
 *  TODO: copy set description from lib/set.rb
 *
 *  Example:
 *
 *    TODO: copy set example from lib/set.rb
 */

/* TODO: create Set struct */
typedef struct {
} Set;

/* TODO: implement */
static void
set_mark(void *ptr)
{
}

/* TODO: implement */
static void
set_free(void *ptr)
{
}

/* TODO: implement */
static void
set_memsize(const void *ptr)
{
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
    if (/* TODO */) {
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
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new set.
 */

static VALUE
rb_set_initialize(VALUE self)
{
    Set *set;
    GetSetPtr(self, set);

    set_initialize(set);

    return self;
}


void
Init_cset(void)
{
    rb_define_alloc_func(rb_cSet, set_alloc);
    rb_define_method(rb_cSet, "initialize", rb_set_initialize, 0);

    rb_provide("set.rb");
}
