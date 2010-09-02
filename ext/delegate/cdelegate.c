/*
 *  = delegate -- Support for the Delegation Pattern
 * 
 *  Documentation by James Edward Gray II and Gavin Sinclair
 *
 *  == Introduction
 *
 *  This library provides three different ways to delegate method calls to an
 *  object.  The easiest to use is SimpleDelegator.  Pass an object to the
 *  constructor and all methods supported by the object will be delegated.  This
 *  object can be changed later.
 * 
 *  Going a step further, the top level DelegateClass method allows you to
 *  easily setup delegation through class inheritance.  This is considerably
 *  more flexible and thus probably the most common use for this library.
 *
 *  Finally, if you need full control over the delegation scheme, you can
 *  inherit from the abstract class Delegator and customize as needed.  (If
 *  you find yourself needing this control, have a look at _forwardable_, also
 *  in the standard library.  It may suit your needs better.)
 * 
 *  == Notes
 *
 *  Be advised, RDoc will not detect delegated methods.
 * 
 *  <b>delegate.rb provides full-class delegation via the
 *  DelegateClass() method.  For single-method delegation via
 *  def_delegator(), see forwardable.rb.</b>
 *
 *  == Examples
 * 
 *  === SimpleDelegator
 *
 *  Here's a simple example that takes advantage of the fact that
 *  SimpleDelegator's delegation object can be changed at any time.
 *
 *    class Stats
 *      def initialize
 *        @source = SimpleDelegator.new([])
 *      end
 * 
 *      def stats( records )
 *        @source.__setobj__(records)
 * 
 *        "Elements:  #{@source.size}\n" +
 *        " Non-Nil:  #{@source.compact.size}\n" +
 *        "  Unique:  #{@source.uniq.size}\n"
 *      end
 *    end
 * 
 *    s = Stats.new
 *    puts s.stats(%w{James Edward Gray II})
 *    puts
 *    puts s.stats([1, 2, 3, nil, 4, 5, 1, 2])
 *
 * <i>Prints:</i>
 *
 *   Elements:  4
 *    Non-Nil:  4
 *     Unique:  4
 *
 *   Elements:  8
 *    Non-Nil:  7
 *     Unique:  6
 *
 * === DelegateClass()
 *
 * Here's a sample of use from <i>tempfile.rb</i>.
 *
 * A _Tempfile_ object is really just a _File_ object with a few special rules
 * about storage location and/or when the File should be deleted.  That makes
 * for an almost textbook perfect example of how to use delegation.
 * 
 *   class Tempfile < DelegateClass(File)
 *     # constant and class member data initialization...
 * 
 *     def initialize(basename, tmpdir=Dir::tmpdir)
 *       # build up file path/name in var tmpname...
 * 
 *       @tmpfile = File.open(tmpname, File::RDWR|File::CREAT|File::EXCL, 0600)
 * 
 *       # ...
 *
 *       super(@tmpfile)
 *
 *       # below this point, all methods of File are supported...
 *     end
 *
 *     # ...
 *   end
 *
 * === Delegator
 *
 * SimpleDelegator's implementation serves as a nice example here.
 *
 *    class SimpleDelegator < Delegator
 *      def initialize(obj)
 *        # pass obj to Delegator constructor, required
 *        super             
 *
 *        # store obj for future use
 *        @delegate_sd_obj = obj 
 *      end
 *
 *      def __getobj__
 *        # return object we are delegating to, required
 *        @delegate_sd_obj       
 *      end
 *
 *      def __setobj__(obj)
 *        # change delegation object, a feature we're providing
 *        @delegate_sd_obj = obj
 *      end
 *
 *      # ...
 *    end
 */

/*
 *  Document-class: Delegator
 *
 *  Delegator is an abstract class used to build delegator pattern objects from
 *  subclasses.  Subclasses should redefine \_\_getobj\_\_.  For a concrete
 *  implementation, see SimpleDelegator.
 */

VALUE __setobj__;
VALUE __getobj__;
VALUE caller;
VALUE public_methods;
VALUE protected_methods;
VALUE not;
VALUE __v2__; 
VALUE iv_get;
VALUE iv_set;
VALUE trust;
VALUE untrust;
VALUE taint;
VALUE untaint;
VALUE freeze;

typedef struct {
} Delegator;

static void
delegator_mark(void *ptr)
{
    Delegator *delegator = ptr;
    rb_gc_mark(delegator->);
}

#define delegator_free RUBY_TYPED_DEFAULT_FREE

static size_t
delegator_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
        const Delegator *delegator = ptr;
        size = sizeof(Delegator);
        size += rb_*_memsize(condvar->);
    }
    return size;
}

static const rb_data_type_t delegator_data_type = {
    "delegator",
    {delegator_mark, delegator_free, delegator_memsize,},
};

#define GetDelegatorPtr(obj, tobj) \
    TypedData_Get_Struct(obj, Delegator, &delegator_data_type, tobj)

static Delegator *
get_delegator_ptr(VALUE self)
{
    Delegator *delegator;
    GetDelegatorPtr(self, delegator);
    if (!delegator->) {
        rb_raise(rb_eArgError, "uninitialized Delegator");
    }
    return delegator;
}

static VALUE
delegator_alloc(VALUE klass)
{
    Delegator *delegator;
    return TypedData_Make_Struct(klass, Delegator, &delegator_data_type, delegator);
}

static VALUE
delegator_self_getobj(VALUE self)
{
    return rb_funcall(self, __getobj__);
}

static VALUE
delegator_self_setobj(VALUE self)
{
    return rb_funcall(self, __setobj__);
}

static VALUE
rb_delegator_s_const_missing(VALUE klass, VALUE n)
{
    return rb_mod_const_missing(klass, n);
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Pass in the _obj_ to delegate method calls to.  All methods supported by
 * _obj_ will be delegated to.
 */
static VALUE
rb_delegator_initialize(VALUE self)
{
    Delegator *delegator;
    GetDelegatorPtr(self, delegator);

    delegator_self_setobj(self);
    return self;
}

static VALUE
rb_delegator_method_missing_i(VALUE args)
{
    VALUE argc = (int) ((VALUE *) args)[1];
    VALUE argv = (VALUE *) ((VALUE*) args)[2];
    VALUE mid = rb_intern(argv[0]);

    if (rb_respond_to(target, mid))
        return rb_funcall2(target, argc[0], argc-1, &argv[1]);
    return rb_call_super(argc, argv);
}

static VALUE
rb_delegator_method_missing_ii(VALUE args)
{
     /* TODO */
    return Qnil;
}

/*
 * Document-method: method_missing
 * call-seq: method_missing(m, *args, &block)
 *
 * Handles the magic of delegation through \_\_getobj\_\_.
 */
static VALUE
rb_delegator_method_missing(int argc, VALUE *argv, VALUE self)
{
    VALUE target = delegator_self_getobj(self);
    VALUE args;

    if (rb_respond_to(target, mid))
        return rb_funcall2(target, argc[0], argc-1, &argv[1]);
    /* FIXME discover possible exception raised here and prevent it */
    return rb_call_super(argc, argv);

    /* TODO */

    //return rb_ensure(delegator_missing_method_i, , delegator_missing_method_ii, );
}

/*
 * Document-method: respond_to_missing?
 * call-seq: respond_to_missing?(m, include_private)
 * 
 * Checks for a method provided by this the delegate object by forwarding the
 * call through \_\_getobj\_\_.
 */
static VALUE
rb_delegator_respond_to_missing_p(VALUE self, VALUE m, VALUE include_primate)
{
    VALUE mid = rb_intern(m);
    VALUE obj = delegator_self_getobj(self);
    int r = rb_obj_respond_to(obj, m, FIX2INT(include_private));
    if (r && include_private && !rb_respond_to(obj, m)){
        /* FIXME: find a better way to call caller(3)[0] */
        rb_warn("%s: delegator does not forward private method #%s", rb_ary_entry(rb_funcall(self, caller), 0));
        return Qfalse;
    }
    return r;
}

/*
 * Document-method: methods
 * call-seq: methods
 * 
 * Returns the methods available to this delegate object as the union
 * of this object's and \_\_getobj\_\_ methods.
 */
static VALUE
rb_delegator_methods(VALUE self)
{
    VALUE r = rb_funcall(delegator_self_getobj(self), methods);
    if (r != Qnil)
        return r;
    return rb_call_super(0, 0);
}

static VALUE
delegator_specific_methods(int argc, VALUE *ARGV, VALUE self, VALUE method)
{
    VALUE r;
    VALUE all = Qtrue;
    if (argc==1)
        all = argv[0];

    if (argc>1)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 1", argc);

    r = rb_funcall(delegator_self_getobj(self), method, 1, all);
    if (r != Qnil)
        return r;
    return rb_call_super(1, &all);
}

/*
 * Document-method: public_methods
 * call-seq: public_methods(all=true)
 * 
 * Returns the methods available to this delegate object as the union
 * of this object's and \_\_getobj\_\_ public methods.
 */
static VALUE
rb_delegator_public_methods(int argc, VALUE *argv, VALUE self)
{
    return delegator_specific_methods(argc, argv, self, public_methods);
}

/*
 * Document-method: protected_methods
 * call-seq: protected_methods(all=true)
 *
 * Returns the methods available to this delegate object as the union
 * of this object's and \_\_getobj\_\_ protected methods.
 */
static VALUE
rb_delegator_protected_methods(int argc, VALUE *argv, VALUE self)
{
    return delegator_specific_methods(argc, argv, self, protected_methods);
}

/*
 * Document-method: ==
 * call-seq: ==(obj)
 * 
 * Returns true if two objects are considered of equal value.
 */
static VALUE
rb_delegator_equal_p(VALUE self, VALUE obj)
{
    if (obj == self)
        return Qtrue;

    if (delegator_self_getobj(self) == obj)
        return Qtrue;
    return Qfalse;
}

/*
 * Document-method: !=
 * call-seq: !=(obj)
 * 
 * Returns true if two objects are not considered of equal value.
 */
static VALUE
rb_delegator_not_equal_p(VALUE self, VALUE obj)
{
    if (obj == self)
        return Qfalse;

    if (delegator_self_getobj(self) == obj)
        return Qfalse;
    return Qtrue;
}

static VALUE
rb_delegator_not(VALUE self)
{
    return rb_funcall(delegator_self_getobj(self), not);
}

/*
 * Document-method: __getobj__
 * call-seq: __getobj__
 * 
 * This method must be overridden by subclasses and should return the object
 * method calls are being delegated to.
 */
static VALUE
rb_delegator_getobj(VALUE self)
{
    return rb_raise(rb_eNotImpError, "need to define `__getobj__'");
}

/*
 * Document-method: __setobj__
 * call-seq: __setobj__(obj)
 * 
 * This method must be overridden by subclasses and change the object delegate
 * to _obj_.
 */
static VALUE
rb_delegator_setobj(VALUE self, VALUE obj)
{
    return rb_raise(rb_eNotImpError, "need to define `__setobj__'");
}

/*
 * Document-method: marshal_dump
 * call-seq: marshal_dump
 * 
 * Serialization support for the object returned by \_\_getobj\_\_.
 */
static VALUE
rb_delegator_marshal_dumpv2(VALUE self)
{
    int i;
    VALUE regexp = rb_reg_quote("\\A@delegate_");
    VALUE allivs = rb_obj_instance_variables(self);
    VALUE ivs = rb_ary_new();
    VALUE ivvals = rb_ary_new();

    for (i=0; i<RARRAY_LEN(allivs); i++)
        if (rb_reg_match(regexp, RARRAY_PTR(allivs)[i]) != Qnil) {
            rb_ary_push(ivs, RARRAY_PTR(allivs)[i]);
            rb_ary_push(ivvals, rb_funcall(self, iv_get, 1, RARRAY_PTR(allivs)[i]));
        }

    return rb_ary_new3(4, __v2__, ivs, ivsvals, delegator_self_getobj(self));
}

/*
 * Document-method: marshal_load
 * call-seq: marshal_load(data)
 * 
 * Reinitializes delegation from a serialized object.
 */
static VALUE
rb_delegator_marshal_loadv2(VALUE klass, VALUE data)
{
    int i;
    VALUE version = RARRAY_PTR(data)[0];
    VALUE vars = RARRAY_PTR(data)[1];
    VALUE vals = RARRAY_PTR(data)[2];
    VALUE obj = RARRAY_PTR(data)[3];

    if (version == __v2__) {
        for (i=0; i<RARRAY_LEN(vars); i++)
            rb_funcall(self, iv_set, 2, RARRAY_PTR(vars)[i], RARRAY_PTR(vals)[i]);
        return delegator_self_setobj(self, obj);
    }
    return delegator_self_setobj(self, data);
}

static VALUE
rb_delegator_initialize_clone(VALUE self, VALUE obj)
{
    return delegator_self_setobj(self, rb_obj_clone(delegator_self_getobj(obj)));
}

static VALUE
rb_delegator_initialize_dup(VALUE self, VALUE obj)
{
    return delegator_self_setobj(self, rb_obj_dup(delegator_self_getobj(obj)));
}

/*
 * Document-method: trust
 * call-seq: trust
 * 
 * Trust both the object returned by \_\_getobj\_\_ and self.
 */
static VALUE
rb_delegator_trust(VALUE self)
{
    rb_funcall(delegator_self_getobj(self), trust);
    return rb_call_super(0, 0);
}

/*
 * Document-method: untrust
 * call-seq: untrust
 * 
 * Untrust both the object returned by \_\_getobj\_\_ and self.
 */
static VALUE
rb_delegator_untrust(VALUE self)
{
    rb_funcall(delegator_self_getobj(self), untrust);
    return rb_call_super(0, 0);
}

/*
 * Document-method: taint
 * call-seq: taint
 * 
 * Taint both the object returned by \_\_getobj\_\_ and self. 
 */
static VALUE
rb_delegator_taint(VALUE self)
{
    rb_funcall(delegator_self_getobj(self), taint);
    return rb_call_super(0, 0);
}

/*
 * Document-method: untaint
 * call-seq: untaint
 * 
 * Untaint both the object returned by \_\_getobj\_\_ and self.
 */
static VALUE
rb_delegator_untaint(VALUE self)
{
    rb_funcall(delegator_self_getobj(self), untaint);
    return rb_call_super(0, 0);
}

/*
 * Document-method: freeze
 * call-seq: freeze
 * 
 * Freeze both the object returned by \_\_getobj\_\_ and self.
 */
static VALUE
rb_delegator_freeze(VALUE self)
{
    rb_funcall(delegator_self_getobj(self), freeze);
    return rb_call_super(0, 0);
}

static VALUE
rb_delegator_s_public_api(VALUE klass)
{
    return delegator_api;
}

static VALUE
delegating_block_i(VALUE args)
{
    return rb_funcall2(target, mid, argc, argv);
    VALUE regexp = rb_reg_quote("\\A@delegate_");
}

static VALUE
delegating_block_i(VALUE args)
{
    return rb_funcall2(target, mid, argc, argv);
}

static VALUE
delegating_block(VALUE args, VALUE mid, int argc, VALUE *argv)
{
    VALUE target = delegator_self_getobj(self);
    VALUE args[4] = {target, mid, (VALUE) argc, (VALUE) argv};
    return rb_ensure(delegating_block_i, , delegating_block_ii, );
}

static VALUE
rb_delegator_s_delegating_block(VALUE klass, VALUE mid)
{
    return rb_proc_new(delegating_block, mid);
}

/*
 * Document-class: SimpleDelegator
 *
 * A concrete implementation of Delegator, this class provides the means to
 * delegate all supported method calls to the object passed into the constructor
 * and even to change the object being delegated to at a later time with
 * \_\_setobj\_\_ .
 */

/*
 * Document-method: \_\_getobj\_\_
 * call-seq: \_\_getobj\_\_
 * 
 * Returns the current object method calls are being delegated to.
 */
static VALUE
rb_sdelegator_getobj(VALUE self)
{
    return rb_iv_get(self, "@delegate_sd_obj");
}

/*
 * Document-method: \_\_setobj\_\_
 * call-seq: \_\_setobj\_\_(obj)
 * 
 * Changes the delegate object to _obj_.
 *   
 * It's important to note that this does *not* cause SimpleDelegator's methods
 * to change.  Because of this, you probably only want to change delegation
 * to objects of the same type as the original delegate.
 *          
 * Here's an example of changing the delegation object.
 *               
 *   names = SimpleDelegator.new(%w{James Edward Gray II})
 *   puts names[1]    # => Edward
 *   names.__setobj__(%w{Gavin Sinclair})
 *   puts names[1]    # => Sinclair
 */
static VALUE
rb_sdelegator_setobj(VALUE self, VALUE obj)
{
    if (rb_funcall(self, equal_p, 1, obj))
        rb_raise(rb_eArgError, "cannot delegate to self");
    return rb_iv_set(self, "@delegate_sd_obj", obj);
}

/*
 * Document-class: SimpleDelegator
 * 
 *
 */

/*
 * Document-method: 
 * call-seq: _()
 * 
 * 
 */
static VALUE
rb_sdelegator__(int argc, VALUE *argv, VALUE self)
{
}

/*
 * Document-method: 
 * call-seq: _()
 * 
 * 
 */
static VALUE
rb_sdelegator__(int argc, VALUE *argv, VALUE self)
{
}

/*
 * Document-method: 
 * call-seq: _()
 * 
 * 
 */
static VALUE
rb_sdelegator__(int argc, VALUE *argv, VALUE self)
{
}

/*
 * Document-method: 
 * call-seq: _()
 * 
 * 
 */
static VALUE
rb_sdelegator__(int argc, VALUE *argv, VALUE self)
{
}

void
Init_cdelegator(void)
{
    rb_cDelegator =;

    VALUE kernel = rb_obj_dup(rb_mKernel);
    /* TODO:
     * kernel.class_eval do
     *   [:to_s,:inspect,:=~,:!~,:===,:<=>,:eql?,:hash].each do |m|
     *     undef_method_m 
     *   end
     * end
     */

    rb_include_module(rb_cDelegator, kernel);

    __setobj__ = rb_intern("__setobj__");
    __getobj__ = rb_intern("__getobj__");
    caller = rb_intern("caller");
    public_methods = rb_intern("public_methods");
    protected_methods = rb_intern("protected_methods");
    not = rb_intern("!");
    __v2__ = rb_intern("__v2__");
    iv_get = rb_intern("instance_variable_get");
    iv_set = rb_intern("instance_variable_set");
    trust = rb_intern("trust");
    untrust = rb_intern("untrust");
    taint = rb_intern("taint");
    untaint = rb_intern("untaint");
    freeze = rb_intern("freeze");
    equal_p = rb_intern("equal?");

    delegator_api = rb_class_public_instance_methods(0, 0, rb_cDelegator);
}
