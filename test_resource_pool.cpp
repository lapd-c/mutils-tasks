#include <iostream>
#include <future>
#include "resource_pool.hpp"

using namespace mutils;

void single_threaded_test(){
	{
	using LockedResource = typename ResourcePool<int>::LockedResource;
	using WeakResource = typename ResourcePool<int>::WeakResource;
	ResourcePool<int> pool{3,2,[](){return new int{0};}};
	auto prefer_1 = [&](){
		auto my_int = pool.acquire();
		assert(*my_int == 0);
		*my_int = 1;
		WeakResource ret{my_int};
		assert(*ret.lock() == 1);
		return ret;
	}();
	auto prefer_2 = [&](){
		auto my_int_2 = pool.acquire();
		*my_int_2 = 2;
		WeakResource ret{my_int_2};
		assert(*ret.lock() == 2);
		return ret;
	}();
	auto prefer_3 = [&](){
		auto my_int_3 = pool.acquire();
		*my_int_3 = 3;
		WeakResource ret{my_int_3};
		assert(*ret.lock() == 3);
		return ret;
	}();
	assert(pool.preferred_full());
	assert(*prefer_2.lock() != 0);
	assert(*prefer_1.lock() != 0);
	assert(*prefer_3.lock() == 3);
	assert(*prefer_2.lock() == 2);
	assert(*prefer_1.lock() == 1);
	{
		auto hold_shared = [&](const auto&, const auto&, const auto&){
			return std::make_pair(pool.acquire(),pool.acquire());
		}(prefer_1.lock(), prefer_2.lock(),prefer_3.lock());
		assert(pool.preferred_full());
		
		WeakResource orphan{[&](const auto &, const auto &, const auto&){
				assert(*pool.acquire() == 0);
				return pool.acquire();
			}(prefer_1.lock(), prefer_2.lock(),prefer_3.lock())};
		auto val = *orphan.lock();
		assert(val == 1 || val == 2 || val == 3);
		assert(*prefer_2.lock() == 2);
		assert(*prefer_1.lock() == 1);
		auto locked_orphan = orphan.lock();
		assert(*locked_orphan == 1 || *locked_orphan == 2 || *locked_orphan == 3);
		if (*locked_orphan == 3){
			assert(*prefer_2.lock() == 2);
			assert(*prefer_1.lock() == 1);
			auto tmp = *prefer_3.lock();
			assert(tmp == 1 || tmp == 2);
		}
		if (*locked_orphan == 2){
			assert(*prefer_1.lock() == 1);
			assert(*prefer_2.lock() == 1 || *prefer_2.lock() == 3);
			assert(*prefer_3.lock() == 3);
		}
		if (*locked_orphan == 1){
			assert(*prefer_1.lock() == 2 || *prefer_2.lock() == 3);
			assert(*prefer_2.lock() == 2);
			assert(*prefer_3.lock() == 3);
		}
		assert(*pool.acquire() != 0);
	}
	{
		auto state = pool.dbg_leak_state();
		assert(state->free_resources.size() == 5);
		auto hold_first_shared = [&](const auto&, const auto&, const auto&){
			return pool.acquire();
		}(prefer_1.lock(), prefer_2.lock(),prefer_3.lock());
		*hold_first_shared = 5;
		assert(state->free_resources.size() == 4);

		assert(ResourcePool<int>::rented_spare::resource_type() == hold_first_shared.which_resource_type());
		
		
		WeakResource spare{[&](const auto &, const auto &, const auto&){
				auto l = pool.acquire();
				assert(ResourcePool<int>::rented_spare::resource_type() == l.which_resource_type());
				assert(*l == 0);
				*l = 4;
				return l;
			}(prefer_1.lock(), prefer_2.lock(),prefer_3.lock())};
		auto val = *spare.lock();
		assert(state->free_resources.size() == 4);
		assert(val > 0 && val <= 4);
		auto locked_spare = spare.lock();
		assert(ResourcePool<int>::rented_spare::resource_type() == locked_spare.which_resource_type()
			   || ResourcePool<int>::rented_preferred::resource_type() == locked_spare.which_resource_type()
			);
		assert(*locked_spare > 0 || *locked_spare <= 4);
		assert(*prefer_3.lock() == 3 || (*locked_spare == 3 && *prefer_3.lock() == 4));
		assert(*prefer_2.lock() == 2 || (*locked_spare == 2 && *prefer_2.lock() == 4));
		assert(*prefer_1.lock() == 1 || (*locked_spare == 1 && *prefer_1.lock() == 4));
		assert(*pool.acquire() != 0);
	}
	WeakResource spare{[&](const auto &, const auto &, const auto&){
			assert(*pool.acquire() > 3);
			return pool.acquire();
		}(prefer_1.lock(), prefer_2.lock(),prefer_3.lock())};
	assert(*spare.lock() == 4 || *spare.lock() == 5);
}
}

void multi_threaded_test(){
	struct Incrementor{
		int i;
		std::mutex m;
		Incrementor(int i):i(i){}
		auto incr() {
			assert(m.try_lock());
			std::unique_lock<std::mutex> lock{m, std::adopt_lock};
			return ++i;
		}
	};
	using LockedResource = typename ResourcePool<Incrementor, int>::LockedResource;
	using WeakResource = typename ResourcePool<Incrementor, int>::WeakResource;
	ResourcePool<Incrementor, int> pool{30,10,[](int i){return new Incrementor{i};}};
	std::vector<std::future<void> > futs;
	for (int i = 0; i < 80; ++i){
		futs.emplace_back(
			std::async(std::launch::async,
					   [i, weak_resource = WeakResource{pool.acquire(std::move(i))}]()  mutable {
						   while (weak_resource.lock(std::move(i))->incr() < 50000);
					   }));
	}
	for (auto& fut : futs){
		fut.get();
	}

	auto state = pool.dbg_leak_state();
	std::cout << "state check: " << std::endl;
	for (const auto &res : state->preferred_resources){
		auto i = res.resource->i;
		if (i < 50000) std::cout << i << std::endl;
	}

}

int main(){
	single_threaded_test();
	multi_threaded_test();

}
