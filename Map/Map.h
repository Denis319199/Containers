#pragma once
#include "Tree.h"

namespace mylib {
	template<class Key, class T, class Compare = std::less<Key>, class Allocator = std::allocator<std::pair<const Key, T>>>
	class Map : public Tree<MapTraits<Key, T, Compare, Allocator>> {
	public:
		using Base				= Tree<MapTraits<Key, T, Compare, Allocator>>;
		using allocator_type	= typename Base::allocator_type;
		using key_type			= typename Base::key_type;
		using mapped_type		= T;
		using value_type		= typename Base::value_type;
		using key_compare		= typename Base::key_compare;

	protected:
		using Node				= TreeNode<value_type, typename std::allocator_traits<allocator_type>::void_pointer>;
		using Alloc				= typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;
		using AllocTraits		= std::allocator_traits<Alloc>;
		using NodePtr			= typename AllocTraits::pointer;
		using TreeValue			= TreeValue<MapTraits<Key, T, Compare, Allocator>>;
		using uncheked_iterator = TreeUncheckedIterator<TreeValue>;

	public:
		using size_type			= typename AllocTraits::size_type;
		using difference_type	= typename AllocTraits::difference_type;
		using pointer			= typename AllocTraits::pointer;
		using const_pointer		= typename AllocTraits::const_pointer;
		using reference			= value_type&;
		using const_reference	= const value_type&;

		using iterator			= TreeIterator<TreeValue>;
		using const_iterator	= TreeConstIterator<TreeValue>;

		Map() : Base(Compare{}, Allocator{}) {}

		explicit Map(const Compare& comp, const Allocator& alloc = Allocator()) : Base(comp, alloc) {}

		explicit Map(const Allocator& alloc) : Base(Compare{}, alloc) {}

		template<class InputIt>
		Map(InputIt first, InputIt last, const Compare& comp = Compare(), const Allocator& alloc = Allocator()) : Base(comp, alloc) {
			this->insert(first, last);
		}

		template<class InputIt>
		Map(InputIt first, InputIt last, const Allocator& alloc) : Base(Compare{}, alloc) {
			this->insert(first, last);
		}

		Map(const Map& other) : Base(other, AllocTraits::select_on_container_copy_construction(other.tree_value.alloc)) {}

		Map(const Map& other, const Allocator& alloc) : Base(other, alloc) {}

		Map(Map&& other) : Base(std::move(other), std::move(other.tree_value.alloc)) {}

		Map(Map&& other, const Allocator& alloc) : Base(std::move(other), alloc) {}

		Map(std::initializer_list<value_type> init, const Compare& comp = Compare(), const Allocator& alloc = Allocator()) : Base(comp, alloc) {
			this->insert(init);
		}

		Map(std::initializer_list<value_type> init, const Allocator alloc) : Base(Compare{}, alloc) {
			this->insert(init);
		}

		using Base::insert;

		template<class P, std::enable_if_t<std::is_constructible_v<value_type, P>, int> = 0>
		std::pair<iterator, bool> insert(P&& value) {
			return this->emplace(std::forward<P>(value));
		}

		template<class P, std::enable_if_t<std::is_constructible_v<value_type, P>, int> = 0>
		iterator insert(const_iterator hint, P&& value) {
			return this->emplaceHint(hint, std::forward<P>(value));
		}

		template<class... Args>
		std::pair<iterator, bool> tryEmplace(const key_type& key, Args&&... args) {
			std::pair<NodePtr, bool> result = tryEmplace_(key, std::forward<Args>(args)...);
			return { iterator(&(this->tree_value), result.first), result.second };
		}

		template<class... Args>
		std::pair<iterator, bool> tryEmplace(key_type&& key, Args&&... args) {
			std::pair<NodePtr, bool> result = tryEmplace_(std::move(key), std::forward<Args>(args)...);
			return { iterator(&(this->tree_value), result.first), result.second };
		}

		template<class... Args>
		iterator tryEmplace(const_iterator hint, const key_type& key, Args&&... args) {
			return this->emplaceHint(hint, key, std::forward<Args>(args)...);
		}

		template<class... Args>
		iterator tryEmplace(const_iterator hint, key_type&& key, Args&&... args) {
			return this->emplaceHint(hint, std::move(key), std::forward<Args>(args)...);
		}

		template<class Key, class... Args>
		std::pair<NodePtr, bool> tryEmplace_(Key&& key, Args&&... args) { 
			TreeFindResult<NodePtr> result = this->findPlaceForNode(key);
			if (result.duplicate) return { result.location.parent, false };
			this->checkGrow();
			NodePtr node_for_insertion = TreeTempNode(this->tree_value.alloc, this->tree_value.head, std::piecewise_construct, std::forward_as_tuple(std::forward<Key>(key)),
				std::forward_as_tuple(std::forward<Args>(args)...)).release();
			return { this->tree_value.insertNode(result.location, node_for_insertion), true };
		}
	};
}