#pragma once

#include <vector>

class ArrayStack {
public:
    ArrayStack();
    ~ArrayStack();

    void push(int value);
    int pop();
    int top() const;
    bool empty() const;
    int size() const;

private:
    int *data;
    int capacity;
    int count;
};

class ArrayQueue {
public:
    ArrayQueue();
    ~ArrayQueue();

    void push(int value);
    int pop();
    int front() const;
    bool empty() const;
    int size() const;

private:
    int *data;
    int capacity;
    int head;
    int tail;
    int count;
};

class ArrayDeque {
public:
    ArrayDeque();
    ~ArrayDeque();

    void pushBack(int value);
    void pushFront(int value);
    int popBack();
    int popFront();
    int front() const;
    int back() const;
    bool empty() const;
    int size() const;

private:
    int *data;
    int capacity;
    int head;
    int tail;
    int count;
};

class LinkedList {
public:
    LinkedList();
    ~LinkedList();

    void pushBack(int value);
    void pushFront(int value);
    void insert(int index, int value);
    int removeAt(int index);
    int get(int index) const;
    bool empty() const;
    int size() const;

private:
    struct Node {
        int value;
        Node *next;
    };

    Node *head;
    Node *tail;
    int count;
};

class BinarySearchTree {
public:
    BinarySearchTree();
    ~BinarySearchTree();

    void insert(int value);
    bool contains(int value) const;
    void remove(int value);
    int min() const;
    int max() const;
    bool empty() const;
    int size() const;
    std::vector<int> inorder() const;

private:
    struct Node {
        int value;
        Node *left;
        Node *right;
    };

    Node *root;
    int count;
};
