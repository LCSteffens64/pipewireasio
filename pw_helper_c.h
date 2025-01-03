#pragma once

#include "pw_helper_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct user_pw_helper;

enum user_pw_default_node_type {
	USER_PW_DEFAULT_INPUT,
	USER_PW_DEFAULT_OUTPUT,
};

struct user_pw_helper *user_pw_create_helper(int argc, char **argv, struct pw_helper_init_args const *conf);
void user_pw_destroy_helper(struct user_pw_helper *helper);

struct pw_node *user_pw_get_default_node(struct user_pw_helper *helper, enum user_pw_default_node_type type);
struct pw_node *user_pw_find_node_by_name(struct user_pw_helper *helper, char const *name);

void user_pw_lock_loop(struct user_pw_helper *helper);
void user_pw_unlock_loop(struct user_pw_helper *helper);

#ifdef __cplusplus
}
#endif
