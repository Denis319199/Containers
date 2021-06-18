#pragma once

#include "Hash.h"

namespace mylib {

	template<class Key, class... Args>
	struct KeyExtractorUnorderedMap {
		static const bool extractable = false;
	};

	template<class Key, class Second>
	struct KeyExtractorUnorderedMap<Key, Key, Second> {
		static const bool extractable = true;

		static const Key& extract(const Key& key, const Second& second) {
			return key;
		}
	};

	template<class Key, class First, class Second>
	struct KeyExtractorUnorderedMap<Key, std::pair<First, Second>> {
		static const bool extractable = std::is_same_v<Key, typename std::decay_t<First>>;

		static const Key& extract(const std::pair<First, Second>& value) {
			return value.first;
		}
	};

	template<class Key, class T, class Hasher, class KeyEqual, class Allocator>
	struct UnorderedMapTraits {
		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<const Key, T>;
		using hasher = Hasher;
		using key_equal = KeyEqual;
		using allocator_type = Allocator;


		template<class... Args>
		using KeyExtractor = KeyExtractorUnorderedMap<Key, Args...>;

		static const Key& getKeyFromValue(const value_type& value) {
			return value.first;
		}
	};

	template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = std::allocator<std::pair<const Key, T>>> 
	class UnorderedMap : public mylib::Hash<UnorderedMapTraits<Key, T, Hash, KeyEqual, Allocator>> {
	public:
		using Base				= typename mylib::Hash<UnorderedMapTraits<Key, T, Hash, KeyEqual, Allocator>>;
		using key_type			= typename Base::key_type;
		using value_type		= typename Base::value_type;
		using hasher			= typename Base::hasher;
		using key_equal			= typename Base::key_equal;
		using allocator_type	= typename Base::allocator_type;
		using AllocTraits		= std::allocator_traits<allocator_type>;
		using size_type			= typename AllocTraits::size_type;
		using List				= List<value_type, allocator_type>;
		using const_iterator	= typename List::const_iterator;
		using iterator			= typename List::iterator;
	

		UnorderedMap() : Base(this->min_buckets_, Hash{}, key_equal{}, Allocator{}) {}

		explicit UnorderedMap(size_type bucket_count, Hash hash = Hash{}, key_equal equal = key_equal{}, const Allocator& allocator = Allocator{})
			: Base(bucket_count, hash, equal, allocator) {}

		UnorderedMap(size_type bucket_count, const Allocator& allocator) : Base(bucket_count, Hash{}, key_equal{}, allocator) {}

		UnorderedMap(size_type bucket_count, Hash hash, const Allocator& allocator) : Base(bucket_count, hash, key_equal{}, allocator) {}

		explicit UnorderedMap(const Allocator& alloc) : Base(this->min_buckets_, Hash{}, key_equal{}, alloc) {}

		template<class InputIt>
		UnorderedMap(InputIt first, InputIt last, size_type bucket_count = this->min_buckets_, Hash hash = Hash{}, key_equal equal = key_equal{},
					 const Allocator& alloc = Allocator{}) : Base(first, last, bucket_count, hash, equal, alloc) {}

		template<class InputIt>
		UnorderedMap(InputIt first, InputIt last, size_type bucket_count, const Allocator& alloc) 
			: Base(first, last, bucket_count, Hash{}, key_equal{}, alloc) {}

		template<class InputIt>
		UnorderedMap(InputIt first, InputIt last, size_type bucket_count, Hash hash, const Allocator& alloc)
			: Base(first, last, bucket_count, hash, key_equal{}, alloc) {}

		UnorderedMap(const UnorderedMap& other) : Base(other, AllocTraits::select_on_container_copy_construction(other.vector_.alloc_)) {}

		UnorderedMap(const UnorderedMap& other, const Allocator& alloc) : Base(other, alloc) {}

		UnorderedMap(UnorderedMap&& other) : Base(std::move(other), std::move(other.vector_.alloc_)) {}

		UnorderedMap(UnorderedMap&& other, const Allocator& alloc) : Base(std::move(other), alloc) {}

		UnorderedMap(std::initializer_list<value_type> init, size_type bucket_count = this->min_buckets_, Hash hash = Hash{}, key_equal equal = key_equal{},
			const Allocator& alloc = Allocator{}) : Base(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}

		UnorderedMap(std::initializer_list<value_type> init, size_type bucket_count, const Allocator& alloc)
			: Base(init.begin(), init.end(), bucket_count, Hash{}, key_equal{}, alloc) {}

		UnorderedMap(std::initializer_list<value_type> init, size_type bucket_count, Hash hash, const Allocator& alloc)
			: Base(init.begin(), init.end(), bucket_count, hash, key_equal{}, alloc) {}
	};
}