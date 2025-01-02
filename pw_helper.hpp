#pragma once

#include <pipewire/node.h>
#include <string>
#include <vector>
#include <span>

#include "pw_helper_common.h"

#ifdef PW_HELPER_LIB
#define PW_HELPER_API __attribute__((__visibility__("default")))
#else
#define PW_HELPER_API
#endif

namespace PwHelper {

struct Helper;
typedef struct pw_helper_init_args InitArgs;

PW_HELPER_API Helper *create_helper(int argc, char **argv, InitArgs const *conf);
PW_HELPER_API void destroy_helper(Helper *helper);

PW_HELPER_API std::vector<struct pw_node *> enumerate_pipewire_endpoints(Helper *helper);
PW_HELPER_API struct pw_node *get_default_input(Helper *helper);
PW_HELPER_API struct pw_node *get_default_output(Helper *helper);

PW_HELPER_API void get_node_props(Helper *helper, struct pw_node *proxy, std::span<std::pair<std::string_view, std::string*>> props);

PW_HELPER_API void lock_loop(Helper *helper);
PW_HELPER_API void unlock_loop(Helper *helper);

}
