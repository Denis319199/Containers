#pragma once
#include "ContainerUtilities.h"

namespace mylib {
	
	template<class ValueType, class VoidPtr>
	struct TreeNode {
		using value_type = ValueType;
		using NodePtr = typename std::pointer_traits<VoidPtr>::template rebind<TreeNode>;

		NodePtr left;
		NodePtr parent;
		NodePtr right;
		ValueType value;
		bool is_nil;
		std::size_t height;

		TreeNode(const TreeNode&) = delete;
		TreeNode(TreeNode&&) = delete;

		template<class Alloc>
		static NodePtr createHeadNode(Alloc& alloc) {
			static_assert(std::is_same_v<Alloc::value_type, TreeNode>, "Mismatch of tree node type and value type of allocator!");
			const auto new_head_node = alloc.allocate(1);
			construct(alloc, std::addressof(new_head_node->left), new_head_node);
			construct(alloc, std::addressof(new_head_node->parent), new_head_node);
			construct(alloc, std::addressof(new_head_node->right), new_head_node);
			new_head_node->is_nil = true;
			new_head_node->height = 0;
			return new_head_node;
		}

		template<class Alloc, class... ValueArgs>
		static NodePtr createNode(Alloc& alloc, NodePtr head_node, ValueArgs&&... value_args) {
			static_assert(std::is_same_v<Alloc::value_type, TreeNode>, "Mismatch of tree node type and value type of allocator!");
			const auto new_node = alloc.allocate(1);
			construct(alloc, std::addressof(new_node->left), head_node);
			construct(alloc, std::addressof(new_node->parent), head_node);
			construct(alloc, std::addressof(new_node->right), head_node);
			construct(alloc, std::addressof(new_node->value), std::forward<ValueArgs>(value_args)...);
			new_node->is_nil = false;
			new_node->height = 1;
			return new_node;
		}

		template<class Alloc>
		static void freeHeadNode(Alloc& alloc, NodePtr head_node) {
			static_assert(std::is_same_v<Alloc::value_type, TreeNode>, "Mismatch of tree node type and value type of allocator!");
			destroy(alloc, std::addressof(head_node->left));
			destroy(alloc, std::addressof(head_node->parent));
			destroy(alloc, std::addressof(head_node->right));
			alloc.deallocate(head_node, 1);
		}

		template<class Alloc>
		static void freeNode(Alloc& alloc, NodePtr node) {
			static_assert(std::is_same_v<Alloc::value_type, TreeNode>, "Mismatch of tree node type and value type of allocator!");
			destroy(alloc, std::addressof(node->value));
			freeHeadNode(alloc, node);
		}
	};

	template<class Allocator>
	struct TreeTempNode {
		using NodePtr = typename std::allocator_traits<Allocator>::pointer;
		using Node = typename std::pointer_traits<NodePtr>::element_type;

		template<class... Args>
		TreeTempNode(Allocator& alloc, NodePtr head, Args... args) : ptr{ Node::createNode(alloc, head, std::forward<Args>(args)...) }, alloc{alloc} {}

		NodePtr release() {
			return std::exchange(ptr, nullptr);
		}

		TreeTempNode(const TreeTempNode&) = delete;
		TreeTempNode& operator=(const TreeTempNode&) = delete;

		~TreeTempNode() {
			if (ptr) Node::freeNode(alloc, ptr);
		}

		NodePtr ptr;
		Allocator& alloc;
	};
	
	template<class Key, class... Args>
	struct KeyExtractor {
		static const bool extractable = false;
	};

	template<class Key, class Second>
	struct KeyExtractor<Key, Key, Second> {
		static const bool extractable = true;

		static const Key& extract(const Key& key, const Second& second) {
			return key;
		}
	};

	template<class Key, class First, class Second>
	struct KeyExtractor<Key, std::pair<First, Second>> {
		static const bool extractable = std::is_same_v<Key, typename std::decay_t<First>>;

		static const Key& extract(const std::pair<First, Second>& value) {
			return value.first;
		}
	};

	template<class Key, class T, class Compare, class Allocator>
	class MapTraits {
	public:
		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<const Key, T>;
		using key_compare = Compare;
		using allocator_type = Allocator;

		template<class... Args>
		using KeyExtractor = KeyExtractor<Key, Args...>;

		template<class T1, class T2>
		static const key_type& getKeyFromValue(const std::pair<T1, T2>& value) {
			return value.first;
		}
	};

	enum class NodeChild {
		left,
		right,
	};

	template<class NodePtr>
	struct NodeID {
		NodePtr parent;
		NodeChild child;
	};

	template<class NodePtr>
	struct TreeFindResult {
		NodeID<NodePtr> location;
		bool duplicate;
	};

	enum class Rotate {
		smallLeft,
		bigLeft,
		smallRight,
		bigRight,
		none
	};

	template<class NodePtr>
	struct CheckBalanceResult {
		NodePtr node;
		Rotate rotate;
	};

	template<class TreeValue>
	class TreeUncheckedIterator;

	template<class TreeValue>
	class TreeConstIterator;

	template<class TreeValue>
	class TreeIterator;

	template<class Traits>
	class TreeValue : public ContainerBase {
	public:
		using allocator_type	= typename Traits::allocator_type;
		using key_type			= typename Traits::key_type;
		using value_type		= typename Traits::value_type;
		using reference			= value_type&;
		using const_reference	= const value_type&;
		using key_compare		= typename Traits::key_compare;
		
		using Node			= TreeNode<value_type, typename std::allocator_traits<allocator_type>::void_pointer>;
		using Alloc			= typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;
		using AllocTraits	= std::allocator_traits<Alloc>;
		using NodePtr		= typename AllocTraits::pointer;

		using size_type			= typename AllocTraits::size_type;
		using difference_type	= typename AllocTraits::difference_type;
		using pointer			= typename AllocTraits::pointer;
		using const_pointer		= typename AllocTraits::const_pointer;

		using uncheked_iterator = TreeUncheckedIterator<TreeValue>;
		using iterator			= TreeIterator<TreeValue>;
		using const_iterator	= TreeConstIterator<TreeValue>;

		template<class AnyKeyCompare, class AnyAlloc>
		TreeValue(AnyKeyCompare&& comp, AnyAlloc&& alloc) : comp{ std::forward<AnyKeyCompare>(comp) }, alloc{ std::forward<AnyAlloc>(alloc) }, head{}, size{} {}

		[[nodiscard]] static NodeChild whichChild(NodePtr node) noexcept { // FIXME: устарело - удалить
			return node->parent->left == node ? NodeChild::left : NodeChild::right;
		}

		[[nodiscard]] static NodePtr minInSubTree(NodePtr node) noexcept {
			while (!node->left->is_nil) {
				node = node->left;
			}
			return node;
		}

		[[nodiscard]] static NodePtr maxInSubTree(NodePtr node) noexcept {
			while (!node->right->is_nil) {
				node = node->right;
			}
			return node;
		}

		[[nodiscard]] static std::ptrdiff_t differenceHeights(NodePtr node) noexcept {
			return node->right->height - node->left->height;
		}

		NodePtr insertNode(const NodeID<NodePtr> loc, NodePtr new_node) noexcept {
			new_node->parent = loc.parent;
			++size;

			if (loc.parent == head) { // If inserted first value
				head->left		= new_node;
				head->parent	= new_node;
				head->right		= new_node;
				return new_node;
			}

			if (loc.child == NodeChild::left) {
				loc.parent->left = new_node;
				if (loc.parent == head->left) head->left = new_node;
			}
			else {
				loc.parent->right = new_node;
				if (loc.parent == head->right) head->right = new_node;
			}

			changeHeights(new_node->parent);
			balanceTree(new_node);
			return new_node;
		}

		void changeHeights(NodePtr node) noexcept {
			std::size_t max_height;
			while (!node->is_nil && (max_height = node->left->height > node->right->height ? node->left->height + 1 : node->right->height + 1) != node->height) {
				node->height = max_height;
				node = node->parent;
			}
		}

		[[nodiscard]] CheckBalanceResult<NodePtr> checkBalance(NodePtr node) const noexcept {
			CheckBalanceResult<NodePtr> result{ node, Rotate::none };
			while (!result.node->is_nil) {
				if (differenceHeights(result.node) == 2) {
					if (differenceHeights(result.node->right) >= 0) result.rotate = Rotate::smallLeft;
					else result.rotate = Rotate::bigLeft;
					return result;
				}
				if (differenceHeights(result.node) == -2) {
					if (differenceHeights(result.node->left) <= 0) result.rotate = Rotate::smallRight;
					else result.rotate = Rotate::bigRight;
					return result;
				}
				result.node = result.node->parent;
			}
			return result;
		}

		void balanceTree(NodePtr node) noexcept {
			CheckBalanceResult<NodePtr> result = checkBalance(node);

			if (result.rotate != Rotate::none) {
				switch (result.rotate) {
				case Rotate::smallLeft:
					leftRotate(result.node);
					break;

				case Rotate::bigLeft:
					rightRotate(result.node->right);
					leftRotate(result.node);
					break;

				case Rotate::smallRight:
					rightRotate(result.node);
					break;

				case Rotate::bigRight:
					leftRotate(result.node->left);
					rightRotate(result.node);
				}
				changeHeights(result.node->parent->parent);
			}
		}

		void extractNode(NodePtr erased_node) noexcept {
			NodePtr balance_node;

			if (erased_node->left->is_nil && erased_node->right->is_nil) { // if erasing node is a leaf
				balance_node = erased_node->parent;

				if (erased_node->parent->is_nil) {
					head->left		= head;
					head->parent	= head;
					head->right		= head;
				}
				else if (erased_node->parent->left == erased_node) {
					erased_node->parent->left = head;
					if (head->left == erased_node) head->left = erased_node->parent;
				}
				else {
					erased_node->parent->right = head;
					if (head->right == erased_node) head->right = erased_node->parent;
				}
			}
			else { // if erasing node is not a leaf
				NodePtr replace_node;

				if (differenceHeights(erased_node) >= 0) replace_node = minInSubTree(erased_node->right); 
				else replace_node = maxInSubTree(erased_node->left);

				if (erased_node->parent->is_nil) head->parent = replace_node;
				else if (erased_node->parent->left == erased_node) erased_node->parent->left = replace_node;
				else erased_node->parent->right = replace_node;

				if (replace_node->parent != erased_node) {
					if (replace_node->parent->left == replace_node) {
						replace_node->parent->left = replace_node->right;
						if (!replace_node->right->is_nil) replace_node->right->parent = replace_node->parent;
					}
					else {
						replace_node->parent->right = replace_node->left;
						if (!replace_node->left->is_nil) replace_node->left->parent = replace_node->parent;
					}

					balance_node			= replace_node->parent;
					replace_node->left		= erased_node->left;
					if (!replace_node->left->is_nil) replace_node->left->parent = replace_node;
					replace_node->right		= erased_node->right;
					if (!replace_node->right->is_nil) replace_node->right->parent = replace_node;
					replace_node->height	= erased_node->height;
				}
				else {
					if (replace_node->parent->left == replace_node) {
						replace_node->right = erased_node->right;
						if (!replace_node->right->is_nil) replace_node->right->parent = replace_node;
					}
					else {
						replace_node->left = erased_node->left;
						if (!replace_node->left->is_nil) replace_node->left->parent = replace_node;
					}

					balance_node = replace_node;
				}

				replace_node->parent = erased_node->parent;
				
				if (head->left == erased_node) head->left = replace_node;
				else if (head->right == erased_node) head->right = replace_node;
			}

			--size;
			changeHeights(balance_node);
			balanceTree(balance_node);
		}

		void orphanPtr(NodePtr node) noexcept {
			IteratorBase** orphan_it = &proxy->first;
			while (*orphan_it) {
				const NodePtr ptr = static_cast<uncheked_iterator*>(*orphan_it)->ptr;
				
				if (node == ptr) {
					(*orphan_it)->proxy = nullptr;
					*orphan_it = (*orphan_it)->next_iterator;
				}
				else orphan_it = &(*orphan_it)->next_iterator;
			}
		}

		template<class OtherTraits>
		void reparentPtr(NodePtr node, TreeValue<OtherTraits>& other) noexcept {
			IteratorBase** orphan_it = &other.proxy->first;
			while (*orphan_it) {
				const NodePtr ptr = static_cast<uncheked_iterator*>(*orphan_it)->ptr;
				
				if (node == ptr) {
					IteratorBase* next_iterator = (*orphan_it)->next_iterator;

					(*orphan_it)->proxy = proxy;
					(*orphan_it)->next_iterator = proxy->first;
					proxy->first = *orphan_it;

					*orphan_it = next_iterator;
				}
				else orphan_it = &(*orphan_it)->next_iterator;
			}
		}

		void leftRotate(NodePtr node) noexcept {
			NodePtr right_child = node->right;

			if (node->parent->is_nil) head->parent = right_child;
			else if (whichChild(node) == NodeChild::left) node->parent->left = right_child;
			else node->parent->right = right_child;

			node->right = right_child->left;
			if (!node->right->is_nil) node->right->parent = node;
			right_child->left = node;
			right_child->parent = node->parent;
			node->parent = right_child;

			node->height = node->right->height + 1;
			right_child->height = node->height + 1;
		}

		void rightRotate(NodePtr node) noexcept {
			NodePtr left_child = node->left;

			if (node->parent->is_nil) head->parent = left_child;
			else if (whichChild(node) == NodeChild::left) node->parent->left = left_child;
			else node->parent->right = left_child;

			node->left = left_child->right;
			if (!node->left->is_nil) node->left->parent = node;
			left_child->right = node;
			left_child->parent = node->parent;
			node->parent = left_child;

			node->height = node->left->height + 1;
			left_child->height = node->height + 1;
		}

		key_compare comp;
		Alloc alloc;
		NodePtr head;
		size_type size;
	};

	template<class Traits>
	class Tree {
	public:
		using allocator_type	= typename Traits::allocator_type;
		using key_type			= typename Traits::key_type;
		using value_type		= typename Traits::value_type;
		using key_compare		= typename Traits::key_compare;

	protected:
		using Node				= TreeNode<value_type, typename std::allocator_traits<allocator_type>::void_pointer>;
		using Alloc				= typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;
		using AllocTraits		= std::allocator_traits<Alloc>;
		using NodePtr			= typename AllocTraits::pointer;
		using TreeValue			= TreeValue<Traits>;
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

		Tree(const key_compare& comp, const allocator_type& alloc) : tree_value(comp, alloc) {
			createEmptyTree();
		}

		template<class AnyAllocator>
		Tree(const Tree& other, AnyAllocator&& alloc) : tree_value(other.tree_value.comp, std::forward<AnyAllocator>(alloc)) {
			createEmptyTree();
			copyOrMoveAllNodes(other, CopyTag{});
			tree_value.size = other.tree_value.size;
		}

		template<class AnyAllocator>
		Tree(Tree&& other, AnyAllocator&& alloc) : tree_value(other.tree_value.comp, std::forward<AnyAllocator>(alloc)) {
			if constexpr (!AllocTraits::is_always_equal::value) {
				if (alloc != other.tree_value.alloc) {
					createEmptyTree();
					copyOrMoveAllNodes(other, MoveTag{});
					tree_value.size = other.tree_value.size;
					other.clear();
					return;
				}
			}
			
			createEmptyTree();
			swapTreeValue(other);
		}

		Tree& operator=(const Tree& other) {
			if (this == &other) return *this;

			tree_value.comp = other.tree_value.comp;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (tree_value.alloc != other.tree_value.alloc) {
					if constexpr (AllocTraits::propagate_on_container_copy_assignment::value) {
						tidy();
						tree_value.alloc = other.tree_value.alloc;
						createEmptyTree();
						copyOrMoveAllNodes(other, CopyTag{});
						tree_value.size = other.tree_value.size;
						return *this;
					}	
				}
			}

			clear();
			copyOrMoveAllNodes(other, CopyTag{}); 
			tree_value.size = other.tree_value.size; 
			return *this;
		}

		Tree& operator=(Tree&& other) {
			if (this == &other) return *this;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (tree_value.alloc != other.tree_value.alloc) {
					tree_value.comp = other.tree_value.comp;
					tree_value.size = other.tree_value.size;

					if constexpr (AllocTraits::propagate_on_container_move_assignment::value) {
						tidy();
						tree_value.alloc = std::move(other.tree_value.alloc);
						createEmptyTree();
						copyOrMoveAllNodes(other, MoveTag{});
					}
					else {
						clear();
						copyOrMoveAllNodes(other, MoveTag{});
					}
					other.clear();
					return *this;
				}
			}

			clear();
			swapTreeValue(other);
			return *this;
		}

		void swap(Tree& other) {
			if (this == &other) return;
			if constexpr (!AllocTraits::propagate_on_container_swap::value) assert(!"propagate_on_container_swap = false");

			std::swap(tree_value.alloc, other.tree_value.alloc);
			swapTreeValue(other);
		}

		template<class OtherTraits>
		void merge(Tree<OtherTraits>& other) {
			static_assert(std::is_same_v<NodePtr, typename Tree<OtherTraits>::NodePtr>, "Different node types");
			assert(tree_value.alloc == other.tree_value.alloc && "Different allocator types");

			if (this == &other) return;

			uncheked_iterator it(&(other.tree_value), other.tree_value.head->left);
			while (!it.ptr->is_nil) {
				NodePtr node_for_insertion = it.ptr;
				++it;

				TreeFindResult<NodePtr> result = findPlaceForNode(Traits::getKeyFromValue(node_for_insertion->value));
				if (result.duplicate) continue;
				
				other.tree_value.extractNode(node_for_insertion);
				node_for_insertion->left	= tree_value.head;
				node_for_insertion->right	= tree_value.head;
				node_for_insertion->height	= 1;
				tree_value.insertNode(result.location, node_for_insertion);
				tree_value.reparentPtr(node_for_insertion, other.tree_value);
			}
		}

		template<class OtherTraits>
		void merge(Tree<OtherTraits>&& other) {
			static_assert(std::is_same_v<NodePtr, typename Tree<OtherTraits>::NodePtr>, "Different node types");
			assert(tree_value.alloc == other.tree_value.alloc && "Different allocator types");

			merge(other);
		}

		~Tree() {
			tidy();
		}

		template<class Tag>
		void copyOrMoveAllNodes(const Tree& other, Tag tag) {
			if (other.tree_value.head->parent->is_nil) return;

			NodePtr ptr = other.tree_value.head->parent;

			tree_value.head->parent			= copyOrMoveNode(ptr, tag);
			tree_value.head->parent->height = ptr->height;

			NodeID<NodePtr> location{ tree_value.head->parent, NodeChild::left };
			if (!ptr->left->is_nil) ptr = ptr->left;
			else if (!ptr->right->is_nil){
				ptr				= ptr->right;
				location.child	= NodeChild::right; 
			}
			
			while (!ptr->parent->is_nil) {
				if (location.child == NodeChild::left) {
					location.parent->left			= copyOrMoveNode(ptr, tag);
					location.parent->left->height	= ptr->height;
					location.parent->left->parent	= location.parent;
				}
				else {
					location.parent->right			= copyOrMoveNode(ptr, tag);
					location.parent->right->height	= ptr->height;
					location.parent->right->parent	= location.parent;
				}

				if (!ptr->left->is_nil) {
					if (location.child == NodeChild::left) location.parent = location.parent->left;
					else location.parent = location.parent->right;
					location.child	= NodeChild::left;
					ptr				= ptr->left;
				}
				else if (!ptr->right->is_nil) {
					if (location.child == NodeChild::left) location.parent = location.parent->left;
					else location.parent = location.parent->right;
					location.child	= NodeChild::right;
					ptr				= ptr->right;
				}
				else {
					while (ptr->parent->right == ptr || ptr->parent->right->is_nil) {
						location.parent = location.parent->parent;
						ptr				= ptr->parent;
					}
					if (!ptr->parent->is_nil) {
						location.child = NodeChild::right;
						ptr = ptr->parent->right;
					}
				}
			}

			tree_value.head->left	= tree_value.minInSubTree(tree_value.head->parent);
			tree_value.head->right	= tree_value.maxInSubTree(tree_value.head->parent);
		}

		[[nodiscard]] NodePtr copyOrMoveNode(NodePtr copy_node, MoveTag) {
			checkGrow();
			if constexpr (std::is_same_v<value_type, const key_type>) {
				return TreeTempNode(tree_value.alloc, tree_value.head, std::move(const_cast<key_type&>(copy_node->value))).release();
			}
			else return TreeTempNode(tree_value.alloc, tree_value.head, std::move(const_cast<std::pair<key_type, Traits::mapped_type>&>(copy_node->value))).release();
		}

		[[nodiscard]] NodePtr copyOrMoveNode(NodePtr copy_node, CopyTag) {
			checkGrow();
			return TreeTempNode(tree_value.alloc, tree_value.head, copy_node->value).release();
		}

		void tidy() {
			clear();
			tree_value.orphanAll();
			Node::freeHeadNode(tree_value.alloc, tree_value.head);
			tree_value.deleteProxy(static_cast<typename AllocTraits::template rebind_alloc<IteratorProxy>>(tree_value.alloc));
		}

		void clear() {
			NodePtr ptr = tree_value.head->parent;
			while (true) {
				if (!ptr->left->is_nil) ptr = ptr->left;
				else if (!ptr->right->is_nil) ptr = ptr->right;
				else break;
			}
		
			tree_value.head->left	= tree_value.head;
			tree_value.head->parent = tree_value.head;
			tree_value.head->right	= tree_value.head;
			tree_value.size			= 0;
			
			while (!ptr->is_nil) {
				NodePtr erasing_node = ptr;

				if (tree_value.whichChild(ptr) != NodeChild::left) {
					ptr = ptr->parent;
				}
				else if (!ptr->parent->right->is_nil) {
					ptr = ptr->parent->right;
					while (true) {
						if (!ptr->left->is_nil) ptr = ptr->left;
						else if (!ptr->right->is_nil) ptr = ptr->right;
						else break;
					}
				}
				else  ptr = ptr->parent;

				tree_value.orphanPtr(erasing_node);
				Node::freeNode(tree_value.alloc, erasing_node);
			}
		}

		void swapTreeValue(Tree& other) {
			std::swap(tree_value.head, other.tree_value.head);
			std::swap(tree_value.comp, other.tree_value.comp);
			std::swap(tree_value.size, other.tree_value.size);
			std::swap(tree_value.proxy, other.tree_value.proxy);

			other.tree_value.proxy->parent = &other.tree_value;
			tree_value.proxy->parent = &tree_value;
		}

		void createEmptyTree() {
			tree_value.head = Node::createHeadNode(tree_value.alloc);
			tree_value.createProxy(static_cast<typename AllocTraits::template rebind_alloc<IteratorProxy>>(tree_value.alloc));
		}

		std::pair<iterator, bool> insert(const value_type& value) {
			std::pair<NodePtr, bool> result = emplace_(value);
			return { iterator(&tree_value, result.first), result.second };
		}

		std::pair<iterator, bool> insert(value_type&& value) {
			std::pair<NodePtr, bool> result = emplace_(std::move(value));
			return { iterator(&tree_value, result.first), result.second };
		}

		template<class InputIt>
		void insert(InputIt first, InputIt last) {
			while (first != last) {
				emplaceHint(tree_value.head, *first);
				++first;
			}
		}

		void insert(std::initializer_list<value_type> init) {
			insert(init.begin(), init.end());
		}

		template<class... Args>
		std::pair<iterator, bool> emplace(Args&&... args) {
			std::pair<NodePtr, bool> result = emplace_(std::forward<Args>(args)...);
			return { iterator(&tree_value, result.first), result.second };
		}

		template<class... Args>
		std::pair<NodePtr, bool> emplace_(Args&&... args) {
			using KeyExtractor = typename Traits::template KeyExtractor<std::decay_t<Args>...>;
			NodePtr node_for_insertion;
			TreeFindResult<NodePtr> result;
			if constexpr (KeyExtractor::extractable) {
				const auto& key = KeyExtractor::extract(std::forward<Args>(args)...);
				result = findPlaceForNode(key);
				if (result.duplicate) return { result.location.parent, false };
				checkGrow();
				node_for_insertion = TreeTempNode(tree_value.alloc, tree_value.head, std::forward<Args>(args)...).release();
			}
			else {
				checkGrow();
				TreeTempNode tmp_node(tree_value.alloc, tree_value.head, std::forward<Args>(args)...);
				result = findPlaceForNode(Traits::getKeyFromValue(tmp_node.ptr->value));
				if (result.duplicate) return { result.location.parent, false };
				node_for_insertion = tmp_node.release();
			}

			return { tree_value.insertNode(result.location, node_for_insertion), true };
		}

		template<class... Args>
		iterator emplaceHint(const_iterator hint, Args&&... args) {
			assert(hint.getContainer() == &tree_value && "Iterator from another container");
			return iterator(&tree_value, emplaceHint(hint.ptr, std::forward<Args>(args)...));
		}

		template<class... Args>
		NodePtr emplaceHint(NodePtr hint, Args&&... args) {
			using KeyExtractor = typename Traits::template KeyExtractor<std::decay_t<Args>...>;
			NodePtr node_for_insertion;
			TreeFindResult<NodePtr> result;

			if constexpr (KeyExtractor::extractable) {
				const auto& key = KeyExtractor::extract(std::forward<Args>(args)...);
				result = findPlaceForNodeWithHint(hint, key);
				if (result.duplicate) return result.location.parent;
				checkGrow();
				node_for_insertion = TreeTempNode(tree_value.alloc, tree_value.head, std::forward<Args>(args)...).release();
			}
			else {
				checkGrow();
				TreeTempNode tmp_node(tree_value.alloc, tree_value.head, std::forward<Args>(args)...);
				result = findPlaceForNodeWithHint(hint, Traits::getKeyFromValue(tmp_node.ptr->value));
				if (result.duplicate) return result.location.parent;
				node_for_insertion = tmp_node.release();
			}

			return tree_value.insertNode(result.location, node_for_insertion);
		}

		size_type count(const key_type& key) const noexcept {
			TreeFindResult<NodePtr> result = findPlaceForNode(key);
			return result.duplicate ? size_type{ 1 } : size_type{ 0 };
		}

		iterator find(const key_type& key) noexcept {
			TreeFindResult<NodePtr> result = findPlaceForNode(key);
			return result.duplicate ? iterator{ &tree_value, result.location.parent } : end();
		}

		const_iterator find(const key_type& key) const noexcept {
			TreeFindResult<NodePtr> result = findPlaceForNode(key);
			return result.duplicate ? const_iterator{ &tree_value, result.location.parent } : cend();
		}

		bool contains(const key_type& key) const noexcept {
			TreeFindResult<NodePtr> result = findPlaceForNode(key);
			return result.duplicate ? true : false;
		}

		[[nodiscard]] TreeFindResult<NodePtr> findPlaceForNode(const key_type& key) const noexcept {
			TreeFindResult<NodePtr> result{ {tree_value.head->parent, NodeChild::right}, false };
			NodePtr try_node = tree_value.head->parent;
			while (!try_node->is_nil) {
				result.location.parent = try_node;
				if (tree_value.comp(Traits::getKeyFromValue(try_node->value), key)) {
					result.location.child = NodeChild::right;
					try_node = try_node->right;
				}
				else if (tree_value.comp(key, Traits::getKeyFromValue(try_node->value))) {
					result.location.child = NodeChild::left;
					try_node = try_node->left;
				}
				else {
					result.duplicate = true;
					return result;
				}
			}
			return result;
		}

		[[nodiscard]] TreeFindResult<NodePtr> findPlaceForNodeWithHint(NodePtr hint, const key_type& key) noexcept {
			if (hint == tree_value.head) {
				if (hint->right->is_nil || tree_value.comp(Traits::getKeyFromValue(tree_value.head->right->value), key)) {
					return { {tree_value.head->right, NodeChild::right}, false };
				}
			}
			else if (hint == tree_value.head->left) {
				if (tree_value.comp(key, Traits::getKeyFromValue(tree_value.head->left->value))) {
					return { {tree_value.head->left, NodeChild::left}, false };
				}
			}
			else if (tree_value.comp(key, Traits::getKeyFromValue(hint->value))) {
				NodePtr prev = (--(uncheked_iterator(&tree_value, hint))).ptr;
				if (tree_value.comp(Traits::getKeyFromValue(prev->value), key)) {
					if (prev->right->is_nil) return { {prev, NodeChild::right}, false };
					else return { {hint, NodeChild::left}, false };
				}
			}
			else if (tree_value.comp(Traits::getKeyFromValue(hint->value), key)) {
				NodePtr next = (++(uncheked_iterator(&tree_value, hint))).ptr;
				if (tree_value.comp(key, Traits::getKeyFromValue(next->value))) {
					if (hint->right->is_nil) return { {hint, NodeChild::right}, false };
					else return { {next, NodeChild::left}, false };
				}
			}
			else return { {hint, NodeChild::right}, true };

			return findPlaceForNode(key);
		}

		iterator erase(const_iterator iter) {
			assert(iter.getContainer() == &tree_value && "Iterator from another container");
			assert(!iter.ptr->is_nil && "Cannot erase the end");
			return iterator(&tree_value, eraseUnwrapped(iter.unwrapIterator()));
		}

		iterator erase(iterator iter) {
			assert(iter.getContainer() == &tree_value && "Iterator from another container");
			assert(!iter.ptr->is_nil && "Cannot erase the end");
			return iterator(&tree_value, eraseUnwrapped(iter.unwrapIterator()));
		}

		iterator erase(iterator first, iterator last) {
			assert(first.getContainer() == &tree_value && last.getContainer() == &tree_value && "Iterator from another container");
			return iterator(&tree_value, eraseUnwrapped(first.unwrapIterator(), last.unwrapIterator()));
		}

		iterator erase(const_iterator first, const_iterator last) {
			assert(first.getContainer() == &tree_value && last.getContainer() == &tree_value && "Iterator from another container");
			return iterator(&tree_value, eraseUnwrapped(first.unwrapIterator(), last.unwrapIterator()));
		}

		size_type erase(const key_type& key) {
			TreeFindResult<NodePtr> result = findPlaceForNode(key);
			if (result.duplicate) {
				tree_value.orphanPtr(result.location.parent);
				tree_value.extractNode(result.location.parent);
				Node::freeNode(tree_value.alloc, result.location.parent);
				return static_cast<size_type>(1);
			}
			else return static_cast<size_type>(0);
		}

		NodePtr eraseUnwrapped(uncheked_iterator iter) {
			NodePtr erased_node = (iter++).ptr;
			tree_value.orphanPtr(erased_node);
			tree_value.extractNode(erased_node);
			Node::freeNode(tree_value.alloc, erased_node);
			return iter.ptr;
		}

		NodePtr eraseUnwrapped(uncheked_iterator first, uncheked_iterator last) {
			if (first.ptr == tree_value.minInSubTree(tree_value.head->parent) && last.ptr->is_nil) {
				clear();
				return last.ptr;
			}

			while (first != last) {
				eraseUnwrapped(first++);
			}
			return last.ptr;
		}

		void checkGrow() const noexcept {
			assert(maxSize() != tree_value.size && "The lack of memory error");
		}

		[[nodiscard]] size_type size() {
			return tree_value.size;
		}

		[[nodiscard]] bool empty() {
			return tree_value.size == size_type{ 0 };
		}

		[[nodiscard]] size_type maxSize() const noexcept {
			return std::min(static_cast<size_type>(std::numeric_limits<difference_type>::max()), AllocTraits::max_size(tree_value.alloc));
		}

		[[nodiscard]] iterator begin() noexcept {
			return iterator(&tree_value, tree_value.head->left);
		}

		[[nodiscard]] const_iterator begin() const noexcept {
			return const_iterator(&tree_value, tree_value.head->left);
		}

		[[nodiscard]] const_iterator cbegin() const noexcept {
			return begin();
		}

		[[nodiscard]] iterator end() noexcept {
			return iterator(&tree_value, tree_value.head);
		}

		[[nodiscard]] const_iterator end() const noexcept {
			return const_iterator(&tree_value, tree_value.head);
		}

		[[nodiscard]] const_iterator cend() const noexcept {
			return end();
		}
	
		TreeValue tree_value;
	};

	template<class TreeValue>
	class TreeUncheckedIterator : public IteratorBase {
	public:
		using NodePtr		= typename TreeValue::NodePtr;
		using value_type	= typename TreeValue::value_type;
		using diffence_type = typename TreeValue::difference_type;
		using reference		= value_type&;
		using pointer		= value_type*;

		TreeUncheckedIterator() : ptr{} {}

		TreeUncheckedIterator(const TreeValue* container, NodePtr ptr) :  ptr{ ptr } {
			this->adopt(container);
		}

		[[nodiscard]] reference operator*() noexcept {
			return ptr->value;
		}

		[[nodiscard]] pointer operator->() noexcept {
			return std::addressof(ptr->value);
		}

		TreeUncheckedIterator& operator++() noexcept {
			if (ptr->is_nil) ptr = ptr->left;
			else if (ptr->right->is_nil) {
				while (!ptr->parent->is_nil && ptr->parent->right == ptr) {
					ptr = ptr->parent;
				}
				ptr = ptr->parent;
			}
			else ptr = TreeValue::minInSubTree(ptr->right);

			return *this;
		}

		TreeUncheckedIterator operator++(int) noexcept {
			TreeUncheckedIterator tmp = *this;
			++*this;
			return tmp;
		}

		TreeUncheckedIterator& operator--() noexcept {
			if (ptr->is_nil) ptr = ptr->right;
			else if (ptr->left->is_nil) {
				while (!ptr->parent->is_nil && ptr->parent->left == ptr) { 
					ptr = ptr->parent;
				}
				ptr = ptr->parent;
			}
			else ptr = TreeValue::maxInSubTree(ptr->left);

			return *this;
		}

		TreeUncheckedIterator operator--(int) noexcept {
			TreeUncheckedIterator tmp = *this;
			--*this;
			return tmp;
		}

		[[nodiscard]] bool operator==(const TreeUncheckedIterator& rhs) {
			return ptr == rhs.ptr;
		}

		[[nodiscard]] bool operator!=(const TreeUncheckedIterator& rhs) {
			return !(*this == rhs);
		}

		NodePtr ptr;
	};

	template<class TreeValue>
	class TreeConstIterator : public TreeUncheckedIterator<TreeValue> {
	public:
		using Base			= TreeUncheckedIterator<TreeValue>;
		using NodePtr		= typename TreeValue::NodePtr;
		using value_type	= typename TreeValue::value_type;
		using diffence_type	= typename TreeValue::difference_type;
		using reference		= const value_type&;
		using pointer		= const value_type*;

		using Base::Base;

		Base unwrapIterator() {
			return Base(static_cast<const TreeValue*>(this->getContainer()), this->ptr);
		}

		[[nodiscard]] reference operator*() const noexcept {
			assert(this->getContainer() && "Invalid iterator error");
			assert(!this->ptr->is_nil && "The try of dereferencing end");
			return this->ptr->value;
		}

		[[nodiscard]] pointer operator->() const noexcept {
			assert(this->getContainer() && "Invalid iterator error");
			assert(!this->ptr->is_nil && "The try of dereferencing end");
			return std::addressof(this->ptr->value);
		}

		TreeConstIterator& operator++() noexcept {
			assert(this->getContainer() && "Invalid iterator error");
			assert(!this->ptr->is_nil && "Incrementing the end error");
			Base::operator++();
			return *this;
		}

		TreeConstIterator operator++(int) noexcept {
			TreeIterator tmp = *this;
			++*this;
			return tmp;
		}

		TreeConstIterator& operator--() noexcept {
			assert(this->getContainer() && "Invalid iterator error");
			assert(this->ptr != static_cast<TreeValue*>(this->getContainer())->head->left && "Decrementing the begin error");
			Base::operator--();
			return *this;
		}

		TreeConstIterator operator--(int) noexcept {
			TreeIterator tmp = *this;
			--*this;
			return tmp;
		}

		[[nodiscard]] bool operator==(const TreeConstIterator& rhs) const noexcept{
			return (this->getContainer() && rhs.getContainer()) ? this->ptr == rhs.ptr : false;
			
		}

		[[nodiscard]] bool operator!=(const TreeConstIterator& rhs) const noexcept {
			return !(*this == rhs);
		}
	};

	template<class TreeValue>
	class TreeIterator : public TreeConstIterator<TreeValue> {
	public:
		using Base			= TreeConstIterator<TreeValue>;
		using NodePtr		= typename TreeValue::NodePtr;
		using value_type	= typename TreeValue::value_type;
		using diffence_type = typename TreeValue::difference_type;
		using reference		= value_type&;
		using pointer		= value_type*;

		using Base::Base;

		typename Base::Base unwrapIterator() {
			return typename Base::Base(static_cast<const TreeValue*>(this->getContainer()), this->ptr);
		}

		[[nodiscard]] reference operator*() const noexcept {
			return const_cast<reference>(Base::operator*());
		}

		[[nodiscard]] pointer operator->() const noexcept {
			return const_cast<pointer>(Base::operator->());
		}

		TreeIterator& operator++() noexcept {
			Base::operator++();
			return *this;
		}

		TreeIterator operator++(int) noexcept {
			TreeIterator tmp = *this;
			++* this;
			return tmp;
		}

		TreeIterator& operator--() noexcept {
			Base::operator--();
			return *this;
		}

		TreeIterator operator--(int) noexcept {
			TreeIterator tmp = *this;
			--* this;
			return tmp;
		}
	};	
}