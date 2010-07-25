require 'test/unit'
require 'thread'
require 'extthread'

class TestSemaphore < Test::Unit::TestCase
  def test_semaphore_1
    producer_consumer(3, 1500, CountingSemaphore)
  end

  def test_semaphore_2
    producer_consumer(30, 1500, CountingSemaphore)
  end

  def test_thread_semaphore_1
    producer_consumer(3, 1500, Thread::CountingSemaphore)
  end

  def test_thread_semaphore_2
    producer_consumer(30, 1500, Thread::CountingSemaphore)
  end

  def producer_consumer(buffer_size, data_count, klass, *args)
    fillcount  = klass.new(0)
    emptycount = klass.new(buffer_size)
    pipe = []
    mutex = Mutex.new

    producer_data = (1..data_count).to_a
    consumer_data = []

    producer = Thread.new do
      while producer_data.size > 0
        emptycount.down
        mutex.synchronize do
          pipe.push producer_data.shift
        end
        fillcount.up
      end
    end

    consumer = Thread.new do
      while consumer_data.size < data_count
        fillcount.down
        mutex.synchronize do
          consumer_data.push pipe.shift
        end
        emptycount.up
      end
    end

    consumer.join
    producer.join

    assert_equal 0, producer_data.size
    assert_equal data_count, consumer_data.size
  end
end
