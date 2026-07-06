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
        swapInts(arr[i], arr[minIndex]);
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

int partition(int arr[], int low, int high)
{
    const int pivot = arr[(low + high) / 2];
    int i = low;
    int j = high;
    while (i <= j) {
        while (arr[i] < pivot) {
            ++i;
        }
        while (arr[j] > pivot) {
            --j;
        }
        if (i <= j) {
            swapInts(arr[i], arr[j]);
            ++i;
            --j;
        }
    }
    return i;
}

void quickSort(int arr[], int low, int high)
{
    if (!arr || low < 0 || high <= low) {
        return;
    }

    const int index = partition(arr, low, high);
    if (low < index - 1) {
        quickSort(arr, low, index - 1);
    }
    if (index < high) {
        quickSort(arr, index, high);
    }
}

void mergeRange(int arr[], int left, int mid, int right)
{
    std::vector<int> temp;
    temp.reserve(right - left + 1);

    int i = left;
    int j = mid + 1;
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) {
            temp.push_back(arr[i++]);
        } else {
            temp.push_back(arr[j++]);
        }
    }
    while (i <= mid) {
        temp.push_back(arr[i++]);
    }
    while (j <= right) {
        temp.push_back(arr[j++]);
    }

    for (int k = 0; k < static_cast<int>(temp.size()); ++k) {
        arr[left + k] = temp[k];
    }
}

void mergeSort(int arr[], int left, int right)
{
    if (!arr || left < 0 || right <= left) {
        return;
    }

    const int mid = left + (right - left) / 2;
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);
    mergeRange(arr, left, mid, right);
}

void heapify(int arr[], int n, int root)
{
    int largest = root;
    const int left = root * 2 + 1;
    const int right = root * 2 + 2;
    if (left < n && arr[left] > arr[largest]) {
        largest = left;
    }
    if (right < n && arr[right] > arr[largest]) {
        largest = right;
    }
    if (largest != root) {
        swapInts(arr[root], arr[largest]);
        heapify(arr, n, largest);
    }
}

void sorting_heap(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    for (int i = n / 2 - 1; i >= 0; --i) {
        heapify(arr, n, i);
    }
    for (int i = n - 1; i > 0; --i) {
        swapInts(arr[0], arr[i]);
        heapify(arr, i, 0);
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
        bool swapped = false;
        for (int j = 0; j < static_cast<int>(arr.size()) - i - 1; ++j) {
            if (lexSort(arr[j + 1], arr[j])) {
                std::string tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                swapped = true;
            }
        }
        if (!swapped) {
            break;
        }
    }
}
