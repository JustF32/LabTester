#include "cc.h"

#include <algorithm>
#include <string>

bool isSorted(const int arr[], int n) {
    for (int i = 1; i < n; ++i) {
        if (arr[i - 1] > arr[i]) return false;
    }
    return true;
}

bool isSortedLex(const std::vector<std::string>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (lexSort(arr[i], arr[i - 1])) return false;
    }
    return true;
}

bool lexSort(const std::string& a, const std::string& b) {
    return a < b;
}

void bubblesortForLex(std::vector<std::string>& arr) {
    std::sort(arr.begin(), arr.end(), lexSort);
}

void bubblesort(int arr[], int n) {
    std::sort(arr, arr + n);
}

void selectionSort(int arr[], int n) {
    std::sort(arr, arr + n);
}

void insertionSort(int arr[], int n) {
    std::sort(arr, arr + n);
}

// Намеренно отсутствует quickSort(...) для имитации ошибки сборки.

void mergeSort(int arr[], int left, int right) {
    if (left <= right) {
        std::sort(arr + left, arr + right + 1);
    }
}

void sorting_heap(int arr[], int n) {
    std::sort(arr, arr + n);
}
