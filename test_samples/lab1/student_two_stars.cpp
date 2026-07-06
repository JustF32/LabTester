#include "cc.h"

#include <string>
#include <vector>

namespace {

void swapInts(int &a, int &b)
{
    int tmp = a;
    a = b;
    b = tmp;
}

int takeFrontSlow(std::vector<int> &values)
{
    const int result = values.front();
    for (int i = 1; i < static_cast<int>(values.size()); ++i) {
        values[i - 1] = values[i];
    }
    values.pop_back();
    return result;
}

void mergeRangeSlow(int arr[], int left, int mid, int right)
{
    std::vector<int> leftPart;
    std::vector<int> rightPart;

    for (int i = left; i <= mid; ++i) {
        leftPart.push_back(arr[i]);
    }
    for (int i = mid + 1; i <= right; ++i) {
        rightPart.push_back(arr[i]);
    }

    int write = left;
    while (!leftPart.empty() && !rightPart.empty()) {
        if (leftPart.front() <= rightPart.front()) {
            arr[write++] = takeFrontSlow(leftPart);
        } else {
            arr[write++] = takeFrontSlow(rightPart);
        }
    }

    while (!leftPart.empty()) {
        arr[write++] = takeFrontSlow(leftPart);
    }
    while (!rightPart.empty()) {
        arr[write++] = takeFrontSlow(rightPart);
    }
}

void heapifyDown(int arr[], int heapSize, int index)
{
    while (true) {
        int largest = index;
        const int left = index * 2 + 1;
        const int right = index * 2 + 2;

        if (left < heapSize && arr[left] > arr[largest]) {
            largest = left;
        }
        if (right < heapSize && arr[right] > arr[largest]) {
            largest = right;
        }
        if (largest == index) {
            return;
        }

        swapInts(arr[index], arr[largest]);
        index = largest;
    }
}

void rebuildHeap(int arr[], int heapSize)
{
    for (int i = heapSize / 2 - 1; i >= 0; --i) {
        heapifyDown(arr, heapSize, i);
    }
}

} // namespace

void bubblesort(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    for (int i = 0; i < n - 1; ++i) {
        bool swapped = false;
        for (int j = 0; j < n - i - 1; ++j) {
            if (arr[j] > arr[j + 1]) {
                swapInts(arr[j], arr[j + 1]);
                swapped = true;
            }
        }
        if (!swapped) {
            break;
        }
    }
}

void selectionSort(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    for (int i = 0; i < n - 1; ++i) {
        int minIndex = i;
        for (int j = i + 1; j < n; ++j) {
            if (arr[j] < arr[minIndex]) {
                minIndex = j;
            }
        }
        if (minIndex != i) {
            swapInts(arr[i], arr[minIndex]);
        }
    }
}

void insertionSort(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    for (int i = 1; i < n; ++i) {
        const int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

void quickSort(int arr[], int low, int high)
{
    if (!arr || low < 0 || high < low) {
        return;
    }

    const int pivot = arr[low];
    int left = low + 1;
    int right = high;

    while (left <= right) {
        while (left <= high && arr[left] <= pivot) {
            ++left;
        }
        while (right > low && arr[right] > pivot) {
            --right;
        }
        if (left < right) {
            swapInts(arr[left], arr[right]);
            ++left;
            --right;
        }
    }

    swapInts(arr[low], arr[right]);
    quickSort(arr, low, right - 1);
    quickSort(arr, right + 1, high);
}

void mergeSort(int arr[], int left, int right)
{
    if (!arr || left < 0 || right <= left) {
        return;
    }

    const int mid = left + (right - left) / 2;
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);
    mergeRangeSlow(arr, left, mid, right);
}

void sorting_heap(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    for (int heapSize = n; heapSize > 1; --heapSize) {
        rebuildHeap(arr, heapSize);
        swapInts(arr[0], arr[heapSize - 1]);
    }
}

bool lexSort(const std::string &a, const std::string &b)
{
    int i = 0;
    while (i < static_cast<int>(a.size()) && i < static_cast<int>(b.size())) {
        if (a[i] < b[i]) {
            return true;
        }
        if (a[i] > b[i]) {
            return false;
        }
        ++i;
    }
    return a.size() < b.size();
}

void bubblesortForLex(std::vector<std::string> &arr)
{
    for (int i = 0; i < static_cast<int>(arr.size()) - 1; ++i) {
        for (int j = i + 1; j < static_cast<int>(arr.size()); ++j) {
            if (lexSort(arr[j], arr[i])) {
                std::string tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}
