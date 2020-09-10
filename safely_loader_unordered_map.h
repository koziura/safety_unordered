#include <memory>
#include <shared_mutex>
#include <unordered_map>

//safely loader unordered atlas map
template <typename key_t, typename value_t>
class unordered_map_safely {
	mutable std::shared_mutex access;
	typedef std::unique_lock<std::shared_mutex> writeLock_t;
	typedef std::shared_lock<std::shared_mutex> readLock_t;

	class value_pack {
		value_t _value;
		bool _busy = true;

	public:
		mutable std::condition_variable cv;

		value_pack() = default;
		value_pack(const value_t& value, bool busy = true)
			: _value(value)
			, _busy(busy)
		{
			if (!_busy) {
				cv.notify_all();
			}
		}

		void set(const value_t& value, bool busy) {
			_value = value;
			_busy = busy;
			if (!_busy) {
				cv.notify_all();
			}
		}

		const value_t& get() const {
			return _value;
		}

		value_t& get() {
			return _value;
		}

		bool isBusy() const {
			return _busy;
		}
	};

	typedef std::unordered_map<key_t, value_pack> safety_map_t;
	safety_map_t safety_map;

public:
	unordered_map_safely() = default;

	void set(const key_t& key, const value_t& value) {
		writeLock_t lock(access);
		safety_map.at(key).set(value, false);
	}

	bool reserve(const key_t& key) {
		writeLock_t lock(access);
		return safety_map.emplace(key, nullptr).second;
	}

	const value_pack& get(const key_t& key) const {
		readLock_t lock(access);
		return safety_map.at(key);
	}

	bool busy(const key_t& key) const {
		readLock_t lock(access);
		const auto item = safety_map.find(key);
		if (item == safety_map.end()) {
			return false;
		}
		return item->second.isBusy();
	}

	bool contains(const key_t& key) const {
		readLock_t lock(access);
		return safety_map.find(key) != safety_map.end();
	}

	void clear() {
		writeLock_t lock(access);
		safety_map.clear();
	}

	bool empty() const {
		readLock_t lock(access);
		return safety_map.empty();
	}
};
