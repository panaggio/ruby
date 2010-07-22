require 'test/unit'
require 'thread'

class TestSemaphore < Test::Unit::TestCase
  def test_semaphore_1
    reader_writer(10, Semaphore)
  end

  def test_semaphore_2
    producer_consumer(3, 5, Semaphore)
  end

  def test_thread_semaphore_1
    reader_writer(10, Thread::Semaphore)
  end

  def test_thread_semaphore_2
    producer_consumer(3, 5, Thread::Semaphore)
  end

  def reader_writer(num_msgs, klass, *args)
    wsem = Semaphore.new
    rsem = Semaphore.new
    readcount = 0

    stream = []
    read_msgs = []
    msgs = (1..num_msgs).to_a

    reader = Thread.new do
      while read_msgs.size < num_msgs
        rsem.wait
          readcount += 1
          wsem.wait if readcount == 1
        rsem.signal

        read_msgs << stream.pop
        p [msgs, stream, read_msgs]

        rsem.wait
          readcount -= 1
          wsem.signal if readcount.zero?
        rsem.signal
      end
    end

    writer = Thread.new do
      while msgs.size > 0
        wsem.wait
          stream << msgs.pop
        p [msgs, stream, read_msgs]
        wsem.signal
      end
    end

    reader.join
    writer.join

    assert_equal stream.size, 0
    assert_equal readcount, 0
    assert_equal msgs.size, 0
    assert_equal read_msgs.size, num_msgs
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
          pipe.push producer_data.pop
          p [producer_data, pipe, consumer_data]
        end
        fillcount.up
      end
    end

    consumer = Thread.new do
      while consumer_data.size < data_count
        fillcount.down
        mutex.synchronize do
          consumer_data.push pipe.pop
          p [producer_data, pipe, consumer_data]
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
