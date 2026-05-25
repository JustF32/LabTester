#pragma once
#include <vector>
#include <string>

void bubblesort(int arr[], int n);
void selectionSort(int arr[], int n);
void insertionSort(int arr[], int n);
void quickSort(int arr[], int low, int high);
void mergeSort(int arr[], int left, int right);
void sorting_heap(int arr[], int n);

bool lexSort(const std::string& a, const std::string& b);
void bubblesortForLex(std::vector<std::string>& arr);

bool isSorted(const int arr[], int n);
bool isSortedLex(const std::vector<std::string>& arr);