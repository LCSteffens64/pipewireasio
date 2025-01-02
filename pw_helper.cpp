#include "pw_helper.hpp"
#include "pw_helper_c.h"
#include "pw_helper_common.h"

#include <chrono>
#include <memory>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <cassert>

#include <thread>
#include <unordered_map>

#include <pipewire/context.h>
#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/main-loop.h>
#include <pipewire/node.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/proxy.h>
#include <pipewire/thread.h>
#include <pipewire/thread-loop.h>
#include <spa/pod/builder.h>

namespace PwHelper {

struct SpaPod {
	struct spa_pod *ptr;

	~SpaPod() {
		std::free(ptr);
	}

	constexpr explicit SpaPod(struct spa_pod *pod = nullptr)
		: ptr(pod) {}

	inline SpaPod(SpaPod &&o) : ptr(o.ptr) { o.ptr = nullptr; }

	inline SpaPod &operator=(SpaPod&& o) {
		std::swap(ptr, o.ptr);
		o = nullptr;
		return *this;
	}

	inline SpaPod &operator=(std::nullptr_t) {
		std::free(ptr);
		ptr = nullptr;
		return *this;
	}

	inline operator struct spa_pod const *() const {
		return ptr;
	}

	static SpaPod make(struct spa_pod const *pod) {
		return SpaPod(spa_pod_copy(pod));
	}

	// Explicit static method to avoid copying by accident
	static SpaPod copy(SpaPod const &from) {
		return make(from.ptr);
	}
};

// Source: <https://en.cppreference.com/w/cpp/container/unordered_map/find#Example>
struct string_view_hasher {
	using hash_type = std::hash<std::string_view>;
	using is_transparent = void;

	std::size_t operator()(const char* str) const        { return hash_type{}(str); }
	std::size_t operator()(std::string_view str) const   { return hash_type{}(str); }
	std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
};

using string_map = std::unordered_map<std::string, std::string, string_view_hasher, std::equal_to<>>;

enum class InitState {
	Init,
	Ready,
	Running,
};

enum class PwInterface {
	Unknown,
	Node,
};

enum class ProxyState {
	Init,
	PropsFilled,
	Fetching,
	UpdateInProgress,
};

struct Proxy {
	PwInterface type;

	static Proxy *get(struct pw_proxy *proxy) {
		return reinterpret_cast<Proxy *>(pw_proxy_get_user_data(proxy));
	}
};

struct Node: Proxy {
	std::atomic<ProxyState> info_state;
	std::atomic<ProxyState> param_state;
	struct spa_hook listener;
	struct pw_node_info info;
	string_map properties;
	std::unordered_map<uint32_t, SpaPod> params;

	static struct pw_node_events const s_events;

	static Node *get(void *proxy) {
		return reinterpret_cast<Node *>(pw_proxy_get_user_data(reinterpret_cast<struct pw_proxy *>(proxy)));
	}

	void init(struct pw_proxy *proxy) {
		new (this) Node;
		Proxy::type = PwInterface::Node;
		info_state.store(ProxyState::Init, std::memory_order_relaxed);
		param_state.store(ProxyState::Init, std::memory_order_relaxed);
		listener = {};
		pw_node_add_listener(proxy, &listener, &s_events, proxy);
		pw_node_enum_params(proxy, 0, PW_ID_ANY, 0, ~(uint32_t)0, nullptr);
	}

	void get_or_wait_for_info(
		struct pw_node_info *out_info,
		string_map *all_props,
		std::span<std::pair<std::string_view, std::string *> const> props
	) {
		// Wait for init
		info_state.wait(ProxyState::Init, std::memory_order_relaxed);
		// Then for updates
		ProxyState state = ProxyState::PropsFilled;
		while (!info_state.compare_exchange_weak(state, ProxyState::Fetching)) {
			assert(state == ProxyState::UpdateInProgress);
			info_state.wait(ProxyState::UpdateInProgress);
		}

		// Our turn to read
		if (out_info)
			*out_info = info;
		#if 0
		if (props) {
			if (props->empty()) {
				// Copy all props
				*props = properties;
			} else {
				// Only copy requested ones
				for (auto it = props->begin(); it != props->end(); ++it) {
					if (auto prop = properties.find(it->first); prop != properties.end()) {
						it->second = prop->second;
					}
				}
			}
		}
		#endif
		if (all_props) {
			// Copy all props
			*all_props = properties;
		}
		if (!props.empty()) {
			// Copy requested ones
			for (auto it = props.begin(); it != props.end(); ++it) {
				if (auto prop = properties.find(it->first); prop != properties.end()) {
					*it->second = prop->second;
				}
			}
		}
		info_state.store(ProxyState::PropsFilled, std::memory_order_release);
		info_state.notify_one();
	}

	#if 0
	SpaPod get_or_wait_for_param(uint32_t id) {
		// Wait for init
		// TODO: Handle multi-step init
		param_state.wait(ProxyState::Init, std::memory_order_relaxed);
		// Then for updates
		ProxyState state = ProxyState::PropsFilled;
		while (!param_state.compare_exchange_weak(state, ProxyState::Fetching)) {
			assert(state == ProxyState::UpdateInProgress);
			param_state.wait(ProxyState::UpdateInProgress);
		}

		// Our turn to read
		if (auto it = params.find(name); it != params.end())
			SpaPod pod = SpaPod::copy();

		param_state.store(ProxyState::PropsFilled, std::memory_order_release);
		param_state.notify_one();
		return pod;
	}
	#endif

	void update(struct pw_node_info const *new_info) {
		ProxyState state = ProxyState::Init;
		if (!info_state.compare_exchange_weak(state, ProxyState::UpdateInProgress)) {
			state = ProxyState::PropsFilled;
			while (!info_state.compare_exchange_weak(state, ProxyState::UpdateInProgress)) {
				assert(state == ProxyState::Fetching);
				info_state.wait(ProxyState::Fetching);
			}
		}

		// We can write now
		info = *new_info;
		properties.clear();
		for (auto *it = info.props->items, *end = it + info.props->n_items; it != end; ++it) {
			std::string key = it->key;
			std::string value = it->value;
			properties.emplace(std::move(key), std::move(value));
		}
		info_state.store(ProxyState::PropsFilled, std::memory_order_release);
		info_state.notify_one();
	}

	void update_param(void *proxy, uint32_t id, uint32_t index, uint32_t next, struct spa_pod const *param) {
		ProxyState state = ProxyState::Init;
		if (!param_state.compare_exchange_weak(state, ProxyState::UpdateInProgress)) {
			state = ProxyState::PropsFilled;
			while (!param_state.compare_exchange_weak(state, ProxyState::UpdateInProgress)) {
				assert(state == ProxyState::Fetching);
				param_state.wait(ProxyState::Fetching);
			}
		}

		SpaPod pod = SpaPod::make(param);
		if (auto it = params.find(id); it != params.end()) {
			it->second = std::move(pod);
		} else {
			params.emplace(id, std::move(pod));
			//pw_node_subscribe_params(proxy, &id, 1);
		}

		if (state == ProxyState::Init && next != PW_ID_ANY) {
			param_state.store(ProxyState::Init, std::memory_order_release);
		} else {
			param_state.store(ProxyState::PropsFilled, std::memory_order_release);
			param_state.notify_one();
		}
	}
};

static void node_info_handler(void *proxy, struct pw_node_info const *info) {
	Node::get(proxy)->update(info);
}

static void node_param_handler(void *proxy, int seq, uint32_t id, uint32_t index, uint32_t next, struct spa_pod const *param) {
	Node::get(proxy)->update_param(proxy, id, index, next, param);
}

struct pw_node_events const Node::s_events = {
	.version = PW_VERSION_NODE_EVENTS,
	.info = node_info_handler,
	.param = node_param_handler,
};

struct Helper {
	//struct pw_main_loop *main_loop = {};
	struct pw_thread_loop *thread_loop = {};
	struct pw_context *context = {};
	struct pw_core *core = {};
	struct pw_registry *registry = {};
	struct spa_hook registry_listener = {};

	struct spa_thread_utils *thread_impl = {};
	pw_helper_thread_creator_t thread_creator = {};
	struct spa_thread_utils thread_utils;

	std::unordered_map<uint32_t, struct pw_proxy *> bound_proxies;

	std::atomic<InitState> init_state = InitState::Init;
	std::atomic<int> roundtrip_state = -1;
	std::mutex state_mutex;

	struct spa_hook roundtrip = {};

	~Helper() {
		if (core) {
			pw_core_disconnect(core);
		}
		if (context) {
			pw_context_destroy(context);
		}
		if (thread_loop) {
			pw_thread_loop_destroy(thread_loop);
		}
	}

	void stop() {
		pw_thread_loop_stop(this->thread_loop);
	}

	void lock() {
		if (init_state.load(std::memory_order_relaxed) == InitState::Running) {
			state_mutex.lock();
		}
	}

	void unlock() {
		if (init_state.load(std::memory_order_relaxed) == InitState::Running) {
			state_mutex.unlock();
		}
	}

	void wait_for_roundtrip() {
		init_state.wait(InitState::Ready, std::memory_order_relaxed);
	}

	PwInterface get_proxy(uint32_t id, struct pw_proxy *&proxy) {
		if (auto it = bound_proxies.find(id); it != bound_proxies.end()) {
			proxy = it->second;
			return Proxy::get(proxy)->type;
		}
		return PwInterface::Unknown;
	}
};

#include "pw_thread.inc.cpp"

using namespace std::string_view_literals;
#define _SV(x) x##sv
#define SV(x) _SV(x)

static PwInterface get_known_interface(char const *type) {
	std::string_view svtype = type;
	if (svtype == SV(PW_TYPE_INTERFACE_Node)) {
		return PwInterface::Node;
	}
	return PwInterface::Unknown;
}

static void registry_global_handler(
	void *data, uint32_t id, uint32_t permissions,
	char const *type, uint32_t version, struct spa_dict const *props
) {
	Helper *This = reinterpret_cast<Helper *>(data);
	switch (get_known_interface(type)) {
		case PwInterface::Node: {
			This->lock();
			auto proxy = reinterpret_cast<struct pw_proxy *>(
				pw_registry_bind(This->registry, id, type, std::min(version, (uint32_t)PW_VERSION_NODE), sizeof(Node)));
			Node::get(proxy)->init(proxy);
			This->bound_proxies.emplace(id, proxy);
			This->unlock();
			break;
		}
		case PwInterface::Unknown: break;
	}
}

static void registry_global_remove_handler(void *data, uint32_t id) {
	Helper *This = reinterpret_cast<Helper *>(data);
	struct pw_proxy *global;
	This->lock();
	switch (This->get_proxy(id, global)) {
		case PwInterface::Node:
			Node::get(global)->~Node();
			goto destroy_proxy;

		destroy_proxy:
			This->bound_proxies.erase(id);
			pw_proxy_destroy(global);
			break;
		case PwInterface::Unknown: break;
	}
	This->unlock();
}

static struct pw_registry_events const s_registry_events = {
	.version = PW_VERSION_REGISTRY_EVENTS,
	.global = registry_global_handler,
	.global_remove = registry_global_remove_handler,
};

static void roundtrip_handler(void *data, uint32_t id, int seq) {
	Helper *This = reinterpret_cast<Helper *>(data);
	if (false) {
		// This is to test whether the initialization code propely waits for the roundtrip.
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	This->init_state.store(InitState::Running, std::memory_order_relaxed);
	This->init_state.notify_all();
}

static struct pw_core_events const s_core_events = {
	.version = PW_VERSION_CORE_EVENTS,
	.done = roundtrip_handler,
};

Helper *create_helper(int argc, char **argv, InitArgs const *conf) {
	pw_init(&argc, &argv);
	printf("PipeWire initialized with version: %s\n", pw_get_library_version());

	std::unique_ptr<Helper> This(new Helper);

	if (!(This->thread_loop = pw_thread_loop_new("pw-loop", NULL)))
	{
		std::fputs("Unable to create the PipeWire loop\n", stderr);
		return nullptr;
	}

	struct pw_properties *init_props = pw_properties_new(
		PW_KEY_CLIENT_NAME, "pw-asio",
		PW_KEY_CLIENT_API, "ASIO",
		nullptr);
	if (conf->app_name) {
		pw_properties_set(init_props, PW_KEY_APP_NAME, conf->app_name);
	}

	if (!(This->context = pw_context_new(
		pw_thread_loop_get_loop(This->thread_loop),
		init_props, 0)))
	{
		std::fputs("Unable to create a PipeWire context\n", stderr);
		return nullptr;
	}

	if (conf->thread_creator) {
		This->thread_creator = conf->thread_creator;
	}

	This->thread_impl = reinterpret_cast<struct spa_thread_utils *>(pw_context_get_object(This->context, SPA_TYPE_INTERFACE_ThreadUtils));
	if (!This->thread_impl) {
		This->thread_impl = pw_thread_utils_get();
	}
	This->thread_utils.iface = SPA_INTERFACE_INIT(
			SPA_TYPE_INTERFACE_ThreadUtils,
			SPA_VERSION_THREAD_UTILS,
			&thread_utils_impl, This.get());
	pw_context_set_object(This->context, SPA_TYPE_INTERFACE_ThreadUtils, &This->thread_utils);

	if (!(This->core = pw_context_connect(This->context, NULL, 0)))
	{
		std::fputs("Unable to connect to a PipeWire server\n", stderr);
		return nullptr;
	}

	if (!(This->registry = pw_core_get_registry(This->core, PW_VERSION_REGISTRY, 0)))
	{
		std::fputs("Unable to get the PipeWire registry\n", stderr);
		return nullptr;
	}

	pw_registry_add_listener(This->registry, &This->registry_listener, &s_registry_events, This.get());

	// Make sure to get all registered nodes before the client asks for them. (could be done
	// at a later point, but let's just wait now)
	pw_core_add_listener(This->core, &This->roundtrip, &s_core_events, This.get());

	This->init_state.store(InitState::Ready, std::memory_order_relaxed);

	This->roundtrip_state.store(0, std::memory_order_relaxed);
	pw_core_sync(This->core, PW_ID_CORE, 0);

	std::puts("[DEBUG] Starting thread");
	if (pw_thread_loop_start(This->thread_loop)) {
		std::fputs("Unable to start the PipeWire loop\n", stderr);
		return nullptr;
	}

	This->wait_for_roundtrip();
	std::puts("[DEBUG] Rountrip done");

	if (conf->loop)
		*conf->loop = pw_thread_loop_get_loop(This->thread_loop);
	if (conf->context)
		*conf->context = This->context;
	if (conf->core)
		*conf->core = This->core;

	return This.release();
}

void destroy_helper(Helper *helper) {
	helper->stop();
	delete helper;
}

std::vector<struct pw_node *> enumerate_pipewire_endpoints(Helper *helper) {
	std::vector<struct pw_node *> nodes;
	helper->lock();
	for (auto it = helper->bound_proxies.begin(), end = helper->bound_proxies.end(); it != end; ++it) {
		if (Proxy::get(it->second)->type == PwInterface::Node) {
			nodes.push_back(reinterpret_cast<struct pw_node *>(it->second));
		}
	}
	helper->unlock();
	return nodes;
}

void get_node_props(Helper *helper, struct pw_node *proxy, std::span<std::pair<std::string_view, std::string*>> props) {
	Node *node = Node::get(proxy);
	node->get_or_wait_for_info(nullptr, nullptr, props);
}

void lock_loop(Helper *helper) {
	pw_thread_loop_lock(helper->thread_loop);
}

void unlock_loop(Helper *helper) {
	pw_thread_loop_unlock(helper->thread_loop);
}

// C API

extern "C" {

struct user_pw_helper *user_pw_create_helper(int argc, char **argv, struct pw_helper_init_args const *conf) {
	return reinterpret_cast<struct user_pw_helper *>(create_helper(argc, argv, conf));
}

void user_pw_destroy_helper(struct user_pw_helper *helper) {
	destroy_helper(reinterpret_cast<Helper *>(helper));
}

void user_pw_lock_loop(struct user_pw_helper *helper) {
	lock_loop(reinterpret_cast<Helper *>(helper));
}

void user_pw_unlock_loop(struct user_pw_helper *helper) {
	unlock_loop(reinterpret_cast<Helper *>(helper));
}

}

}
