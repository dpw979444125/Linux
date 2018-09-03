#pragma once

#include<stdarg.h>
#include<string>
#include<iostream>
#include<stdlib.h>
#include<malloc.h>

using namespace std;

typedef void(*HANDLE_FUNC)();

//һ���ռ�������
//ģ��C++��set_new_handler() �Դ����ڴ治������
template <int inst> // Ԥ������ instance
class __MallocAllocTemplate {
private:

	static HANDLE_FUNC __malloc_alloc_oom_handler;

	static void* OOM_Malloc(size_t n)
	{
		while (1)//���ϳ����ͷţ����ã����ͷţ�������...
		{
			if (__malloc_alloc_oom_handler == 0)
			{
				throw bad_alloc();
			}
			else
			{
				__malloc_alloc_oom_handler();  //���ô������̣���ͼ�ͷ��ڴ�
				void* ret = malloc(n);		   //�ٴγ��������ڴ�
				if (ret)
					return ret;
			}
		}
	}
public:
	static void* Allocate(size_t n)
	{
		//��һ��ֱ��ʹ��malloc()
		void *result = malloc(n);
		//�޷���������ʱ������OOM_Malloc()
		if (0 == result)
			result = OOM_Malloc(n);

		return result;
	}

	//��һ��������ֱ��ʹ��free
	static void Deallocate(void *p, size_t /* n */)
	{
		free(p);
	}

	//����C++��set_new_handler
	//�����ͨ����ָ�����Լ���out-of-memory handler
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
	cout << "�ͷ��ڴ�" << endl;
}

void Test_Alloc1()
{
	__MallocAllocTemplate<0>::SetMallocHandler(FreeMemory);

	void* p = __MallocAllocTemplate<0>::Allocate(0x7fffffff);
	__MallocAllocTemplate<0>::Deallocate(p, 40);
}

///////////////////////////////////////////////////////////////////////
//�ڶ����ռ�������
//ά��16��������������16��С������Ĵ���������
//�ڴ��(memory pool)��malloc���ö���
//����ڴ治�㣬ת����һ��������
//��������������128bytes,��ת���õ�һ��������
template <bool threads, int inst>       //��һ���������ڶ��̻߳����£��˴������۶��̻߳���������
                                        //�ڶ�������Ҳû�����ó�
class __DefaultAllocTemplate
{
public:
	// 65	72		8
	// 72	79
	//���������С������ʹ�õ�n��free-list��n��0��ʼ����
	static size_t FREELIST_INDEX(size_t n)
	{
		return ((n + __ALIGN - 1) / __ALIGN - 1);
	}

	// 65	72	-> 72
	// 72	79
	//��bytes�ϵ���8�ı���
	static size_t ROUND_UP(size_t bytes)
	{
		return (((bytes)+__ALIGN - 1) & ~(__ALIGN - 1));
	}

	//���ڴ������ȡ�ռ��freelistʹ��
	//����һ���ռ䣬������nobjs����СΪ"size"������
	//�������nobjs�������������㣬nobjs���ܻή��
	static void* ChunkAlloc(size_t size, size_t& nobjs)     
	{
		size_t totalbytes = nobjs * size;
		size_t leftbytes = _endfree - _startfree;   //�ڴ��ʣ��ռ�

		//�ڴ��ʣ��ռ���ȫ����������
		if (leftbytes >= totalbytes)
		{
			void* ret = _startfree;
			_startfree += totalbytes;
			return ret;
		}

		//�ڴ��ʣ��ռ��²�����ȫ���������������㹻��Ӧһ����һ�����ϵ�����
		else if (leftbytes > size)
		{
			nobjs = leftbytes / size;
			totalbytes = size * nobjs;

			void* ret = _startfree;
			_startfree += totalbytes;
			return ret;
		}

		//�ڴ��ʣ��ռ���һ������Ĵ�С���޷��ṩ
		else
		{
			// �ȴ����ʣ���С���ڴ�
			if (leftbytes > 0)
			{
				//����Ѱ���ʵ���free list
				size_t index = FREELIST_INDEX(leftbytes);
				//����free list, ���ڴ���еĲ���ռ����
				((Obj*)_startfree)->_freelistlink = _freelist[index];
				_freelist[index] = (Obj*)_startfree;
			}

			// ����
			size_t bytesToGet = totalbytes * 2 + ROUND_UP(_heapsize >> 4);
			_startfree = (char*)malloc(bytesToGet);

			//����heap�ռ䣬���������ڴ��
			if (_startfree == NULL)
			{
				// ���ż�����������ӵ�еĶ������ⲻ������˺�
				//�����㳢�����ý�С�����飬��Ϊ���ڶ���̻���
				//�����׵�������
				//������Ѱ�ʵ���free list
				size_t index = FREELIST_INDEX(size);
				for (; index < __NFREELISTS; ++index)
				{
					//���free list������δ������
					//����free list���ͷ�δ������
					if (_freelist[index])
					{
						Obj* obj = _freelist[index];
						_freelist[index] = obj->_freelistlink;
						//�ݹ�����Լ���Ϊ������nobjs
						return ChunkAlloc(size, nobjs);
						//�κβ�����ͷ�ս��������ʵ���free-list����
					}
				}

				// ɽ��ˮ�������һ��
				//����һ���ռ�������
				_startfree = (char*)__MallocAllocTemplate<0>::Allocate(bytesToGet);
			}

			_heapsize += bytesToGet;
			_endfree = _startfree + bytesToGet;

			//�ݹ�����Լ���Ϊ������nobjs
			return ChunkAlloc(size, nobjs);
		}
	}

	//����һ����СΪn�Ķ��󣬲����ܼ����СΪn���������鵽free-list
	//ȱʡȡ��20����СΪbytes������
	static void* Refill(size_t bytes)
	{
		size_t nobjs = 20;
		//����chunk_alloc(), ����ȡ��nobjs��������Ϊfree list���½ڵ�
		char* chunk = (char*)ChunkAlloc(bytes, nobjs);
		//���ֻ���һ�����飬�������ͷ�����������ã�free list���½ڵ�
		if (nobjs == 1)
			return chunk;
		//����׼������free list, �����½ڵ�

		size_t index = FREELIST_INDEX(bytes);    //���λ��

		//������chunk�ռ��ڽ���free list
		Obj* cur = (Obj*)(chunk + bytes);  

		//��free listָ���µĿռ�(ȡ���ڴ��)
		_freelist[index] = cur;

		//��free list�ĸ��ڵ㴮������
		for (size_t i = 0; i < nobjs - 2; ++i)
		{
			//�ӵ�һ����ʼ����0�������ظ��ͻ���
			Obj* next = (Obj*)((char*)cur + bytes);
			cur->_freelistlink = next;

			cur = next;
		}

		cur->_freelistlink = NULL;

		return chunk;
	}

	static void* Allocate(size_t n)
	{
		//����128�͵���һ��������
		if (n > __MAX_BYTES)
		{
			return __MallocAllocTemplate<0>::Allocate(n);
		}

		//Ѱ��16��free-lists���ʵ���һ��
		size_t index = FREELIST_INDEX(n);
		if (_freelist[index] == NULL)
		{
			//û�ҵ����õ�free list��׼���������free list
			return Refill(ROUND_UP(n));
		}
		//����free list
		else
		{
			Obj* ret = _freelist[index];
			_freelist[index] = ret->_freelistlink;
			return ret;
		}
		//���������е�һ�����飬������ͷ�ڵ�ָ����һ��
	}

	//�ռ��ͷź���
	static void Deallocate(void* p, size_t n)
	{
		//����128����һ��������
		if (n > __MAX_BYTES)
		{
			__MallocAllocTemplate<0>::Deallocate(p, n);
		}
		else
		{
			//������գ�����free list
			size_t index = FREELIST_INDEX(n);

			((Obj*)p)->_freelistlink = _freelist[index];
			_freelist[index] = (Obj*)p;
		}
	}

private:
	enum { __ALIGN = 8 };         //С��������ϵ��߽�
	enum { __MAX_BYTES = 128 };   //С�����������
	enum { __NFREELISTS = __MAX_BYTES / __ALIGN };   //free-lists����

	//free-lists�Ľڵ㹹��
	union Obj
	{
		union Obj* _freelistlink;
		char client_data[1];    /* The client sees this.        */
	};

	//16��free-lists
	static Obj* _freelist[__NFREELISTS];

	// �ڴ��
	static char* _startfree;           //�ڴ����ʼλ�ã�ֻ��chunk_alloc()�б仯
	static char* _endfree;             //�ڴ�ؽ���λ�ã�ֻ��chunk_alloc()�б仯
	static size_t _heapsize;
};

template <bool threads, int inst>
typename __DefaultAllocTemplate<threads, inst>::Obj*
__DefaultAllocTemplate<threads, inst>::_freelist[__NFREELISTS] = { 0 };


// �ڴ��
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

//����alloc������Ϊ��һ����ڶ�����������SGI��Ϊ���ٰ�װһ���ӿ�
//ʹ�������Ľӿ��ܹ�����STL���
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
