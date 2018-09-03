#pragma once

#include<stdarg.h>
#include<string>
#include<iostream>
#include<stdlib.h>
#include<malloc.h>

using namespace std;

typedef void(*HANDLE_FUNC)();

//一级空间配置器
//模拟C++的set_new_handler() 以处理内存不足的情况
template <int inst> // 预留参数 instance
class __MallocAllocTemplate {
private:

	static HANDLE_FUNC __malloc_alloc_oom_handler;

	static void* OOM_Malloc(size_t n)
	{
		while (1)//不断尝试释放，配置，再释放，再配置...
		{
			if (__malloc_alloc_oom_handler == 0)
			{
				throw bad_alloc();
			}
			else
			{
				__malloc_alloc_oom_handler();  //调用处理例程，企图释放内存
				void* ret = malloc(n);		   //再次尝试配置内存
				if (ret)
					return ret;
			}
		}
	}
public:
	static void* Allocate(size_t n)
	{
		//第一级直接使用malloc()
		void *result = malloc(n);
		//无法满足需求时，改用OOM_Malloc()
		if (0 == result)
			result = OOM_Malloc(n);

		return result;
	}

	//第一级配置器直接使用free
	static void Deallocate(void *p, size_t /* n */)
	{
		free(p);
	}

	//仿真C++的set_new_handler
	//你可以通过它指定你自己的out-of-memory handler
	static HANDLE_FUNC SetMallocHandler(HANDLE_FUNC f)
	{
		HANDLE_FUNC old = f;
		__malloc_alloc_oom_handler = f;
		return old;
	}
};

template<int inst>
HANDLE_FUNC __MallocAllocTemplate<inst>::__malloc_alloc_oom_handler = 0;

void FreeMemory()
{
	cout << "释放内存" << endl;
}

void Test_Alloc1()
{
	__MallocAllocTemplate<0>::SetMallocHandler(FreeMemory);

	void* p = __MallocAllocTemplate<0>::Allocate(0x7fffffff);
	__MallocAllocTemplate<0>::Deallocate(p, 40);
}

///////////////////////////////////////////////////////////////////////
//第二级空间配置器
//维护16个自由链表，负责16种小型区块的次配置能力
//内存池(memory pool)以malloc配置而得
//如果内存不足，转调用一级配置器
//如果需求区块大于128bytes,就转调用第一级配置器
template <bool threads, int inst>       //第一个参数用于多线程环境下，此处不讨论多线程环境，无用
                                        //第二个参数也没派上用场
class __DefaultAllocTemplate
{
public:
	// 65	72		8
	// 72	79
	//根据区块大小，决定使用第n号free-list，n从0开始计算
	static size_t FREELIST_INDEX(size_t n)
	{
		return ((n + __ALIGN - 1) / __ALIGN - 1);
	}

	// 65	72	-> 72
	// 72	79
	//将bytes上调到8的倍数
	static size_t ROUND_UP(size_t bytes)
	{
		return (((bytes)+__ALIGN - 1) & ~(__ALIGN - 1));
	}

	//从内存池子中取空间给freelist使用
	//配置一大块空间，可容纳nobjs个大小为"size"的区块
	//如果配置nobjs个区块有所不便，nobjs可能会降低
	static void* ChunkAlloc(size_t size, size_t& nobjs)     
	{
		size_t totalbytes = nobjs * size;
		size_t leftbytes = _endfree - _startfree;   //内存池剩余空间

		//内存池剩余空间完全满足需求量
		if (leftbytes >= totalbytes)
		{
			void* ret = _startfree;
			_startfree += totalbytes;
			return ret;
		}

		//内存池剩余空间下不能完全满足需求量，但足够供应一个或一个以上的区块
		else if (leftbytes > size)
		{
			nobjs = leftbytes / size;
			totalbytes = size * nobjs;

			void* ret = _startfree;
			_startfree += totalbytes;
			return ret;
		}

		//内存池剩余空间连一个区块的大小都无法提供
		else
		{
			// 先处理掉剩余的小块内存
			if (leftbytes > 0)
			{
				//首先寻找适当的free list
				size_t index = FREELIST_INDEX(leftbytes);
				//调整free list, 将内存池中的残余空间编入
				((Obj*)_startfree)->_freelistlink = _freelist[index];
				_freelist[index] = (Obj*)_startfree;
			}

			// 申请
			size_t bytesToGet = totalbytes * 2 + ROUND_UP(_heapsize >> 4);
			_startfree = (char*)malloc(bytesToGet);

			//调整heap空间，用来补充内存池
			if (_startfree == NULL)
			{
				// 试着检视我们手上拥有的东西，这不会造成伤害
				//不打算尝试配置较小的区块，因为那在多进程机器
				//上容易导致灾难
				//以下搜寻适当的free list
				size_t index = FREELIST_INDEX(size);
				for (; index < __NFREELISTS; ++index)
				{
					//如果free list内尚有未用区块
					//调整free list以释放未用区块
					if (_freelist[index])
					{
						Obj* obj = _freelist[index];
						_freelist[index] = obj->_freelistlink;
						//递归调用自己，为了修正nobjs
						return ChunkAlloc(size, nobjs);
						//任何残余零头终将被编入适当的free-list备用
					}
				}

				// 山穷水尽，最后一搏
				//调用一级空间配置器
				_startfree = (char*)__MallocAllocTemplate<0>::Allocate(bytesToGet);
			}

			_heapsize += bytesToGet;
			_endfree = _startfree + bytesToGet;

			//递归调用自己，为了修正nobjs
			return ChunkAlloc(size, nobjs);
		}
	}

	//返回一个大小为n的对象，并可能加入大小为n的其他区块到free-list
	//缺省取得20个大小为bytes的区块
	static void* Refill(size_t bytes)
	{
		size_t nobjs = 20;
		//调用chunk_alloc(), 尝试取得nobjs个区块作为free list的新节点
		char* chunk = (char*)ChunkAlloc(bytes, nobjs);
		//如果只获得一个区块，这个区块就分配给调用者用，free list无新节点
		if (nobjs == 1)
			return chunk;
		//否则准备调整free list, 纳入新节点

		size_t index = FREELIST_INDEX(bytes);    //算出位置

		//以下在chunk空间内建立free list
		Obj* cur = (Obj*)(chunk + bytes);  

		//让free list指向新的空间(取自内存池)
		_freelist[index] = cur;

		//将free list的各节点串接起来
		for (size_t i = 0; i < nobjs - 2; ++i)
		{
			//从第一个开始，第0个将返回给客户端
			Obj* next = (Obj*)((char*)cur + bytes);
			cur->_freelistlink = next;

			cur = next;
		}

		cur->_freelistlink = NULL;

		return chunk;
	}

	static void* Allocate(size_t n)
	{
		//大于128就调用一级配置器
		if (n > __MAX_BYTES)
		{
			return __MallocAllocTemplate<0>::Allocate(n);
		}

		//寻找16个free-lists中适当的一个
		size_t index = FREELIST_INDEX(n);
		if (_freelist[index] == NULL)
		{
			//没找到可用的free list，准备重新填充free list
			return Refill(ROUND_UP(n));
		}
		//调整free list
		else
		{
			Obj* ret = _freelist[index];
			_freelist[index] = ret->_freelistlink;
			return ret;
		}
		//拿下链表中第一个区块，将链表头节点指向下一个
	}

	//空间释放函数
	static void Deallocate(void* p, size_t n)
	{
		//大于128调用一级配置器
		if (n > __MAX_BYTES)
		{
			__MallocAllocTemplate<0>::Deallocate(p, n);
		}
		else
		{
			//区块回收，纳入free list
			size_t index = FREELIST_INDEX(n);

			((Obj*)p)->_freelistlink = _freelist[index];
			_freelist[index] = (Obj*)p;
		}
	}

private:
	enum { __ALIGN = 8 };         //小型区块的上调边界
	enum { __MAX_BYTES = 128 };   //小型区块的上限
	enum { __NFREELISTS = __MAX_BYTES / __ALIGN };   //free-lists个数

	//free-lists的节点构造
	union Obj
	{
		union Obj* _freelistlink;
		char client_data[1];    /* The client sees this.        */
	};

	//16个free-lists
	static Obj* _freelist[__NFREELISTS];

	// 内存池
	static char* _startfree;           //内存池起始位置，只在chunk_alloc()中变化
	static char* _endfree;             //内存池结束位置，只在chunk_alloc()中变化
	static size_t _heapsize;
};

template <bool threads, int inst>
typename __DefaultAllocTemplate<threads, inst>::Obj*
__DefaultAllocTemplate<threads, inst>::_freelist[__NFREELISTS] = { 0 };


// 内存池
template <bool threads, int inst>
char* __DefaultAllocTemplate<threads, inst>::_startfree = NULL;

template <bool threads, int inst>
char* __DefaultAllocTemplate<threads, inst>::_endfree = NULL;

template <bool threads, int inst>
size_t __DefaultAllocTemplate<threads, inst>::_heapsize = 0;

void Test_Alloc2()
{
	for (size_t i = 0; i < 20; ++i)
	{
		__DefaultAllocTemplate<false, 0>::Allocate(6);
	}

	__DefaultAllocTemplate<false, 0>::Allocate(6);
}

//
#ifdef __USE_MALLOC
typedef __MallocAllocTemplate<0> alloc;
#else
typedef __DefaultAllocTemplate<false, 0> alloc;
#endif

//无论alloc被定义为第一级或第二级配置器，SGI还为它再包装一个接口
//使配置器的接口能够符合STL规格
template<class T, class Alloc>
class SimpleAlloc {
public:
	static T* Allocate(size_t n)
	{
		return 0 == n ? 0 : (T*)Alloc::Allocate(n * sizeof(T));
	}

	static T* Allocate(void)
	{
		return (T*)Alloc::Allocate(sizeof(T));
	}

	static void Deallocate(T *p, size_t n)
	{
		if (0 != n)
			Alloc::Deallocate(p, n * sizeof(T));
	}

	static void Deallocate(T *p)
	{
		Alloc::Deallocate(p, sizeof(T));
	}
};
