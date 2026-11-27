/* Rebax C++ convenience wrapper; it intentionally uses no exceptions or RTTI. */
#ifndef REBAX_HPP
#define REBAX_HPP
#include "rebax.h"
namespace rebax {
using Vec2 = rebax_vec2_t;
using Transform2D = rebax_transform2d_t;
inline void log(const char *message) { rebax_log(message); }
inline float delta_time() { return rebax_delta_time(); }
inline int key_down(int key) { return rebax_key_down(key); }
inline int find_node(const char *name) { return rebax_find_node(name); }
inline int set_transform(int node, const Transform2D &value) { return rebax_node_set_transform2d(node, &value); }
inline int get_transform(int node, Transform2D &value) { return rebax_node_get_transform2d(node, &value); }
inline void quit() { rebax_quit(); }
}
#endif
