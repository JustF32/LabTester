#include "cc.h"

#include <string>
#include <vector>

namespace {

int checkedSize(int n)
{
    return n > 10 ? 10 : n;
}

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

    const int m = checkedSize(n);
    for (int i = 0; i < m - 1; ++i) {
        for (int j = 0; j < m - i - 1; ++j) {
            if (arr[j] > arr[j + 1]) {
                swapInts(arr[j], arr[j + 1]);
            }
        }
    }
}

void selectionSort(int arr[], int n)
{
    if (!arr || n <= 1) {
        return;
    }

    const int m = checkedSize(n);
    for (int i = 0; i < m - 1; ++i) {
        int minIndex = i;
        for (int j = i + 1; j < m; ++j) {
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

    const int m = checkedSize(n);
    for (int i = 1; i < m; ++i) {
        const int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

int partitionSmall(int arr[], int low, int high)
{
    const int pivot = arr[high];
    int border = low - 1;
    for (int i = low; i < high; ++i) {
        if (arr[i] <= pivot) {
            ++border;
            swapInts(arr[border], arr[i]);
        }
    }
    swapInts(arr[border + 1], arr[high]);
    return border + 1;
}

void quickSort(int arr[], int low, int high)
{
    if (!arr || low < 0 || high <= low) {
        return;
    }
    if (high - low + 1 > 10) {
        high = low + 9;
    }
    const int pivot = partitionSmall(arr, low, high);
    quickSort(arr, low, pivot - 1);
    quickSort(arr, pivot + 1, high);
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
    if (right - left + 1 > 10) {
        right = left + 9;
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
    const int m = checkedSize(n);
    for (int i = m / 2 - 1; i >= 0; --i) {
        heapify(arr, m, i);
    }
    for (int i = m - 1; i > 0; --i) {
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
    const int m = arr.size() > 10 ? 10 : static_cast<int>(arr.size());
    for (int i = 0; i < m - 1; ++i) {
        for (int j = 0; j < m - i - 1; ++j) {
            if (lexSort(arr[j + 1], arr[j])) {
                std::string tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}
