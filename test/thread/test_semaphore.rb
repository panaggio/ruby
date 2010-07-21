require 'test/unit'
require 'thread'

class TestSemaphore < Test::Unit::TestCase
  def test_semaphore
    grind(3, 5, Semaphore)
  end

  def test_thread_semaphore
    grind(3, 5, Thread::Semaphore)
  end

  def grind(buffer_size, data_count, klass, *args)
    fillcount  = klass.new(0)
    emptycount = klass.new(buffer_size)
    pipe = []
    mutex = Mutex.new

    producer_data = (1..data_count).to_a
    producer = Thread.new do
      while producer_data.size > 0
        emptycount.down
        mutex.synchronize do
          pipe.push producer_data.pop
          p pipe
        end
        fillcount.up
      end
    end

    consumer_data = []
    consumer = Thread.new do
      while consumer_data.size < data_count
        fillcount.down
        mutex.synchronize do
          consumer_data.push pipe.pop
          p pipe
        end
        emptycount.up
      end
    end

    consumer.join
    producer.join

    assert_equal 0, sem.num_waiting
    assert_equal 0, producer_data.size
    assert_equal data_count, consumer_data.size
  end
end
