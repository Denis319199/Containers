#include "ContainerUtilities.h"

void mylib::ContainerBase::orphanAll() noexcept {
	IteratorBase* it = proxy->first;
	while (it) {
		it->proxy = nullptr;
		it = it->next_iterator;
	}
	proxy->first = nullptr;
}

mylib::IteratorBase::IteratorBase() noexcept : proxy{}, next_iterator{} {}

mylib::IteratorBase::IteratorBase(const IteratorBase& rhs) noexcept : proxy{}, next_iterator{} {
	*this = rhs;
}

mylib::IteratorBase& mylib::IteratorBase::operator=(const IteratorBase& rhs) noexcept {
	if (rhs.proxy) adopt(rhs.proxy->parent);
	else orphanMe();
	return *this;
}

mylib::IteratorBase::~IteratorBase() {
	orphanMe();
}

void mylib::IteratorBase::adopt(const ContainerBase* parent) noexcept {
	if (parent) {
		IteratorProxy* parent_proxy = parent->proxy;

		if (parent_proxy != proxy) {
			orphanMe();
			next_iterator = parent_proxy->first;
			parent_proxy->first = this;
			proxy = parent_proxy;
		}
	}
	else orphanMe();
}

void mylib::IteratorBase::orphanMe() noexcept {
	if (proxy) {
		IteratorBase** it = &proxy->first;
		while (*it && *it != this) {
			it = &(*it)->next_iterator;
		}
		*it = next_iterator;
		proxy = nullptr;
	}
}

const mylib::ContainerBase* mylib::IteratorBase::getContainer() const noexcept {
	return proxy ? proxy->parent : nullptr;
}