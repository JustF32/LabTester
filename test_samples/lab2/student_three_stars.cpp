#include "cc.h"

#include <functional>
#include <vector>

ArrayStack::ArrayStack() : data(new int[8]), capacity(8), count(0) {}

ArrayStack::~ArrayStack()
{
    delete[] data;
}

void ArrayStack::push(int value)
{
    if (count == capacity) {
        const int newCapacity = capacity * 2;
        int *next = new int[newCapacity];
        for (int i = 0; i < count; ++i) {
            next[i] = data[i];
        }
        delete[] data;
        data = next;
        capacity = newCapacity;
    }
    data[count++] = value;
}

int ArrayStack::pop()
{
    return data[--count];
}

int ArrayStack::top() const
{
    return data[count - 1];
}

bool ArrayStack::empty() const
{
    return count == 0;
}

int ArrayStack::size() const
{
    return count;
}

ArrayQueue::ArrayQueue() : data(new int[8]), capacity(8), head(0), tail(0), count(0) {}

ArrayQueue::~ArrayQueue()
{
    delete[] data;
}

void ArrayQueue::push(int value)
{
    if (count == capacity) {
        const int newCapacity = capacity * 2;
        int *next = new int[newCapacity];
        for (int i = 0; i < count; ++i) {
            next[i] = data[(head + i) % capacity];
        }
        delete[] data;
        data = next;
        capacity = newCapacity;
        head = 0;
        tail = count;
    }
    data[tail] = value;
    tail = (tail + 1) % capacity;
    ++count;
}

int ArrayQueue::pop()
{
    const int value = data[head];
    head = (head + 1) % capacity;
    --count;
    return value;
}

int ArrayQueue::front() const
{
    return data[head];
}

bool ArrayQueue::empty() const
{
    return count == 0;
}

int ArrayQueue::size() const
{
    return count;
}

ArrayDeque::ArrayDeque() : data(new int[8]), capacity(8), head(0), tail(0), count(0) {}

ArrayDeque::~ArrayDeque()
{
    delete[] data;
}

void ArrayDeque::pushBack(int value)
{
    if (count == capacity) {
        const int newCapacity = capacity * 2;
        int *next = new int[newCapacity];
        for (int i = 0; i < count; ++i) {
            next[i] = data[(head + i) % capacity];
        }
        delete[] data;
        data = next;
        capacity = newCapacity;
        head = 0;
        tail = count;
    }
    data[tail] = value;
    tail = (tail + 1) % capacity;
    ++count;
}

void ArrayDeque::pushFront(int value)
{
    if (count == capacity) {
        const int newCapacity = capacity * 2;
        int *next = new int[newCapacity];
        for (int i = 0; i < count; ++i) {
            next[i] = data[(head + i) % capacity];
        }
        delete[] data;
        data = next;
        capacity = newCapacity;
        head = 0;
        tail = count;
    }
    head = (head - 1 + capacity) % capacity;
    data[head] = value;
    ++count;
}

int ArrayDeque::popBack()
{
    tail = (tail - 1 + capacity) % capacity;
    --count;
    return data[tail];
}

int ArrayDeque::popFront()
{
    const int value = data[head];
    head = (head + 1) % capacity;
    --count;
    return value;
}

int ArrayDeque::front() const
{
    return data[head];
}

int ArrayDeque::back() const
{
    return data[(tail - 1 + capacity) % capacity];
}

bool ArrayDeque::empty() const
{
    return count == 0;
}

int ArrayDeque::size() const
{
    return count;
}

LinkedList::LinkedList() : head(nullptr), tail(nullptr), count(0) {}

LinkedList::~LinkedList()
{
    while (head) {
        Node *next = head->next;
        delete head;
        head = next;
    }
    tail = nullptr;
    count = 0;
}

void LinkedList::pushBack(int value)
{
    Node *node = new Node{value, nullptr};
    if (!tail) {
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
    ++count;
}

void LinkedList::pushFront(int value)
{
    Node *node = new Node{value, head};
    head = node;
    if (!tail) {
        tail = node;
    }
    ++count;
}

void LinkedList::insert(int index, int value)
{
    if (index <= 0) {
        pushFront(value);
        return;
    }
    if (index >= count) {
        pushBack(value);
        return;
    }
    Node *prev = head;
    for (int i = 0; i < index - 1; ++i) {
        prev = prev->next;
    }
    prev->next = new Node{value, prev->next};
    ++count;
}

int LinkedList::removeAt(int index)
{
    if (index <= 0) {
        Node *old = head;
        const int value = old->value;
        head = head->next;
        if (tail == old) {
            tail = head;
        }
        delete old;
        --count;
        return value;
    }
    Node *prev = head;
    for (int i = 0; i < index - 1; ++i) {
        prev = prev->next;
    }
    Node *old = prev->next;
    const int value = old->value;
    prev->next = old->next;
    if (tail == old) {
        tail = prev;
    }
    delete old;
    --count;
    return value;
}

int LinkedList::get(int index) const
{
    Node *node = head;
    for (int i = 0; i < index; ++i) {
        node = node->next;
    }
    return node->value;
}

bool LinkedList::empty() const
{
    return count == 0;
}

int LinkedList::size() const
{
    return count;
}

BinarySearchTree::BinarySearchTree() : root(nullptr), count(0) {}

BinarySearchTree::~BinarySearchTree()
{
    std::function<void(Node *)> destroy = [&](Node *node) {
        if (!node) {
            return;
        }
        destroy(node->left);
        destroy(node->right);
        delete node;
    };
    destroy(root);
}

void BinarySearchTree::insert(int value)
{
    Node **current = &root;
    while (*current) {
        if (value == (*current)->value) {
            return;
        }
        current = value < (*current)->value ? &((*current)->left) : &((*current)->right);
    }
    *current = new Node{value, nullptr, nullptr};
    ++count;
}

bool BinarySearchTree::contains(int value) const
{
    Node *current = root;
    while (current) {
        if (value == current->value) {
            return true;
        }
        current = value < current->value ? current->left : current->right;
    }
    return false;
}

void BinarySearchTree::remove(int value)
{
    Node **current = &root;
    while (*current && (*current)->value != value) {
        current = value < (*current)->value ? &((*current)->left) : &((*current)->right);
    }
    if (!*current) {
        return;
    }

    Node *target = *current;
    if (!target->left) {
        *current = target->right;
        delete target;
        --count;
    } else if (!target->right) {
        *current = target->left;
        delete target;
        --count;
    } else {
        Node **successor = &(target->right);
        while ((*successor)->left) {
            successor = &((*successor)->left);
        }
        target->value = (*successor)->value;
        Node *old = *successor;
        *successor = old->right;
        delete old;
        --count;
    }
}

int BinarySearchTree::min() const
{
    Node *current = root;
    while (current->left) {
        current = current->left;
    }
    return current->value;
}

int BinarySearchTree::max() const
{
    Node *current = root;
    while (current->right) {
        current = current->right;
    }
    return current->value;
}

bool BinarySearchTree::empty() const
{
    return count == 0;
}

int BinarySearchTree::size() const
{
    return count;
}

std::vector<int> BinarySearchTree::inorder() const
{
    std::vector<int> result;
    std::function<void(Node *)> walk = [&](Node *node) {
        if (!node) {
            return;
        }
        walk(node->left);
        result.push_back(node->value);
        walk(node->right);
    };
    walk(root);
    return result;
}
