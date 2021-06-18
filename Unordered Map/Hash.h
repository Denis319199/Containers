#pragma once

#include "List.h"

namespace mylib {

	template<class Key, class... Args>
	struct KeyExtractorUnorderedSet {
		static const bool extractable = false;
	};

	template<class Key, class First>
	struct KeyExtractorUnorderedSet<Key, First> {
		static const bool extractable = std::is_same_v<Key, std::decay_t<First>>;

		static const Key& extract(const First& value) {
			return value;
		}
	};

	template<class Key, class Hasher, class KeyEqual, class Allocator>
	struct UnorderedSetTraits {
		using key_type = Key;
		using value_type = const Key;
		using hasher = Hasher;
		using key_equal = KeyEqual;
		using allocator_type = Allocator;

		template<class... Args>
		using KeyExtractor = KeyExtractorUnorderedSet<Key, Args...>;

		static const Key& getKeyFromValue(const value_type& value) {
			return value;
		}
	};

	template<class Alloc>
	struct TmpHashVector {
		using VecPtr	= typename std::allocator_traits<Alloc>::pointer;
		using size_type = typename std::allocator_traits<Alloc>::size_type;

		TmpHashVector(Alloc& alloc, size_type size) : alloc_(alloc), ptr_(alloc.allocate(size)), size_(size) {}

		TmpHashVector(const TmpHashVector&) = delete;
		TmpHashVector& operator=(const TmpHashVector&) = delete;

		VecPtr release() {
			return std::exchange(ptr_, nullptr);
		}

		~TmpHashVector() {
			if (ptr_) alloc_.deallocate(ptr_, size_);
		}

		Alloc& alloc_;
		VecPtr ptr_;
		size_type size_;
	};

	template<class T, class Allocator>
	class HashVector {
	public:
		using Alloc				= typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
		using AllocTraits		= std::allocator_traits<Alloc>;
		using VecPtr			= typename AllocTraits::pointer;
		using size_type			= typename AllocTraits::size_type;
		using difference_type	= typename AllocTraits::difference_type;
		using NodePtr			= typename T::NodePtr;

		template<class AnyAlloc>
		HashVector(AnyAlloc&& alloc) : alloc_{ std::forward<AnyAlloc>(alloc) }, ptr_{}, size_{ 1 } {}

		~HashVector() {
			tidy();
		}

		HashVector(const HashVector&) = delete;
		HashVector& operator=(const HashVector&) = delete;

		void resize(size_type new_size, NodePtr head) {
			tidy();

			checkGrow(new_size);
			size_ = new_size;
			TmpHashVector tmp_ptr(alloc_, size_);
			ptr_ = unfancy(tmp_ptr.release());
				
			for (size_type i = 0; i < size_; ++i) {
				construct(alloc_, ptr_ + i, head, head);
			}
		}

		void checkGrow(size_type size) const noexcept {
			assert(maxSize() >= size && "The lack of memory");
		}

		[[nodiscard]] size_type maxSize() const noexcept {
			return std::min(static_cast<size_type>(std::numeric_limits<difference_type>::max()), AllocTraits::max_size(alloc_));
		}

		void tidy() {
			if (ptr_) {
				for (size_type i = 0; i < size_; ++i) {
					destroy(alloc_, ptr_ + i);
				}
				alloc_.deallocate(ptr_, size_);
			}
		}

		Alloc alloc_;
		T* ptr_;
		size_type size_; // The quantity of buckets must be power of 2
	};

	template<class Ptr>
	struct VectorValue {
		using NodePtr = Ptr;

		VectorValue(NodePtr first, NodePtr last) : first_{ first }, last_{ last } {}

		NodePtr first_;
		NodePtr last_;
	};

	template<class NodePtr>
	struct FindResult {
		NodePtr duplicate;
		VectorValue<NodePtr>* bucket;
	};

	template<class Traits>
	class Hash {
	public:
		using key_type			= typename Traits::key_type;
		using value_type		= typename Traits::value_type;
		using hasher			= typename Traits::hasher;
		using key_equal			= typename Traits::key_equal;
		using allocator_type	= typename Traits::allocator_type;
		using AllocTraits		= std::allocator_traits<allocator_type>;
		using size_type			= typename AllocTraits::size_type;
		using List				= List<value_type, allocator_type>;
		using NodePtr			= typename List::NodePtr;
		using const_iterator	= typename List::const_iterator;
		using iterator			= typename List::iterator;
		using VectorValue		= VectorValue<NodePtr>;
		
		Hash(size_type bucket_count, hasher hash, key_equal equal, const allocator_type& alloc) 
				: list_{ alloc }, vector_{ alloc }, max_load_factor_{ 1.0f }, hash_{ hash }, equal_{ equal }  {
			vector_.resize(getRequiredBucketsAmount(bucket_count), list_.list_value.head);
		}

		template<class InputIt>
		Hash(InputIt first, InputIt last, size_type bucket_count, hasher hash, key_equal equal, const allocator_type& alloc)
				: list_{ alloc }, vector_{ alloc }, max_load_factor_{ 1.0f }, hash_{ hash }, equal_{ equal } {
			vector_.resize(getRequiredBucketsAmount(bucket_count), list_.list_value.head);
			insert(first, last);
		}

		template<class AnyAlloc>
		Hash(const Hash& other, AnyAlloc&& alloc) : list_{ std::forward<AnyAlloc>(alloc) }, vector_{ std::forward<AnyAlloc>(alloc) },
												    max_load_factor_{ other.max_load_factor_ }, hash_{ other.hash_ }, equal_{ other.equal_ } {
			const NodePtr list_head = list_.list_value.head;
			list_.insertRange(list_head, other.begin(), other.end());
			vector_.resize(other.bucketCount(), list_head);
			rehashHashVector();
		}

		template<class AnyAlloc>
		Hash(Hash&& other, AnyAlloc&& alloc) : list_{ std::forward<AnyAlloc>(alloc) }, vector_{ std::forward<AnyAlloc>(alloc) },
											   max_load_factor_{ other.max_load_factor_ }, hash_{ other.hash_ }, equal_{ other.equal_ } {
			const NodePtr list_head = list_.list_value.head;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (vector_.alloc_ != other.vector_.alloc_) {
					list_.insertRange(list_head, other.begin(), other.end(), MoveTag{});
					vector_.resize(other.bucketCount(), list_head);
					rehashHashVector();
					other.clear();
				}
			}

			vector_.resize(min_buckets_, list_head);
			swapValue(other);
		}

		Hash& operator=(const Hash& other) {
			max_load_factor_ = other.max_load_factor_;
			hash_ = other.hash_;
			equal_ = other.equal_;

			if constexpr (!AllocTraits::is_always_equal::value) {
				if (vector_.alloc_ != other.vector_.alloc_) {
					if constexpr (AllocTraits::propagate_on_container_copy_assignment::value) {
						tidy();
						list_.list_value.alloc = other.list_.list_value.alloc;
						vector_.alloc_ = other.vector_.alloc_;
						list_.createEmptyList();
						const NodePtr list_head = list_.list_value.head;
						list_.insertRange(list_head, other.begin(), other.end());
						vector_.resize(other.bucketCount(), list_head);
						rehashHashVector();
						return *this;
					}
				}
			}

			list_.copyOrMoveList(other.list_, CopyTag{});
			const size_type other_bucket_count = other.bucketCount();
			if (bucketCount() != other_bucket_count) vector_.resize(other_bucket_count, list_.list_value.head);
			rehashHashVector();
			return *this;
		}

		Hash& operator=(Hash&& other) {
			if constexpr (!AllocTraits::is_always_equal::value) {
				if (vector_.alloc_ != other.vector_.alloc_) {
					max_load_factor_ = other.max_load_factor_;
					hash_ = other.hash_;
					equal_ = other.equal_;

					if constexpr (AllocTraits::propagate_on_container_move_assignment::value) {
						tidy();
						list_.list_value.alloc = std::move(other.list_.list_value.alloc);
						vector_.alloc_ = std::move(other.vector_.alloc_);
						list_.createEmptyList();
						const NodePtr list_head = list_.list_value.head;
						list_.insertRange(list_head, other.begin(), other.end(), MoveTag{});
						vector_.resize(other.bucketCount(), list_head);
					}
					else {
						list_.copyOrMoveList(other.list_, MoveTag{});
						const size_type other_bucket_count = other.bucketCount();
						if (bucketCount() != other_bucket_count) vector_.resize(other_bucket_count, list_.list_value.head);
					}

					rehashHashVector();
					other.clear();
					return *this;
				}
			}

			clear();
			swapValue(other);
			return *this;
		}

		void tidy() {
			list_.tidy();
			vector_.tidy();
		}

		void clear() {
			const NodePtr list_head = list_.list_value.head;
			list_.clear();
			vector_.resize(min_buckets_, list_head);
		}

		[[nodiscard]] FindResult<NodePtr> findPlace(const key_type& key) const noexcept {
			VectorValue* bucket = getBucket(key);

			FindResult<NodePtr> result{ nullptr, bucket };
			NodePtr ptr = bucket->first_;
			NodePtr last = bucket->last_->next;

			if (ptr == list_.list_value.head) return result;	

			while (ptr != last) {
				if (equal_(Traits::getKeyFromValue(ptr->value), key)) {
					result.duplicate = ptr;
					return result;
				}
				ptr = ptr->next;
			}
			return result;
		}

		[[nodiscard]] VectorValue* getBucket(const key_type& key) const noexcept {
			size_type vector_index = static_cast<size_type>(hash_(key)) % bucketCount();
			return vector_.ptr_ + vector_index;
		}

		std::pair<iterator, bool> insert(const value_type& value) {
			return emplace(value_type);
		}

		std::pair<iterator, bool> insert(value_type&& value) {
			return emplace(std::move(value_type));
		}

		template<class InputIt>
		void insert(InputIt first, InputIt last) {
			while (first != last) {
				emplace(*first);
				++first;
			}
		}

		void insert(std::initializer_list<value_type> ilist) {
			insert(ilist.begin(), ilist.end());
		}

		template<class... Args>
		std::pair<iterator, bool> emplace(Args&&... args) {
			using KeyExtractor = typename Traits::template KeyExtractor<std::decay_t<Args>...>;

			FindResult<NodePtr> result;
			NodePtr new_node;
			ListTmpNodes tmp_node(list_.list_value.alloc);

			if constexpr (KeyExtractor::extractable) {
				result = findPlace(KeyExtractor::extract(std::forward<Args>(args)...));
				if (result.duplicate) return { { &list_.list_value, result.duplicate }, false };
				tmp_node.createNode(std::forward<Args>(args)...);
			}
			else {
				tmp_node.createNode(std::forward<Args>(args)...);
				result = findPlace(Traits::getKeyFromValue(tmp_node.first->value));
				if (result.duplicate) return { { &list_.list_value, result.duplicate }, false };
			}

			const size_type size = ++list_.list_value.size;
			const NodePtr list_head = list_.list_value.head;

			if (checkRehash()) {
				vector_.resize(getRequiredBucketsAmount(size), list_head);
				rehashHashVector();
				result = findPlace(Traits::getKeyFromValue(tmp_node.first->value));
			}
			new_node = tmp_node.insertNodes(result.bucket->first_);

			if (result.bucket->last_ == list_head) result.bucket->last_ = new_node;
			result.bucket->first_ = new_node;

			return { { &list_.list_value, new_node}, true };
		}

		iterator erase(const_iterator pos) {
			return { &list_.list_value, eraseNode(pos.ptr) };
		}

		NodePtr eraseNode(NodePtr ptr) {
			VectorValue* bucket = getBucket(Traits::getKeyFromValue(ptr->value));
			if (bucket->first_ == ptr) {
				if (bucket->last_ == ptr) {
					const NodePtr list_head = list_.list_value.head;
					bucket->first_ = list_head;
					bucket->last_ = list_head;
				}
				else bucket->first_ = ptr->next;
			}
			else if (bucket->last_ == ptr) bucket->last_ = ptr->prev;

			return list_.eraseNode(ptr);
		}

		iterator erase(const_iterator first, const_iterator last) {
			return { &list_.list_value, eraseRange(first.ptr, last.ptr) };
		}

		NodePtr eraseRange(NodePtr first, NodePtr last) {
			VectorValue* last_bucket = getBucket(Traits::getKeyFromValue(last->value));
			VectorValue* bucket = getBucket(Traits::getKeyFromValue(first->value));
			const NodePtr list_head = list_.list_value.head;

			NodePtr next_ptr = bucket->last_->next;

			if (bucket->first_ == first) {
				bucket->first_ = list_head;
				bucket->last_ = list_head;
			}
			else {
				bucket->last_ = first->prev;
			}
			bucket = getBucket(Traits::getKeyFromValue(next_ptr->value));

			while (last_bucket->first_ != bucket->first_) {
				next_ptr = bucket->last_->next;
				bucket->first_ = list_head;
				bucket->last_ = list_head;
				bucket = getBucket(Traits::getKeyFromValue(next_ptr->value));
			}

			last_bucket->first_ = last;
				
			return list_.eraseRange(first, last);
		}

		size_type erase(const key_type& key) {
			FindResult<NodePtr> result = findPlace(key);
			if (result.duplicate) {
				eraseNode(result.duplicate);
				return 1;
			}
			return 0;
		}

		[[nodiscard]] iterator find(const key_type& key) noexcept {
			FindResult<NodePtr> result = findPlace(key);
			return result.duplicate ? iterator{ &list_.list_value, result.duplicate } : end();
		}

		[[nodiscard]] const_iterator find(const key_type& key) const noexcept {
			FindResult<NodePtr> result = findPlace(key);
			return result.duplicate ? const_iterator{ &list_.list_value, result.duplicate } : cend();
		}

		[[nodiscard]] bool contains(const key_type& key) const noexcept {
			FindResult<NodePtr> result = findPlace(key);
			return result.duplicate ? true : false;
		}

		void swap(Hash& other) {
			if (&other != this) {
				if constexpr (!AllocTraits::is_always_equal::value) {
					if constexpr (!AllocTraits::propagate_on_container_swap::value) assert(!"propagate_on_container_swap = false");
					std::swap(list_.list_value.alloc, other.list_.list_value.alloc);
					std::swap(vector_.alloc_, other.vector_.alloc_);
				}
				swapValue(other);
			}
		}

		void swapValue(Hash& other) {
			list_.swapValue(other.list_);

			std::swap(vector_.ptr_, other.vector_.ptr_);
			std::swap(vector_.size_, other.vector_.size_);

			std::swap(max_load_factor_, other.max_load_factor_);
			std::swap(hash_, other.hash_);
			std::swap(equal_, other.equal_);
		}

		[[nodiscard]] size_type getRequiredBucketsAmount(size_type for_size) const noexcept {
			const size_type req_buckets = std::max(static_cast<size_type>(static_cast<float>(for_size) / max_load_factor_), min_buckets_);
			size_type cur_buckets = bucketCount();

			if (cur_buckets >= req_buckets) return cur_buckets;
			else if (cur_buckets < 512 && cur_buckets << 3 >= req_buckets) return cur_buckets << 3;
			else {
				while (req_buckets >= cur_buckets) {
					cur_buckets <<= 1;
				}
			}
			return cur_buckets;
		}

		void rehash(size_type buckets) {
			const size_type req_buckets = getRequiredBucketsAmount(buckets);

			if (bucketCount() != req_buckets) {
				vector_.resize(req_buckets, list_.list_value.head);
				rehashHashVector();
			}
		}

		void rehashHashVector() {
			const NodePtr list_head = list_.list_value.head;
			NodePtr ptr = list_head->next;

			while (ptr != list_head) {
				VectorValue* bucket = getBucket(Traits::getKeyFromValue(ptr->value));

				if (bucket->first_ == list_head) {
					bucket->first_ = ptr;
					bucket->last_ = ptr;

					ptr = ptr->next;
				}
				else {
					NodePtr next_ptr = ptr->next;
					list_.list_value.extractNode(ptr);

					NodePtr old_first = bucket->first_;
					ptr->next = old_first;
					ptr->prev = old_first->prev;
					old_first->prev->next = ptr;
					old_first->prev = ptr;

					bucket->first_ = ptr;

					ptr = next_ptr;
				}
			}
		}

		[[nodiscard]] bool checkRehash() const noexcept {
			return max_load_factor_ < static_cast<float>(size()) / static_cast<float>(bucketCount());
		}

		[[nodiscard]] size_type bucketCount() const noexcept {
			return vector_.size_;
		}

		[[nodiscard]] size_type size() const noexcept {
			return list_.list_value.size;
		}

		[[nodiscard]] bool empty() const noexcept {
			return size() == 0;
		}

		allocator_type getAllocator() const noexcept {
			return static_cast<allocator_type>(vector_.alloc_);
		}

		void maxLoadFactor(float new_max_load_factor) noexcept {
			assert(new_max_load_factor > 0 && "Max load factor less than 0");
			max_load_factor_ = new_max_load_factor;
		}

		[[nodiscard]] float maxLoadFactor() const noexcept {
			return max_load_factor_;
		}

		[[nodiscard]] iterator begin() noexcept {
			return list_.begin();
		}

		[[nodiscard]] const_iterator begin() const noexcept {
			return list_.cbegin();
		}

		[[nodiscard]] const_iterator cbegin() const noexcept {
			return list_.cbegin();
		}

		[[nodiscard]] iterator end() noexcept {
			return list_.end();
		}

		[[nodiscard]] const_iterator end() const noexcept {
			return list_.cend();
		}

		[[nodiscard]] const_iterator cend() const noexcept {
			return list_.cend();
		}

		List list_;
		HashVector<VectorValue, allocator_type> vector_;
		float max_load_factor_;
		hasher hash_;
		key_equal equal_;
		static const size_type min_buckets_ = 8; // A minimal size of buckets must be power of 2 
	};
}