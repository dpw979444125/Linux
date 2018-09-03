#pragma once

#include "Allocator.h"

template<class T>
struct ListNode
{
	ListNode<T>* _next;
	ListNode<T>* _prev;
	T _data;

	ListNode(const T& x)
		:_data(x)
		, _next(NULL)
		, _prev(NULL)
	{}
};

// <T, T&, T*>
// <T, const T&, const T*>
template<class T, class Ref, class Ptr>
struct __ListIterator
{
	typedef ListNode<T> Node;
	typedef __ListIterator<T, Ref, Ptr> Self;
	Node* _node;

	__ListIterator(Node* node)
		:_node(node)
	{}

	Ref operator*()
	{
		return _node->_data;
	}

	Ptr operator->()
	{
		&(operator*());
	}

	Self& operator++()
	{
		_node = _node->_next;
		return *this;
	}

	Self& operator++(int)
	{
		Self tmp(*this);
		_node = _node->_next;
		return tmp;
	}

	Self& operator--()
	{
		_node = _node->_prev;
		return *this;
	}

	Self& operator--(int)
	{
		Self tmp(*this);
		_node = _node->_prev;
		return tmp;
	}

	bool operator!=(const Self& s) const
	{
		return _node != s._node;
	}

	bool operator==(const Self& s) const
	{
		return _node == s._node;
	}
};

template<class T, class Alloc = alloc>
class MyList
{
	typedef ListNode<T> Node;
public:
	typedef __ListIterator<T, T&, T*> Iterator;
	typedef __ListIterator<T, const T&, const T*> ConstIterator;

	typedef SimpleAlloc<Node, Alloc> ListNodeAllocator;

	Node* BuyNode(const T& x)
	{
		Node* node = ListNodeAllocator::Allocate();
		new(node)Node(x);

		return node;
	}

	void DestoryNode(Node* node)
	{
		node->~Node();
		ListNodeAllocator::Deallocate(node);
	}

	MyList()
		:_head(BuyNode(T()))
	{
		_head->_next = _head;
		_head->_prev = _head;
	}

	~MyList()
	{
		Clear();

		DestoryNode(_head);
		_head = NULL;
	}

	void Clear()
	{
		Iterator cur = Begin();
		while (cur != End())
		{
			Iterator del = cur;
			++cur;

			DestoryNode(del._node);
		}

		_head->_next = _head;
		_head->_prev = _head;
	}

	void PushBack(const T& x)
	{
		Insert(End(), x);
	}

	void PopBack()
	{
		Erase(--End());
	}

	void PushFront(const T& x)
	{
		Insert(Begin(), x);
	}

	void PopFront()
	{
		Erase(Begin());
	}

	Iterator Begin()
	{
		return Iterator(_head->_next);
	}

	Iterator End()
	{
		return Iterator(_head);
	}

	ConstIterator Begin() const
	{
		return ConstIterator(_head->_next);
	}

	ConstIterator End() const
	{
		return ConstIterator(_head);
	}


	Iterator Insert(Iterator pos, const T& x)
	{
		Node* cur = pos._node;
		Node* prev = cur->_prev;
		//Node* newnode = new Node(x);
		Node* newnode = BuyNode(x);
		//prev newnode cur

		prev->_next = newnode;
		newnode->_prev = prev;

		newnode->_next = cur;
		cur->_prev = newnode;

		return newnode;
	}

	// 设计迭代器不失效的Erase
	Iterator Erase(Iterator& pos)
	{
		//assert(pos != End());
		assert(pos != Iterator(_head));

		Node* cur = pos._node;
		Node* prev = cur->_prev;
		Node* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
		//delete cur;
		DestoryNode(cur);

		pos = prev;

		return next;
	}

private:
	Node * _head;
};

void PrintList(const MyList<int>& l)
{
	MyList<int>::ConstIterator it = l.Begin();
	while (it != l.End())
	{
		//*it = 10;
		cout << *it << " ";
		++it;
	}
	cout << endl;
}

void TestMyList()
{
	MyList<int> l;
	l.PushBack(1);
	l.PushBack(2);
	l.PushBack(3);
	l.PushBack(4);

	//MyList<int>::Iterator it = l.Begin();
	//while (it != l.End())
	//{
	//	if (*it % 2 == 0)
	//	{
	//		//it = l.Erase(it);
	//		l.Erase(it);
	//	}

	//	++it;
	//}

	PrintList(l);
}

template<class Alloc>
void TestListAlloc(size_t n)
{
	MyList<string, Alloc> l;
	for (size_t i = 0; i < n; ++i)
	{
		l.PushBack("1111111111");
	}
};

#include <time.h>

void TestListOP()
{
	size_t n = 1000;
	size_t begin1 = clock();

	for (size_t i = 0; i < n; ++i)
	{
		TestListAlloc<__MallocAllocTemplate<0>>(10000);
	}

	size_t end1 = clock();

	size_t begin2 = clock();

	for (size_t i = 0; i < n; ++i)
	{
		TestListAlloc<__DefaultAllocTemplate<false, 0>>(10000);
	}
	size_t end2 = clock();

	cout << end1 - begin1 << endl;
	cout << end2 - begin2 << endl;
}
