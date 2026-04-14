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
