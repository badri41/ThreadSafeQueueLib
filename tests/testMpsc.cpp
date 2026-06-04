#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <chrono>

#include <lockfreeMpscUnbounded/queue.hpp> 

using namespace tsfqueue::impl;


class MPSCTest : public ::testing::Test {
protected:
	lockfreeMpscUnbounded<int, std::allocator<int>, true> q;
	
	void SetUp() override {
	 
	}
	void TearDown() override {
	   
	}
};

// --- BASIC TESTS ---

TEST_F(MPSCTest, Is_Empty_Initially) {
	EXPECT_TRUE(q.empty());
	EXPECT_EQ(q.getSize(), 0u);
}

TEST_F(MPSCTest, Push_Pop_Works) {
	q.push(100);
	EXPECT_FALSE(q.empty());
	EXPECT_EQ(q.getSize(), 1u);
	
	int result = 0;
	EXPECT_TRUE(q.tryPop(result));
	EXPECT_EQ(result, 100);
	EXPECT_TRUE(q.empty());
}

TEST_F(MPSCTest, Emplace_Works) {
	lockfreeMpscUnbounded<std::string, std::allocator<std::string>, true> strQ;
	strQ.emplace(5, 'A'); // Constructs "AAAAA" in place
	EXPECT_EQ(strQ.getSize(), 1u);
	
	std::string res;
	EXPECT_TRUE(strQ.tryPop(res));
	EXPECT_EQ(res, "AAAAA");
}

TEST_F(MPSCTest, Maintains_Order_SPSC) {
	q.push(1);
	q.push(2);
	q.push(3);
	
	int result = 0;
	q.tryPop(result); EXPECT_EQ(result, 1);
	q.tryPop(result); EXPECT_EQ(result, 2);
	q.tryPop(result); EXPECT_EQ(result, 3);
}

TEST_F(MPSCTest, Peek_Works_Without_Removing) {
	q.push(42);
	
	int peekVal = 0;
	EXPECT_TRUE(q.peek(peekVal));
	EXPECT_EQ(peekVal, 42);
	EXPECT_EQ(q.getSize(), 1u);
}

TEST_F(MPSCTest, Move_Constructor_Works) {
	q.push(10);
	q.push(20);

	lockfreeMpscUnbounded<int, std::allocator<int>, true> movedQ(std::move(q));

	// Original should be empty after move
	EXPECT_TRUE(q.empty());
	EXPECT_EQ(movedQ.getSize(), 2u);

	int val;
	EXPECT_TRUE(movedQ.tryPop(val)); EXPECT_EQ(val, 10);
	EXPECT_TRUE(movedQ.tryPop(val)); EXPECT_EQ(val, 20);
}

// --- ADVANCED / RIGOROUS TESTS ---

TEST(MPSCObjectTests, No_Memory_Leaks) {
	auto myObject = std::make_shared<int>(99);
	std::weak_ptr<int> tracker = myObject;
	
	{
		lockfreeMpscUnbounded<std::shared_ptr<int>> queue;
		queue.push(myObject);
		myObject.reset(); 
		EXPECT_FALSE(tracker.expired());
	}
	EXPECT_TRUE(tracker.expired()); 
}

// testing multiple producers and single consumer
TEST_F(MPSCTest, Multiple_Producers_Single_Consumer) {
	const int numProducers = 4;
	const int itemsPerProducer = 25000;
	const int totalItems = numProducers * itemsPerProducer;

	std::atomic<bool> startFlag{false};
	std::atomic<int> producersDone{0};

	auto producer = [&](int producerId) {
		while (!startFlag.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
		for (int i = 0; i < itemsPerProducer; ++i) {
			q.push(producerId * itemsPerProducer + i);
		}
		producersDone.fetch_add(1, std::memory_order_release);
	};

	std::atomic<int> popCount{0};
	auto consumer = [&]() {
		while (!startFlag.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}

		while (popCount.load(std::memory_order_acquire) < totalItems) {
			int val = -1;
			if (q.tryPop(val)) {
				popCount.fetch_add(1, std::memory_order_relaxed);
			} else if (producersDone.load(std::memory_order_acquire) == numProducers && q.empty()) {
				// Safety break if we missed something (should not happen in a correct mpsc)
				break;
			} else {
				std::this_thread::yield();
			}
		}
	};

	std::vector<std::thread> threads;
	for (int i = 0; i < numProducers; ++i) {
		threads.emplace_back(producer, i);
	}
	threads.emplace_back(consumer);

	// Start all
	startFlag.store(true, std::memory_order_release);

	for (auto& t : threads) {
		t.join();
	}

	EXPECT_TRUE(q.empty());
	EXPECT_EQ(popCount.load(), totalItems);
}

// Structure for integrity check
struct MpscData {
	int producerId;
	int seqId;
	char payload[32];

	MpscData() : producerId(-1), seqId(-1) { payload[0] = '\0'; }

	MpscData(int pId, int sId) : producerId(pId), seqId(sId) {
		snprintf(payload, sizeof(payload), "P%d-S%d", pId, sId);
	}

	bool isValid() const {
		if (producerId < 0 || seqId < 0) return false;
		char expected[32];
		snprintf(expected, sizeof(expected), "P%d-S%d", producerId, seqId);
		return strncmp(payload, expected, 32) == 0;
	}
};

TEST(MPSCObjectTests, MultiThreaded_ComplexObjectIntegrity) {
	lockfreeMpscUnbounded<MpscData> objQ;
	const int numProducers = 6;
	const int itemsPerProducer = 50000;
	const int totalItems = numProducers * itemsPerProducer;

	std::atomic<bool> startFlag{false};

	auto producer = [&](int pId) {
		while (!startFlag.load(std::memory_order_acquire)) {
			// spin
		}
		for (int i = 0; i < itemsPerProducer; ++i) {
			objQ.push(MpscData(pId, i));
		}
	};

	auto consumer = [&]() {
		while (!startFlag.load(std::memory_order_acquire)) {
			// spin
		}
		
		int count = 0;
		while (count < totalItems) {
			MpscData data;
			// using waitAndPop
			objQ.waitAndPop(data);
			EXPECT_TRUE(data.isValid());
			count++;
		}
	};

	std::vector<std::thread> producers;
	for (int i = 0; i < numProducers; ++i) {
		producers.emplace_back(producer, i);
	}
	std::thread cons(consumer);

	startFlag.store(true, std::memory_order_release);

	for (auto& p : producers) p.join();
	cons.join();

	EXPECT_TRUE(objQ.empty());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
