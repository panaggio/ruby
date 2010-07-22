#include <ruby.h>

RUBY_EXTERN size_t rb_objspace_data_type_memsize(VALUE);
RUBY_EXTERN size_t rb_ary_memsize(VALUE);

static VALUE
rb_ary_buf_new(void)
{
    VALUE ary = rb_ary_tmp_new(1);
    OBJ_UNTRUST(ary);
    return ary;
}

static void
wakeup_first_thread(VALUE list)
{
    VALUE thread;

    while (!NIL_P(thread = rb_ary_shift(list))) {
       if (RTEST(rb_thread_wakeup_alive(thread))) break;
    }
}

static void
wakeup_all_threads(VALUE list)
{
    VALUE thread, list0 = list;
    long i;

    list = rb_ary_subseq(list, 0, LONG_MAX);
    rb_ary_clear(list0);
    for (i = 0; i < RARRAY_LEN(list); ++i) {
       thread = RARRAY_PTR(list)[i];
       rb_thread_wakeup_alive(thread);
    }
    RB_GC_GUARD(list);
}

/*
 *  Document-class: ConditionVariable
 *
 *  ConditionVariable objects augment class Mutex. Using condition variables,
 *  it is possible to suspend while in the middle of a critical section until a
 *  resource becomes available.
 *
 *  Example:
 *
 *    require 'thread'
 *
 *    mutex = Mutex.new
 *    resource = ConditionVariable.new
 *
 *    a = Thread.new {
 *      mutex.synchronize {
 *        # Thread 'a' now needs the resource
 *        resource.wait(mutex)
 *        # 'a' can now have the resource
 *      }
 *    }
 *
 *    b = Thread.new {
 *      mutex.synchronize {
 *        # Thread 'b' has finished using the resource
 *        resource.signal
 *      }
 *    }
 */

typedef struct {
    VALUE waiters;
} CondVar;

static void
condvar_mark(void *ptr)
{
    CondVar *condvar = ptr;
    rb_gc_mark(condvar->waiters);
}

#define condvar_free RUBY_TYPED_DEFAULT_FREE

static size_t
condvar_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
       const CondVar *condvar = ptr;
       size = sizeof(CondVar);
       size += rb_ary_memsize(condvar->waiters);
    }
    return size;
}

static const rb_data_type_t condvar_data_type = {
    "condvar",
    {condvar_mark, condvar_free, condvar_memsize,},
};

#define GetCondVarPtr(obj, tobj) \
    TypedData_Get_Struct(obj, CondVar, &condvar_data_type, tobj)

static CondVar *
get_condvar_ptr(VALUE self)
{
    CondVar *condvar;
    GetCondVarPtr(self, condvar);
    if (!condvar->waiters) {
       rb_raise(rb_eArgError, "uninitialized CondionVariable");
    }
    return condvar;
}

static VALUE
condvar_alloc(VALUE klass)
{
    CondVar *condvar;
    return TypedData_Make_Struct(klass, CondVar, &condvar_data_type, condvar);
}

static void
condvar_initialize(CondVar *condvar)
{
    condvar->waiters = rb_ary_buf_new();
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new condvar.
 */

static VALUE
rb_condvar_initialize(VALUE self)
{
    CondVar *condvar;
    GetCondVarPtr(self, condvar);

    condvar_initialize(condvar);

    return self;
}

struct sleep_call {
    int argc;
    VALUE *argv;
};

static VALUE
do_sleep(VALUE args)
{
    struct sleep_call *p = (struct sleep_call *)args;
    return rb_funcall(p->argv[0], rb_intern("sleep"), p->argc-1, p->argv+1);
}

static VALUE
delete_current_thread(VALUE ary)
{
    return rb_ary_delete(ary, rb_thread_current());
}

/*
 * Document-method: wait
 * call-seq: wait(mutex, timeout=nil)
 *
 * Releases the lock held in +mutex+ and waits; reacquires the lock on wakeup.
 *
 * If +timeout+ is given, this method returns after +timeout+ seconds passed,
 * even if no other thread doesn't signal.
 */

static VALUE
rb_condvar_wait(int argc, VALUE *argv, VALUE self)
{
    VALUE waiters = get_condvar_ptr(self)->waiters;
    struct sleep_call args;

    args.argc = argc;
    args.argv = argv;
    rb_ary_push(waiters, rb_thread_current());
    rb_ensure(do_sleep, (VALUE)&args, delete_current_thread, waiters);
    return self;
}

/*
 * Document-method: signal
 * call-seq: signal
 *
 * Wakes up the first thread in line waiting for this lock.
 */

static VALUE
rb_condvar_signal(VALUE self)
{
    wakeup_first_thread(get_condvar_ptr(self)->waiters);
    return self;
}

/*
 * Document-method: broadcast
 * call-seq: broadcast
 *
 * Wakes up all threads waiting for this lock.
 */

static VALUE
rb_condvar_broadcast(VALUE self)
{
    wakeup_all_threads(get_condvar_ptr(self)->waiters);
    return self;
}

/*
 *  Document-class: Queue
 *
 *  This class provides a way to synchronize communication between threads.
 *
 *  Example:
 *
 *    require 'thread'
 *    queue = Queue.new
 *
 *  producer = Thread.new do
 *    5.times do |i|
 *      sleep rand(i) # simulate expense
 *      queue << i
 *      puts "#{i} produced"
 *    end
 *  end
 *
 *  consumer = Thread.new do
 *    5.times do |i|
 *      value = queue.pop
 *      sleep rand(i/2) # simulate expense
 *      puts "consumed #{value}"
 *    end
 *  end
 *
 */

typedef struct {
    VALUE mutex;
    VALUE que;
    VALUE waiting;
} Queue;

static void
queue_mark(void *ptr)
{
    Queue *queue = ptr;
    rb_gc_mark(queue->mutex);
    rb_gc_mark(queue->que);
    rb_gc_mark(queue->waiting);
}

#define queue_free RUBY_TYPED_DEFAULT_FREE

static size_t
queue_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
       const Queue *queue = ptr;
       size = sizeof(Queue);
       size += rb_objspace_data_type_memsize(queue->mutex);
       size += rb_ary_memsize(queue->que);
       size += rb_ary_memsize(queue->waiting);
    }
    return size;
}

static const rb_data_type_t queue_data_type = {
    "queue",
    {queue_mark, queue_free, queue_memsize,},
};

#define GetQueuePtr(obj, tobj) \
    TypedData_Get_Struct(obj, Queue, &queue_data_type, tobj)

static Queue *
get_queue_ptr(VALUE self)
{
    Queue *queue;
    GetQueuePtr(self, queue);
    if (!queue->mutex || !queue->que || !queue->waiting) {
       rb_raise(rb_eArgError, "uninitialized Queue");
    }
    return queue;
}

static VALUE
queue_alloc(VALUE klass)
{
    Queue *queue;
    return TypedData_Make_Struct(klass, Queue, &queue_data_type, queue);
}

static void
queue_initialize(Queue *queue)
{
    queue->mutex = rb_mutex_new();
    RBASIC(queue->mutex)->klass = 0;
    queue->que = rb_ary_buf_new();
    queue->waiting = rb_ary_buf_new();
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new queue.
 */

static VALUE
rb_queue_initialize(VALUE self)
{
    Queue *queue;
    GetQueuePtr(self, queue);

    queue_initialize(queue);

    return self;
}

struct synchronize_call_args {
    Queue *queue;
    VALUE (*func)(Queue *queue, VALUE arg);
    VALUE arg;
};

static VALUE
queue_synchronize_call(VALUE args)
{
    struct synchronize_call_args *p = (struct synchronize_call_args *)args;
    return (*p->func)(p->queue, p->arg);
}

static VALUE
queue_synchronize(Queue *queue, VALUE (*func)(Queue *queue, VALUE arg), VALUE arg)
{
    struct synchronize_call_args args;
    args.queue = queue;
    args.func = func;
    args.arg = arg;
    rb_mutex_lock(queue->mutex);
    return rb_ensure(queue_synchronize_call, (VALUE)&args, rb_mutex_unlock, queue->mutex);
}

static VALUE
queue_do_push(Queue *queue, VALUE obj)
{
    rb_ary_push(queue->que, obj);
    wakeup_first_thread(queue->waiting);
    return Qnil;
}

/*
 * Document-method: push
 * call-seq: push(obj)
 *
 * Pushes +obj+ to the queue.
 */

static VALUE
rb_queue_push(VALUE self, VALUE obj)
{
    queue_synchronize(get_queue_ptr(self), queue_do_push, obj);
    return self;
}

static VALUE
queue_do_pop(Queue *queue, VALUE should_block)
{
    while (!RARRAY_LEN(queue->que)) {
       if (!(int)should_block) {
           rb_raise(rb_eThreadError, "queue empty");
       }
       rb_ary_push(queue->waiting, rb_thread_current());
       rb_mutex_sleep(queue->mutex, Qnil);
    }

    return rb_ary_shift(queue->que);
}

static int
queue_pop_should_block(int argc, VALUE *argv)
{
    int should_block = 1;
    switch (argc) {
      case 0:
       break;
      case 1:
       should_block = !RTEST(argv[0]);
       break;
      default:
       rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
    }
    return should_block;
}

/*
 * Document-method: pop
 * call_seq: pop(non_block=false)
 *
 * Retrieves data from the queue.  If the queue is empty, the calling thread is
 * suspended until data is pushed onto the queue.  If +non_block+ is true, the
 * thread isn't suspended, and an exception is raised.
 */

static VALUE
rb_queue_pop(int argc, VALUE *argv, VALUE self)
{
    Queue *queue = get_queue_ptr(self);
    int should_block = queue_pop_should_block(argc, argv);
    return queue_synchronize(queue, queue_do_pop, (VALUE)should_block);
}

static inline unsigned long
queue_length(Queue *queue)
{
    return (unsigned long)RARRAY_LEN(queue->que);
}

static inline unsigned long
queue_num_waiting(Queue *queue)
{
    return (unsigned long)RARRAY_LEN(queue->waiting);
}

/*
 * Document-method: empty?
 * call-seq: empty?
 *
 * Returns +true+ if the queue is empty.
 */

static VALUE
rb_queue_empty_p(VALUE self)
{
    return queue_length(get_queue_ptr(self)) == 0 ? Qtrue : Qfalse;
}

/*
 * Document-method: clear
 * call-seq: clear
 *
 * Removes all objects from the queue.
 */

static VALUE
rb_queue_clear(VALUE self)
{
    Queue *queue = get_queue_ptr(self);

    rb_ary_clear(queue->que);

    return self;
}

/*
 * Document-method: length
 * call-seq: length
 *
 * Returns the length of the queue.
 */

static VALUE
rb_queue_length(VALUE self)
{
    unsigned long len = queue_length(get_queue_ptr(self));
    return ULONG2NUM(len);
}

/*
 * Document-method: num_waiting
 * call-seq: num_waiting
 *
 * Returns the number of threads waiting on the queue.
 */

static VALUE
rb_queue_num_waiting(VALUE self)
{
    long len = queue_num_waiting(get_queue_ptr(self));
    return ULONG2NUM(len);
}

/*
 *  Document-class: SizedQueue
 *
 * This class represents queues of specified size capacity.  The push operation
 * may be blocked if the capacity is full.
 *
 * See Queue for an example of how a SizedQueue works.
 */

typedef struct  {
    Queue queue_;
    VALUE queue_wait;
    unsigned long max;
} SizedQueue;

static void
szqueue_mark(void *ptr)
{
    SizedQueue *szqueue = ptr;
    queue_mark(&szqueue->queue_);
    rb_gc_mark(szqueue->queue_wait);
}

#define szqueue_free queue_free

static size_t
szqueue_memsize(const void *ptr)
{
    size_t size = 0;
    if (ptr) {
       const SizedQueue *szqueue = ptr;
       size = sizeof(SizedQueue) - sizeof(Queue);
       size += queue_memsize(&szqueue->queue_);
       size += rb_ary_memsize(szqueue->queue_wait);
    }
    return size;
}

static const rb_data_type_t szqueue_data_type = {
    "sized_queue",
    {szqueue_mark, szqueue_free, szqueue_memsize,},
    &queue_data_type,
};

#define GetSizedQueuePtr(obj, tobj) \
    TypedData_Get_Struct(obj, SizedQueue, &szqueue_data_type, tobj)

static SizedQueue *
get_szqueue_ptr(VALUE self)
{
    SizedQueue *szqueue;
    GetSizedQueuePtr(self, szqueue);
    if (!szqueue->queue_.mutex || !szqueue->queue_.que || !szqueue->queue_.waiting || !szqueue->queue_wait) {
       rb_raise(rb_eArgError, "uninitialized Queue");
    }
    return szqueue;
}

static VALUE
szqueue_alloc(VALUE klass)
{
    SizedQueue *szqueue;
    return TypedData_Make_Struct(klass, SizedQueue, &szqueue_data_type, szqueue);
}

/*
 * Document-method: new
 * call-seq: new(max)
 *
 * Creates a fixed-length queue with a maximum size of +max+.
 */

static VALUE
rb_szqueue_initialize(VALUE self, VALUE vmax)
{
    long max;
    SizedQueue *szqueue;
    GetSizedQueuePtr(self, szqueue);

    max = NUM2LONG(vmax);
    if (max <= 0) {
       rb_raise(rb_eArgError, "queue size must be positive");
    }
    queue_initialize(&szqueue->queue_);
    szqueue->queue_wait = rb_ary_buf_new();
    szqueue->max = (unsigned long)max;

    return self;
}

/*
 * Document-method: max
 * call-seq: max
 *
 * Returns the maximum size of the queue.
 */

static VALUE
rb_szqueue_max_get(VALUE self)
{
    unsigned long max = get_szqueue_ptr(self)->max;
    return ULONG2NUM(max);
}

/*
 * Document-method: max=
 * call-seq: max=(n)
 *
 * Sets the maximum size of the queue.
 */

static VALUE
rb_szqueue_max_set(VALUE self, VALUE vmax)
{
    SizedQueue *szqueue = get_szqueue_ptr(self);
    long max = NUM2LONG(vmax), diff = 0;
    VALUE t;

    if (max <= 0) {
       rb_raise(rb_eArgError, "queue size must be positive");
    }
    if ((unsigned long)max > szqueue->max) {
       diff = max - szqueue->max;
    }
    szqueue->max = max;
    while (diff > 0 && !NIL_P(t = rb_ary_shift(szqueue->queue_wait))) {
       rb_thread_wakeup_alive(t);
    }
    return vmax;
}

static VALUE
szqueue_do_push(Queue *queue, VALUE obj)
{
    SizedQueue *szqueue = (SizedQueue *)queue;

    while (queue_length(queue) >= szqueue->max) {
       rb_ary_push(szqueue->queue_wait, rb_thread_current());
       rb_mutex_sleep(queue->mutex, Qnil);
    }
    return queue_do_push(queue, obj);
}

/*
 * Document-method: push
 * call-seq: push(obj)
 *
 * Pushes +obj+ to the queue.  If there is no space left in the queue, waits
 * until space becomes available.
 */

static VALUE
rb_szqueue_push(VALUE self, VALUE obj)
{
    queue_synchronize(&get_szqueue_ptr(self)->queue_, szqueue_do_push, obj);
    return self;
}

static VALUE
szqueue_do_pop(Queue *queue, VALUE should_block)
{
    SizedQueue *szqueue = (SizedQueue *)queue;
    VALUE retval = queue_do_pop(queue, should_block);

    if (queue_length(queue) < szqueue->max) {
       wakeup_first_thread(szqueue->queue_wait);
    }

    return retval;
}

/*
 * Document-method: pop
 * call_seq: pop(non_block=false)
 *
 * Returns the number of threads waiting on the queue.
 */

static VALUE
rb_szqueue_pop(int argc, VALUE *argv, VALUE self)
{
    SizedQueue *szqueue = get_szqueue_ptr(self);
    int should_block = queue_pop_should_block(argc, argv);
    return queue_synchronize(&szqueue->queue_, szqueue_do_pop, (VALUE)should_block);
}

/*
 * Document-method: pop
 * call_seq: pop(non_block=false)
 *
 * Returns the number of threads waiting on the queue.
 */

static VALUE
rb_szqueue_num_waiting(VALUE self)
{
    SizedQueue *szqueue = get_szqueue_ptr(self);
    long len = queue_num_waiting(&szqueue->queue_);
    len += RARRAY_LEN(szqueue->queue_wait);
    return ULONG2NUM(len);
}

/*
 * Document-class: Semaphore
 *
 * TODO: Copy lib/thread.rb documentation to here
 *
 * Example:
 *
 *   require 'thread'
 *
 *   TODO: Copy lib/thread.rb example to here
 */

#define GetSempahorePtr(obj, tobj) \
    TypedData_Get_Struct(obj, semaphore_t, &semaphore_data_type, tobj)

/* TODO: check if there's no need for a structure */
#define semaphore_t rb_thread_semaphore_t

#define semaphore_mark NULL

static void
semaphore_free(void *ptr)
{
   if (ptr) {
       semaphore_t *sem = ptr;
       native_sem_destroy(sem);
   }
   ruby_xfree(ptr);
}

static size_t
semaphore_memsize(const void *ptr)
{
    return ptr ? sizeof(semaphore_t) : 0;
}

static const rb_data_type_t semaphore_data_type = {
    "semaphore",
    {semaphore_mark, semaphore_free, semaphore_memsize,},
};

static VALUE
semaphore_alloc(VALUE klass)
{
    VALUE volatile obj;
    semaphore_t *sem;

    obj = TypedData_Make_Struct(klass, semaphore_t, &semaphore_data_type, sem);
    return obj;
}

static VALUE semaphore_initialize1(VALUE self, unsigned int init_value);

/*
 *  call-seq:
 *     Semaphore.new   -> semaphore
 *
 *  Creates a new Semaphore
 */
static VALUE
semaphore_initialize(int argc, VALUE *argv, VALUE self)
{
    unsigned int init_value = 0;
    switch (argc) {
        case 0:
         break;
        case 1:
         init_value = NUM2UINT(argv[0]);
         break;
        default:
         rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
    }

    semaphore_initialize1(self, init_value);
    return self;
}
static VALUE
semaphore_initialize1(VALUE self, unsigned int init_value)
{
    semaphore_t *sem;
    GetSempahorePtr(self, sem);

    native_sem_initialize(sem, init_value);
    return self;
}

VALUE
rb_semaphore_new(unsigned int init_value)
{
    VALUE sem = semaphore_alloc(rb_cSemaphore);
    return semaphore_initialize1(sem, init_value);
}

/*
 * call-seq:
 *    semaphore.wait
 *
 * Attempts to enter and waits if the semaphore is already full
 */
VALUE
rb_semaphore_wait(VALUE self)
{
    semaphore_t *sem;
    GetSempahorePtr(self, sem);
    native_sem_wait(sem);
    return self;
}

/*
 * call-seq:
 *    semaphore.signal
 *
 * Leaves and let another thread in, if there's any waiting
 */
VALUE
rb_semaphore_signal(VALUE self)
{
    semaphore_t *sem;
    GetSempahorePtr(self, sem);
    native_sem_signal(sem);
    return self;
}

#ifndef UNDER_THREAD
#define UNDER_THREAD 1
#endif

void
Init_extthread(void)
{
#if UNDER_THREAD
#define DEFINE_CLASS_UNDER_THREAD(name, super) rb_define_class_under(rb_cThread, #name, super)
#define ALIAS_GLOBCAL_CONST(name) do {                 \
       ID id = rb_intern_const(#name);                 \
       if (!rb_const_defined_at(rb_cObject, id)) {     \
           rb_const_set(rb_cObject, id, rb_c##name);   \
       }                                               \
    } while (0)
#else
#define DEFINE_CLASS_UNDER_THREAD(name, super) rb_define_class(name, super)
#define ALIAS_GLOBCAL_CONST(name) do { /* nothing */ } while (0)
#endif
    VALUE rb_cConditionVariable = DEFINE_CLASS_UNDER_THREAD(ConditionVariable, rb_cObject);
    VALUE rb_cQueue = DEFINE_CLASS_UNDER_THREAD(Queue, rb_cObject);
    VALUE rb_cSizedQueue = DEFINE_CLASS_UNDER_THREAD(SizedQueue, rb_cQueue);
    VALUE rb_cSemaphore = DEFINE_CLASS_UNDER_THREAD(Semaphore, rb_cObject);

    rb_define_alloc_func(rb_cConditionVariable, condvar_alloc);
    rb_define_method(rb_cConditionVariable, "initialize", rb_condvar_initialize, 0);
    rb_define_method(rb_cConditionVariable, "wait", rb_condvar_wait, -1);
    rb_define_method(rb_cConditionVariable, "signal", rb_condvar_signal, 0);
    rb_define_method(rb_cConditionVariable, "broadcast", rb_condvar_broadcast, 0);

    rb_define_alloc_func(rb_cQueue, queue_alloc);
    rb_define_method(rb_cQueue, "initialize", rb_queue_initialize, 0);
    rb_define_method(rb_cQueue, "push", rb_queue_push, 1);
    rb_define_method(rb_cQueue, "pop", rb_queue_pop, -1);
    rb_define_method(rb_cQueue, "empty?", rb_queue_empty_p, 0);
    rb_define_method(rb_cQueue, "clear", rb_queue_clear, 0);
    rb_define_method(rb_cQueue, "length", rb_queue_length, 0);
    rb_define_method(rb_cQueue, "num_waiting", rb_queue_num_waiting, 0);
    rb_alias(rb_cQueue, rb_intern("enq"), rb_intern("push"));
    rb_alias(rb_cQueue, rb_intern("<<"), rb_intern("push"));
    rb_alias(rb_cQueue, rb_intern("deq"), rb_intern("pop"));
    rb_alias(rb_cQueue, rb_intern("shift"), rb_intern("pop"));
    rb_alias(rb_cQueue, rb_intern("size"), rb_intern("length"));

    rb_define_alloc_func(rb_cSizedQueue, szqueue_alloc);
    rb_define_method(rb_cSizedQueue, "initialize", rb_szqueue_initialize, 1);
    rb_define_method(rb_cSizedQueue, "max", rb_szqueue_max_get, 0);
    rb_define_method(rb_cSizedQueue, "max=", rb_szqueue_max_set, 1);
    rb_define_method(rb_cSizedQueue, "push", rb_szqueue_push, 1);
    rb_define_method(rb_cSizedQueue, "pop", rb_szqueue_pop, -1);
    rb_define_method(rb_cSizedQueue, "num_waiting", rb_szqueue_num_waiting, 0);
    rb_alias(rb_cSizedQueue, rb_intern("enq"), rb_intern("push"));
    rb_alias(rb_cSizedQueue, rb_intern("<<"), rb_intern("push"));
    rb_alias(rb_cSizedQueue, rb_intern("deq"), rb_intern("pop"));
    rb_alias(rb_cSizedQueue, rb_intern("shift"), rb_intern("pop"));

    rb_cSemaphore = DEFINE_CLASS_UNDER_THREAD("Semaphore", rb_cObject);
    rb_define_alloc_func(rb_cSemaphore, semaphore_alloc);
    rb_define_method(rb_cSemaphore, "initialize", semaphore_initialize, -1);
    rb_define_method(rb_cSemaphore, "wait", rb_semaphore_wait, 0);
    rb_define_method(rb_cSemaphore, "signal", rb_semaphore_signal, 0);
    rb_alias(rb_cSemaphore, rb_intern("down"), rb_intern("wait"));
    rb_alias(rb_cSemaphore, rb_intern("P"), rb_intern("wait"));
    rb_alias(rb_cSemaphore, rb_intern("up"), rb_intern("signal"));
    rb_alias(rb_cSemaphore, rb_intern("V"), rb_intern("signal"));

    rb_provide("thread.rb");
    ALIAS_GLOBCAL_CONST(ConditionVariable);
    ALIAS_GLOBCAL_CONST(Queue);
    ALIAS_GLOBCAL_CONST(SizedQueue);
    ALIAS_GLOBCAL_CONST(Semaphore);
}
