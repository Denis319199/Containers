#pragma once
#include "ContainerUtilities.h"

namespace mylib {
	template<class ValueType, class VoidPtr>
	struct ListNode {
		using value_type	= ValueType;
		using NodePtr		= typename std::pointer_traits<VoidPtr>::template rebind<ListNode>;

		NodePtr next;
		NodePtr prev;
		value_type value;

		template<class Alloc>
		static NodePtr createHeadNode(Alloc& alloc) {
			static_assert(std::is_same_v<typename Alloc::value_type, ListNode>, "Mismatch of tree node type and value type of allocator!");
			const NodePtr new_node = alloc.allocate(1);
			construct(alloc, std::addressof(new_node->next), new_node);
			construct(alloc, std::addressof(new_node->prev), new_node);
			return new_node;
		}

		template<class Alloc>
		static void freeHeadNode(Alloc& alloc, NodePtr head) {
			static_assert(std::is_same_v<typename Alloc::value_type, ListNode>, "Mismatch of tree node type and value type of allocator!");
			destroy(alloc, std::addressof(head->next));
			destroy(alloc, std::addressof(head->prev));
			alloc.deallocate(head, 1);
		}

		template<class Alloc>
		static void freeNode(Alloc& alloc, NodePtr node) {
			static_assert(std::is_same_v<typename Alloc::value_type, ListNode>, "Mismatch of tree node type and value type of allocator!");
			destroy(alloc, std::addressof(node->value));
			freeHeadNode(alloc, node);
		}

		template<class Alloc>
		static void freeNonHeadNode(Alloc& alloc, NodePtr head) {
			NodePtr erased_node = head->next;
			while (erased_node != head) {
				erased_node = erased_node->next;
				freeNode(alloc, erased_node->prev);
			}
		}
	};

	template<class Alloc>
	struct ListTmpNodes {
		using NodePtr = typename std::allocator_traits<Alloc>::pointer;
		using Node = typename std::allocator_traits<Alloc>::value_type;
		using value_type = typename Node::value_type;
		using size_type = typename std::allocator_traits<Alloc>::size_type;
		
		ListTmpNodes(Alloc& alloc) : first{}, last{}, added{ 0 }, alloc{ alloc } {}

		ListTmpNodes(const Alloc&) = delete;
		ListTmpNodes& operator=(const Alloc&) = delete;

		template<class... Args>
		void createNode(Args&&... args) {
			first = last = alloc.allocate(1); // throws
			construct(alloc, std::addressof(first->value), std::forward<Args>(args)...); // throws
		}
	
		template<class... Args>
		void createNumOfNodes(size_type count, const Args&... args) {
			if (count <= 0) return;

			first = last = alloc.allocate(1); // throws
			construct(alloc, std::addressof(first->value), args...); // throws
			--count;
			++added;

			while (count) {
				construct(alloc, std::addressof(last->next), alloc.allocate(1)); // throws
				construct(alloc, std::addressof(last->next->prev), last);
				last = last->next;
				construct(alloc, std::addressof(last->value), args...); // throws
				--count;
				++added;
			}
		}

		template<class InputIt, class Tag>
		void createNodesFromRange(InputIt first, InputIt last, Tag tag) {
			if (first == last) return;

			this->first = this->last = alloc.allocate(1); // throws
			construct(alloc, std::addressof(this->first->value), copyOrMoveObject(*first, tag)); // throws
			++added;
			++first;

			while (first != last) {
				construct(alloc, std::addressof(this->last->next), alloc.allocate(1)); // throws
				construct(alloc, std::addressof(this->last->next->prev), this->last);
				this->last = this->last->next;
				construct(alloc, std::addressof(this->last->value), copyOrMoveObject(*first, tag)); // throws
				++added;
				++first;
			}
		}

		NodePtr insertNodes(NodePtr where) noexcept {
			construct(alloc, std::addressof(first->prev), where->prev);
			where->prev->next = first;
			construct(alloc, std::addressof(last->next), where);
			where->prev = last;
			return std::exchange(first, nullptr);
		}

		~ListTmpNodes() {
			if (!first) return;

			construct(alloc, std::addressof(first->prev), NodePtr{});
			construct(alloc, std::addressof(last->next), NodePtr{});

			while (first) {
				Node::freeNode(alloc, std::exchange(first, first->next));
			}
		}

		NodePtr first;
		NodePtr last;
		size_type added;
		Alloc& alloc;
	};

	template<class ValueType, class AllocType, class SizeType, class DifferenceType, class Pointer,
			 class ConstPoiner, class Reference, class ConstReference, class NodePtrType>
	struct ListTypesWrapper {
		using value_type		= ValueType;
		using Alloc				= AllocType;
		using size_type			= SizeType;
		using difference_type	= DifferenceType;
		using pointer			= Pointer;
		using const_pointer		= ConstPoiner;
		using reference			= Reference;
		using const_reference	= ConstReference;
		using NodePtr			= NodePtrType;
	};

	template<class ListValue>
	class ListUncheckedIterator; 
								
	template<class ListValue>
	class ListConstIterator;

	template<class ListValue>
	class ListIterator;

	template<class Traits>
	class Hash;

	template<class ListTypesWrapper>
	class ListValue : public ContainerBase {
	public:
		using value_type		= typename ListTypesWrapper::value_type;
		using Alloc				= typename ListTypesWrapper::Alloc;
		using size_type			= typename ListTypesWrapper::size_type;
		using difference_type	= typename ListTypesWrapper::difference_type;
		using pointer			= typename ListTypesWrapper::pointer;
		using const_pointer		= typename ListTypesWrapper::const_pointer;
		using reference			= typename ListTypesWrapper::reference;
		using const_reference	= typename ListTypesWrapper::const_reference;
		using NodePtr			= typename ListTypesWrapper::NodePtr;

		using uncheked_iterator = ListUncheckedIterator<ListValue>;
		using const_iterator	= ListConstIterator<ListValue>;
		using iterator			= ListIterator<ListValue>;

		template<class Allocator>
		ListValue(Allocator&& alloc) : head{}, size{}, alloc{std::forward<Allocator>(alloc)} {}

		NodePtr extractNode(NodePtr node) {
			node->prev->next = node->next;
			node->next->prev = node->prev;

			return node;
		}

		NodePtr extractNodes(NodePtr first, NodePtr last) {
			first->prev->next = last;
			last->prev = first->prev;

			return last;
		}

		void orphanNonHead() {
			IteratorBase** orphan_it = &this->proxy->first;

			while (*orphan_it) {
				const NodePtr ptr = static_cast<uncheked_iterator*>(*orphan_it)->ptr;

				if (ptr != head) {
					(*orphan_it)->proxy = nullptr;
					*orphan_it = (*orphan_it)->next_iterator;
				}
				else orphan_it = &(*orphan_it)->next_iterator;
			}
		}

		void orphanPtr(NodePtr node) {
			IteratorBase** orphan_it = &this->proxy->first;

			while (*orphan_it) {
				const NodePtr ptr = static_cast<uncheked_iterator*>(*orphan_it)->ptr;

				if (ptr == node) {
					(*orphan_it)->proxy = nullptr;
					*orphan_it = (*orphan_it)->next_iterator;
				}
				else orphan_it = &(*orphan_it)->next_iterator;
			}
		}

		template<class OtherListTypesWrapper>
		void reparentPtr(NodePtr node, ListValue<OtherListTypesWrapper>& other) {
			IteratorBase** reparent_it = &other.proxy->first;

			while (*reparent_it) {
				const NodePtr ptr = static_cast<uncheked_iterator>(*reparent_it)->ptr;

				if (ptr == node) {
					IteratorBase* next_it = (*reparent_it)->next_iterator;

					(*reparent_it)->proxy = this->proxy;
					(*reparent_it)->next_iterator = this->proxy->first;
					this->proxy->first = *reparent_it;

					*reparent_it = next_it;
				}
				else reparent_it = &(*reparent_it)->next_iterator;
			}
		}

		NodePtr head;
		size_type size;
		Alloc alloc;
	};

	template<class T, class Allocator = std::allocator<T>>
	class List {
	public:
		using value_type		= T;
		using allocator_type	= Allocator;
		
	private:
		using Node			= typename ListNode<value_type, typename std::allocator_traits<allocator_type>::void_pointer>;
		using Alloc			= typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;
		using AllocTraits	= std::allocator_traits<Alloc>;
		using NodePtr		= typename AllocTraits::pointer;

	public:
		using size_type			= typename AllocTraits::size_type;
		using difference_type	= typename AllocTraits::difference_type;
		using pointer			= typename AllocTraits::pointer;
		using const_pointer		= typename AllocTraits::const_pointer;
		using reference			= value_type&;
		using const_reference	= const value_type&;
		
	private:
		using ListTypesWrapper	= ListTypesWrapper<value_type, Alloc, size_type, difference_type, pointer, const_pointer, reference, const_reference, NodePtr>;
		using ListValue			= ListValue<ListTypesWrapper>;
		using uncheked_iterator = ListUncheckedIterator<ListValue>;
		
	public:
		using const_iterator	= ListConstIterator<ListValue>;
		using iterator			= ListIterator<ListValue>;
		
		List() : list_value(Allocator{}) {
			createEmptyList();
		}

		explicit List(const Allocator& alloc) : list_value(alloc) {
			createEmptyList();
		}

		explicit List(size_type count, const T& value, const Allocator& alloc = Allocator{}) : list_value(alloc) {
			createEmptyList();
			insertNumOfNodes(list_value.head, count, value);
		}

		explicit List(size_type count, const Allocator& alloc = Allocator{}) : list_value(alloc) {
			createEmptyList();
			insertNumOfNodes(list_value.head, count);
		}

		template<class InputIt>
		explicit List(InputIt first, InputIt last, const Allocator& alloc = Allocator{}) : list_value(alloc) {
			createEmptyList();
			insertRange(list_value.head, first, last);
		}

		List(const List& other) : list_value(AllocTraits::select_on_container_copy_construction(other.list_value.alloc)) {
			createEmptyList();
			insertRange(list_value.head, other.begin(), other.end());
		}

		List(const List& other, const Allocator& alloc) : list_value(alloc) {
			createEmptyList();
			insertRange(list_value.head, other.begin(), other.end());
		}

		List(List&& other) : list_value(std::move(other.list_value.alloc)) {
			createEmptyList();
			swapValue(other);
		}

		List(List&& other, const Allocator& alloc) : list_value(alloc) {
			createEmptyList();

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (list_value.alloc != other.list_value.alloc) {
					insertRange(list_value.head, other.begin(), other.end(), MoveTag{});
					other.clear();
					return;
				}
			}

			swapValue(other);
		}

		List(std::initializer_list<T> init, const Allocator& alloc = Allocator{}) : list_value(alloc) {
			createEmptyList();
			insertRange(list_value.head, init.begin(), init.end());
		}

	private:
		template<class Tag>
		void copyOrMoveList(List& other, Tag tag) {
			NodePtr other_head		= other.list_value.head;
			NodePtr insert_node		= other.list_value.head->next;
			NodePtr head			= list_value.head;
			NodePtr revalue_node	= list_value.head->next;

			if (list_value.size >= other.list_value.size) {
				while (insert_node != other_head) {
					revalue_node->value = copyOrMoveObject(insert_node->value, tag);
					insert_node = insert_node->next;
					revalue_node = revalue_node->next;
				}
				eraseRange(revalue_node, head);
			}
			else {
				while (revalue_node != head) {
					revalue_node->value = copyOrMoveObject(insert_node->value, tag);
					insert_node = insert_node->next;
					revalue_node = revalue_node->next;
				}
				insertRange(head, uncheked_iterator{ &other.list_value, insert_node }, uncheked_iterator{ &other.list_value, other_head }, tag);
			}

			list_value.size = other.list_value.size;
		}

	public:
		List& operator=(const List& other) {
			if (this == &other) return *this;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (list_value.alloc != other.list_value.alloc) {
					if constexpr (AllocTraits::propagate_on_container_copy_assignment::value) {
						tidy();
						list_value.alloc = other.list_value.alloc;
						createEmptyList();
						insertRange(list_value.head, other.begin(), other.end());
						return *this;
					}	
				}
			}

			copyOrMoveList(other, CopyTag{});
			return *this;
		}

		List& operator=(List&& other) noexcept(AllocTraits::is_always_equal::value) {
			if (this == &other) return *this;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (list_value.alloc != other.list_value.alloc) {
					if constexpr (AllocTraits::propagate_on_container_move_assignment::value) {
						tidy();
						list_value.alloc = std::move(other.list_value.alloc);
						createEmptyList();
						insertRange(list_value.head, other.begin(), other.end(), MoveTag{});
					}
					else  copyOrMoveList(other, MoveTag{}); 
					other.clear();
						
					return *this;
				}
			}

			clear();
			swapValue(other);

			return *this;
		}

	private:
		void swapValue(List& other) {
			std::swap(list_value.head, other.list_value.head);
			std::swap(list_value.size, other.list_value.size);

			std::swap(list_value.proxy, other.list_value.proxy);
			list_value.proxy->parent = &list_value;
			other.list_value.proxy->parent = &other.list_value;
		}

	public:
		~List() {
			tidy();
		}

	private:
		void createEmptyList() {
			list_value.head = Node::createHeadNode(list_value.alloc);
			list_value.createProxy(static_cast<typename AllocTraits::template rebind_alloc<IteratorProxy>>(list_value.alloc));
		}

		void tidy() {
			list_value.orphanAll();
			Node::freeNonHeadNode(list_value.alloc, list_value.head);
			Node::freeHeadNode(list_value.alloc, list_value.head);
			list_value.deleteProxy(static_cast<typename AllocTraits::template rebind_alloc<IteratorProxy>>(list_value.alloc));
			list_value.size = 0;
		}

	public:
		void clear() {
			list_value.orphanNonHead();
			Node::freeNonHeadNode(list_value.alloc, list_value.head);
			list_value.head->prev = list_value.head;
			list_value.head->next = list_value.head;
			list_value.size = 0;
		}

	public:
		allocator_type getAllocator() {
			return static_cast<allocator_type>(list_value.alloc);
		}

		reference front() {
			return list_value.head->next->value;
		}

		const_reference front() const {
			return list_value.head->next->value;
		}

		reference back() {
			return list_value.head->prev->value;
		}

		const_reference back() const {
			return list_value.head->prev->value;
		}

		iterator insert(const_iterator where, const T& value) {
			return emplace(where, value);
		}

		iterator insert(const_iterator where, T&& value) {
			return emplace(where, std::move(value));
		}

	private:
		template<class... Args>
		NodePtr insertNumOfNodes(NodePtr where, size_type count, const Args&... args) {
			ListTmpNodes new_nodes(list_value.alloc);
			new_nodes.createNumOfNodes(count, args...);
			list_value.size += count;
			return new_nodes.insertNodes(where);
		}

	public:
		iterator insert(const_iterator where, size_type count, const T& value) {
			assert(where.getContainer() == &list_value && "Iterator doesn't belong to the container");
			return { &list_value, insertNumOfNodes(where.ptr, count, value) };
		}

	private:
		template<class InputIt, class Tag = CopyTag>
		NodePtr insertRange(NodePtr where, InputIt first, InputIt last, Tag tag = Tag{}) {
			ListTmpNodes new_nodes(list_value.alloc);
			new_nodes.createNodesFromRange(first, last, tag);
			list_value.size += new_nodes.added;
			return new_nodes.insertNodes(where);
		}

	public:
		template<class InputIt, std::void_t<typename std::iterator_traits<InputIt>::iterator_category>* = nullptr>
		iterator insert(const_iterator where, InputIt first, InputIt last) {
			assert(where.getContainer() == &list_value && "Iterator doesn't belong to the container");
			return iterator{ &list_value, insertRange(where.ptr, first, last) };
		}

		iterator insert(const_iterator where, std::initializer_list<T> ilist) {
			return insert(where, ilist.begin(), ilist.end());
		}

		template<class... Args>
		iterator emplace(const_iterator where, Args&&... args) {
			assert(where.getContainer() == &list_value && "Iterator doesn't belong to the container");
			return iterator{ &list_value, emplaceNode(where.ptr, std::forward<Args>(args)...) };
		}

	private:
		template<class... Args>
		NodePtr emplaceNode(const NodePtr where, Args&&... args) { 
			checkGrow();
			ListTmpNodes new_node(list_value.alloc);
			new_node.createNode(std::forward<Args>(args)...);
			++list_value.size;
			return new_node.insertNodes(where);
		}

	public:
		iterator erase(const_iterator iter) {
			assert(iter.getContainer() && "Invalid iterator");
			assert(iter.getContainer() == &list_value && "Iterator from another container");
			return iterator{ &list_value, eraseNode(iter.ptr) };
		}

		iterator erase(const_iterator first, const_iterator last) {
			assert(first.getContainer() && last.getContainer() && "Invalid iterator");
			assert(first.getContainer() == &list_value && last.getContainer() == &list_value && "Iterator from another container");
			return iterator{ &list_value, eraseRange(first.ptr, last.ptr) };
		}

	private:
		NodePtr eraseNode(NodePtr ptr) { 
			NodePtr next_node = ptr->next;
			list_value.extractNode(ptr);
			list_value.orphanPtr(ptr);
			Node::freeNode(list_value.alloc, ptr);
			--list_value.size;
			return next_node;
		}

		NodePtr eraseRange(NodePtr first, NodePtr last) { 
			if (first == last) return last;

			if (first == list_value.head->next && last == list_value.head) {
				clear();
				return last;
			}

			list_value.extractNodes(first, last);
			while (first != last) {
				list_value.orphanPtr(first);
				Node::freeNode(list_value.alloc, std::exchange(first, first->next));
				--list_value.size;
			}

			return last;
		}

	public:
		void pushBack(const T& value) {
			emplaceNode(list_value.head, value);
		}

		void pushBack(T&& value) {
			emplaceNode(list_value.head, std::move(value));
		}

		template<class... Args>
		reference emplaceBack(Args&&... args) {
			return emplaceNode(list_value.head, std::forward<Args>(args)...)->value;
		}

		void popBack() {
			eraseNode(list_value.head->prev);
		}

		void pushFront(const T& value) {
			emplaceNode(list_value.head->next, value);
		}

		void pushFront(T&& value) {
			emplaceNode(list_value.head->next, std::move(value));
		}

		template<class... Args>
		reference emplaceFront(Args&&... args) {
			return emplaceNode(list_value.head->next->next, std::forward<Args>(args)...)->value;
		}

		void popFront() {
			eraseNode(list_value.head->next);
		}

		void swap(List& other) noexcept(AllocTraits::is_always_equal::value) {
			if (this == &other) return;
			if constexpr (!AllocTraits::is_always_equal::value) {
				if constexpr (!AllocTraits::propagate_on_container_swap::value) assert(!"propagate_on_container_swap = false");
				std::swap(list_value.alloc, other.list_value.alloc);
			}

			swapValue(other);
		}

	private:
		void checkGrow() const noexcept {
			assert(maxSize() != list_value.size && "The lack of memory error");
		}

	public:
		[[nodiscard]] bool empty() const noexcept {
			return list_value.size == 0;
		}

		[[nodiscard]] size_type size() const noexcept {
			return list_value.size;
		}

		[[nodiscard]] size_type maxSize() const noexcept {
			return std::min(static_cast<size_type>(std::numeric_limits<difference_type>::max()), AllocTraits::max_size(list_value.alloc));
		}

		[[nodiscard]] iterator begin() noexcept {
			return iterator{ &list_value, list_value.head->next };
		}

		[[nodiscard]] const_iterator begin() const noexcept {
			return const_iterator{ &list_value, list_value.head->next };
		}

		[[nodiscard]] const_iterator cbegin() const noexcept {
			return const_iterator{ &list_value, list_value.head->next };
		}

		[[nodiscard]] iterator end() noexcept {
			return iterator{ &list_value, list_value.head };
		}

		[[nodiscard]] const_iterator end() const noexcept {
			return const_iterator{ &list_value, list_value.head };
		}

		[[nodiscard]] const_iterator cend() const noexcept {
			return const_iterator{ &list_value, list_value.head };
		}

		template<class Compare>
		void sort(Compare cmp) {
			std::size_t step = 1;
			std::size_t list_size = list_value.size;
			NodePtr head_ptr = list_value.head;

			while (list_size > step) {
				NodePtr left_ptr = head_ptr->next;
				NodePtr right_ptr = left_ptr;
				for (std::size_t i = 0; i < step; ++i) right_ptr = right_ptr->next;

				std::size_t n_proccesed_nodes = 0;

				while (right_ptr != head_ptr) {
					std::size_t left_counter = 0;
					std::size_t right_counter = 0;

					while (left_counter != step && right_counter != step) {
						if (right_ptr == head_ptr) break;
						
						if (cmp(left_ptr->value, right_ptr->value)) {
							left_ptr = left_ptr->next;
							++left_counter;
						}
						else {
							NodePtr next_right_ptr = right_ptr->next;

							right_ptr->prev->next = right_ptr->next;
							right_ptr->next->prev = right_ptr->prev;
							right_ptr->next = left_ptr;
							right_ptr->prev = left_ptr->prev;
							left_ptr->prev->next = right_ptr;
							left_ptr->prev = right_ptr;
							
							right_ptr = next_right_ptr;
							++right_counter;
						}
					}

					n_proccesed_nodes += step + step;
					if (n_proccesed_nodes + step >= list_size) break;

					if (right_counter == step) left_ptr = right_ptr;
					else {
						left_ptr = right_ptr;
						for (; right_counter < step; ++right_counter) left_ptr = left_ptr->next;
					}
					
					right_ptr = left_ptr;
					for (std::size_t i = 0; i < step; ++i) right_ptr = right_ptr->next;			
				}

				step <<= 1;
			}
		}

	private:
		ListValue list_value; // list value

		template<class Traits>
		friend class Hash;
	};

	template<class ListValue>
	class ListUncheckedIterator : public IteratorBase {
	public:
		using iterator_category = std::bidirectional_iterator_tag;

		using value_type		= typename ListValue::value_type;
		using size_type			= typename ListValue::size_type;
		using difference_type	= typename ListValue::difference_type;
		using pointer			= value_type*;
		using reference			= typename ListValue::reference;
		using NodePtr			= typename ListValue::NodePtr;

		ListUncheckedIterator() : ptr{} {}

		ListUncheckedIterator(const ListValue* container, NodePtr ptr) : ptr{ ptr } {
			this->adopt(container);
		}

		[[nodiscard]] reference operator*() const noexcept {
			return ptr->value;
		}

		[[nodiscard]] pointer operator->() const noexcept {
			return std::addressof(ptr->value);
		}

		ListUncheckedIterator& operator++() noexcept {
			ptr = ptr->next;
			return *this;
		}

		ListUncheckedIterator operator++(int) noexcept {
			ListUncheckedIterator tmp = *this;
			++*this;
			return tmp;
		}

		ListUncheckedIterator& operator--() noexcept {
			ptr = ptr->prev;
			return *this;
		}

		ListUncheckedIterator operator--(int) noexcept {
			ListUncheckedIterator tmp = *this;
			--*this;
			return tmp;
		}

		[[nodiscard]] bool operator==(const ListUncheckedIterator& rhs) const noexcept {
			return ptr == rhs.ptr;
		}

		[[nodiscard]] bool operator!=(const ListUncheckedIterator& rhs) const noexcept {
			return !(*this == rhs);
		}

		NodePtr ptr;
	};

	template<class ListValue>
	class ListConstIterator : public ListUncheckedIterator<ListValue> {
	public:
		using iterator_category = std::bidirectional_iterator_tag;

		using Base				= ListUncheckedIterator<ListValue>;
		using value_type		= typename ListValue::value_type;
		using size_type			= typename ListValue::size_type;
		using difference_type	= typename ListValue::difference_type;
		using pointer			= const value_type*;
		using reference			= typename ListValue::const_reference;
		using NodePtr			= typename ListValue::NodePtr;

		using Base::Base;

		[[nodiscard]] Base unwrapIterator() const noexcept {
			return Base(static_cast<const ListValue*>(this->getContainer()), this->ptr);
		}

		[[nodiscard]] reference operator*() const noexcept {
			assert(this->getContainer() && "Invalid iterator");
			assert(static_cast<const ListValue*>(this->getContainer())->head != this->ptr && "Cannot dereference the end");
			return this->ptr->value;
		}

		[[nodiscard]] pointer operator->() const noexcept {
			assert(this->getContainer() && "Invalid iterator");
			assert(static_cast<const ListValue*>(this->getContainer())->head != this->ptr && "Cannot dereference the end");
			return std::addressof(this->ptr->value);
		}

		ListConstIterator& operator++() noexcept {
			assert(this->getContainer() && "Invalid iterator");
			assert(static_cast<const ListValue*>(this->getContainer())->head != this->ptr && "Cannot increment the end");
			this->ptr = this->ptr->next;
			return *this;
		}

		ListConstIterator operator++(int) noexcept {
			ListConstIterator tmp = *this;
			++*this;
			return tmp;
		}

		ListConstIterator& operator--() noexcept {
			assert(this->getContainer() && "Invalid iterator");
			assert(static_cast<const ListValue*>(this->getContainer())->head->next != this->ptr && "Cannot decrement the begin");
			this->ptr = this->ptr->prev;
			return *this;
		}

		ListConstIterator operator--(int) noexcept {
			ListConstIterator tmp = *this;
			--*this;
			return tmp;
		}

		[[nodiscard]] bool operator==(const ListConstIterator& rhs) const noexcept {
			return this->getContainer() && rhs.getContainer() ? this->ptr == rhs.ptr : false;
		}

		[[nodiscard]] bool operator!=(const ListConstIterator& rhs) const noexcept {
			return !(*this == rhs);
		}
	};

	template<class ListValue>
	class ListIterator : public ListConstIterator<ListValue> {
	public:
		using iterator_category = std::bidirectional_iterator_tag;

		using Base				= ListConstIterator<ListValue>;
		using value_type		= typename ListValue::value_type;
		using size_type			= typename ListValue::size_type;
		using difference_type	= typename ListValue::difference_type;
		using pointer			= value_type*;
		using reference			= typename ListValue::reference;
		using NodePtr			= typename ListValue::NodePtr;

		using Base::Base;

		[[nodiscard]] reference operator*() const noexcept {
			return const_cast<reference>(Base::operator*());
		}

		[[nodiscard]] pointer operator->() const noexcept {
			return const_cast<pointer>(Base::operator->());
		}

		ListIterator& operator++() noexcept {
			Base::operator++();
			return *this;
		}

		ListIterator operator++(int) noexcept {
			ListIterator tmp = *this;
			Base::operator++();
			return tmp;
		}

		ListIterator& operator--() noexcept {
			Base::operator--();
			return *this;
		}

		ListIterator operator--(int) noexcept {
			ListIterator tmp = *this;
			Base::operator--();
			return tmp;
		}
	};
}