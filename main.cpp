#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <sstream>
#include <random>

#include "safely_loader_unordered_map.h"

struct Atlas {
	Atlas(std::string name) {
		using namespace std;
		random_device rd;
		mt19937 gen(rd());
		uniform_int_distribution<> dist(10, 1000);
		auto val = dist(gen);
		auto thread_id = this_thread::get_id();
		stringstream ss;
		ss << thread_id;
		printf("add thread id: %s name: %s, load ms:%d\n",
			   ss.str().c_str(), name.c_str(), val);

		this_thread::sleep_for(chrono::milliseconds(val));
	}

};
typedef std::shared_ptr<Atlas> atlasPtr_t;

class atlasResourceLoader {
public:
	atlasPtr_t allocate(const std::string& fname);
	bool isAllocated(const std::string& fname) const;
	bool contains(const std::string& fname) const;
	void join(const std::string& fname) const;
	void release();

private:
	unordered_map_safely<std::string, atlasPtr_t> atlasMap;
};

atlasPtr_t atlasResourceLoader::allocate(const std::string& fname) {
	if (atlasMap.contains(fname)) {
		return atlasMap.get(fname).get();
	} else if (atlasMap.reserve(fname)) {
		auto atlas = std::make_shared<Atlas>(fname);
		atlasMap.set(fname, atlas);
		return atlas;
	}
	return nullptr;
}

bool atlasResourceLoader::isAllocated(const std::string& fname) const {
	return atlasMap.busy(fname);
}

void atlasResourceLoader::join(const std::string& fname) const {
	const auto& pack = atlasMap.get(fname);
	if (pack.isBusy()) {
		std::mutex cv_m;
		std::unique_lock<std::mutex> lk(cv_m);
		pack.cv.wait(lk);
	}
}

void atlasResourceLoader::release() {
	atlasMap.clear();
}

bool atlasResourceLoader::contains(const std::string& fname) const {
	return atlasMap.contains(fname);
}

int main()
{
	atlasResourceLoader sharedAtlasLoder;
	std::list<std::future<void>> hanlelist;

	typedef std::vector<std::string> string_list_t;

	string_list_t list;
	list.emplace_back("bubble");
	list.emplace_back("skeleton");
	list.emplace_back("rose");
	list.emplace_back("cactus");
	list.emplace_back("car");
	list.emplace_back("ufo");
	list.emplace_back("cat");
	list.emplace_back("hall");
	list.emplace_back("kitchen");
	list.emplace_back("facade");
	list.emplace_back("cinema");

	using namespace std;
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> dist(0, (int)list.size()-1);

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	for (int var = 1; var < 1000; ++var) {
		auto name = list.at(dist(gen));
		hanlelist.emplace_back(
					std::async([&sharedAtlasLoder, &dist, &gen, &list, name]
		{
			if (sharedAtlasLoder.isAllocated(name)) {
				auto thread_id = this_thread::get_id();
				stringstream ss;
				ss << thread_id;
				printf("thread id: %s join name: %s\n",
					   ss.str().c_str(), name.c_str());
				sharedAtlasLoder.join(name);
			} else {
				if (!sharedAtlasLoder.contains(name)) {
					sharedAtlasLoder.allocate(name);
				} else {
					printf("contains name: %s\n", name.c_str());
				}
			}
		}));
	}

	for (auto& handle : hanlelist) {
		handle.wait();
	}
	hanlelist.clear();
	printf("finish!\n");

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

	return 0;
}
