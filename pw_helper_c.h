#pragma once

#include "pw_helper_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct user_pw_helper;

struct user_pw_helper *user_pw_create_helper(int argc, char **argv, struct pw_helper_init_args const *conf);
void user_pw_destroy_helper(struct user_pw_helper *helper);

void user_pw_lock_loop(struct user_pw_helper *helper);
void user_pw_unlock_loop(struct user_pw_helper *helper);

#ifdef __cplusplus
}
#endif
