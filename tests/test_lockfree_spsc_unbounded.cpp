#include <gtest/gtest.h>
#include <thread>
#include <string>
#include <memory>

#include <lockfree_spsc_unbounded/queue.hpp> 

using namespace tsfqueue::impl;


class SPSCTest : public ::testing::Test {
protected:
	lockfree_spsc_unbounded<int> q;
	
	
	void SetUp() override {
	 
	}
	void TearDown() override {
	   
	}
};

//Basic Tests

// checking if it is empty initially
TEST_F(SPSCTest, Is_Empty_Initially) {
	EXPECT_TRUE(q.empty());
	EXPECT_EQ(q.size(), 0);
}

// checking if push and pop works
TEST_F(SPSCTest, Push_Pop_Works) {
	q.push(100);
	EXPECT_FALSE(q.empty());
	
	int result = 0;
	EXPECT_TRUE(q.try_pop(result));
	EXPECT_EQ(result, 100);
}

// checking if the queue maintains the FIFO order
TEST_F(SPSCTest, Maintains_Order) {
	q.push(1);
	q.push(2);
	q.push(3);
	
	int result = 0;
	q.try_pop(result); EXPECT_EQ(result, 1);
	q.try_pop(result); EXPECT_EQ(result, 2);
	q.try_pop(result); EXPECT_EQ(result, 3);
}

// checking if peek works
TEST_F(SPSCTest, Peek_Works_Without_Removing) {
	q.push(42);
	
	int peek_val = 0;
	EXPECT_TRUE(q.peek(peek_val));
	EXPECT_EQ(peek_val, 42);
	
	
	EXPECT_EQ(q.size(), 1);
}


// checking for memoryleaks since we are doing dynamic memory allocation
// basically  by the domino effect if the last pointed value is not there then everything else which is  a superset of it has also been destroyed properly
// this test can also be thought of as checking if my queues destructore destroys  everything properly in a proper manner
TEST(SPSCObjectTests, No_Memory_Leaks) {
   
// used shared pointer so that i can have this tracker concept  to prove the domino effect

	auto my_object = std::make_shared<int>(99);
	std::weak_ptr<int> tracker = my_object;
	
	{
		lockfree_spsc_unbounded<std::shared_ptr<int>> queue;
		
	   
		queue.push(my_object);
		my_object.reset();   // because the pushing mechanism creates a copy and increases strong reference count of the control block so -1 for the object.reset
		// now only queue's copied shared ptr has ownership
		
		EXPECT_FALSE(tracker.expired());
		
		
	}
	
	EXPECT_TRUE(tracker.expired()); 
}


// checking if the queue can handle large objects
TEST(SPSCObjectTests, Handles_Large_Objects) {
	lockfree_spsc_unbounded<std::string> string_queue;
	
	
	std::string s(1000, 'X'); 
	
	string_queue.push(s);
	
	std::string output;
	EXPECT_TRUE(string_queue.try_pop(output));
	EXPECT_EQ(output.length(), 1000);
}

//move_assignment operator is working
TEST(SPSCObjectTests, move_assignment_operator_check) {
	lockfree_spsc_unbounded<std::unique_ptr<int>> ptr_queue;
	
	// We can't copy unique_ptr, so the queue MUST move it correctly.
	ptr_queue.push(std::make_unique<int>(777));
	
	std::unique_ptr<int> result;
	EXPECT_TRUE(ptr_queue.try_pop(result));
	
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(*result, 777);
}

//testing spsc
TEST_F(SPSCTest,Testing_SPSC) {
	const int total = 100000;
	
	
	auto producer = [&]() {
		for (int i = 0; i < total; ++i) {
			q.push(i);
		}
	};
	
	
	auto consumer = [&]() {
		for (int i = 0; i < total; ++i) {
			int val = -1;
		   
			q.wait_and_pop(val); 
			EXPECT_EQ(val, i);
		}
	};
	
	std::thread prod_thread(producer);
	std::thread cons_thread(consumer);
	
	prod_thread.join();
	cons_thread.join();
	
	
	EXPECT_TRUE(q.empty());
}




//final check and also checking peek function
TEST_F(SPSCTest, final_spsccheck_with_peek_and_pop) {
	const int total = 50000;
	
	auto producer = [&]() {
		for (int i = 0; i < total; ++i) q.push(i);
	};
	
	auto consumer = [&]() {
		int expected = 0;
		while (expected < total) {
			int peek_val = -1;
			int pop_val = -1;
			
			
			if (q.peek(peek_val)) {
				
				bool success = q.try_pop(pop_val);
				EXPECT_TRUE(success);
				EXPECT_EQ(peek_val, pop_val);
				EXPECT_EQ(pop_val, expected);
				expected++;
			}
		}
	};
	
	std::thread t1(producer);
	std::thread t2(consumer);
	t1.join();
	t2.join();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
