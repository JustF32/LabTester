#include <gtest/gtest.h>

#include "cc.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string vectorToString(const std::vector<int> &values)
{
    std::ostringstream out;
    out << "[";
    const std::size_t headCount = values.size() > 50 ? 25 : values.size();
    for (std::size_t i = 0; i < headCount; ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << values[i];
    }
    if (values.size() > headCount) {
        out << ", ...";
        const std::size_t tailStart = values.size() > 8 ? values.size() - 8 : headCount;
        for (std::size_t i = tailStart; i < values.size(); ++i) {
            out << ", " << values[i];
        }
    }
    out << "]";
    return out.str();
}

void recordCase(const std::string &input, const std::string &expected, const std::string &actual)
{
    ::testing::Test::RecordProperty("input_data", input);
    ::testing::Test::RecordProperty("expected_output", expected);
    ::testing::Test::RecordProperty("actual_output", actual);
}

void runTimedCase(const std::string &name, long long maxMilliseconds, const std::function<void()> &body)
{
    auto done = std::make_shared<std::promise<void>>();
    std::future<void> future = done->get_future();
    auto error = std::make_shared<std::string>();

    const auto started = std::chrono::steady_clock::now();
    std::thread worker([body, done, error]() {
        try {
            body();
        } catch (const std::exception &ex) {
            *error = ex.what();
        } catch (...) {
            *error = "Неизвестное исключение.";
        }
        done->set_value();
    });
    worker.detach();

    if (future.wait_for(std::chrono::milliseconds(maxMilliseconds)) != std::future_status::ready) {
        const std::string expected = "Ограничение времени: " + std::to_string(maxMilliseconds)
            + " мс. Операции структуры данных должны завершиться в лимит и вернуть корректный результат.";
        const std::string actual = "Превышен лимит времени: проверка не завершилась за "
            + std::to_string(maxMilliseconds) + " мс.";
        recordCase(name, expected, actual);
        ::testing::Test::RecordProperty("duration_ms", std::to_string(maxMilliseconds));
        ADD_FAILURE() << "Runtime error: " << actual;
        return;
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    ::testing::Test::RecordProperty("duration_ms", std::to_string(elapsedMs));

    const std::string expected = "Лимит времени: " + std::to_string(maxMilliseconds)
        + " мс; результат операций должен быть корректным.";
    if (!error->empty()) {
        recordCase(name, expected, "Исключение во время выполнения: " + *error);
        FAIL() << *error;
        return;
    }

    const bool hadCorrectnessFailure = ::testing::Test::HasFailure();
    const std::string actual = hadCorrectnessFailure
        ? "Выполнено за " + std::to_string(elapsedMs)
            + " мс, но проверка результата провалена. Подробности ниже."
        : (elapsedMs > maxMilliseconds
            ? "Выполнено за " + std::to_string(elapsedMs) + " мс; лимит "
                + std::to_string(maxMilliseconds) + " мс превышен."
            : "Выполнено за " + std::to_string(elapsedMs) + " мс; лимит не превышен.");
    recordCase(name, expected, actual);

    EXPECT_LE(elapsedMs, maxMilliseconds)
        << "Структура работает слишком медленно: " << elapsedMs
        << " мс при лимите " << maxMilliseconds << " мс.";
}

std::vector<int> makeValues(int count, int seed = 42)
{
    std::vector<int> values(count);
    std::iota(values.begin(), values.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(values.begin(), values.end(), rng);
    return values;
}

} // namespace

TEST(Stack_Basic, StartsEmpty)
{
    ArrayStack stack;
    recordCase("новый стек", "empty=true; size=0", "empty=" + std::to_string(stack.empty()) + "; size=" + std::to_string(stack.size()));
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

TEST(Stack_Basic, PushIncreasesSize)
{
    ArrayStack stack;
    stack.push(10);
    stack.push(20);
    recordCase("push 10, 20", "size=2; top=20", "size=" + std::to_string(stack.size()) + "; top=" + std::to_string(stack.top()));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.top(), 20);
}

TEST(Stack_Basic, PopReturnsLast)
{
    ArrayStack stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_EQ(stack.pop(), 3);
    EXPECT_EQ(stack.pop(), 2);
    recordCase("push 1,2,3; pop twice", "[3, 2]", "[3, 2]");
}

TEST(Stack_Basic, TopDoesNotRemove)
{
    ArrayStack stack;
    stack.push(7);
    stack.push(8);
    EXPECT_EQ(stack.top(), 8);
    EXPECT_EQ(stack.top(), 8);
    EXPECT_EQ(stack.size(), 2);
}

TEST(Stack_Basic, HandlesNegativeValues)
{
    ArrayStack stack;
    stack.push(-5);
    stack.push(0);
    stack.push(-10);
    EXPECT_EQ(stack.pop(), -10);
    EXPECT_EQ(stack.pop(), 0);
    EXPECT_EQ(stack.pop(), -5);
}

TEST(Stack_Basic, BecomesEmptyAfterPops)
{
    ArrayStack stack;
    stack.push(4);
    stack.push(5);
    stack.pop();
    stack.pop();
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

TEST(Stack_Basic, MaintainsOrderForTenValues)
{
    ArrayStack stack;
    for (int i = 0; i < 10; ++i) {
        stack.push(i);
    }
    for (int i = 9; i >= 0; --i) {
        EXPECT_EQ(stack.pop(), i);
    }
}

TEST(Stack_Basic, AlternatingPushPop)
{
    ArrayStack stack;
    stack.push(1);
    EXPECT_EQ(stack.pop(), 1);
    stack.push(2);
    stack.push(3);
    EXPECT_EQ(stack.pop(), 3);
    stack.push(4);
    EXPECT_EQ(stack.pop(), 4);
    EXPECT_EQ(stack.pop(), 2);
}

TEST(Stack_Basic, Duplicates)
{
    ArrayStack stack;
    stack.push(6);
    stack.push(6);
    stack.push(6);
    EXPECT_EQ(stack.pop(), 6);
    EXPECT_EQ(stack.pop(), 6);
    EXPECT_EQ(stack.pop(), 6);
}

TEST(Stack_Basic, WideRangeValues)
{
    ArrayStack stack;
    stack.push(2147483647);
    stack.push(-2147483647);
    EXPECT_EQ(stack.pop(), -2147483647);
    EXPECT_EQ(stack.pop(), 2147483647);
}

TEST(Stack_Advanced, GrowsBeyondInitialCapacity)
{
    ArrayStack stack;
    for (int i = 0; i < 128; ++i) {
        stack.push(i * 3);
    }
    ASSERT_EQ(stack.size(), 128);
    for (int i = 127; i >= 0; --i) {
        EXPECT_EQ(stack.pop(), i * 3);
    }
}

TEST(Stack_Advanced, ReusesAfterEmptying)
{
    ArrayStack stack;
    for (int i = 0; i < 20; ++i) stack.push(i);
    for (int i = 0; i < 20; ++i) stack.pop();
    for (int i = 100; i < 110; ++i) stack.push(i);
    EXPECT_EQ(stack.top(), 109);
    EXPECT_EQ(stack.size(), 10);
}

TEST(Stack_Advanced, LongAlternatingSequence)
{
    ArrayStack stack;
    for (int i = 0; i < 200; ++i) {
        stack.push(i);
        EXPECT_EQ(stack.pop(), i);
    }
    EXPECT_TRUE(stack.empty());
}

TEST(Stack_Advanced, PartialDrainKeepsOlderValues)
{
    ArrayStack stack;
    for (int i = 1; i <= 50; ++i) stack.push(i);
    for (int i = 50; i >= 26; --i) EXPECT_EQ(stack.pop(), i);
    EXPECT_EQ(stack.top(), 25);
    EXPECT_EQ(stack.size(), 25);
}

TEST(Stack_Advanced, ManyDuplicateBlocks)
{
    ArrayStack stack;
    for (int i = 0; i < 30; ++i) stack.push(i % 3);
    for (int i = 29; i >= 0; --i) EXPECT_EQ(stack.pop(), i % 3);
}

TEST(Stack_Performance, PushMany)
{
    runTimedCase("300000 push в стек", 900, []() {
        ArrayStack stack;
        for (int i = 0; i < 300000; ++i) stack.push(i);
        ASSERT_EQ(stack.size(), 300000);
    });
}

TEST(Stack_Performance, PopMany)
{
    runTimedCase("300000 push/pop в стек", 900, []() {
        ArrayStack stack;
        for (int i = 0; i < 300000; ++i) stack.push(i);
        ASSERT_EQ(stack.size(), 300000);
        for (int i = 299999; i >= 0; --i) EXPECT_EQ(stack.pop(), i);
    });
}

TEST(Stack_Performance, AlternatingMany)
{
    runTimedCase("250000 чередований push/pop", 900, []() {
        ArrayStack stack;
        for (int i = 0; i < 250000; ++i) {
            stack.push(i);
            EXPECT_EQ(stack.pop(), i);
        }
    });
}

TEST(Stack_Performance, TopMany)
{
    runTimedCase("200000 чтений top", 700, []() {
        ArrayStack stack;
        for (int i = 0; i < 10000; ++i) stack.push(i);
        long long sum = 0;
        for (int i = 0; i < 200000; ++i) sum += stack.top();
        EXPECT_GT(sum, 0);
    });
}

TEST(Stack_Performance, GrowthWaves)
{
    runTimedCase("20 волн роста стека", 900, []() {
        ArrayStack stack;
        for (int wave = 0; wave < 20; ++wave) {
            for (int i = 0; i < 12000; ++i) stack.push(i);
            ASSERT_EQ(stack.size(), wave * 6000 + 12000);
            for (int i = 0; i < 6000; ++i) stack.pop();
        }
        EXPECT_GT(stack.size(), 0);
    });
}

TEST(Queue_Basic, StartsEmpty)
{
    ArrayQueue queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(Queue_Basic, PushIncreasesSize)
{
    ArrayQueue queue;
    queue.push(10);
    queue.push(20);
    EXPECT_EQ(queue.size(), 2);
    EXPECT_EQ(queue.front(), 10);
}

TEST(Queue_Basic, PopReturnsFirst)
{
    ArrayQueue queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
}

TEST(Queue_Basic, FrontDoesNotRemove)
{
    ArrayQueue queue;
    queue.push(5);
    queue.push(6);
    EXPECT_EQ(queue.front(), 5);
    EXPECT_EQ(queue.front(), 5);
    EXPECT_EQ(queue.size(), 2);
}

TEST(Queue_Basic, HandlesNegativeValues)
{
    ArrayQueue queue;
    queue.push(-1);
    queue.push(-2);
    EXPECT_EQ(queue.pop(), -1);
    EXPECT_EQ(queue.pop(), -2);
}

TEST(Queue_Basic, BecomesEmptyAfterPops)
{
    ArrayQueue queue;
    queue.push(1);
    queue.push(2);
    queue.pop();
    queue.pop();
    EXPECT_TRUE(queue.empty());
}

TEST(Queue_Basic, MaintainsOrderForTenValues)
{
    ArrayQueue queue;
    for (int i = 0; i < 10; ++i) queue.push(i);
    for (int i = 0; i < 10; ++i) EXPECT_EQ(queue.pop(), i);
}

TEST(Queue_Basic, AlternatingPushPop)
{
    ArrayQueue queue;
    queue.push(1);
    EXPECT_EQ(queue.pop(), 1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(queue.pop(), 2);
    queue.push(4);
    EXPECT_EQ(queue.pop(), 3);
    EXPECT_EQ(queue.pop(), 4);
}

TEST(Queue_Basic, Duplicates)
{
    ArrayQueue queue;
    queue.push(7);
    queue.push(7);
    EXPECT_EQ(queue.pop(), 7);
    EXPECT_EQ(queue.pop(), 7);
}

TEST(Queue_Basic, WideRangeValues)
{
    ArrayQueue queue;
    queue.push(2147483647);
    queue.push(-2147483647);
    EXPECT_EQ(queue.pop(), 2147483647);
    EXPECT_EQ(queue.pop(), -2147483647);
}

TEST(Queue_Advanced, GrowsBeyondInitialCapacity)
{
    ArrayQueue queue;
    for (int i = 0; i < 150; ++i) queue.push(i);
    ASSERT_EQ(queue.size(), 150);
    for (int i = 0; i < 150; ++i) EXPECT_EQ(queue.pop(), i);
}

TEST(Queue_Advanced, WrapAroundAfterPops)
{
    ArrayQueue queue;
    for (int i = 0; i < 80; ++i) queue.push(i);
    ASSERT_EQ(queue.size(), 80);
    for (int i = 0; i < 50; ++i) EXPECT_EQ(queue.pop(), i);
    for (int i = 80; i < 140; ++i) queue.push(i);
    ASSERT_EQ(queue.size(), 90);
    for (int i = 50; i < 140; ++i) EXPECT_EQ(queue.pop(), i);
}

TEST(Queue_Advanced, ReusesAfterEmptying)
{
    ArrayQueue queue;
    for (int i = 0; i < 30; ++i) queue.push(i);
    for (int i = 0; i < 30; ++i) queue.pop();
    for (int i = 100; i < 110; ++i) queue.push(i);
    EXPECT_EQ(queue.front(), 100);
    EXPECT_EQ(queue.size(), 10);
}

TEST(Queue_Advanced, PartialDrainKeepsFront)
{
    ArrayQueue queue;
    for (int i = 1; i <= 60; ++i) queue.push(i);
    for (int i = 1; i <= 35; ++i) EXPECT_EQ(queue.pop(), i);
    EXPECT_EQ(queue.front(), 36);
    EXPECT_EQ(queue.size(), 25);
}

TEST(Queue_Advanced, ManyDuplicateBlocks)
{
    ArrayQueue queue;
    for (int i = 0; i < 45; ++i) queue.push(i % 5);
    for (int i = 0; i < 45; ++i) EXPECT_EQ(queue.pop(), i % 5);
}

TEST(Queue_Performance, PushMany)
{
    runTimedCase("250000 push в очередь", 900, []() {
        ArrayQueue queue;
        for (int i = 0; i < 250000; ++i) queue.push(i);
        EXPECT_EQ(queue.size(), 250000);
    });
}

TEST(Queue_Performance, PopMany)
{
    runTimedCase("160000 pop из очереди", 900, []() {
        ArrayQueue queue;
        for (int i = 0; i < 160000; ++i) queue.push(i);
        ASSERT_EQ(queue.size(), 160000);
        for (int i = 0; i < 160000; ++i) EXPECT_EQ(queue.pop(), i);
    });
}

TEST(Queue_Performance, AlternatingMany)
{
    runTimedCase("220000 чередований push/pop", 900, []() {
        ArrayQueue queue;
        for (int i = 0; i < 220000; ++i) {
            queue.push(i);
            EXPECT_EQ(queue.pop(), i);
        }
    });
}

TEST(Queue_Performance, WrapAroundMany)
{
    runTimedCase("много циклических операций очереди", 900, []() {
        ArrayQueue queue;
        for (int i = 0; i < 100000; ++i) queue.push(i);
        ASSERT_EQ(queue.size(), 100000);
        for (int i = 0; i < 70000; ++i) EXPECT_EQ(queue.pop(), i);
        for (int i = 100000; i < 180000; ++i) queue.push(i);
        ASSERT_EQ(queue.size(), 110000);
        for (int i = 70000; i < 180000; ++i) EXPECT_EQ(queue.pop(), i);
    });
}

TEST(Queue_Performance, FrontMany)
{
    runTimedCase("200000 чтений front", 700, []() {
        ArrayQueue queue;
        for (int i = 0; i < 10000; ++i) queue.push(i);
        long long sum = 0;
        for (int i = 0; i < 200000; ++i) sum += queue.front();
        EXPECT_EQ(sum, 0);
    });
}

TEST(Deque_Basic, StartsEmpty)
{
    ArrayDeque deque;
    EXPECT_TRUE(deque.empty());
    EXPECT_EQ(deque.size(), 0);
}

TEST(Deque_Basic, PushBackAndFront)
{
    ArrayDeque deque;
    deque.pushBack(2);
    deque.pushFront(1);
    EXPECT_EQ(deque.front(), 1);
    EXPECT_EQ(deque.back(), 2);
}

TEST(Deque_Basic, PopFront)
{
    ArrayDeque deque;
    deque.pushBack(1);
    deque.pushBack(2);
    EXPECT_EQ(deque.popFront(), 1);
    EXPECT_EQ(deque.popFront(), 2);
}

TEST(Deque_Basic, PopBack)
{
    ArrayDeque deque;
    deque.pushBack(1);
    deque.pushBack(2);
    EXPECT_EQ(deque.popBack(), 2);
    EXPECT_EQ(deque.popBack(), 1);
}

TEST(Deque_Basic, MixedEnds)
{
    ArrayDeque deque;
    deque.pushFront(2);
    deque.pushBack(3);
    deque.pushFront(1);
    EXPECT_EQ(deque.popFront(), 1);
    EXPECT_EQ(deque.popBack(), 3);
    EXPECT_EQ(deque.popFront(), 2);
}

TEST(Deque_Basic, FrontBackDoNotRemove)
{
    ArrayDeque deque;
    deque.pushBack(4);
    deque.pushBack(5);
    EXPECT_EQ(deque.front(), 4);
    EXPECT_EQ(deque.back(), 5);
    EXPECT_EQ(deque.size(), 2);
}

TEST(Deque_Basic, HandlesNegativeValues)
{
    ArrayDeque deque;
    deque.pushFront(-1);
    deque.pushBack(-2);
    EXPECT_EQ(deque.popFront(), -1);
    EXPECT_EQ(deque.popBack(), -2);
}

TEST(Deque_Basic, BecomesEmptyAfterPops)
{
    ArrayDeque deque;
    deque.pushBack(1);
    deque.popBack();
    EXPECT_TRUE(deque.empty());
}

TEST(Deque_Basic, MaintainsBackOrder)
{
    ArrayDeque deque;
    for (int i = 0; i < 10; ++i) deque.pushBack(i);
    for (int i = 0; i < 10; ++i) EXPECT_EQ(deque.popFront(), i);
}

TEST(Deque_Basic, MaintainsFrontOrder)
{
    ArrayDeque deque;
    for (int i = 0; i < 10; ++i) deque.pushFront(i);
    for (int i = 9; i >= 0; --i) EXPECT_EQ(deque.popFront(), i);
}

TEST(Deque_Advanced, GrowsBeyondInitialCapacity)
{
    ArrayDeque deque;
    for (int i = 0; i < 150; ++i) {
        if (i % 2 == 0) deque.pushFront(i);
        else deque.pushBack(i);
    }
    ASSERT_EQ(deque.size(), 150);
}

TEST(Deque_Advanced, WrapAroundBothEnds)
{
    ArrayDeque deque;
    for (int i = 0; i < 80; ++i) deque.pushBack(i);
    ASSERT_EQ(deque.size(), 80);
    for (int i = 0; i < 40; ++i) EXPECT_EQ(deque.popFront(), i);
    for (int i = 80; i < 120; ++i) deque.pushBack(i);
    ASSERT_EQ(deque.size(), 80);
    EXPECT_EQ(deque.front(), 40);
    EXPECT_EQ(deque.back(), 119);
}

TEST(Deque_Advanced, ReusesAfterEmptying)
{
    ArrayDeque deque;
    for (int i = 0; i < 30; ++i) deque.pushFront(i);
    for (int i = 0; i < 30; ++i) deque.popBack();
    deque.pushFront(10);
    deque.pushBack(20);
    EXPECT_EQ(deque.front(), 10);
    EXPECT_EQ(deque.back(), 20);
}

TEST(Deque_Advanced, AlternatingOppositeEnds)
{
    ArrayDeque deque;
    for (int i = 0; i < 60; ++i) {
        deque.pushFront(i);
        deque.pushBack(-i);
    }
    ASSERT_EQ(deque.size(), 120);
    for (int i = 59; i >= 0; --i) EXPECT_EQ(deque.popFront(), i);
    for (int i = 59; i >= 0; --i) EXPECT_EQ(deque.popBack(), -i);
}

TEST(Deque_Advanced, SizeAfterMixedOperations)
{
    ArrayDeque deque;
    for (int i = 0; i < 100; ++i) deque.pushBack(i);
    ASSERT_EQ(deque.size(), 100);
    for (int i = 0; i < 25; ++i) deque.popFront();
    for (int i = 0; i < 25; ++i) deque.popBack();
    EXPECT_EQ(deque.size(), 50);
    EXPECT_EQ(deque.front(), 25);
    EXPECT_EQ(deque.back(), 74);
}

TEST(Deque_Performance, PushBackMany)
{
    runTimedCase("220000 pushBack", 900, []() {
        ArrayDeque deque;
        for (int i = 0; i < 220000; ++i) deque.pushBack(i);
        EXPECT_EQ(deque.size(), 220000);
    });
}

TEST(Deque_Performance, PushFrontMany)
{
    runTimedCase("120000 pushFront", 900, []() {
        ArrayDeque deque;
        for (int i = 0; i < 120000; ++i) deque.pushFront(i);
        EXPECT_EQ(deque.size(), 120000);
    });
}

TEST(Deque_Performance, PopBothEndsMany)
{
    runTimedCase("180000 pop с двух концов", 900, []() {
        ArrayDeque deque;
        for (int i = 0; i < 180000; ++i) deque.pushBack(i);
        ASSERT_EQ(deque.size(), 180000);
        for (int i = 0; i < 90000; ++i) deque.popFront();
        for (int i = 0; i < 90000; ++i) deque.popBack();
        EXPECT_TRUE(deque.empty());
    });
}

TEST(Deque_Performance, AlternatingMany)
{
    runTimedCase("120000 смешанных операций дека", 900, []() {
        ArrayDeque deque;
        for (int i = 0; i < 120000; ++i) {
            deque.pushFront(i);
            deque.pushBack(-i);
            EXPECT_EQ(deque.popFront(), i);
            EXPECT_EQ(deque.popBack(), -i);
        }
    });
}

TEST(Deque_Performance, WrapAroundMany)
{
    runTimedCase("циклический дек", 900, []() {
        ArrayDeque deque;
        for (int i = 0; i < 80000; ++i) deque.pushBack(i);
        ASSERT_EQ(deque.size(), 80000);
        for (int i = 0; i < 60000; ++i) deque.popFront();
        for (int i = 0; i < 80000; ++i) deque.pushFront(i);
        ASSERT_EQ(deque.size(), 100000);
        EXPECT_EQ(deque.size(), 100000);
    });
}

TEST(LinkedList_Basic, StartsEmpty)
{
    LinkedList list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
}

TEST(LinkedList_Basic, PushBack)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(2);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
}

TEST(LinkedList_Basic, PushFront)
{
    LinkedList list;
    list.pushFront(1);
    list.pushFront(2);
    EXPECT_EQ(list.get(0), 2);
    EXPECT_EQ(list.get(1), 1);
}

TEST(LinkedList_Basic, InsertMiddle)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(3);
    list.insert(1, 2);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
    EXPECT_EQ(list.get(2), 3);
}

TEST(LinkedList_Basic, RemoveFirst)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(2);
    EXPECT_EQ(list.removeAt(0), 1);
    EXPECT_EQ(list.get(0), 2);
}

TEST(LinkedList_Basic, RemoveLast)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(2);
    EXPECT_EQ(list.removeAt(1), 2);
    EXPECT_EQ(list.size(), 1);
}

TEST(LinkedList_Basic, RemoveMiddle)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(2);
    list.pushBack(3);
    EXPECT_EQ(list.removeAt(1), 2);
    EXPECT_EQ(list.get(1), 3);
}

TEST(LinkedList_Basic, HandlesNegativeValues)
{
    LinkedList list;
    list.pushBack(-1);
    list.pushBack(-2);
    EXPECT_EQ(list.get(0), -1);
    EXPECT_EQ(list.get(1), -2);
}

TEST(LinkedList_Basic, BecomesEmptyAfterRemove)
{
    LinkedList list;
    list.pushBack(5);
    EXPECT_EQ(list.removeAt(0), 5);
    EXPECT_TRUE(list.empty());
}

TEST(LinkedList_Basic, MaintainsOrderForTenValues)
{
    LinkedList list;
    for (int i = 0; i < 10; ++i) list.pushBack(i);
    for (int i = 0; i < 10; ++i) EXPECT_EQ(list.get(i), i);
}

TEST(LinkedList_Advanced, InsertAtBeginning)
{
    LinkedList list;
    list.pushBack(2);
    list.insert(0, 1);
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 2);
}

TEST(LinkedList_Advanced, InsertAtEnd)
{
    LinkedList list;
    list.pushBack(1);
    list.insert(1, 2);
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list.get(1), 2);
}

TEST(LinkedList_Advanced, ManyHeadOperations)
{
    LinkedList list;
    for (int i = 0; i < 100; ++i) list.pushFront(i);
    for (int i = 99; i >= 0; --i) EXPECT_EQ(list.removeAt(0), i);
}

TEST(LinkedList_Advanced, TailUpdatesAfterRemoveLast)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(2);
    list.removeAt(1);
    list.pushBack(3);
    EXPECT_EQ(list.get(1), 3);
}

TEST(LinkedList_Advanced, ComplexMixedOperations)
{
    LinkedList list;
    list.pushBack(1);
    list.pushBack(4);
    list.insert(1, 2);
    list.insert(2, 3);
    ASSERT_EQ(list.size(), 4);
    EXPECT_EQ(list.removeAt(1), 2);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 3);
    EXPECT_EQ(list.get(2), 4);
}

TEST(LinkedList_Performance, PushFrontMany)
{
    runTimedCase("120000 pushFront списка", 900, []() {
        LinkedList list;
        for (int i = 0; i < 120000; ++i) list.pushFront(i);
        EXPECT_EQ(list.size(), 120000);
    });
}

TEST(LinkedList_Performance, PushBackMany)
{
    runTimedCase("80000 pushBack списка", 900, []() {
        LinkedList list;
        for (int i = 0; i < 80000; ++i) list.pushBack(i);
        EXPECT_EQ(list.size(), 80000);
    });
}

TEST(LinkedList_Performance, RemoveHeadMany)
{
    runTimedCase("80000 удалений головы списка", 900, []() {
        LinkedList list;
        for (int i = 0; i < 80000; ++i) list.pushBack(i);
        ASSERT_EQ(list.size(), 80000);
        for (int i = 0; i < 80000; ++i) EXPECT_EQ(list.removeAt(0), i);
    });
}

TEST(LinkedList_Performance, AlternatingHeadMany)
{
    runTimedCase("100000 чередований головы списка", 900, []() {
        LinkedList list;
        for (int i = 0; i < 100000; ++i) {
            list.pushFront(i);
            EXPECT_EQ(list.removeAt(0), i);
        }
    });
}

TEST(LinkedList_Performance, SequentialGetModerate)
{
    runTimedCase("последовательные get списка", 1000, []() {
        LinkedList list;
        for (int i = 0; i < 6000; ++i) list.pushBack(i);
        long long sum = 0;
        for (int i = 0; i < 6000; ++i) sum += list.get(i);
        EXPECT_EQ(sum, 17997000);
    });
}

TEST(BinarySearchTree_Basic, StartsEmpty)
{
    BinarySearchTree tree;
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.size(), 0);
}

TEST(BinarySearchTree_Basic, InsertAndContains)
{
    BinarySearchTree tree;
    tree.insert(5);
    tree.insert(3);
    tree.insert(7);
    EXPECT_TRUE(tree.contains(5));
    EXPECT_TRUE(tree.contains(3));
    EXPECT_TRUE(tree.contains(7));
}

TEST(BinarySearchTree_Basic, DoesNotContainMissing)
{
    BinarySearchTree tree;
    tree.insert(5);
    EXPECT_FALSE(tree.contains(4));
}

TEST(BinarySearchTree_Basic, MinMax)
{
    BinarySearchTree tree;
    tree.insert(5);
    tree.insert(2);
    tree.insert(8);
    EXPECT_EQ(tree.min(), 2);
    EXPECT_EQ(tree.max(), 8);
}

TEST(BinarySearchTree_Basic, InorderSorted)
{
    BinarySearchTree tree;
    for (int value : {5, 2, 8, 1, 3}) tree.insert(value);
    const std::vector<int> expected {1, 2, 3, 5, 8};
    EXPECT_EQ(tree.inorder(), expected);
}

TEST(BinarySearchTree_Basic, RemoveLeaf)
{
    BinarySearchTree tree;
    for (int value : {5, 2, 8}) tree.insert(value);
    tree.remove(2);
    EXPECT_FALSE(tree.contains(2));
    EXPECT_EQ(tree.size(), 2);
}

TEST(BinarySearchTree_Basic, RemoveNodeWithOneChild)
{
    BinarySearchTree tree;
    for (int value : {5, 2, 1, 8}) tree.insert(value);
    tree.remove(2);
    EXPECT_FALSE(tree.contains(2));
    EXPECT_TRUE(tree.contains(1));
}

TEST(BinarySearchTree_Basic, RemoveNodeWithTwoChildren)
{
    BinarySearchTree tree;
    for (int value : {5, 2, 8, 1, 3}) tree.insert(value);
    tree.remove(2);
    const std::vector<int> expected {1, 3, 5, 8};
    EXPECT_EQ(tree.inorder(), expected);
}

TEST(BinarySearchTree_Basic, HandlesNegativeValues)
{
    BinarySearchTree tree;
    tree.insert(-5);
    tree.insert(0);
    tree.insert(-10);
    EXPECT_EQ(tree.min(), -10);
    EXPECT_TRUE(tree.contains(-5));
}

TEST(BinarySearchTree_Basic, SizeAfterOperations)
{
    BinarySearchTree tree;
    tree.insert(1);
    tree.insert(2);
    tree.insert(3);
    tree.remove(2);
    EXPECT_EQ(tree.size(), 2);
}

TEST(BinarySearchTree_Advanced, DuplicateValuesIgnored)
{
    BinarySearchTree tree;
    tree.insert(5);
    tree.insert(5);
    tree.insert(5);
    EXPECT_EQ(tree.size(), 1);
    EXPECT_EQ(tree.inorder(), std::vector<int>({5}));
}

TEST(BinarySearchTree_Advanced, RemoveRootWithTwoChildren)
{
    BinarySearchTree tree;
    for (int value : {10, 5, 15, 3, 7, 12, 18}) tree.insert(value);
    tree.remove(10);
    EXPECT_FALSE(tree.contains(10));
    EXPECT_EQ(tree.inorder(), std::vector<int>({3, 5, 7, 12, 15, 18}));
}

TEST(BinarySearchTree_Advanced, RemoveMissingDoesNotChangeTree)
{
    BinarySearchTree tree;
    for (int value : {4, 2, 6}) tree.insert(value);
    tree.remove(100);
    EXPECT_EQ(tree.size(), 3);
    EXPECT_EQ(tree.inorder(), std::vector<int>({2, 4, 6}));
}

TEST(BinarySearchTree_Advanced, ManyRandomValuesSortedInorder)
{
    BinarySearchTree tree;
    std::vector<int> values = makeValues(200, 77);
    for (int value : values) tree.insert(value);
    std::sort(values.begin(), values.end());
    EXPECT_EQ(tree.inorder(), values);
}

TEST(BinarySearchTree_Advanced, RemoveManyKeepsSearchValid)
{
    BinarySearchTree tree;
    for (int value : {50, 25, 75, 10, 30, 60, 90, 5, 15}) tree.insert(value);
    for (int value : {25, 75, 10}) tree.remove(value);
    EXPECT_FALSE(tree.contains(25));
    EXPECT_FALSE(tree.contains(75));
    EXPECT_TRUE(tree.contains(60));
    EXPECT_EQ(tree.inorder(), std::vector<int>({5, 15, 30, 50, 60, 90}));
}

TEST(BinarySearchTree_Performance, InsertRandomMany)
{
    runTimedCase("45000 случайных insert BST", 1200, []() {
        BinarySearchTree tree;
        for (int value : makeValues(45000, 501)) tree.insert(value);
        EXPECT_EQ(tree.size(), 45000);
    });
}

TEST(BinarySearchTree_Performance, ContainsRandomMany)
{
    runTimedCase("45000 contains BST", 1200, []() {
        BinarySearchTree tree;
        std::vector<int> values = makeValues(45000, 502);
        for (int value : values) tree.insert(value);
        for (int value : values) EXPECT_TRUE(tree.contains(value));
    });
}

TEST(BinarySearchTree_Performance, InorderMany)
{
    runTimedCase("inorder для 45000 узлов BST", 1200, []() {
        BinarySearchTree tree;
        for (int value : makeValues(45000, 503)) tree.insert(value);
        std::vector<int> ordered = tree.inorder();
        EXPECT_EQ(ordered.size(), 45000u);
        EXPECT_TRUE(std::is_sorted(ordered.begin(), ordered.end()));
    });
}

TEST(BinarySearchTree_Performance, RemoveRandomMany)
{
    runTimedCase("20000 remove BST", 1400, []() {
        BinarySearchTree tree;
        std::vector<int> values = makeValues(40000, 504);
        for (int value : values) tree.insert(value);
        for (int i = 0; i < 20000; ++i) tree.remove(values[i]);
        EXPECT_EQ(tree.size(), 20000);
    });
}

TEST(BinarySearchTree_Performance, MixedOperationsMany)
{
    runTimedCase("смешанные операции BST", 1400, []() {
        BinarySearchTree tree;
        std::vector<int> values = makeValues(35000, 505);
        for (int value : values) tree.insert(value);
        for (int i = 0; i < 15000; ++i) EXPECT_TRUE(tree.contains(values[i]));
        for (int i = 0; i < 12000; ++i) tree.remove(values[i]);
        for (int i = 12000; i < 18000; ++i) tree.insert(values[i] + 1000000);
        EXPECT_GT(tree.size(), 0);
    });
}
