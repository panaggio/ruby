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
set_do_with_enum(VALUE self, VALUE (*func)(ANYARGS), Set *o_set, VALUE a_enum)
{
    int i;
    Set *set = get_set_ptr(self);

    if (TYPE(a_enum) == T_ARRAY)
        for (i=0; i<RARRAY_LEN(a_enum); i++)
            func(RARRAY_PTR(a_enum)[i], set, o_set);
    else {
        VALUE proc = rb_proc_new(func, 0);
        /* TODO: create a way to pass the Proc as a block to rb_funcall */
        if (rb_respond_to(self, rb_intern("each_entry")))
            rb_funcall(a_enum, rb_intern("each_entry"), 0);
        else if (rb_respond_to(self, rb_intern("each")))
            rb_funcall(a_enum, rb_intern("each"), 0);
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
static void
rb_set_do_with_enum(VALUE self, VALUE a_enum)
{
    if (TYPE(a_enum) == T_ARRAY)
        rb_ary_each(a_enum);
    else if (rb_respond_to(self, rb_intern("each_entry")))
        rb_funcall(a_enum, rb_intern("each_entry"), 0);
    else if (rb_respond_to(self, rb_intern("each")))
        rb_funcall(a_enum, rb_intern("each"), 0);
    else
        rb_raise(rb_eArgError, "value must be enumerable");
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
set_merge(Set *self, Set *other_set)
{
    /* TODO find a better way of running Hash#update */
    rb_funcall(self->hash, rb_intern("update"), 1, other_set->hash);
}

static VALUE
set_subtract_i(VALUE e, Set *set, Set *o_set)
{
    set_delete(set, e);
    return ST_CONTINUE;    
}

static VALUE
set_merge_i(VALUE e, Set *set, Set *o_set)
{
    set_add(set, e);
    return ST_CONTINUE;
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
    Set *set = get_set_ptr(self);
    Set *a_enum_set;
    /* TODO: check if there's not a better way of checking classes */
    if (rb_class_of(self) == rb_class_of(a_enum)){
        a_enum_set = get_set_ptr(a_enum);
        set_merge(set, a_enum_set);
    }
    else
        set_do_with_enum(self, set_merge_i, 0, a_enum);

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

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new set.
 */
static VALUE
rb_set_initialize(int argc, VALUE *argv, VALUE klass)
{
    Set *set;
    /* TODO: check if this allocation is necessary */
    VALUE self;// = set_alloc(klass);
    GetSetPtr(self, set);

    if (argc == 0 || argc == 1)
        set->hash = rb_hash_new();
    else
       rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);

    if (argc == 0) return self;

    if (rb_block_given_p())
        rb_set_do_with_enum(self, argv[0]);
    else
        rb_set_merge(self, argv[0]);

    return self;
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
    return rb_set_initialize(argc, argv, klass);
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
    /* TODO find a better way of running Hash#clear than copying code */
    rb_funcall(set->hash, rb_intern("clear"), 0);
    return self;
}

static void
set_replace(Set *dest, Set *orig)
{
    dest->hash = rb_hash_dup(orig->hash);
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
    Set *set = get_set_ptr(self), *a_enum_set;
    /* TODO: check if there's not better way of checking classes */
    if (rb_class_of(self) == rb_class_of(a_enum)) {
        a_enum_set = get_set_ptr(a_enum);
        set_replace(set, a_enum_set);
    }
    else {
        rb_set_clear(self);
        rb_set_merge(self,a_enum);
    }

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
    set_replace(set, origset);
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

static VALUE
set_size(Set *set)
{
    return RHASH_SIZE(set->hash);
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
    return set_size(set);
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
    return RHASH_EMPTY_P(set->hash) ? Qtrue : Qfalse;
}

static int
set_to_a_i(VALUE key, VALUE value, VALUE ary)
{
    if (key == Qundef) return ST_CONTINUE;
    rb_ary_push(ary, rb_assoc_new(key, value));
    return ST_CONTINUE;
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
    Set *set = get_set_ptr(self);
    VALUE ary = rb_ary_new();
    rb_hash_foreach(set->hash, set_to_a_i, ary);
    OBJ_INFECT(ary, set->hash);

    return ary;
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
    VALUE *test = (VALUE *) ((VALUE *) args)[0];
    /* TODO: Check if ST_STOP can break the caller rb_hash_foreach */
    if (rb_hash_lookup(set->hash, e) != Qtrue) {
        *test = Qfalse;
        return ST_STOP;
    }
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

/*
 * Document-method: superset?
 * call-seq: superset?(set)
 *
 * Returns true if the set is a superset of the given set.
 */
static VALUE
rb_set_superset_p(VALUE self, VALUE other)
{
    /* TODO: See if it's needed to check other type.
     *       I think get_set_ptr does the job */
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

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
    /* TODO: See if it's needed to check other type.
     *       I think get_set_ptr does the job */
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

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
    /* TODO: See if it's needed to check other type.
     *       I think get_set_ptr does the job */
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

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
    /* TODO: See if it's needed to check other type.
     *       I think get_set_ptr does the job */
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

    if (set_size(other_set) <= set_size(self_set))
        return Qfalse;

    return set_test_all_p(self_set, other_set);
}

static VALUE
set_no_block_given(VALUE self, ID method_id)
{
    if (!rb_block_given_p())
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
    set_no_block_given(self, rb_intern("each"));

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
    set_no_block_given(self, rb_intern("delete_if"));

    Set *set = get_set_ptr(self);
    rb_hash_foreach(set->hash, set_delete_if_i, (VALUE) set);
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
    set_no_block_given(self, rb_intern("keep_if"));

    Set *set = get_set_ptr(self);
    rb_hash_foreach(set->hash, set_keep_if_i, (VALUE) set);
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
    set_no_block_given(self, rb_intern("collect!"));

    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set = get_set_ptr(new);
    Set *self_set = get_set_ptr(self);

    rb_hash_foreach(self_set->hash, set_collect_bang_i, (VALUE) new_set);

    set_replace(self_set, new_set);

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
    set_no_block_given(self, rb_intern("reject!"));

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
    set_no_block_given(self, rb_intern("select!"));

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
    set_do_with_enum(self, set_subtract_i, 0, a_enum);
    return self;
}

static VALUE
set_dup(VALUE self)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set  = get_set_ptr(new);
    Set *orig_set = get_set_ptr(self);
    set_replace(new_set, orig_set);

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
rb_set_intersection_i(VALUE e, Set *set, Set *o_set)
{
    if (set_includes(set, e))
        set_add(o_set, e);
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
    set_do_with_enum(self, rb_set_intersection_i, new_set, a_enum);
    return new;
}

static VALUE
rb_set_exclusive_i(VALUE e, Set *set, Set *o_set)
{
    if (set_includes(o_set, e) == Qtrue)
        set_delete(o_set, e);
    else
        set_add(o_set, e);
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
    Set *new_set = get_set_ptr(new);
    set_do_with_enum(self, rb_set_exclusive_i, new_set, a_enum);
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

    self_set = get_set_ptr(self);
    other_set = get_set_ptr(other);

    /* TODO: Find a better way to call Hash#== */
    if (rb_obj_is_kind_of(other, rb_class_of(self)) == Qtrue)
        return rb_funcall(self_set->hash, rb_intern("=="), 1, other_set->hash);

    if (rb_obj_is_kind_of(other, rb_cSet) == Qtrue && set_size(self_set) == set_size(other_set))
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
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

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
static int
rb_set_classify(VALUE self)
{
    Set *set = get_set_ptr(self);
    VALUE hash = rb_hash_new();
    VALUE args[2] = {hash, self};

    set_no_block_given(self, rb_intern("classify"));

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

    set_no_block_given(self, rb_to_id(rb_intern("divide")));

    /* TODO: discover how to get the passed block to call proc_arity */
    
    if (rb_proc_arity() == 2) {
        /* TODO: Find a better way of calling require */
        rb_eval_string("require 'tsort'");
        VALUE dig = rb_hash_new();
        /* TODO: Find a better way of passint TSort as an argument */
        rb_extend_object(dig, rb_eval_string("TSort"));
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

#define InspectKey rb_intern("__inspect_key__")

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
    VALUE ids = rb_thread_local_aref(cur_thread, InspectKey);

    static VALUE
    rb_set_inspect_i(VALUE n)
    {
        rb_ary_push(ids, rb_obj_id(self));
        return rb_sprintf("#<%s: {%s}>", rb_class2name(rb_class_of(self)), rb_ary_subseq(rb_set_to_a(self), 1, -2));
    }

    if (ids == Qnil){
        ids = rb_ary_new();
        rb_thread_aset(cur_thread, InspectKey, ids);
    }

    if (rb_ary_includes(ids, rb_obj_id(self)))
        return rb_sprintf("#<%s: {...}>", rb_class2name(rb_class_of(self)));

    return rb_ensure(rb_set_inspect_i, 0, rb_ary_pop, ids);
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

void
Init_cset(void)
{
    rb_cSet  = rb_define_class("CSet", rb_cObject);

    rb_define_alloc_func(rb_cSet, set_alloc);
    rb_define_method(rb_cSet, "initialize", rb_set_initialize, 0);
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
    rb_define_method(rb_cSet, "superset?", rb_set_superset_bang, 1);
    rb_define_method(rb_cSet, "proper_superset?", rb_set_proper_superset_bang, 1);
    rb_define_method(rb_cSet, "subset?", rb_set_subset_bang, 1);
    rb_define_method(rb_cSet, "proper_subset?", rb_set_proper_subset_bang, 1);
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
    rb_define_method(rb_cSet, "^", rb_set_exclusice, 1);
    rb_define_method(rb_cSet, "==", rb_set_equal, 1);
    rb_define_method(rb_cSet, "hash", rb_set_hash, 0);
    rb_define_method(rb_cSet, "eql?", rb_set_eql_p, 1);
    rb_define_method(rb_cSet, "classify", rb_set_, 0);
    rb_define_method(rb_cSet, "divide", rb_set_divide, 0);
    rb_define_method(rb_cSet, "inspect", rb_set_inspect, 0);
    rb_define_method(rb_cSet, "pretty_print", rb_set_pretty_print, 1);
    rb_define_method(rb_cSet, "pretty_print_cycle", rb_set_pretty_print_cycle, 1);
    

    rb_provide("set.rb");
}
