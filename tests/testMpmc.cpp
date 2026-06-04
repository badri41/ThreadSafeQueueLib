#include "tsfqueue.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

class BlockingMPMCQueueBasicTest : public ::testing::Test {
protected:
  using IntQueue = tsfqueue::blockingMpmcUnbounded<int>;

  void SetUp() override {}

  void TearDown() override {}
};

class BlockingMPMCQueueThreadTest : public ::testing::Test {
protected:
  using IntQueue = tsfqueue::blockingMpmcUnbounded<int>;

  void WaitForThreads(std::vector<std::thread> &threads) {
	for (auto &t : threads) {
	  if (t.joinable()) {
		t.join();
	  }
	}
  }
};

class BlockingMPMCQueueTypeTest : public ::testing::TestWithParam<int> {
protected:
  using IntQueue = tsfqueue::blockingMpmcUnbounded<int>;
};

// BASIC TESTS

TEST_F(BlockingMPMCQueueBasicTest, Fault_in_initialisation_that_is_constructor) {
  IntQueue q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0);
}

TEST_F(BlockingMPMCQueueBasicTest, Push_Single_Element) {
  IntQueue q;
  q.push(42);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 1);
}

TEST_F(BlockingMPMCQueueBasicTest, Push_Multiple_Elements) {
  IntQueue q;
  for (int i = 0; i < 10; ++i) {
	q.push(i);
  }
  EXPECT_EQ(q.size(), 10);
  EXPECT_FALSE(q.empty());
}

TEST_F(BlockingMPMCQueueBasicTest, Try_PopEmpty_Queue_Returns_False) {
  IntQueue q;
  int value = -1;
  bool result = q.tryPop(value);
  EXPECT_FALSE(result);
  EXPECT_EQ(value, -1);
  EXPECT_TRUE(q.empty());
}

TEST_F(BlockingMPMCQueueBasicTest, Try_PopEmpty_Queue_Returns_Null) {
  IntQueue q;
  auto result = q.tryPop();
  EXPECT_EQ(result, nullptr);
}

TEST_F(BlockingMPMCQueueBasicTest, Try_Pop_Single_Element) {
  IntQueue q;
  q.push(42);

  int value;
  bool result = q.tryPop(value);

  EXPECT_TRUE(result);
  EXPECT_EQ(value, 42);
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0);
}

TEST_F(BlockingMPMCQueueBasicTest, Try_Pop_Multiple_Elements) {
  IntQueue q;

  for (int i = 0; i < 5; ++i) {
	q.push(i * 10);
  }

  for (int i = 0; i < 5; ++i) {
	int value;
	bool result = q.tryPop(value);
	EXPECT_TRUE(result);
	EXPECT_EQ(value, i * 10);
  }

  EXPECT_TRUE(q.empty());
}

TEST_F(BlockingMPMCQueueBasicTest, Try_Pop_Shared_Ptr_Variant) {
  IntQueue q;
  q.push(99);

  auto result = q.tryPop();
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(*result, 99);
}

TEST_F(BlockingMPMCQueueBasicTest, FIFO_Order) {
  IntQueue q;
  std::vector<int> values = {100, 200, 300, 400, 500};

  for (int v : values) {
	q.push(v);
  }

  for (int expected : values) {
	int actual;
	q.tryPop(actual);
	EXPECT_EQ(actual, expected);
  }
}

TEST_F(BlockingMPMCQueueBasicTest, Correct_size_tracking) {
  IntQueue q;

  EXPECT_EQ(q.size(), 0);

  for (int i = 0; i < 20; ++i) {
	q.push(i);
	EXPECT_EQ(q.size(), i + 1);
  }

  for (int i = 0; i < 20; ++i) {
	int dummy;
	q.tryPop(dummy);
	EXPECT_EQ(q.size(), 19 - i);
  }
}

TEST_F(BlockingMPMCQueueBasicTest, Wait_And_Pop_Shared_Ptr_Variant) {
  IntQueue q;
  q.push(42);

  auto result = q.waitAndPop();
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(*result, 42);
}

TEST_F(BlockingMPMCQueueBasicTest, Wait_And_Pop_Reference_Variant) {
  IntQueue q;
  q.push(123);

  int value;
  q.waitAndPop(value);
  EXPECT_EQ(value, 123);
}

// MULTI-THREADED TESTS

TEST_F(BlockingMPMCQueueThreadTest, Single_Producer_Single_Consumer) {
  IntQueue q;
  std::vector<int> produced;
  std::vector<int> consumed;

  auto producer = std::thread([&q, &produced]() {
	for (int i = 0; i < 100; ++i) {
	  q.push(i);
	  produced.push_back(i);
	  std::this_thread::yield(); // Allow context switches
	}
  });

  auto consumer = std::thread([&q, &consumed]() {
	for (int i = 0; i < 100; ++i) {
	  int value;
	  while (!q.tryPop(value)) {
		std::this_thread::yield();
	  }
	  consumed.push_back(value);
	}
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(consumed, produced);
  EXPECT_TRUE(q.empty());
}

TEST_F(BlockingMPMCQueueThreadTest, Multiple_Producers_Single_Consumer) {
  IntQueue q;
  const int NUM_PRODUCERS = 4;
  const int ITEMS_PER_PRODUCER = 25;

  std::vector<std::thread> producers;
  std::vector<int> consumed;

  //  producer threads
  for (int p = 0; p < NUM_PRODUCERS; ++p) {
	producers.emplace_back([&q, p]() {
	  for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
		q.push(p * 1000 + i);
	  }
	});
  }

  // Consumer thread
  auto consumer = std::thread([&q, &consumed]() {
	int count = 0;
	while (count < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
	  int value;
	  if (q.tryPop(value)) {
		consumed.push_back(value);
		++count;
	  }
	  std::this_thread::yield();
	}
  });

  WaitForThreads(producers);
  consumer.join();

  EXPECT_EQ(consumed.size(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);
  EXPECT_TRUE(q.empty());
}

TEST_F(BlockingMPMCQueueThreadTest, Single_Producer_Multiple_Consumers) {
  IntQueue q;
  const int NUM_CONSUMERS = 4;
  const int TOTAL_ITEMS = 100;

  std::vector<std::thread> consumers;
  std::vector<std::vector<int>> consumed(NUM_CONSUMERS);
  std::mutex resultMutex;
  std::atomic<bool> producerDone{false};

  // Launch consumer threads
  for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
	consumers.emplace_back([&q, &consumed, &resultMutex, &producerDone, c]() {
	  int value;
	  // Keep trying to pop items until producer is done AND queue is empty
	  while (true) {
		if (q.tryPop(value)) {
		  {
			std::lock_guard<std::mutex> lock(resultMutex);
			consumed[c].push_back(value);
		  }
		} else if (producerDone) {
		  // Producer done and got nothing, exit
		  break;
		} else {
		  // Queue empty but producer still working, wait and retry
		  std::this_thread::yield();
		}
	  }
	});
  }

  // Producer thread
  auto producer = std::thread([&q, TOTAL_ITEMS]() {
	for (int i = 0; i < TOTAL_ITEMS; ++i) {
	  q.push(i);
	}
  });

  producer.join();
  producerDone = true;

  WaitForThreads(consumers);

  // {checking all the items are consumed no dupliactes are there or nothing is
  // missed}
  size_t totalConsumed = 0;
  for (const auto &c : consumed) {
	totalConsumed += c.size();
  }
  EXPECT_EQ(totalConsumed, static_cast<size_t>(TOTAL_ITEMS));
  EXPECT_TRUE(q.empty());
}

TEST_F(BlockingMPMCQueueThreadTest, MultipleProducersMultipleConsumers) {
  IntQueue q;
  const int NUM_PRODUCERS = 3;
  const int NUM_CONSUMERS = 3;
  const int ITEMS_PER_PRODUCER = 50;

  std::vector<std::thread> producerThreads;
  std::vector<std::thread> consumerThreads;
  std::vector<int> allConsumed;
  std::mutex resultMutex;
  std::atomic<bool> producersDone{false};

  // Launch producer threads
  for (int p = 0; p < NUM_PRODUCERS; ++p) {
	producerThreads.emplace_back([&q, p]() {
	  for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
		q.push(p * 10000 + i);
	  }
	});
  }

  // Launch consumer threads
  for (int c = 0; c < NUM_CONSUMERS; ++c) {
	consumerThreads.emplace_back([&q, &allConsumed, &resultMutex, &producersDone]() {
	  int value;
	  while (true) {
		if (q.tryPop(value)) {
		  std::lock_guard<std::mutex> lock(resultMutex);
		  allConsumed.push_back(value);
		} else if (producersDone.load(std::memory_order_acquire)) {
		  // Producers are done; try one last drain in case items were pushed
		  // between our tryPop and the flag check
		  while (q.tryPop(value)) {
			std::lock_guard<std::mutex> lock(resultMutex);
			allConsumed.push_back(value);
		  }
		  break;
		} else {
		  std::this_thread::yield();
		}
	  }
	});
  }

  // Wait for all producers to finish
  for (auto &t : producerThreads) {
	t.join();
  }

  // Signal consumers that no more items will be produced
  producersDone.store(true, std::memory_order_release);

  // Wait for all consumers to finish draining
  for (auto &t : consumerThreads) {
	t.join();
  }

  EXPECT_EQ(allConsumed.size(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);
  EXPECT_TRUE(q.empty());
}

// DATA INTEGRITY TESTS

TEST_F(BlockingMPMCQueueBasicTest, DataIntegrity_LargeNumbers) {
  IntQueue q;
  std::vector<int> original = {-2147483648, -1, 0, 1, 2147483647};

  for (int val : original) {
	q.push(val);
  }

  for (int expected : original) {
	int actual;
	q.tryPop(actual);
	EXPECT_EQ(actual, expected);
  }
}

TEST_F(BlockingMPMCQueueBasicTest, DataIntegrity_After_Multiple_Pushes) {
  IntQueue q;
  const int NUM_ITERATIONS = 1000;

  for (int i = 0; i < NUM_ITERATIONS; ++i) {
	q.push(i);
	int popped;
	q.tryPop(popped);
	EXPECT_EQ(popped, i);
	EXPECT_TRUE(q.empty());
  }
}

// EDGE CASES AND STRESS TESTS

TEST_F(BlockingMPMCQueueBasicTest, AlternatingPushPop) {
  IntQueue q;

  for (int i = 0; i < 100; ++i) {
	q.push(i);
	EXPECT_FALSE(q.empty());

	int val;
	q.tryPop(val);
	EXPECT_EQ(val, i);
	EXPECT_TRUE(q.empty());
  }
}

TEST_F(BlockingMPMCQueueThreadTest, RapidProducerConsumer) {
  IntQueue q;
  const int ITEMS = 500;
  std::vector<int> consumed;
  std::mutex resultMutex;

  auto producer = std::thread([&q, ITEMS]() {
	for (int i = 0; i < ITEMS; ++i) {
	  q.push(i);
	}
  });

  auto consumer = std::thread([&q, &consumed, &resultMutex, ITEMS]() {
	int count = 0;
	while (count < ITEMS) {
	  int value;
	  if (q.tryPop(value)) {
		{
		  std::lock_guard<std::mutex> lock(resultMutex);
		  consumed.push_back(value);
		}
		++count;
	  }
	}
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(consumed.size(), ITEMS);
}

TEST_F(BlockingMPMCQueueBasicTest, LargeQueueCapacity) {
  IntQueue q;
  const int LARGE_SIZE = 10000;

  // Push large number of items
  for (int i = 0; i < LARGE_SIZE; ++i) {
	q.push(i);
  }
  EXPECT_EQ(q.size(), LARGE_SIZE);

  // Pop all items
  for (int i = 0; i < LARGE_SIZE; ++i) {
	int val;
	bool result = q.tryPop(val);
	EXPECT_TRUE(result);
	EXPECT_EQ(val, i);
  }
  EXPECT_TRUE(q.empty());
}

// SIZE AND EMPTY CHECKS

TEST_F(BlockingMPMCQueueBasicTest, Empty_After_Push_And_Pop) {
  IntQueue q;

  for (int i = 0; i < 50; ++i) {
	q.push(i);
	EXPECT_EQ(q.size(), 1);
	EXPECT_FALSE(q.empty());

	int val;
	q.tryPop(val);
	EXPECT_EQ(q.size(), 0);
	EXPECT_TRUE(q.empty());
  }
}

TEST_F(BlockingMPMCQueueBasicTest, Size_Tracking) {
  IntQueue q;

  size_t expectedSize = 0;

  for (int i = 0; i < 100; ++i) {
	q.push(i);
	++expectedSize;
	EXPECT_EQ(q.size(), expectedSize);
  }

  for (int i = 0; i < 100; ++i) {
	int dummy;
	q.tryPop(dummy);
	--expectedSize;
	EXPECT_EQ(q.size(), expectedSize);
  }
}

// PARAMETERIZED TESTS (Run same test with multiple scenarios)

INSTANTIATE_TEST_SUITE_P(QueueSizeVariations, BlockingMPMCQueueTypeTest,
						 ::testing::Values(0, 1, 10, 100, 1000, 10000));

TEST_P(BlockingMPMCQueueTypeTest, PushPopMultipleItems) {
  IntQueue q;
  int count = GetParam();

  for (int i = 0; i < count; ++i) {
	q.push(i);
  }

  for (int i = 0; i < count; ++i) {
	int val;
	q.tryPop(val);
	EXPECT_EQ(val, i);
  }

  EXPECT_TRUE(q.empty());
}
