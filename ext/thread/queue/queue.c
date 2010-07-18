#include <ruby.h>

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

typedef struct _Queue {
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

#if 0
static void
queue_free(void *ptr)
{
    if (ptr) {
       Queue *queue = ptr;
       long i, size = RARRAY_LEN(queue->waiting);
       for (i = 0; i < size; i++) {
           rb_thread_kill(rb_ary_entry(queue->waiting, i));
       }
       ruby_xfree(ptr);
    }
}
#else
#define queue_free RUBY_TYPED_DEFAULT_FREE
#endif

RUBY_EXTERN size_t rb_objspace_data_type_memsize(VALUE);
RUBY_EXTERN size_t rb_ary_memsize(VALUE);

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
rb_ary_buf_new(void)
{
    VALUE ary = rb_ary_tmp_new(1);
    OBJ_UNTRUST(ary);
    return ary;
}

static VALUE
queue_alloc(VALUE klass)
{
    Queue *queue;
    return TypedData_Make_Struct(klass, Queue, &queue_data_type, queue);
}

/*
 * Document-method: new
 * call-seq: new
 *
 * Creates a new queue.
 *
 */

static VALUE
rb_queue_initialize(VALUE self)
{
    Queue *queue;
    GetQueuePtr(self, queue);

    queue->mutex = rb_mutex_new();
    RBASIC(queue->mutex)->klass = 0;
    queue->que = rb_ary_buf_new();
    queue->waiting = rb_ary_buf_new();

    return self;
}

/*
 * Document-method: push
 * call-seq: push(obj)
 *
 * Pushes +obj+ to the queue.
 *
 */

static VALUE
rb_queue_push(VALUE self, VALUE obj)
{
    VALUE thread;
    Queue *queue = get_queue_ptr(self);

    rb_mutex_lock(queue->mutex);

    rb_ary_push(queue->que, obj);

    do {
       thread = rb_ary_shift(queue->waiting);
    } while (!NIL_P(thread) && RTEST(rb_thread_wakeup_alive(thread)));

    rb_mutex_unlock(queue->mutex);

    return self;
}

/*
 * Document-method: pop
 * call_seq: pop(non_block=false)
 *
 * Retrieves data from the queue.  If the queue is empty, the calling thread is
 * suspended until data is pushed onto the queue.  If +non_block+ is true, the
 * thread isn't suspended, and an exception is raised.
 *
 */

static VALUE
rb_queue_pop(int argc, VALUE *argv, VALUE self)
{
    int should_block;
    VALUE poped, current_thread;
    Queue *queue = get_queue_ptr(self);

    switch (argc) {
      case 0:
       should_block = 1;
       break;
      case 1:
       should_block = !RTEST(argv[0]);
       break;
      default:
       rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
    }

    rb_mutex_lock(queue->mutex);

    while (!RARRAY_LEN(queue->que)) {
       if (!should_block) {
           rb_mutex_unlock(queue->mutex);
           rb_raise(rb_eThreadError, "queue empty");
       }
       current_thread = rb_thread_current();
       rb_ary_push(queue->waiting, current_thread);

       rb_mutex_sleep(queue->mutex, Qnil);
    }

    poped = rb_ary_shift(queue->que);

    rb_mutex_unlock(queue->mutex);

    return poped;
}

/*
 * Document-method: empty?
 * call-seq: empty?
 *
 * Returns +true+ if the queue is empty.
 *
 */

static VALUE
rb_queue_empty_p(VALUE self)
{
    VALUE result;
    Queue *queue = get_queue_ptr(self);

    rb_mutex_lock(queue->mutex);
    result = RARRAY_LEN(queue->que) == 0 ? Qtrue : Qfalse;
    rb_mutex_unlock(queue->mutex);

    return result;
}

/*
 * Document-method: clear
 * call-seq: clear
 *
 * Removes all objects from the queue.
 *
 */

static VALUE
rb_queue_clear(VALUE self)
{
    Queue *queue = get_queue_ptr(self);

    rb_mutex_lock(queue->mutex);
    rb_ary_clear(queue->que);
    rb_mutex_unlock(queue->mutex);

    return self;
}

/*
 * Document-method: length
 * call-seq: length
 *
 * Returns the length of the queue.
 *
 */

static VALUE
rb_queue_length(VALUE self)
{
    long len;
    VALUE result;
    Queue *queue = get_queue_ptr(self);

    rb_mutex_lock(queue->mutex);
    len = RARRAY_LEN(queue->que);
    result = ULONG2NUM(len);
    rb_mutex_unlock(queue->mutex);

    return result;
}

/*
 * Document-method: num_waiting
 * call-seq: num_waiting
 *
 * Returns the number of threads waiting on the queue.
 *
 */

static VALUE
rb_queue_num_waiting(VALUE self)
{
    long len;
    VALUE result;
    Queue *queue = get_queue_ptr(self);

    rb_mutex_lock(queue->mutex);
    len = RARRAY_LEN(queue->waiting);
    result = ULONG2NUM(len);
    rb_mutex_unlock(queue->mutex);

    return result;
}

void
Init_queue(void)
{
    VALUE rb_cQueue = rb_define_class_under(rb_cThread, "Queue", rb_cObject);
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
}
