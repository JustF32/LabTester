// sort_test.cpp
#include <gtest/gtest.h>

#include "cc.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

bool arraysEqual(const int* a, const int* b, int size)
{
    for (int i = 0; i < size; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

std::string intArrayToString(const int* arr, int size)
{
    if (arr == nullptr || size <= 0) {
        return "[]";
    }

    std::ostringstream ss;
    ss << "[";
    for (int i = 0; i < size; ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << arr[i];
    }
    ss << "]";
    return ss.str();
}

std::string intVectorToString(const std::vector<int>& values)
{
    if (values.empty()) {
        return "[]";
    }
    return intArrayToString(values.data(), static_cast<int>(values.size()));
}

std::string stringVectorToString(const std::vector<std::string>& values)
{
    if (values.empty()) {
        return "[]";
    }

    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << values[i];
    }
    ss << "]";
    return ss.str();
}

void recordIntCase(
    const std::vector<int>& input,
    const std::vector<int>& expected,
    const int* actual,
    int actualSize
)
{
    ::testing::Test::RecordProperty("input_data", intVectorToString(input));
    ::testing::Test::RecordProperty("expected_output", intVectorToString(expected));
    ::testing::Test::RecordProperty("actual_output", intArrayToString(actual, actualSize));
}

void recordTextCase(const std::string& input, const std::string& expected, const std::string& actual)
{
    ::testing::Test::RecordProperty("input_data", input);
    ::testing::Test::RecordProperty("expected_output", expected);
    ::testing::Test::RecordProperty("actual_output", actual);
}

class SortingTest : public ::testing::Test {
protected:
    int* arr;
    int n;
    int* expected;

    void SetUp() override
    {
        n = 10;
        arr = new int[n]{5, 2, 9, 1, 5, 6, 3, 8, 4, 7};
        expected = new int[n]{1, 2, 3, 4, 5, 5, 6, 7, 8, 9};
    }

    void TearDown() override
    {
        delete[] arr;
        delete[] expected;
    }
};

TEST_F(SortingTest, BubbleSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    bubblesort(arr, n);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST_F(SortingTest, SelectionSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    selectionSort(arr, n);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST_F(SortingTest, InsertionSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    insertionSort(arr, n);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST_F(SortingTest, MergeSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    mergeSort(arr, 0, n - 1);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST_F(SortingTest, QuickSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    quickSort(arr, 0, n - 1);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST_F(SortingTest, HeapSort_Small)
{
    const std::vector<int> input(arr, arr + n);
    const std::vector<int> expectedValues(expected, expected + n);

    sorting_heap(arr, n);
    recordIntCase(input, expectedValues, arr, n);

    EXPECT_TRUE(isSorted(arr, n)) << "actual_output=" << intArrayToString(arr, n);
    EXPECT_TRUE(arraysEqual(arr, expected, n)) << "actual_output=" << intArrayToString(arr, n);
}

TEST(SortingEdgeCases, DotAsCharInIntArray)
{
    int arr[] = {5, 2, 9, '.', 5, 6, 3, 8, 4, 7};
    int expected[] = {2, 3, 4, 5, 5, 6, 7, 8, 9, 46};

    const std::vector<int> input(arr, arr + 10);
    const std::vector<int> expectedValues(expected, expected + 10);

    bubblesort(arr, 10);
    recordIntCase(input, expectedValues, arr, 10);

    EXPECT_TRUE(arraysEqual(arr, expected, 10)) << "actual_output=" << intArrayToString(arr, 10);
    EXPECT_EQ(arr[9], 46);
}

TEST(SortingEdgeCases, Empty)
{
    int* empty = nullptr;
    bubblesort(empty, 0);
    selectionSort(empty, 0);
    insertionSort(empty, 0);
    int dummy;
    mergeSort(&dummy, 0, -1);
    quickSort(&dummy, 0, -1);
    sorting_heap(&dummy, 0);

    recordTextCase("[]", "no crash on empty input", "no crash on empty input");
    SUCCEED();
}

TEST(SortingEdgeCases, SingleElement)
{
    int single[] = {42};
    int expected[] = {42};

    const std::vector<int> input(single, single + 1);
    const std::vector<int> expectedValues(expected, expected + 1);

    bubblesort(single, 1);
    recordIntCase(input, expectedValues, single, 1);

    EXPECT_EQ(single[0], 42) << "actual_output=" << intArrayToString(single, 1);
}

TEST(SortingEdgeCases, Sorted)
{
    int sorted[] = {1, 2, 3, 4, 5};
    int expected[] = {1, 2, 3, 4, 5};

    const std::vector<int> input(sorted, sorted + 5);
    const std::vector<int> expectedValues(expected, expected + 5);

    quickSort(sorted, 0, 4);
    recordIntCase(input, expectedValues, sorted, 5);

    EXPECT_TRUE(arraysEqual(sorted, expected, 5)) << "actual_output=" << intArrayToString(sorted, 5);
}

TEST(SortingEdgeCases, RevSorted)
{
    int reverse[] = {5, 4, 3, 2, 1};
    int expected[] = {1, 2, 3, 4, 5};

    const std::vector<int> input(reverse, reverse + 5);
    const std::vector<int> expectedValues(expected, expected + 5);

    mergeSort(reverse, 0, 4);
    recordIntCase(input, expectedValues, reverse, 5);

    EXPECT_TRUE(arraysEqual(reverse, expected, 5)) << "actual_output=" << intArrayToString(reverse, 5);
}

TEST(SortingEdgeCases, AllSame)
{
    int same[] = {7, 7, 7, 7, 7};
    int expected[] = {7, 7, 7, 7, 7};

    const std::vector<int> input(same, same + 5);
    const std::vector<int> expectedValues(expected, expected + 5);

    sorting_heap(same, 5);
    recordIntCase(input, expectedValues, same, 5);

    EXPECT_TRUE(arraysEqual(same, expected, 5)) << "actual_output=" << intArrayToString(same, 5);
}

TEST(SortingEdgeCases, Negatives)
{
    int data[] = {10, -5, 0, 23, -100, 50};
    int expected[] = {-100, -5, 0, 10, 23, 50};
    int n = 6;

    const std::vector<int> input(data, data + n);
    const std::vector<int> expectedValues(expected, expected + n);

    insertionSort(data, n);
    recordIntCase(input, expectedValues, data, n);

    EXPECT_TRUE(arraysEqual(data, expected, n)) << "actual_output=" << intArrayToString(data, n);
}

TEST(SortingRandom, BubbleSort_Random100)
{
    const int N = 100;
    int* data = new int[N];
    int* expected = new int[N];

    std::srand(42);
    for (int i = 0; i < N; ++i) {
        data[i] = std::rand() % 1000;
        expected[i] = data[i];
    }

    std::vector<int> input(data, data + N);
    std::sort(expected, expected + N);
    std::vector<int> expectedValues(expected, expected + N);

    bubblesort(data, N);
    recordIntCase(input, expectedValues, data, N);

    EXPECT_TRUE(arraysEqual(data, expected, N)) << "actual_output=" << intArrayToString(data, N);

    delete[] data;
    delete[] expected;
}

using SortFunc = void(*)(int*, int);
void mergeSortWrapper(int* arr, int n) { mergeSort(arr, 0, n - 1); }
void quickSortWrapper(int* arr, int n) { quickSort(arr, 0, n - 1); }

class ParamSortTest : public ::testing::TestWithParam<SortFunc> {};

TEST_P(ParamSortTest, SortsCorrectly)
{
    int data[] = {64, 34, 25, 12, 22, 11, 90};
    int expected[] = {11, 12, 22, 25, 34, 64, 90};
    int n = 7;

    const std::vector<int> input(data, data + n);
    const std::vector<int> expectedValues(expected, expected + n);

    SortFunc func = GetParam();
    func(data, n);
    recordIntCase(input, expectedValues, data, n);

    EXPECT_TRUE(arraysEqual(data, expected, n)) << "actual_output=" << intArrayToString(data, n);
    EXPECT_TRUE(isSorted(data, n)) << "actual_output=" << intArrayToString(data, n);
}

INSTANTIATE_TEST_SUITE_P(
    AllSorts,
    ParamSortTest,
    ::testing::Values(
        bubblesort,
        selectionSort,
        insertionSort,
        mergeSortWrapper,
        quickSortWrapper,
        sorting_heap
    )
);

TEST(LexSort, BasicStrings)
{
    std::vector<std::string> words = {"banana", "apple", "cherry", "apricot"};
    const std::vector<std::string> input = words;
    std::vector<std::string> expected = {"apple", "apricot", "banana", "cherry"};

    bubblesortForLex(words);

    recordTextCase(stringVectorToString(input), stringVectorToString(expected), stringVectorToString(words));

    EXPECT_EQ(words, expected) << "actual_output=" << stringVectorToString(words);
    EXPECT_TRUE(isSortedLex(words));
}

TEST(LexSort, CompareFunction)
{
    const bool r1 = lexSort("apple", "banana");
    const bool r2 = lexSort("banana", "apple");
    const bool r3 = lexSort("test", "testing");
    const bool r4 = lexSort("testing", "test");
    const bool r5 = lexSort("same", "same");

    recordTextCase(
        "apple<banana; banana<apple; test<testing; testing<test; same<same",
        "[true, false, true, false, false]",
        std::string("[")
            + (r1 ? "true" : "false") + ", "
            + (r2 ? "true" : "false") + ", "
            + (r3 ? "true" : "false") + ", "
            + (r4 ? "true" : "false") + ", "
            + (r5 ? "true" : "false") + "]"
    );

    EXPECT_TRUE(r1);
    EXPECT_FALSE(r2);
    EXPECT_TRUE(r3);
    EXPECT_FALSE(r4);
    EXPECT_FALSE(r5);
}
