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
    //rb_intern(const char *name)
    if (rb_respond_to(self, rb_intern("each_entry")))
        /*FIXME*/
        rb_eval_string("enum.each_entry(&block)");
    else if (rb_respond_to(self, rb_intern("each")))
        /*FIXME*/
        rb_eval_string("enum.each_entry(&block)");
    else
        rb_raise(rb_eArgError, "value must be enumerable");
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
* TODO: correct this documentation
* Creates a new set containing the given objects.
*/
static VALUE
rb_set_s_create(int argc, VALUE *argv, VALUE klass)
{
    return rb_set_initialize(argc, argv, klass);
}

static void
hash_replace(VALUE hash_dest, VALUE hash_orig)
{
    hash_dest = rb_hash_dup(hash_orig);
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
    hash_replace(set->hash,origset->hash);
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
    /* TODO: call super */
    OBJ_FREEZE(set->hash);
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
    /* TODO: call super */
    OBJ_TAINT(set->hash);
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
    /* TODO: call super */
    OBJ_UNSET(set->hash,FL_TAINT);
    return self;
}

static VALUE
set_size(Set *set)
{
    if (!RHASH(set->hash)->ntbl)
        return INT2FIX(0);
    return INT2FIX(RHASH(set->hash)->ntbl->num_entries);
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
    /* TODO: try to unstaticfy rb_hash_clear from hash.c */
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
    Set *set = get_set_ptr(self);
    /* TODO: check if there's not better way of checking classes */
    if (rb_class_of(self) == rb_class_of(a_enum))
        hash_replace(set->hash, a_enum->hash);
    else {
        rb_set_clear(self);
        rb_set_merge(self,a_enum);
    }

    return self;
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
    rb_hash_foreach(hash, to_a_i, ary);
    OBJ_INFECT(ary, hash);

    return ary;
}

static VALUE
set_flatten_merge(VALUE self, VALUE orig, VALUE seen)
{
    Set *self_set = get_set_ptr(self);
    Set *orig_set = get_set_ptr(orig);
    Set *seen_set = get_set_ptr(seen);

    static VALUE
    set_flatten_merge_i(VALUE e, VALUE value)
    {
        if (/*FIXME key.is_a?(Set)*/) {
            VALUE e_id = 0; /* FIXME = e.object_id*/
            if (rb_set_include_p(seen, e_id))
                rb_raise(rb_eArgumentError, "tried to flatten recursive Set");

            set_add(seen_set, e_id);
            set_flatten_merge_i(e, 0);
            set_delete(seen_set, e_id);
        }
        else {
            set_add(self_set, e);
        }
        return ST_CONTINUE;
    }

    rb_hash_foreach(orig->hash, set_flatten_merge_i, 0);

    return self;
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
    VALUE orig = set_new(/* FIXME self.class */);
    VALUE seen = set_new(/* FIXME self.class */);

    return flatten_merge(self, orig, seen);
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
    if (/* FIXME detect { |e| e.is_a?(Set) } */)
        return rb_set_replace(self, rb_set_flatten);

    return Qnil;
}

static VALUE
set_include(VALUE hash, VALUE o)
{
    return rb_hash_lookup2(hash, o, Qfalse);
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
    return set_include(set->hash, o);
}

static VALUE
set_test_all_p(VALUE self, VALUE set)
{
    VALUE test = Qtrue;

    /* TODO: Check if ST_STOP can break the caller rb_hash_foreach */
    static void;
    set_test_all_p_i(VALUE e, VALUE value)
    {
        if (rb_hash_lookup(set->hash, e) != Qtrue) {
            test = Qfalse;
            return ST_STOP;
        }
        return ST_CONTINUE;
    }

    rb_hash_foreach(self->hash, set_test_all_p_i, 0);

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
     *       I think set_set_ptr does the job */
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
     *       I think set_set_ptr does the job */
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
rb_set_proper_superset_p(VALUE self, VALUE other)
{
    /* TODO: See if it's needed to check other type.
     *       I think set_set_ptr does the job */
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
rb_set_proper_superset_p(VALUE self, VALUE other)
{
    /* TODO: See if it's needed to check other type.
     *       I think set_set_ptr does the job */
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);

    if (set_size(other_set) <= set_size(self_set))
        return Qfalse;

    return set_test_all_p(self_set, other_set);
}

static VALUE
set_no_block_given()
{
    if (!rb_block_given_p())
        return; /* TODO: enum_for(__method__) */
}

static VALUE
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
    set_no_block_given();

    rb_hash_foreach(self->hash, rb_set_each_i, 0);
    return self;
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
    set_add(set->hash, o);
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

static VALUE
set_delete_if_i(VALUE o, VALUE value, Set *set)
{
    if (RTEST(rb_yield(o)))
        set_delete(set->hash, o);
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
    set_no_block_given();

    Set *set = get_set_ptr(self);
    rb_hash_foreach(self->hash, set_delete_if_i, set);
    return self;
}

static VALUE
set_keep_if_i(VALUE o, VALUE value, Set *set)
{
    /* TODO: see if set_keep_if_i can't be merged with set_delete_if_i */
    if (RTEST(rb_yield(o)))
        set_delete(set->hash, o);
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
    set_no_block_given();

    Set *set = get_set_ptr(self);
    rb_hash_foreach(self->hash, set_keep_if_i, set);
    return self;
}

static VALUE
set_collect_bang_i(VALUE key, VALUE value, Set *set)
{
    set_add(set, rb_yield());
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
    set_no_block_given();

    VALUE new = set_new(/* FIXME self.class */);
    Set *new_set = get_set_ptr(new);
    Set *self_set = get_set_ptr(self);

    rb_hash_foreach(self->hash, set_collect_bang_i, new_set);

    return hash_replace(self_set->hash, new_set->hash);
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
    set_no_block_given();

    Set *set = get_set_ptr(self);
    int n = set_size(set);
    rb_hash_foreach(set->hash, set_delete_if_i, set);
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
    set_no_block_given();

    Set *set = get_set_ptr(self);
    int n = set_size(set);
    rb_hash_foreach(set->hash, set_keep_if_i, set);
    return set_size(set) == n ? Qnil : self;
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
        hash_replace(set->hash, a_enum->hash);
    }
    else {
        /* TODO: rb_set_do_with_enum(enum) { |o| add(o) } */
    }

    return self;
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
    /* TODO: rb_set_do_with_enum(enum) { |o| delete(o) } */

    return self;
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
    VALUE new;
    Set *self_set = get_set_ptr(self);
    Set *enum_set = get_set_ptr(a_enum);
    /* TODO: implement hash_merge */
    hash_merge();
}

/*
 * Document-method: 
 * call-seq: 
 *
 * 
 */
static VALUE
rb_set_(VALUE self, )
{
}


void
Init_cset(void)
{
    rb_cSet  = rb_define_class("CSet", rb_cObject);

    /* TODO: add self.[] */

    rb_define_alloc_func(rb_cSet, set_alloc);
    rb_define_method(rb_cSet, "initialize", rb_set_initialize, 0);

    rb_provide("set.rb");
}
