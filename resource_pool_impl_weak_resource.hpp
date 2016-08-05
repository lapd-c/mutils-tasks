#pragma once

namespace mutils {
	template<typename T, typename... Args>
	typename ResourcePool<T,Args...>::LockedResource ResourcePool<T,Args...>::WeakResource::lock(Args && ... a){
		assert(parent);
		auto locked = rsource.lock();
		auto single_locked = single_resource.lock();
		if ((locked && locked->t) || (single_locked && single_locked->t))
			return LockedResource(*this);
		else if (index_preference){
			auto to_return = 
				parent->acquire_with_preference(parent,index_preference,std::forward<Args>(a)...);
			*this = to_return;
			return to_return;
		}
		else {
			auto to_return =
				parent->acquire_new_preference(parent,std::forward<Args>(a)...);
			*this = to_return;
			return to_return;
		}
	}

	template<typename T, typename... Args>
	bool ResourcePool<T,Args...>::WeakResource::is_locked() const {
		return !rsource.expired() || !single_resource.expired();
	}

	template<typename T, typename... Args>
	typename ResourcePool<T,Args...>::LockedResource ResourcePool<T,Args...>::WeakResource::acquire_if_locked() const {
		auto l1 = rsource.lock();
		auto l2 = single_resource.lock();
		if (l1 || l2){
			return LockedResource{*this};
		}
		else throw ResourceInvalidException{};
	}

	template<typename T, typename... Args>
	typename ResourcePool<T,Args...>::WeakResource&
	ResourcePool<T,Args...>::WeakResource::operator=(const LockedResource& lr)
	{
		index_preference = lr.index_preference;
		parent = lr.parent;
		rsource = lr.rsource;
		single_resource = lr.single_resource;
		assert((index_preference && parent && lr.rsource)
			   || (parent && lr.single_resource));
		assert(index_preference ? index_preference.use_count() > 1 : true);
		assert(index_preference ? lr.index_preference.use_count() > 1 : true);
		return *this;
	}

	template<typename T, typename... Args>
	ResourcePool<T,Args...>::WeakResource::WeakResource(const LockedResource& lr)
		:index_preference(lr.index_preference),
		 parent(lr.parent),
		 rsource(lr.rsource),
		 single_resource(lr.single_resource)
	{
		assert((index_preference && parent && lr.rsource)
			   || (parent && lr.single_resource));
		assert(index_preference ? index_preference.use_count() > 1 : true);
		assert(index_preference ? lr.index_preference.use_count() > 1 : true);
	}

	template<typename T, typename... Args>
	ResourcePool<T,Args...>::WeakResource::WeakResource(WeakResource&& o)
		:index_preference(o.index_preference),
		 parent(o.parent),
		 rsource(o.rsource),
		 single_resource(o.single_resource)
	{
		o.index_preference.reset();
		o.parent.reset();
		o.rsource.reset();
		o.single_resource.reset();
	}
}
