require 'test/unit'
require "cset"

class TC_CSet < Test::Unit::TestCase
  def test_aref
    assert_nothing_raised {
      CSet[]
      CSet[nil]
      CSet[1,2,3]
    }

    assert_equal(0, CSet[].size)
    assert_equal(1, CSet[nil].size)
    assert_equal(1, CSet[[]].size)
    assert_equal(1, CSet[[nil]].size)

    set = CSet[2,4,6,4]
    assert_equal(CSet.new([2,4,6]), set)
  end

  def test_s_new
    assert_nothing_raised {
      CSet.new()
      CSet.new(nil)
      CSet.new([])
      CSet.new([1,2])
      CSet.new('a'..'c')
    }
    assert_raises(ArgumentError) {
      CSet.new(false)
    }
    assert_raises(ArgumentError) {
      CSet.new(1)
    }
    assert_raises(ArgumentError) {
      CSet.new(1,2)
    }

    assert_equal(0, CSet.new().size)
    assert_equal(0, CSet.new(nil).size)
    assert_equal(0, CSet.new([]).size)
    assert_equal(1, CSet.new([nil]).size)

    ary = [2,4,6,4]
    set = CSet.new(ary)
    ary.clear
    assert_equal(false, set.empty?)
    assert_equal(3, set.size)

    ary = [1,2,3]

    s = CSet.new(ary) { |o| o * 2 }
    assert_equal([2,4,6], s.sort)
  end

  def test_clone
    set1 = CSet.new
    set2 = set1.clone
    set1 << 'abc'
    assert_equal(CSet.new, set2)
  end

  def test_dup
    set1 = CSet[1,2]
    set2 = set1.dup

    assert_not_same(set1, set2)

    assert_equal(set1, set2)

    set1.add(3)

    assert_not_equal(set1, set2)
  end

  def test_size
    assert_equal(0, CSet[].size)
    assert_equal(2, CSet[1,2].size)
    assert_equal(2, CSet[1,2,1].size)
  end

  def test_empty?
    assert_equal(true, CSet[].empty?)
    assert_equal(false, CSet[1, 2].empty?)
  end

  def test_clear
    set = CSet[1,2]
    ret = set.clear

    assert_same(set, ret)
    assert_equal(true, set.empty?)
  end

  def test_replace
    set = CSet[1,2]
    ret = set.replace('a'..'c')

    assert_same(set, ret)
    assert_equal(CSet['a','b','c'], set)
  end

  def test_to_a
    set = CSet[1,2,3,2]
    ary = set.to_a

    assert_equal([1,2,3], ary.sort)
  end

  def test_flatten
    # test1
    set1 = CSet[
      1,
      CSet[
	5,
	CSet[7,
	  CSet[0]
	],
	CSet[6,2],
	1
      ],
      3,
      CSet[3,4]
    ]

    set2 = set1.flatten
    set3 = CSet.new(0..7)

    assert_not_same(set2, set1)
    assert_equal(set3, set2)

    # test2; destructive
    orig_set1 = set1
    set1.flatten!

    assert_same(orig_set1, set1)
    assert_equal(set3, set1)

    # test3; multiple occurrences of a set in an set
    set1 = CSet[1, 2]
    set2 = CSet[set1, CSet[set1, 4], 3]

    assert_nothing_raised {
      set2.flatten!
    }

    assert_equal(CSet.new(1..4), set2)

    # test4; recursion
    set2 = CSet[]
    set1 = CSet[1, set2]
    set2.add(set1)

    assert_raises(ArgumentError) {
      set1.flatten!
    }

    # test5; miscellaneous
    empty = CSet[]
    set =  CSet[CSet[empty, "a"],CSet[empty, "b"]]

    assert_nothing_raised {
      set.flatten
    }

    set1 = empty.merge(CSet["no_more", set])

    assert_nil(CSet.new(0..31).flatten!)

    x = CSet[CSet[],CSet[1,2]].flatten!
    y = CSet[1,2]

    assert_equal(x, y)
  end

  def test_include?
    set = CSet[1,2,3]

    assert_equal(true, set.include?(1))
    assert_equal(true, set.include?(2))
    assert_equal(true, set.include?(3))
    assert_equal(false, set.include?(0))
    assert_equal(false, set.include?(nil))

    set = CSet["1",nil,"2",nil,"0","1",false]
    assert_equal(true, set.include?(nil))
    assert_equal(true, set.include?(false))
    assert_equal(true, set.include?("1"))
    assert_equal(false, set.include?(0))
    assert_equal(false, set.include?(true))
  end

  def test_superset?
    set = CSet[1,2,3]

    assert_raises(ArgumentError) {
      set.superset?()
    }

    assert_raises(ArgumentError) {
      set.superset?(2)
    }

    assert_raises(ArgumentError) {
      set.superset?([2])
    }

    assert_equal(true, set.superset?(CSet[]))
    assert_equal(true, set.superset?(CSet[1,2]))
    assert_equal(true, set.superset?(CSet[1,2,3]))
    assert_equal(false, set.superset?(CSet[1,2,3,4]))
    assert_equal(false, set.superset?(CSet[1,4]))

    assert_equal(true, CSet[].superset?(CSet[]))
  end

  def test_proper_superset?
    set = CSet[1,2,3]

    assert_raises(ArgumentError) {
      set.proper_superset?()
    }

    assert_raises(ArgumentError) {
      set.proper_superset?(2)
    }

    assert_raises(ArgumentError) {
      set.proper_superset?([2])
    }

    assert_equal(true, set.proper_superset?(CSet[]))
    assert_equal(true, set.proper_superset?(CSet[1,2]))
    assert_equal(false, set.proper_superset?(CSet[1,2,3]))
    assert_equal(false, set.proper_superset?(CSet[1,2,3,4]))
    assert_equal(false, set.proper_superset?(CSet[1,4]))

    assert_equal(false, CSet[].proper_superset?(CSet[]))
  end

  def test_subset?
    set = CSet[1,2,3]

    assert_raises(ArgumentError) {
      set.subset?()
    }

    assert_raises(ArgumentError) {
      set.subset?(2)
    }

    assert_raises(ArgumentError) {
      set.subset?([2])
    }

    assert_equal(true, set.subset?(CSet[1,2,3,4]))
    assert_equal(true, set.subset?(CSet[1,2,3]))
    assert_equal(false, set.subset?(CSet[1,2]))
    assert_equal(false, set.subset?(CSet[]))

    assert_equal(true, CSet[].subset?(CSet[1]))
    assert_equal(true, CSet[].subset?(CSet[]))
  end

  def test_proper_subset?
    set = CSet[1,2,3]

    assert_raises(ArgumentError) {
      set.proper_subset?()
    }

    assert_raises(ArgumentError) {
      set.proper_subset?(2)
    }

    assert_raises(ArgumentError) {
      set.proper_subset?([2])
    }

    assert_equal(true, set.proper_subset?(CSet[1,2,3,4]))
    assert_equal(false, set.proper_subset?(CSet[1,2,3]))
    assert_equal(false, set.proper_subset?(CSet[1,2]))
    assert_equal(false, set.proper_subset?(CSet[]))

    assert_equal(false, CSet[].proper_subset?(CSet[]))
  end

  def test_each
    ary = [1,3,5,7,10,20]
    set = CSet.new(ary)

    ret = set.each { |o| }
    assert_same(set, ret)

    e = set.each
    assert_instance_of(Enumerator, e)

    assert_nothing_raised {
      set.each { |o|
	ary.delete(o) or raise "unexpected element: #{o}"
      }

      ary.empty? or raise "forgotten elements: #{ary.join(', ')}"
    }
  end

  def test_add
    set = CSet[1,2,3]

    ret = set.add(2)
    assert_same(set, ret)
    assert_equal(CSet[1,2,3], set)

    ret = set.add?(2)
    assert_nil(ret)
    assert_equal(CSet[1,2,3], set)

    ret = set.add(4)
    assert_same(set, ret)
    assert_equal(CSet[1,2,3,4], set)

    ret = set.add?(5)
    assert_same(set, ret)
    assert_equal(CSet[1,2,3,4,5], set)
  end

  def test_delete
    set = CSet[1,2,3]

    ret = set.delete(4)
    assert_same(set, ret)
    assert_equal(CSet[1,2,3], set)

    ret = set.delete?(4)
    assert_nil(ret)
    assert_equal(CSet[1,2,3], set)

    ret = set.delete(2)
    assert_equal(set, ret)
    assert_equal(CSet[1,3], set)

    ret = set.delete?(1)
    assert_equal(set, ret)
    assert_equal(CSet[3], set)
  end

  def test_delete_if
    set = CSet.new(1..10)
    ret = set.delete_if { |i| i > 10 }
    assert_same(set, ret)
    assert_equal(CSet.new(1..10), set)

    set = CSet.new(1..10)
    ret = set.delete_if { |i| i % 3 == 0 }
    assert_same(set, ret)
    assert_equal(CSet[1,2,4,5,7,8,10], set)
  end

  def test_collect!
    set = CSet[1,2,3,'a','b','c',-1..1,2..4]

    ret = set.collect! { |i|
      case i
      when Numeric
	i * 2
      when String
	i.upcase
      else
	nil
      end
    }

    assert_same(set, ret)
    assert_equal(CSet[2,4,6,'A','B','C',nil], set)
  end

  def test_reject!
    set = CSet.new(1..10)

    ret = set.reject! { |i| i > 10 }
    assert_nil(ret)
    assert_equal(CSet.new(1..10), set)

    ret = set.reject! { |i| i % 3 == 0 }
    assert_same(set, ret)
    assert_equal(CSet[1,2,4,5,7,8,10], set)
  end

  def test_merge
    set = CSet[1,2,3]

    ret = set.merge([2,4,6])
    assert_same(set, ret)
    assert_equal(CSet[1,2,3,4,6], set)
  end

  def test_subtract
    set = CSet[1,2,3]

    ret = set.subtract([2,4,6])
    assert_same(set, ret)
    assert_equal(CSet[1,3], set)
  end

  def test_plus
    set = CSet[1,2,3]

    ret = set + [2,4,6]
    assert_not_same(set, ret)
    assert_equal(CSet[1,2,3,4,6], ret)
  end

  def test_minus
    set = CSet[1,2,3]

    ret = set - [2,4,6]
    assert_not_same(set, ret)
    assert_equal(CSet[1,3], ret)
  end

  def test_and
    set = CSet[1,2,3,4]

    ret = set & [2,4,6]
    assert_not_same(set, ret)
    assert_equal(CSet[2,4], ret)
  end

  def test_xor
    set = CSet[1,2,3,4]
    ret = set ^ [2,4,5,5]
    assert_not_same(set, ret)
    assert_equal(CSet[1,3,5], ret)
  end

  def test_eq
    set1 = CSet[2,3,1]
    set2 = CSet[1,2,3]

    assert_equal(set1, set1)
    assert_equal(set1, set2)
    assert_not_equal(CSet[1], [1])

    set1 = Class.new(CSet)["a", "b"]
    set2 = CSet["a", "b", set1]
    set1 = set1.add(set1.clone)

#    assert_equal(set1, set2)
#    assert_equal(set2, set1)
    assert_equal(set2, set2.clone)
    assert_equal(set1.clone, set1)

    assert_not_equal(CSet[Exception.new,nil], CSet[Exception.new,Exception.new], "[ruby-dev:26127]")
  end

  # def test_hash
  # end

  # def test_eql?
  # end

  def test_classify
    set = CSet.new(1..10)
    ret = set.classify { |i| i % 3 }

    assert_equal(3, ret.size)
    assert_instance_of(Hash, ret)
    ret.each_value { |value| assert_instance_of(CSet, value) }
    assert_equal(CSet[3,6,9], ret[0])
    assert_equal(CSet[1,4,7,10], ret[1])
    assert_equal(CSet[2,5,8], ret[2])
  end

  def test_divide
    set = CSet.new(1..10)
    ret = set.divide { |i| i % 3 }

    assert_equal(3, ret.size)
    n = 0
    ret.each { |s| n += s.size }
    assert_equal(set.size, n)
    assert_equal(set, ret.flatten)

    set = CSet[7,10,5,11,1,3,4,9,0]
    ret = set.divide { |a,b| (a - b).abs == 1 }

    assert_equal(4, ret.size)
    n = 0
    ret.each { |s| n += s.size }
    assert_equal(set.size, n)
    assert_equal(set, ret.flatten)
    ret.each { |s|
      if s.include?(0)
	assert_equal(CSet[0,1], s)
      elsif s.include?(3)
	assert_equal(CSet[3,4,5], s)
      elsif s.include?(7)
	assert_equal(CSet[7], s)
      elsif s.include?(9)
	assert_equal(CSet[9,10,11], s)
      else
	raise "unexpected group: #{s.inspect}"
      end
    }
  end

  def test_inspect
    set1 = CSet[1]

    assert_equal('#<CSet: {1}>', set1.inspect)

    set2 = CSet[CSet[0], 1, 2, set1]
    assert_equal(false, set2.inspect.include?('#<CSet: {...}>'))

    set1.add(set2)
    assert_equal(true, set1.inspect.include?('#<CSet: {...}>'))
  end

  # def test_pretty_print
  # end

  # def test_pretty_print_cycle
  # end
end

class TC_SortedCSet < Test::Unit::TestCase
  def test_sortedset
    s = SortedCSet[4,5,3,1,2]

    assert_equal([1,2,3,4,5], s.to_a)

    prev = nil
    s.each { |o| assert(prev < o) if prev; prev = o }
    assert_not_nil(prev)

    s.map! { |o| -2 * o }

    assert_equal([-10,-8,-6,-4,-2], s.to_a)

    prev = nil
    ret = s.each { |o| assert(prev < o) if prev; prev = o }
    assert_not_nil(prev)
    assert_same(s, ret)

    s = SortedCSet.new([2,1,3]) { |o| o * -2 }
    assert_equal([-6,-4,-2], s.to_a)

    s = SortedCSet.new(['one', 'two', 'three', 'four'])
    a = []
    ret = s.delete_if { |o| a << o; o.start_with?('t') }
    assert_same(s, ret)
    assert_equal(['four', 'one'], s.to_a)
    assert_equal(['four', 'one', 'three', 'two'], a)

    s = SortedCSet.new(['one', 'two', 'three', 'four'])
    a = []
    ret = s.reject! { |o| a << o; o.start_with?('t') }
    assert_same(s, ret)
    assert_equal(['four', 'one'], s.to_a)
    assert_equal(['four', 'one', 'three', 'two'], a)

    s = SortedCSet.new(['one', 'two', 'three', 'four'])
    a = []
    ret = s.reject! { |o| a << o; false }
    assert_same(nil, ret)
    assert_equal(['four', 'one', 'three', 'two'], s.to_a)
    assert_equal(['four', 'one', 'three', 'two'], a)
  end
end

class TC_Enumerable < Test::Unit::TestCase
  def test_to_set
    ary = [2,5,4,3,2,1,3]

    set = ary.to_set
    assert_instance_of(CSet, set)
    assert_equal([1,2,3,4,5], set.sort)

    set = ary.to_set { |o| o * -2 }
    assert_instance_of(CSet, set)
    assert_equal([-10,-8,-6,-4,-2], set.sort)

    set = ary.to_set(SortedCSet)
    assert_instance_of(SortedCSet, set)
    assert_equal([1,2,3,4,5], set.to_a)

    set = ary.to_set(SortedCSet) { |o| o * -2 }
    assert_instance_of(SortedCSet, set)
    assert_equal([-10,-8,-6,-4,-2], set.sort)
  end
end

# class TC_RestricedCSet < Test::Unit::TestCase
#   def test_s_new
#     assert_raises(ArgumentError) { RestricedCSet.new }
#
#     s = RestricedCSet.new([-1,2,3]) { |o| o > 0 }
#     assert_equal([2,3], s.sort)
#   end
#
#   def test_restriction_proc
#     s = RestricedCSet.new([-1,2,3]) { |o| o > 0 }
#
#     f = s.restriction_proc
#     assert_instance_of(Proc, f)
#     assert(f[1])
#     assert(!f[0])
#   end
#
#   def test_replace
#     s = RestricedCSet.new(-3..3) { |o| o > 0 }
#     assert_equal([1,2,3], s.sort)
#
#     s.replace([-2,0,3,4,5])
#     assert_equal([3,4,5], s.sort)
#   end
#
#   def test_merge
#     s = RestricedCSet.new { |o| o > 0 }
#     s.merge(-5..5)
#     assert_equal([1,2,3,4,5], s.sort)
#
#     s.merge([10,-10,-8,8])
#     assert_equal([1,2,3,4,5,8,10], s.sort)
#   end
# end
