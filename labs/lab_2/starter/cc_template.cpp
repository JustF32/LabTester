#include "cc.h"

// Реализуйте структуры данных без std::stack, std::queue, std::deque,
// std::list, std::set/std::map и готовых контейнерных операций вместо логики.

ArrayStack::ArrayStack() : data(nullptr), capacity(0), count(0) {}
ArrayStack::~ArrayStack() {}
void ArrayStack::push(int value) {}
int ArrayStack::pop() { return 0; }
int ArrayStack::top() const { return 0; }
bool ArrayStack::empty() const { return true; }
int ArrayStack::size() const { return 0; }

ArrayQueue::ArrayQueue() : data(nullptr), capacity(0), head(0), tail(0), count(0) {}
ArrayQueue::~ArrayQueue() {}
void ArrayQueue::push(int value) {}
int ArrayQueue::pop() { return 0; }
int ArrayQueue::front() const { return 0; }
bool ArrayQueue::empty() const { return true; }
int ArrayQueue::size() const { return 0; }

ArrayDeque::ArrayDeque() : data(nullptr), capacity(0), head(0), tail(0), count(0) {}
ArrayDeque::~ArrayDeque() {}
void ArrayDeque::pushBack(int value) {}
void ArrayDeque::pushFront(int value) {}
int ArrayDeque::popBack() { return 0; }
int ArrayDeque::popFront() { return 0; }
int ArrayDeque::front() const { return 0; }
int ArrayDeque::back() const { return 0; }
bool ArrayDeque::empty() const { return true; }
int ArrayDeque::size() const { return 0; }

LinkedList::LinkedList() : head(nullptr), tail(nullptr), count(0) {}
LinkedList::~LinkedList() {}
void LinkedList::pushBack(int value) {}
void LinkedList::pushFront(int value) {}
void LinkedList::insert(int index, int value) {}
int LinkedList::removeAt(int index) { return 0; }
int LinkedList::get(int index) const { return 0; }
bool LinkedList::empty() const { return true; }
int LinkedList::size() const { return 0; }

BinarySearchTree::BinarySearchTree() : root(nullptr), count(0) {}
BinarySearchTree::~BinarySearchTree() {}
void BinarySearchTree::insert(int value) {}
bool BinarySearchTree::contains(int value) const { return false; }
void BinarySearchTree::remove(int value) {}
int BinarySearchTree::min() const { return 0; }
int BinarySearchTree::max() const { return 0; }
bool BinarySearchTree::empty() const { return true; }
int BinarySearchTree::size() const { return 0; }
std::vector<int> BinarySearchTree::inorder() const { return {}; }
