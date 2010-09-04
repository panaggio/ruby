#include <ruby.h>

RUBY_EXTERN size_t rb_ary_memsize(VALUE);

VALUE rb_cSet;
VALUE rb_cSortedSet;

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

static size_t
set_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
        /* const Set *set = ptr; */
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

static void
set_do_with_enum(Set *set, VALUE a_enum, VALUE (*func)(ANYARGS), int argc, VALUE *argv)
{
    int i;

    if (TYPE(a_enum) == T_ARRAY)
        for (i=0; i<RARRAY_LEN(a_enum); i++)
            func(RARRAY_PTR(a_enum)[i], (VALUE) set, argc, argv);
    else {
        if (rb_respond_to(a_enum, rb_intern("each_entry")))
            rb_block_call(a_enum, rb_intern("each_entry"), argc, argv, func, (VALUE) set);
        else if (rb_respond_to(a_enum, rb_intern("each")))
            rb_block_call(a_enum, rb_intern("each"), argc, argv, func, (VALUE) set);
        else
            rb_raise(rb_eArgError, "value must be enumerable");
    }
}

/*
 * Document-method: do_with_enum
 * call-seq: do_with_enum(enum, &block)
 *
 * Iterates over enum and add each of it's elements (after yielded to block)
 * to the set.
 */
static VALUE
rb_set_do_with_enum(VALUE self, VALUE a_enum)
{
    if (TYPE(a_enum) == T_ARRAY)
        return rb_ary_each(a_enum);
    else if (rb_respond_to(a_enum, rb_intern("each_entry")))
        return rb_funcall(a_enum, rb_intern("each_entry"), 0);
    else if (rb_respond_to(a_enum, rb_intern("each")))
        return rb_funcall(a_enum, rb_intern("each"), 0);
    rb_raise(rb_eArgError, "value must be enumerable");
    return Qnil;
}

static void
set_add(Set *set, VALUE o)
{
    rb_hash_aset(set->hash, o, Qtrue);
}

/*
 * Document-method: add
 * call-seq: add(o)
 *
 * Adds the given object to the set and returns self.  Use +merge+ to
 * add many elements at once.
 */
static VALUE
rb_set_add(VALUE self, VALUE o)
{
    Set *set = get_set_ptr(self);
    set_add(set, o);
    return self;
}

static void
set_delete(Set *set, VALUE o)
{
    rb_hash_delete(set->hash, o);
}

/*
 * Document-method: delete
 * call-seq: delete(o)
 *
 * Deletes the given object from the set and returns self.  Use +subtract+ to
 * delete many items at once.
 */
static VALUE
rb_set_delete(VALUE self, VALUE o)
{
    Set *set = get_set_ptr(self);
    set_delete(set, o);
    return self;
}

static void
sets_merge(Set *self, Set *other_set)
{
    /* TODO find a better way of running Hash#update */
    rb_funcall(self->hash, rb_intern("update"), 1, other_set->hash);
}

static VALUE
set_subtract_i(VALUE e, VALUE set, int argc, VALUE *argv)
{
    set_delete((Set *) set, e);
    return ST_CONTINUE;    
}

static VALUE
set_merge_i(VALUE e, VALUE set, int argc, VALUE *argv)
{
    set_add((Set *) set, e);
    return ST_CONTINUE;
}

static void
set_merge(Set *set, VALUE a_enum)
{
    if (rb_class_of(a_enum) == rb_cSet) {
        Set *a_enum_set = get_set_ptr(a_enum);
        sets_merge(set, a_enum_set);
    }
    else
        set_do_with_enum(set, a_enum, set_merge_i, 0, 0);
}

/*
 * Document-method: merge
 * call-seq: merge(enum)
 *
 * Merges the elements of the given enumerable object to the set and
 * returns self.
 */
static VALUE
rb_set_merge(VALUE self, VALUE a_enum)
{
    set_merge(get_set_ptr(self), a_enum);
    return self;
}

static VALUE
set_new(VALUE klass)
{
    Set *set;
    VALUE self = set_alloc(klass);
    GetSetPtr(self, set);
    set->hash = rb_hash_new();

    return self;
}

static VALUE
set_initialize_i(VALUE e, VALUE set, int argc, VALUE *argv)
{
    set_add((Set *) set, rb_yield(e));
    return ST_CONTINUE;
}

static void
set_initialize(int argc, VALUE *argv, Set *set)
{
    if (argc == 0 || argc == 1) {
        if (set->hash != Qnil)
            set->hash = rb_hash_new();
    }
    else
       rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);

    if (argc && argv[0]!=Qnil) {
        if (rb_block_given_p())
            set_do_with_enum(set, argv[0], set_initialize_i, 0, 0);
        else
            set_merge(set, argv[0]);
    }
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
    set_initialize(argc, argv, set);
    rb_iv_set(self, "@hash", set->hash);
    return self;
}

static void
set_s_create(Set *set, int argc, VALUE *argv)
{
    int i=0;
    for (i=0; i<argc; i++)
        set_add(set, argv[i]);
}

/*
 * Document-method: self.[]
 * call-seq: self.[]
 *
 * Creates a new set containing the given objects.
 */
static VALUE
rb_set_s_create(int argc, VALUE *argv, VALUE klass)
{
    VALUE self = set_new(klass);
    Set *set = get_set_ptr(self);
    set_s_create(set, argc, argv);
    return self;
}

static void
set_clear(Set *set)
{
    set->hash = rb_hash_new();
}

/*
 * Document-method: clear
 * call-seq: clear
 *
 * Removes all elements and returns self.
 */
static VALUE
rb_set_clear(VALUE self)
{
    Set *set = get_set_ptr(self);
    set_clear(set);
    return self;
}

static void
sets_replace(Set *dest, Set *orig)
{
    dest->hash = rb_hash_dup(orig->hash);
}

static void
set_replace(Set *set, VALUE a_enum)
{
    if (rb_class_of(a_enum) == rb_cSet) {
        Set *a_enum_set = get_set_ptr(a_enum);
        sets_replace(set, a_enum_set);
    }
    else {
        set_clear(set);
        set_merge(set, a_enum);
    }
}

/*
 * Document-method: replace
 * call-seq: replace(enum)
 *
 * Replaces the contents of the set with the contents of the given
 * enumerable object and returns self.
 */
static VALUE
rb_set_replace(VALUE self, VALUE a_enum)
{
    set_replace(get_set_ptr(self), a_enum);
    return self;
}

/*
 * Document-method: initialize_copy
 * call-seq: initialize_copy(orig)
 *
 * Copy internal hash.
 */
static VALUE
rb_set_initialize_copy(VALUE self, VALUE orig)
{
    Set *set = get_set_ptr(self);
    Set *origset = get_set_ptr(orig);
    sets_replace(set, origset);
    return self;
}

/*
 * Document-method: freeze
 * call-seq: freeze
 *
 * Freezes the set.
 */
static VALUE
rb_set_freeze(VALUE self)
{
    Set *set = get_set_ptr(self);
    OBJ_FREEZE(set->hash);
    rb_obj_freeze(self);
    return self;
}

/*
 * Document-method: taint
 * call-seq: taint
 *
 * Taints the set.
 */
static VALUE
rb_set_taint(VALUE self)
{
    Set *set = get_set_ptr(self);
    OBJ_TAINT(set->hash);
    rb_obj_taint(self);
    return self;
}

/*
 * Document-method: untaint
 * call-seq: untaint
 *
 * Untaints the set.
 */
static VALUE
rb_set_untaint(VALUE self)
{
    Set *set = get_set_ptr(self);
    FL_UNSET(set->hash,FL_TAINT);
    rb_obj_untaint(self);
    return self;
}

static int
set_size(Set *set)
{
    if (!RHASH(set->hash)->ntbl)
        return 0;
    return RHASH(set->hash)->ntbl->num_entries;
}

/*
 * Document-method: size
 * call-seq: size
 *
 * Returns the number of elements.
 */
static VALUE
rb_set_size(VALUE self)
{
    Set *set = get_set_ptr(self);
    return INT2FIX(set_size(set));
}

static VALUE
set_empty_p(Set *set)
{
    return RHASH_EMPTY_P(set->hash) ? Qtrue : Qfalse;
}

/*
 * Document-method: empty?
 * call-seq: empty?
 *
 * Returns the number of elements.
 */
static VALUE
rb_set_empty_p(VALUE self)
{
    Set *set = get_set_ptr(self);
    return set_empty_p(set);
}

static int
set_to_a_i(VALUE key, VALUE value, VALUE ary)
{
    if (key == Qundef) return ST_CONTINUE;
    rb_ary_push(ary, key);
    return ST_CONTINUE;
}

static VALUE
set_to_a(Set *set)
{
    VALUE ary = rb_ary_new();
    rb_hash_foreach(set->hash, set_to_a_i, ary);
    OBJ_INFECT(ary, set->hash);

    return ary;
}

/*
 * Document-method: to_a
 * call-seq: to_a
 *
 * Converts the set to an array.  The order of elements is uncertain.
 */
static VALUE
rb_set_to_a(VALUE self)
{
    return set_to_a(get_set_ptr(self));
}

static VALUE
set_includes(Set *set, VALUE o)
{
    return rb_hash_lookup2(set->hash, o, Qfalse);
}

/*
 * Document-method: include?
 * call-seq: include?(o)
 *
 * Returns true if the set contains the given object.
 */
static VALUE
rb_set_include_p(VALUE self, VALUE o)
{
    Set *set = get_set_ptr(self);
    return set_includes(set, o);
}

static int
set_flatten_merge_i(VALUE e, VALUE value, VALUE args)
{
    VALUE e_id;
    Set *self_set = (Set *) ((VALUE *) args)[0];
    Set *seen_set = (Set *) ((VALUE *) args)[1];

    if (rb_obj_is_kind_of(e, rb_cSet) == Qtrue) {
        e_id = rb_obj_id(e);
        if (set_includes(seen_set, e_id))
            rb_raise(rb_eArgError, "tried to flatten recursive Set");

        set_add(seen_set, e_id);
        set_flatten_merge_i(e, 0, args);
        set_delete(seen_set, e_id);
    }
    else {
        set_add(self_set, e);
    }
    return ST_CONTINUE;
}

static VALUE
set_flatten_merge(VALUE self, VALUE orig, VALUE seen)
{
    Set *orig_set = get_set_ptr(orig);

    VALUE args[2] = {(VALUE) get_set_ptr(self), (VALUE) get_set_ptr(seen)};

    rb_hash_foreach(orig_set->hash, set_flatten_merge_i, (VALUE) args);

    return self;
}

static VALUE
rb_set_flatten_merge(int argc, VALUE *argv, VALUE self)
{
    VALUE orig, seen;

    if (argc == 1)
        seen = set_new(rb_cSet);
    else if (argc == 2)
        seen = argv[1];
    else
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)", argc);

    orig = argv[0];

    return set_flatten_merge(self, orig, seen);
}

/*
 * Document-method: flatten
 * call-seq: flatten
 *
 * Returns a new set that is a copy of the set, flattening each
 * containing set recursively.
 */
static VALUE
rb_set_flatten(VALUE self)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE klass = rb_class_of(self);
    VALUE orig = set_new(klass);
    VALUE seen = set_new(klass);

    return set_flatten_merge(self, orig, seen);
}

static int
set_detect_i(VALUE key, VALUE value, VALUE return_val)
{
    VALUE *ret_val = (VALUE *) return_val;
    if (rb_obj_is_kind_of(key, rb_cSet) != Qtrue)
        return ST_CONTINUE;
    *ret_val = key;
    return ST_STOP;
}

static VALUE
set_detect(VALUE self)
{
    VALUE return_val = Qnil;
    Set *set = get_set_ptr(self);
    rb_hash_foreach(set->hash, set_detect_i, (VALUE) &return_val);
    return return_val;
}

/*
 * Document-method: flatten!
 * call-seq: flatten!
 *
 * Equivalent to Set#flatten, but replaces the receiver with the
 * result in place.  Returns nil if no modifications were made.
 */
static VALUE
rb_set_flatten_bang(VALUE self)
{
    if (set_detect(self) != Qnil)
        return rb_set_replace(self, rb_set_flatten(self));
    return Qnil;
}

static int
set_test_all_p_i(VALUE e, VALUE value, VALUE args)
{
    Set *set = (Set *) ((VALUE *) args)[0];
    VALUE *test = (VALUE *) ((VALUE *) args)[1];
    *test = set_includes(set, e);
    if (*test == Qfalse)
        return ST_STOP;
    return ST_CONTINUE;
}

static VALUE
set_test_all_p(Set *self, Set *set)
{
    VALUE test = Qtrue;
    VALUE args[2] = {(VALUE) set, (VALUE) &test};

    rb_hash_foreach(self->hash, set_test_all_p_i, (VALUE) args);

    return test;
}

static VALUE
set_is_a_set(VALUE o)
{
    if (rb_obj_is_kind_of(o, rb_cSet) == Qfalse)
        rb_raise(rb_eArgError, "value must be a set");
    return Qtrue;
}

/*
 * Document-method: superset?
 * call-seq: superset?(set)
 *
 * Returns true if the set is a superset of the given set.
 */
static VALUE
rb_set_superset_p(VALUE self, VALUE other)
{
    Set *self_set;
    Set *other_set;

    set_is_a_set(other);

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    if (set_size(self_set) < set_size(other_set))
        return Qfalse;

    return set_test_all_p(other_set, self_set);
}

/*
 * Document-method: proper_superset?
 * call-seq: peorper_superset?(set)
 *
 * Returns true if the set is a proper superset of the given set.
 */
static VALUE
rb_set_proper_superset_p(VALUE self, VALUE other)
{
    Set *self_set;
    Set *other_set;

    set_is_a_set(other);

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    if (set_size(self_set) <= set_size(other_set))
        return Qfalse;

    return set_test_all_p(other_set, self_set);
}

/*
 * Document-method: subset?
 * call-seq: subset?(set)
 *
 * Returns true if the set is a subset of the given set.
 */
static VALUE
rb_set_subset_p(VALUE self, VALUE other)
{
    Set *self_set;
    Set *other_set;

    set_is_a_set(other);

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    if (set_size(other_set) < set_size(self_set))
        return Qfalse;

    return set_test_all_p(self_set, other_set);
}

/*
 * Document-method: proper_subset?
 * call-seq: proper_subset?(set)
 *
 * Returns true if the set is a proper subset of the given set.
 */
static VALUE
rb_set_proper_subset_p(VALUE self, VALUE other)
{
    Set *self_set;
    Set *other_set;

    set_is_a_set(other);

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    if (set_size(other_set) <= set_size(self_set))
        return Qfalse;

    return set_test_all_p(self_set, other_set);
}

static VALUE
set_no_block_given(VALUE self, ID method_id)
{
    return rb_enumeratorize(self, ID2SYM(method_id), 0, 0);
}

static int
rb_set_each_i(VALUE key, VALUE value)
{
    rb_yield(key);
    return ST_CONTINUE;
}

/*
 * Document-method: each
 * call-seq: each(&block)
 *
 * Calls the given block once for each element in the set, passing
 * the element as parameter.  Returns an enumerator if no block is
 * given.
 */
static VALUE
rb_set_each(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("each"));

    Set *set = get_set_ptr(self);
    rb_hash_foreach(set->hash, rb_set_each_i, 0);
    return self;
}

/*
 * Document-method: add?
 * call-seq: add?(o)
 *
 * Adds the given object to the set and returns self.  If the
 * object is already in the set, returns nil.
 */
static VALUE
rb_set_add_bang(VALUE self, VALUE o)
{
    Set *set = get_set_ptr(self);
    if (RTEST(rb_hash_lookup2(set->hash, o, Qnil)))
        return Qnil;
    set_add(set, o);
    return self;
}

/*
 * Document-method: delete?
 * call-seq: delete?(o)
 *
 * Deletes the given object from the set and returns self.  If the
 * object is not in the set, returns nil.
 */
static VALUE
rb_set_delete_bang(VALUE self, VALUE o)
{
    Set *set = get_set_ptr(self);
    if (!RTEST(rb_hash_lookup2(set->hash, o, Qnil)))
        return Qnil;
    set_delete(set, o);
    return self;
}

static int
set_delete_if_i(VALUE o, VALUE value, VALUE arg)
{
    Set *set = (Set *) arg;
    if (RTEST(rb_yield(o)))
        set_delete(set, o);
    return ST_CONTINUE;
}

static void
set_delete_if(Set *set)
{
    rb_hash_foreach(set->hash, set_delete_if_i, (VALUE) set);
}

/*
 * Document-method: delete_if 
 * call-seq: delete_if(&block)
 *
 * Deletes every element of the set for which block evaluates to
 * true, and returns self.
 */
static VALUE
rb_set_delete_if(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("delete_if"));

    Set *set = get_set_ptr(self);
    set_delete_if(set);
    return self;
}

static int
set_keep_if_i(VALUE o, VALUE value, VALUE arg)
{
    Set *set = (Set *) arg;
    /* TODO: see if set_keep_if_i can't be merged with set_delete_if_i */
    if (RTEST(rb_yield(o)))
        set_delete(set, o);
    return ST_CONTINUE;
}

static void
set_keep_if(Set *set)
{
    rb_hash_foreach(set->hash, set_keep_if_i, (VALUE) set);
}

/*
 * Document-method: keep_if 
 * call-seq: keep_if(&block)
 *
 * Deletes every element of the set for which block evaluates to
 * false, and returns self.
 */
static VALUE
rb_set_keep_if(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("keep_if"));

    Set *set = get_set_ptr(self);
    set_keep_if(set);
    return self;
}

static int
set_collect_bang_i(VALUE key, VALUE value, VALUE arg)
{
    Set *set = (Set *) arg;
    set_add(set, rb_yield(key));
    return ST_CONTINUE;
}

/*
 * Document-method: collect!
 * call-seq: collect!(&block)
 *
 * Replaces the elements with ones returned by collect().
 */
static VALUE
rb_set_collect_bang(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("collect!"));

    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set = get_set_ptr(new);
    Set *self_set = get_set_ptr(self);

    rb_hash_foreach(self_set->hash, set_collect_bang_i, (VALUE) new_set);

    sets_replace(self_set, new_set);

    return self;
}

/*
 * Document-method: reject!
 * call-seq: reject!(&block)
 *
 * Equivalent to Set#delete_if, but returns nil if no changes were
 * made.
 */
static VALUE
rb_set_reject_bang(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("reject!"));

    Set *set = get_set_ptr(self);
    int n = set_size(set);
    rb_hash_foreach(set->hash, set_delete_if_i, (VALUE) set);
    return set_size(set) == n ? Qnil : self;
}

/*
 * Document-method: select!
 * call-seq: select!(&block)
 *
 * Equivalent to Set#keep_if, but returns nil if no changes were
 * made.
 */
static VALUE
rb_set_select_bang(VALUE self)
{
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("select!"));

    Set *set = get_set_ptr(self);
    int n = set_size(set);
    rb_hash_foreach(set->hash, set_keep_if_i, (VALUE) set);
    return set_size(set) == n ? Qnil : self;
}

/*
 * Document-method: subtract
 * call-seq: subtract(enum)
 *
 * Deletes every element that appears in the given enumerable object
 * and returns self.
 */
static VALUE
rb_set_subtract(VALUE self, VALUE a_enum)
{
    set_do_with_enum(get_set_ptr(self), a_enum, set_subtract_i, 0, 0);
    return self;
}

static VALUE
set_dup(VALUE self)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set  = get_set_ptr(new);
    Set *orig_set = get_set_ptr(self);
    sets_replace(new_set, orig_set);

    return new;
}

/*
 * Document-method: |
 * call-seq: |(enum)
 *
 * Returns a new set built by merging the set and the elements of the
 * given enumerable object.
 */
static VALUE
rb_set_union(VALUE self, VALUE a_enum)
{
    VALUE new = set_dup(self);
    return rb_set_merge(new, a_enum);
}

/*
 * Document-method: -
 * call-seq: -(enum)
 *
 * Returns a new set built by duplicating the set, removing every
 * element that appears in the given enumerable object.
 */
static VALUE
rb_set_difference(VALUE self, VALUE a_enum)
{
    VALUE new = set_dup(self);
    rb_set_subtract(new, a_enum);
    return new;
}

static VALUE
set_intersection_i(VALUE e, VALUE set, int argc, VALUE *argv)
{
    if (set_includes((Set *) set, e))
        set_add((Set *) argv[0], e);
    return ST_CONTINUE;
}

/*
 * Document-method: & 
 * call-seq: &(enum)
 *
 * Returns a new set containing elements common to the set and the
 * given enumerable object.
 */
static VALUE
rb_set_intersection(VALUE self, VALUE a_enum)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set = get_set_ptr(new);
    set_do_with_enum(get_set_ptr(self), a_enum, set_intersection_i, 1, (VALUE *) &new_set);
    return new;
}

static int
set_exclusive_i(VALUE e, VALUE v, VALUE argv)
{
    Set *set = (Set *) argv;
    if (set_includes(set, e) == Qtrue)
        set_delete(set, e);
    else
        set_add(set, e);
    return ST_CONTINUE;
}

/*
 * Document-method: ^
 * call-seq: ^(enum)
 *
 * Returns a new set containing elements exclusive between the set
 * and the given enumerable object.  (set ^ enum) is equivalent to
 * ((set | enum) - (set & enum)).
 */
static VALUE
rb_set_exclusive(VALUE self, VALUE a_enum)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    VALUE args;

    Set *self_set = get_set_ptr(self);
    Set *new_set = get_set_ptr(new);
    set_merge(new_set, a_enum);

    rb_hash_foreach(self_set->hash, set_exclusive_i, (VALUE) new_set);
    return new;
}

/*
 * Document-method: ==
 * call-seq: ==(other)
 *
 * Returns true if two sets are equal.  The equality of each couple
 * of elements is defined according to Object#eql?.
 */
static VALUE
rb_set_equal(VALUE self, VALUE other)
{
    Set *self_set, *other_set;
    if (self == other)
        return Qtrue;

    if (rb_class_of(self) != rb_class_of(other))
        return Qfalse;

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    /* TODO: Find a better way to call Hash#== */
    if (rb_obj_is_instance_of(other, rb_class_of(self)) == Qtrue)
        return rb_funcall(self_set->hash, rb_intern("=="), 1, other_set->hash);

    if (rb_obj_is_instance_of(other, rb_cSet) == Qtrue && set_size(self_set) == set_size(other_set))
        return set_test_all_p(self_set, other_set);
    return Qfalse;
}

static VALUE
rb_set_hash(VALUE self)
{
    Set *set = get_set_ptr(self);
    return set->hash;
}

static VALUE
rb_set_eql_p(VALUE self, VALUE other)
{
    Set *self_set;
    Set *other_set;

    if (rb_obj_is_kind_of(other, rb_cSet) == Qfalse)
        return Qfalse;

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    if (rb_class_of(other) != rb_cSet)
        return Qfalse;
    /* TODO: Find a better way to call Hash#eql? */
    return rb_funcall(self_set->hash, rb_intern("eql?"), 1, other_set->hash);
}

static int
set_classify_i(VALUE i, VALUE value, VALUE args)
{
    Set *set;
    VALUE hash = ((VALUE *) args)[0];
    VALUE self = ((VALUE *) args)[1];

    VALUE x = rb_yield(i);
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));

    VALUE hash_val = rb_hash_lookup2(hash, x, Qnil);
    if (hash_val == Qnil) {
        rb_hash_aset(hash, x, new);
        set = get_set_ptr(new);
    }
    else
        set = get_set_ptr(hash_val);
    set_add(set, i);
    return ST_CONTINUE;
}

/*
 * Document-method: classify
 * call-seq: classify(&block)
 *
 * Classifies the set by the return value of the given block and
 * returns a hash of {value => set of elements} pairs.  The block is
 * called once for each element of the set, passing the element as
 * parameter.
 *
 * e.g.:
 *
 *   require 'set'
 *   files = Set.new(Dir.glob("*.rb"))
 *   hash = files.classify { |f| File.mtime(f).year }
 *   p hash    # => {2000=>#<Set: {"a.rb", "b.rb"}>,
 *             #     2001=>#<Set: {"c.rb", "d.rb", "e.rb"}>,
 *             #     2002=>#<Set: {"f.rb"}>}
 */
static VALUE
rb_set_classify(VALUE self)
{
    Set *set = get_set_ptr(self);
    VALUE hash = rb_hash_new();
    VALUE args[2] = {hash, self};

    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("classify"));

    rb_hash_foreach(set->hash, set_classify_i, (VALUE) args);

    return hash;
}

static VALUE
tsort_each_node(VALUE tsort)
{
    return rb_funcall(tsort, rb_to_id(rb_intern("each_key")), 0);
}

static VALUE
tsort_each_child(VALUE tsort, VALUE node)
{
    return rb_funcall(rb_hash_fetch(tsort, node), rb_to_id(rb_intern("each")), 0);
}

static int
set_divide_i_i(VALUE v, VALUE value, VALUE args)
{
    VALUE u = ((VALUE *) args)[0];
    VALUE a = ((VALUE *) args)[1];

    rb_yield_values(2, u, v);
    rb_ary_push(a, v);

    return ST_CONTINUE;
}

static int
set_divide_i(VALUE u, VALUE value, VALUE args)
{
    VALUE dig = ((VALUE *) args)[0];
    Set *set = (Set *) ((VALUE *) args)[1];

    VALUE a = rb_ary_new();

    VALUE fargs[2] = {u, a};
    rb_funcall(dig, rb_to_id(rb_intern("[]=")), 2, u, a);
    rb_hash_foreach(set->hash, set_divide_i_i, (VALUE) fargs);

    return ST_CONTINUE;
}

static int
hash_values_i(VALUE key, VALUE value, VALUE ary)
{
    if (key == Qundef) return ST_CONTINUE;
    rb_ary_push(ary, value);
    return ST_CONTINUE;
}

/*
 * Document-method: divide
 * call-seq: divide(&block)
 *
 * Divides the set into a set of subsets according to the commonality
 * defined by the given block.
 *
 * If the arity of the block is 2, elements o1 and o2 are in common
 * if block.call(o1, o2) is true.  Otherwise, elements o1 and o2 are
 * in common if block.call(o1) == block.call(o2).
 *
 * e.g.:
 *
 *   require 'set'
 *   numbers = Set[1, 3, 4, 6, 9, 10, 11]
 *   set = numbers.divide { |i,j| (i - j).abs == 1 }
 *   p set     # => #<Set: {#<Set: {1}>,
 *             #            #<Set: {11, 9, 10}>,
 *             #            #<Set: {3, 4}>,
 *             #            #<Set: {6}>}>
 */
static VALUE
rb_set_divide(VALUE self)
{
    VALUE new = set_new(rb_class_of(self));
    VALUE ary, args[2];
    Set *set;

    if (!rb_block_given_p())
        return set_no_block_given(self, rb_to_id(rb_intern("divide")));

    VALUE proc = rb_block_proc();
    if (rb_proc_arity(proc) == 2) {
        rb_require("tsort");
        VALUE dig = rb_hash_new();
        rb_extend_object(dig, rb_const_get(rb_cObject, rb_intern("TSort")));
        rb_define_singleton_method(dig,"tsort_each_node", tsort_each_node, 0);
        rb_define_singleton_method(dig,"tsort_each_child", tsort_each_child, 1);

        set = get_set_ptr(self);
        args[0] = dig;
        args[1] = (VALUE) set;
        rb_hash_foreach(set->hash, set_divide_i, (VALUE) args);

        /* TODO: Find a better way of calling each_strongly_connected_component */
        rb_eval_string("dig.each_strongly_connected_component { |css| new.add(self.class.new(css)) }");
        return new;
    }

    /* TODO: Find a better way to call Hash#values */
    ary = rb_ary_new();
    rb_hash_foreach(rb_set_classify(self), hash_values_i, ary);
    return ary;
}

static VALUE
set_inspect_i(VALUE args)
{
    VALUE self = ((VALUE *) args)[0];
    VALUE ids = ((VALUE *) args)[1];
    VALUE inspect_str;

    rb_ary_push(ids, rb_obj_id(self));

    inspect_str = rb_ary_to_s(rb_set_to_a(self));
    inspect_str = rb_str_substr(inspect_str, 1, rb_str_strlen(inspect_str)-2);
    return rb_sprintf("#<%s: {%s}>", rb_obj_classname(self), RSTRING_PTR(inspect_str));
}

/*
 * Document-method: inspect
 * call-seq: inspect
 *
 * Returns a string containing a human-readable representation of the
 * set. ("#<Set: {element1, element2, ...}>")
 */
static VALUE
rb_set_inspect(VALUE self)
{   
    VALUE cur_thread = rb_thread_current();
    VALUE inspect_key = rb_intern("__inspect_key__");
    VALUE ids = rb_thread_local_aref(cur_thread, inspect_key);
    VALUE args[2];

    if (ids == Qnil){
        ids = rb_ary_new();
        rb_thread_local_aset(cur_thread, inspect_key, ids);
    }

    if (rb_ary_includes(ids, rb_obj_id(self)))
        return rb_sprintf("#<%s: {...}>", rb_obj_classname(self));

    args[0] = self;
    args[1] = ids;
    return rb_ensure(set_inspect_i, (VALUE) args, rb_ary_pop, ids);
}

static VALUE
rb_set_pretty_print(VALUE self, VALUE pp)
{
    /* TODO: Find a better way of calling PrettyPrint#text */
    rb_funcall(pp, rb_intern("text"), 1, rb_sprintf("#<%s: {", rb_class2name(rb_class_of(self))));
    rb_eval_string("pp.nest(1) { pp.seplist(self) { |o| pp.pp o } }");
    return rb_funcall(pp, rb_intern("text"), 1, rb_sprintf("}>"));
}

static VALUE
rb_set_pretty_print_cycle(VALUE self, VALUE pp)
{
    /* TODO: Find a better way of calling PrettyPrint#text */
    Set *set = get_set_ptr(self);
    return rb_funcall(pp, rb_intern("text"), 1, rb_sprintf("#<%s: {%s}>", rb_class2name(rb_class_of(self)), set_empty_p(set)==Qtrue? "" : "..."));
}

/* FIXME: don't know how to do it by now
static VALUE
rb_rbtree_sset_initialize(int argc, VALUE *argv, VALUE self)
{
    Set *set = GetSetPtr(self);
    set->hash = rbtree_s_create(0, 0, RBTree);
    return rb_set_initialize(argc, argv, self);
}

static VALUE
rb_rbtree_sset_add(VALUE self, VALUE o)
{
    if (!rb_respond_to(o, rb_intern("<=>")))
        rb_raise(rb_eArgError, "value must respond to <=>");

    return rb_set_add(self, o);
}
*/

/*
 * Document-class: SortedSet
 *
 * SortedSet implements a Set that guarantees that it's element are
 * yielded in sorted order (according to the return values of their
 * #<=> methods) when iterating over them.
 *
 * All elements that are added to a SortedSet must respond to the <=>
 * method for comparison.
 *
 * Also, all elements must be <em>mutually comparable</em>: <tt>el1 <=>
 * el2</tt> must not return <tt>nil</tt> for any elements <tt>el1</tt>
 * and <tt>el2</tt>, else an ArgumentError will be raised when
 * iterating over the SortedSet.
 *
 * == Example
 *
 *   require "set"
 *
 *   set = SortedSet.new([2, 1, 5, 6, 4, 5, 3, 3, 3])
 *   ary = []
 *
 *   set.each do |obj|
 *     ary << obj
 *   end
 *
 *   p ary # => [1, 2, 3, 4, 5, 6]
 *
 *   set2 = SortedSet.new([1, 2, "3"])
 *   set2.each { |obj| }
 *   # => raises ArgumentError: comparison of Fixnum with String failed
 */

typedef struct {
    Set set_;
    VALUE keys;
} SortedSet;

static void
sset_mark(void *ptr)
{
    SortedSet *sset = ptr;
    set_mark(&sset->set_);
    rb_gc_mark(sset->keys);
}

#define sset_free RUBY_TYPED_DEFAULT_FREE

static size_t
sset_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
        const SortedSet *sset = ptr;
        size = sizeof(SortedSet) - sizeof(Set);
        size += set_memsize(&sset->set_);
        size += rb_ary_memsize(sset->keys);
    }
    return size;
}

static const rb_data_type_t sset_data_type = {
    "sortedset",
    {sset_mark, sset_free, sset_memsize,},
};

#define GetSortedSetPtr(obj, tobj) \
    TypedData_Get_Struct(obj, SortedSet, &sset_data_type, tobj)

static SortedSet *
get_sset_ptr(VALUE self)
{
    SortedSet *sset;
    GetSortedSetPtr(self, sset);
    if (!sset->keys || !sset->set_.hash) {
       rb_raise(rb_eArgError, "uninitialized SortedSet");
    }
    return sset;
}

static VALUE
sset_alloc(VALUE klass)
{
    SortedSet *sset;
    return TypedData_Make_Struct(klass, SortedSet, &sset_data_type, sset);
}

static VALUE
sset_new(VALUE klass)
{
    SortedSet *sset;
    VALUE self = sset_alloc(klass);
    GetSortedSetPtr(self, sset);
    sset->keys = Qnil;
    sset->set_.hash = rb_hash_new();

    return self;
}

static VALUE
rb_sset_initialize(int argc, VALUE *argv, VALUE self)
{
    SortedSet *sset;
    GetSortedSetPtr(self, sset);
    sset->keys = Qnil;
    set_initialize(argc, argv, &sset->set_);
    rb_iv_set(self, "@hash", sset->set_.hash);
    return self;
}

static VALUE
rb_sset_s_create(int argc, VALUE *argv, VALUE klass)
{
    VALUE self = sset_new(klass);
    SortedSet *sset = get_sset_ptr(self);
    set_s_create(&sset->set_, argc, argv);
    return self;
}

static VALUE
rb_sset_clear(VALUE self)
{
    SortedSet *sset = get_sset_ptr(self);
    sset->keys = Qnil;
    set_clear(&sset->set_);
    return self;
}

static VALUE
rb_sset_replace(VALUE self, VALUE a_enum)
{
    SortedSet *sset = get_sset_ptr(self);
    sset->keys = Qnil;
    set_replace(&sset->set_, a_enum);
    return self;
}

static VALUE
rb_sset_add(VALUE self, VALUE o)
{
    SortedSet *sset = get_sset_ptr(self);
    if (!rb_respond_to(o, rb_intern("<=>")))
        rb_raise(rb_eArgError, "value must respond to <=>");

    sset->keys = Qnil;
    set_add(&sset->set_, o);
    return self;
}

static VALUE
rb_sset_delete(VALUE self, VALUE o)
{
    SortedSet *sset = get_sset_ptr(self);
    sset->keys = Qnil;
    set_delete(&sset->set_, o);
    return self;
}

static VALUE
rb_sset_delete_if(VALUE self)
{
    SortedSet *sset = get_sset_ptr(self);
    int n;

    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("delete_if"));

    n = set_size(&sset->set_);
    set_delete_if(&sset->set_);
    if (set_size(&sset->set_) != n)
        sset->keys = Qnil;

    return self;
}

static VALUE
rb_sset_keep_if(VALUE self)
{
    SortedSet *sset = get_sset_ptr(self);
    int n;

    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("keep_if"));

    n = set_size(&sset->set_);
    set_keep_if(&sset->set_);
    if (set_size(&sset->set_) != n)
        sset->keys = Qnil;

    return self;
}

static VALUE
rb_sset_merge(VALUE self, VALUE a_enum)
{
    SortedSet *sset = get_sset_ptr(self);
    sset->keys = Qnil;
    set_merge(&sset->set_, a_enum);
    return self;
}

static void sset_to_a(SortedSet *sset)
{
    if (sset->keys == Qnil)
        sset->keys = rb_ary_sort_bang(set_to_a(&sset->set_));
}

static VALUE
rb_sset_to_a(VALUE self) {
    SortedSet *sset = get_sset_ptr(self);
    sset_to_a(sset);
    return sset->keys;
}

static VALUE
rb_sset_each(VALUE self) {
    SortedSet *sset = get_sset_ptr(self);
    int i;
    if (!rb_block_given_p())
        return set_no_block_given(self, rb_intern("each"));

    sset_to_a(sset);
    for (i=0; i<RARRAY_LEN(sset->keys); i++)
        rb_yield(RARRAY_PTR(sset->keys)[i]);

    return self;
}

# ifndef MAX
#define MAX(i,j) (i>j ? i : j)
# endif
static VALUE
rb_enum_to_set(int argc, VALUE *argv, VALUE obj)
{
    VALUE klass = rb_cSet;
    /* TODO: set a good value to fargs */
    VALUE self, fargv[100];
    int i, fargc;

    fargc = MAX(1,argc);
    for (i=0; i<argc; i++)
        fargv[i] = argv[i];

    if (argc > 0)
        klass = argv[0];

    fargv[0] = obj;
    
    /* TODO: implement in a generic way to accomodate other classes */
    if (klass == rb_cSet) {
        self = set_alloc(klass);
        return rb_set_initialize(fargc, fargv, self);
    }
    else if (klass == rb_cSortedSet) {
        self = sset_alloc(klass);
        return rb_sset_initialize(fargc, fargv, self);
    }
}

VALUE
rb_set_dump(VALUE self, VALUE limit)
{
    return rb_marshal_dump(Qnil, limit);
}

VALUE
rb_set_s_load(VALUE klass, VALUE str)
{
    VALUE set = set_alloc(klass);
    VALUE garbage = rb_marshal_load(str);

    return set;
}

void
Init_cset(void)
{
    rb_cSet = rb_define_class("CSet", rb_cObject);
    rb_cSortedSet = rb_define_class("SortedCSet", rb_cSet);

    rb_define_method(rb_mEnumerable, "to_set", rb_enum_to_set, -1);

    rb_include_module(rb_cSet, rb_mEnumerable);

    rb_define_alloc_func(rb_cSet, set_alloc);
    rb_define_const(rb_cSet, "InspectKey", ID2SYM(rb_intern("__inspect_key__")));
    rb_define_method(rb_cSet, "initialize", rb_set_initialize, -1);
    rb_define_singleton_method(rb_cSet, "[]", rb_set_s_create, -1);
    rb_define_private_method(rb_cSet, "do_with_enum", rb_set_do_with_enum, 1);
    rb_define_method(rb_cSet, "initialize_copy", rb_set_initialize_copy, 1);
    rb_define_method(rb_cSet, "freeze", rb_set_freeze, 0);
    rb_define_method(rb_cSet, "taint", rb_set_taint, 0);
    rb_define_method(rb_cSet, "untaint", rb_set_untaint, 0);
    rb_define_method(rb_cSet, "size", rb_set_size, 0);
    rb_define_method(rb_cSet, "empty?", rb_set_empty_p, 0);
    rb_define_method(rb_cSet, "clear", rb_set_clear, 0);
    rb_define_method(rb_cSet, "replace", rb_set_replace, 1);
    rb_define_method(rb_cSet, "to_a", rb_set_to_a, 0);
    rb_define_protected_method(rb_cSet, "flatten_merge", rb_set_flatten_merge, -1);
    rb_define_method(rb_cSet, "flatten", rb_set_flatten, 0);
    rb_define_method(rb_cSet, "flatten!", rb_set_flatten_bang, 0);
    rb_define_method(rb_cSet, "include?", rb_set_include_p, 1);
    rb_define_method(rb_cSet, "superset?", rb_set_superset_p, 1);
    rb_define_method(rb_cSet, "proper_superset?", rb_set_proper_superset_p, 1);
    rb_define_method(rb_cSet, "subset?", rb_set_subset_p, 1);
    rb_define_method(rb_cSet, "proper_subset?", rb_set_proper_subset_p, 1);
    rb_define_method(rb_cSet, "each", rb_set_each, 0);
    rb_define_method(rb_cSet, "add", rb_set_add, 1);
    rb_define_method(rb_cSet, "add?", rb_set_add_bang, 1);
    rb_define_method(rb_cSet, "delete", rb_set_delete, 1);
    rb_define_method(rb_cSet, "delete?", rb_set_delete_bang, 1);
    rb_define_method(rb_cSet, "delete_if", rb_set_delete_if, 0);
    rb_define_method(rb_cSet, "keep_if", rb_set_keep_if, 0);
    rb_define_method(rb_cSet, "collect!", rb_set_collect_bang, 0);
    rb_define_method(rb_cSet, "reject!", rb_set_reject_bang, 0);
    rb_define_method(rb_cSet, "select!", rb_set_select_bang, 0);
    rb_define_method(rb_cSet, "merge", rb_set_merge, 1);
    rb_define_method(rb_cSet, "subtract", rb_set_subtract, 1);
    rb_define_method(rb_cSet, "|", rb_set_union, 1);
    rb_define_method(rb_cSet, "-", rb_set_difference, 1);
    rb_define_method(rb_cSet, "&", rb_set_intersection, 1);
    rb_define_method(rb_cSet, "^", rb_set_exclusive, 1);
    rb_define_method(rb_cSet, "==", rb_set_equal, 1);
    rb_define_method(rb_cSet, "hash", rb_set_hash, 0);
    rb_define_method(rb_cSet, "eql?", rb_set_eql_p, 1);
    rb_define_method(rb_cSet, "classify", rb_set_classify, 0);
    rb_define_method(rb_cSet, "divide", rb_set_divide, 0);
    rb_define_method(rb_cSet, "inspect", rb_set_inspect, 0);
    rb_define_method(rb_cSet, "pretty_print", rb_set_pretty_print, 1);
    rb_define_method(rb_cSet, "pretty_print_cycle", rb_set_pretty_print_cycle, 1);
    rb_alias(rb_cSet, rb_intern("length"), rb_intern("size"));
    rb_alias(rb_cSet, rb_intern("member?"), rb_intern("include?"));
    rb_alias(rb_cSet, rb_intern("<<"), rb_intern("add"));
    rb_alias(rb_cSet, rb_intern("map!"), rb_intern("collect!"));
    rb_alias(rb_cSet, rb_intern("+"), rb_intern("|"));
    rb_alias(rb_cSet, rb_intern("union"), rb_intern("|"));
    rb_alias(rb_cSet, rb_intern("difference"), rb_intern("-"));
    rb_alias(rb_cSet, rb_intern("intersection"), rb_intern("&"));

    rb_define_alloc_func(rb_cSortedSet, sset_alloc);
    rb_define_method(rb_cSortedSet, "initialize", rb_sset_initialize, -1);
    rb_define_singleton_method(rb_cSortedSet, "[]", rb_sset_s_create, -1);
    rb_define_method(rb_cSortedSet, "clear", rb_sset_clear, 0);
    rb_define_method(rb_cSortedSet, "replace", rb_sset_replace, 1);
    rb_define_method(rb_cSortedSet, "add", rb_sset_add, 1);
    rb_define_method(rb_cSortedSet, "delete", rb_sset_delete, 1);
    rb_define_method(rb_cSortedSet, "delete_if", rb_sset_delete_if, 0);
    rb_define_method(rb_cSortedSet, "keep_if", rb_sset_keep_if, 0);
    rb_define_method(rb_cSortedSet, "merge", rb_sset_merge, 1);
    rb_define_method(rb_cSortedSet, "to_a", rb_sset_to_a, 0);
    rb_define_method(rb_cSortedSet, "each", rb_sset_each, 0);
    rb_alias(rb_cSortedSet, rb_intern("<<"), rb_intern("add"));

    rb_define_method(rb_cSet, "_dump", rb_set_dump, 1);
    rb_define_singleton_method(rb_cSet, "_load", rb_set_s_load, 1);

    rb_provide("cset.rb");
}
