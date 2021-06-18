#pragma once
#include <memory>
#include <algorithm>
#include <cassert>

namespace mylib {
	template<class Alloc, class T, class... Args>
	void construct(Alloc& alloc, T ptr, Args&&... args) {
		std::allocator_traits<Alloc>::construct(alloc, ptr, std::forward<Args>(args)...);
	}

	template<class Alloc, class T>
	void destroy(Alloc& alloc, T ptr) {
		std::allocator_traits<Alloc>::destroy(alloc, ptr);
	}

	template<class Ptr>
	constexpr auto unfancy(Ptr ptr) {
		return std::addressof(*ptr);
	}

	template<class T>
	constexpr T* unfancy(T* ptr) {
		return ptr;
	}

	struct MoveTag {
		explicit MoveTag() {}
	};

	struct CopyTag {
		explicit CopyTag() {}
	};

	template<class T>
	T& copyOrMoveObject(T& obj, CopyTag) {
		return obj;
	}

	template<class T>
	T&& copyOrMoveObject(T& obj, MoveTag) {
		return std::move(obj);
	}

	class ContainerBase;
	class IteratorBase;
	struct IteratorProxy {
		IteratorProxy() : parent{}, first{} {}
		IteratorProxy(ContainerBase* parent) : parent{ parent }, first{} {}

		const ContainerBase* parent;
		IteratorBase* first;
	};

	class ContainerBase { 
	public:
		ContainerBase() : proxy{} {}
		ContainerBase(const ContainerBase&) = delete;
		ContainerBase& operator=(const ContainerBase&) = delete;

		template<class Alloc>
		void createProxy(Alloc&& alloc) {
			proxy = unfancy(alloc.allocate(1));
			::new(proxy) IteratorProxy(this);
		}

		void orphanAll() noexcept;

		template<class Alloc>
		void deleteProxy(Alloc&& alloc) {
			alloc.deallocate(proxy, 1);
		}

		IteratorProxy* proxy;
	};

	class IteratorBase {
	public:
		IteratorBase() noexcept;

		IteratorBase(const IteratorBase& rhs) noexcept;

		IteratorBase& operator=(const IteratorBase& rhs) noexcept;

		~IteratorBase();

		void adopt(const ContainerBase* parent) noexcept;
		
		void orphanMe() noexcept;

		const ContainerBase* getContainer() const noexcept;

		IteratorProxy* proxy;
		IteratorBase* next_iterator;
	};
}