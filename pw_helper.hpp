#pragma once

#include <pipewire/node.h>
#include <string>
#include <vector>
#include <span>

namespace PwHelper {

struct Helper;

Helper *create_helper(int argc, char **argv);
void destroy_helper(Helper *helper);

std::vector<struct pw_node *> enumerate_pipewire_endpoints(Helper *helper);
struct pw_node *get_default_input(Helper *helper);
struct pw_node *get_default_output(Helper *helper);

void get_node_props(Helper *helper, struct pw_node *proxy, std::span<std::pair<std::string_view, std::string*>> props);

}
