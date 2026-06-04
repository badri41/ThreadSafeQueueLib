#include <gtest/gtest.h>
#include <thread>
#include <string>
#include <memory>

#include <lockfreeSpscBounded/queue.hpp> 

using namespace tsfqueue::impl;


class spscBoundedTest : public ::testing::Test {
protected:
	lockfreeSpscBounded<int, 100000> q;
	
	void SetUp() override {
	 
	}
	void TearDown() override {
	   
	}
};

//Basic Tests

// checking if it is empty initially
TEST_F(spscBoundedTest, Is_Empty_Initially) {
	EXPECT_TRUE(q.empty());
	EXPECT_EQ(q.size(), 0);
}

// checking if push and pop works
TEST_F(spscBoundedTest, Push_Pop_Works) {
	EXPECT_TRUE(q.tryPush(100));
	EXPECT_FALSE(q.empty());
	
	int result = 0;
	EXPECT_TRUE(q.tryPop(result));
	EXPECT_EQ(result, 100);
}

// checking if the queue maintains the FIFO order
TEST_F(spscBoundedTest, Maintains_Order) {
	EXPECT_TRUE(q.tryPush(1));
	EXPECT_TRUE(q.tryPush(2));
	EXPECT_TRUE(q.tryPush(3));
	
	int result = 0;
	EXPECT_TRUE(q.tryPop(result)); EXPECT_EQ(result, 1);
	EXPECT_TRUE(q.tryPop(result)); EXPECT_EQ(result, 2);
	EXPECT_TRUE(q.tryPop(result)); EXPECT_EQ(result, 3);
}

// checking if peek works
TEST_F(spscBoundedTest, Peek_Works_Without_Removing) {
	EXPECT_TRUE(q.tryPush(42));
	
	int peekVal = 0;
	EXPECT_TRUE(q.peek(peekVal));
	EXPECT_EQ(peekVal, 42);
	
	EXPECT_EQ(q.size(), 1);
}


// checking for memoryleaks since we are doing dynamic memory allocation
// basically  by the domino effect if the last pointed value is not there then everything else which is  a superset of it has also been destroyed properly
// this test can also be thought of as checking if my queues destructore destroys  everything properly in a proper manner
TEST(spscBoundedTests, No_Memory_Leaks) {
   
// used shared pointer so that i can have this tracker concept  to prove the domino effect

	auto myObject = std::make_shared<int>(99);
	std::weak_ptr<int> tracker = myObject;
	
	{
		lockfreeSpscBounded<std::shared_ptr<int>, 10> queue;
		
	   
		EXPECT_TRUE(queue.tryPush(myObject));
		myObject.reset();   // because the pushing mechanism creates a copy and increases strong reference count of the control block so -1 for the object.reset
		// now only queue's copied shared ptr has ownership
		
		EXPECT_FALSE(tracker.expired());
		
		
	}
	
	EXPECT_TRUE(tracker.expired()); 
}


// checking if the queue can handle large objects
TEST(spscBoundedTests, Handles_Large_Objects) {
	lockfreeSpscBounded<std::string, 100> stringQueue;
	
	
	std::string s(1000, 'X'); 
	
	EXPECT_TRUE(stringQueue.tryPush(s));
	
	std::string output;
	EXPECT_TRUE(stringQueue.tryPop(output));
	EXPECT_EQ(output.length(), 1000);
}



//testing spsc
TEST_F(spscBoundedTest,Testing_SPSC) {
	const int total = 100000;
	
	
	auto producer = [&]() {
		for (int i = 0; i < total; ++i) {
			q.waitAndPush(i);
		}
	};
	
	
	auto consumer = [&]() {
		for (int i = 0; i < total; ++i) {
			int val = -1;
		   
			q.waitAndPop(val); 
			EXPECT_EQ(val, i);
		}
	};
	
	std::thread prodThread(producer);
	std::thread consThread(consumer);
	
	prodThread.join();
	consThread.join();
	
	
	EXPECT_TRUE(q.empty());
}


//final check and also checking peek function
TEST_F(spscBoundedTest, finalSpsccheckWithPeekAndPop) {
	const int total = 50000;
	
	auto producer = [&]() {
		for (int i = 0; i < total; ++i) q.waitAndPush(i);
	};
	
	auto consumer = [&]() {
		int expected = 0;
		while (expected < total) {
			int peekVal = -1;
			int popVal = -1;
			
			
			if (q.peek(peekVal)) {
				
				bool success = q.tryPop(popVal);
				EXPECT_TRUE(success);
				EXPECT_EQ(peekVal, popVal);
				EXPECT_EQ(popVal, expected);
				expected++;
			}
		}
	};
	
	std::thread t1(producer);
	std::thread t2(consumer);
	t1.join();
	t2.join();
}

TEST(spscBoundedTests, Try_Push_Fails_When_Full) {
	lockfreeSpscBounded<int, 2> queue;
	EXPECT_TRUE(queue.tryPush(1));
	EXPECT_TRUE(queue.tryPush(2));
	EXPECT_FALSE(queue.tryPush(3)); // Should fail because capacity is 2
}

// --- RIGOROUS TESTS ---

// 1. Stress testing wrap-around with a small queue capacity.
// This forces the queue to wrap around frequently and triggers full/empty conditions constantly.
TEST(spscBoundedTests, HighContention_WrapAround) {
	lockfreeSpscBounded<int, 1024> smallQ;
	const int total = 5000000; // 5 million items

	std::atomic<bool> startFlag{false};

	auto producer = [&]() {
		while (!startFlag.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
		for (int i = 0; i < total; ++i) {
			smallQ.waitAndPush(i);
		}
	};

	auto consumer = [&]() {
		while (!startFlag.load(std::memory_order_acquire)) {
			std::this_thread::yield();
		}
		for (int i = 0; i < total; ++i) {
			int val = -1;
			smallQ.waitAndPop(val);
			EXPECT_EQ(val, i);
		}
	};

	std::thread prod(producer);
	std::thread cons(consumer);

	// Start both threads simultaneously
	startFlag.store(true, std::memory_order_release);

	prod.join();
	cons.join();

	EXPECT_TRUE(smallQ.empty());
}

// 2. Hammering tryPush and tryPop in tight loops
// Tests the non-blocking endpoints under heavy load.
TEST(spscBoundedTests, TryPushPop_Spinning) {
	lockfreeSpscBounded<int, 4096> spinQ;
	const int total = 5000000;

	auto producer = [&]() {
		for (int i = 0; i < total; ++i) {
			while (!spinQ.tryPush(i)) {
				// busy spin (no yield, maximizing contention)
			}
		}
	};

	auto consumer = [&]() {
		for (int i = 0; i < total; ++i) {
			int val = -1;
			while (!spinQ.tryPop(val)) {
				// busy spin
			}
			EXPECT_EQ(val, i);
		}
	};

	std::thread prod(producer);
	std::thread cons(consumer);
	prod.join();
	cons.join();
}

// 3. Complex Object Integrity Test
// Ensures that large structures don't suffer from partial reads (tearing) during concurrent access.
struct ComplexData {
	int id;
	double values[4];
	char name[16];

	ComplexData() : id(-1) {
		for (int i = 0; i < 4; ++i) values[i] = 0.0;
		name[0] = '\0';
	}

	ComplexData(int i) : id(i) {
		for (int j = 0; j < 4; ++j) values[j] = i * 3.14 + j;
		snprintf(name, sizeof(name), "Name%d", i);
	}

	bool isValid(int expectedId) const {
		if (id != expectedId) return false;
		for (int j = 0; j < 4; ++j) {
			if (values[j] != expectedId * 3.14 + j) return false;
		}
		char expectedName[16];
		snprintf(expectedName, sizeof(expectedName), "Name%d", expectedId);
		if (strncmp(name, expectedName, 16) != 0) return false;
		return true;
	}
};

TEST(spscBoundedTests, ComplexObjectIntegrity) {
	lockfreeSpscBounded<ComplexData, 1024> objQ;
	const int total = 1000000;

	auto producer = [&]() {
		for (int i = 0; i < total; ++i) {
			objQ.waitAndPush(ComplexData(i));
		}
	};

	auto consumer = [&]() {
		for (int i = 0; i < total; ++i) {
			ComplexData data;
			objQ.waitAndPop(data);
			EXPECT_TRUE(data.isValid(i));
		}
	};

	std::thread prod(producer);
	std::thread cons(consumer);
	prod.join();
	cons.join();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
