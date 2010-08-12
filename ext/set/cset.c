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

static void
set_do_with_array(Set *set, void (*func)(ANYARGS), Set *o_set, VALUE arr)
{
    int i;

    for (i=0; i<RARRAY_LEN(arr); i++)
        func(RARRAY_PTR(arr)[i], set, o_set);
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

static void
set_replace(Set *dest, Set *orig)
{
    dest->hash = rb_hash_dup(orig->hash);
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
    OBJ_UNSET(set->hash,FL_TAINT);
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

static VALUE
hash_clear(VALUE hash)
{
    rb_hash_modify_check(hash);
    if (!RHASH(hash)->ntbl)
        return hash;
    if (RHASH(hash)->ntbl->num_entries > 0) {
        if (RHASH(hash)->iter_lev > 0)
            rb_hash_foreach(hash, clear_i, 0);
        else
            st_clear(RHASH(hash)->ntbl);
    }

    return hash;
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
    hash_clear(set->hash);
    return self;
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
        set_replace(set, a_enum);
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
        if (rb_obj_kind_of(e, rb_cSet)) {
            VALUE e_id = rb_obj_id(e);
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

    return rb_set_flatten_merge(self, orig, seen);
}

static VALUE
set_detect_i(VALUE key, VALUE value, VALUE *return_val)
{
    if (rb_obj_kind_of(key, rb_cSet) != Qtrue)
        return ST_CONTINUE;
    *return_val = key;
    return ST_STOP;
}

static VALUE
set_detect(VALUE self)
{
    VALUE return_val = Qnil;
    Set *set = get_set_ptr(self);
    rb_hash_foreach(self->hash, set_detect_i, &return_val);
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
rb_set_proper_superset_p(VALUE self, VALUE other)
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
rb_set_proper_superset_p(VALUE self, VALUE other)
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
set_no_block_given(VALUE self, VALUE method)
{
    if (!rb_block_given_p())
        return rb_enumeratorize(self, method, 0, 0);
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
    set_no_block_given(self, rb_intern("each"));

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
    set_no_block_given(self, rb_intern("delete_if"));

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
    set_no_block_given(self, rb_intern("keep_if"));

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
    set_no_block_given(self, rb_intern("collect!"));

    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    Set *new_set = get_set_ptr(new);
    Set *self_set = get_set_ptr(self);

    rb_hash_foreach(self->hash, set_collect_bang_i, new_set);

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
    set_no_block_given(self, rb_intern("select!"));

    Set *set = get_set_ptr(self);
    int n = set_size(set);
    rb_hash_foreach(set->hash, set_keep_if_i, set);
    return set_size(set) == n ? Qnil : self;
}

static int
rb_hash_update_i(VALUE key, VALUE value, VALUE hash)
{
    if (key == Qundef) return ST_CONTINUE;
    hash_update(hash, key);
    st_insert(RHASH(hash)->ntbl, key, value);
    return ST_CONTINUE;
}

static int
rb_hash_update_block_i(VALUE key, VALUE value, VALUE hash)
{
    if (key == Qundef) return ST_CONTINUE;
    if (rb_hash_has_key(hash, key)) {
        value = rb_yield_values(3, key, rb_hash_aref(hash, key), value);
    }
    hash_update(hash, key);
    st_insert(RHASH(hash)->ntbl, key, value);
    return ST_CONTINUE;
}

static VALUE
hash_update(VALUE hash1, VALUE hash2)
{
    rb_hash_modify(hash1);
    hash2 = to_hash(hash2);
    if (rb_block_given_p()) {
        rb_hash_foreach(hash2, rb_hash_update_block_i, hash1);
    }
    else {
        rb_hash_foreach(hash2, rb_hash_update_i, hash1);
    }
    return hash1;
}

static void
set_merge(Set *self, Set *other_set)
{
    /* TODO find a better way of running Hash#update than copying code */
    hash_update(self->hash, other_set->hash);
}

static void
set_merge_i(VALUE e, Set *set, Set *o_set)
{
    set_add(set, e);
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
    else if (TYPE(a_enum) == T_ARRAY)
        set_do_with_array(set, rb_set_merge_i, 0, a_enum);
    else {
        /* TODO: rb_set_do_with_enum(enum) { |o| add(o) } */
    }

    return self;
}

static void
set_subtract(Set *self, Set *other_set)
{
    /*TODO: implement*/
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

static VALUE
set_dup(VALUE set)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_f(self));
    Set *new_set  = get_set_ptr(new);
    Set *orig_set = get_set_ptr(set);
    set_replace(new_set, self_set);

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
    Set *enum_set = get_set_ptr(a_enum);
    set_merge(new_set, enum_set);

    return new;
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
    Set *new_set  = get_set_ptr(new);
    Set *enum_set = get_set_ptr(a_enum);
    set_subtract(new_set, enum_set);

    return new;
}

/*
 * Document-method: & 
 * call-seq: &(enum)
 *
 * Returns a new set containing elements common to the set and the
 * given enumerable object.
 */
static VALUE
rb_set_intersction(VALUE self, VALUE a_enum)
{
    /* TODO: check if there's not better way of checking classes */
    VALUE new = set_new(rb_class_of(self));
    /* TODO: rb_set_do_with_enum(enum) { |o| add(o) if include?(o)} */
    return new;
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
    /* TODO: rb_set_each(self)
             { |o| if n.include?(o) then n.delete(o) else n.add(o) end } */
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
    /* TODO: implement */
}

static VALUE
rb_set_hash(VALUE self)
{
    Set *set = get_set_ptr(self);
    return set->hash;
}

static VALUE
rb_set_eql(VALUE self, VALUE other)
{
    Set *self_set = get_set_ptr(self);
    Set *other_set = get_set_ptr(other);
    if (rb_class_of(other) != rb_cSet)
        return Qfalse;
    /* TODO: implement hash_eql*/
    return hash_eql(self_set->hash, other_set->hash);
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

    static VALUE
    set_classify_i(VALUE key, VALUE value)
    {
        VALUE x = rb_yield(key);
        /* TODO: check if there's not better way of checking classes */
        VALUE new = set_new(rb_class_of(self));
        Set *new_set;
        if (rb_hash_lookup2(hash, x, Qnil) == Qnil) {
            rb_hash_aset(hash, x, new);
            new_set = get_set_ptr(new);
            set_add(new, key);
        }
        return ST_CONTINUE;
    }

    set_no_block_given(self, rb_intern("classify"));

    rb_hash_foreach(self->hash, set_classify_i, 0);

    return hash;
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
    /* TODO: implement */
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
    /* TODO: implement */
}

static VALUE
rb_set_pretty_print(VALUE self, VALUE pp)
{   
    /* TODO: implement */
}

static VALUE
rb_set_pretty_print_cycle(VALUE self, VALUE pp)
{   
    /* TODO: implement */
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
