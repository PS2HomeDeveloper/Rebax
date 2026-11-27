/* Native Script node: forwards the lifecycle to the project's rebax_game_start/update/stop hooks. */
#include <stddef.h>
#include "node_interface.h"
#include "rebax.h"
typedef struct { const char *source_file; const char *script_id; int owner; int started; } native_script_instance_t;
static int g_native_script_active;
static void native_script_init(void *self) {
    native_script_instance_t *instance = (native_script_instance_t *)self;
    instance->owner = 0;
    if (g_native_script_active) return;
    g_native_script_active = 1;
    instance->owner = 1;
}
static void native_script_update(void *self, float dt) {
    native_script_instance_t *instance = (native_script_instance_t *)self;
    if (!instance->owner) return;
    if (!instance->started) { instance->started = 1; rebax_game_start(); }
    rebax_game_update(dt);
}
static void native_script_draw(void *self) { (void)self; }
static void native_script_destroy(void *self) {
    native_script_instance_t *instance = (native_script_instance_t *)self;
    if (!instance->owner) return;
    if (instance->started) rebax_game_stop();
    instance->owner = 0;
    g_native_script_active = 0;
}
static const node_property_t native_script_properties[] = {
    { "Source File", NODE_PROPERTY_TYPE_STRING, { .s = "source/native_script.c" }, 0 },
    { "Script ID", NODE_PROPERTY_TYPE_STRING, { .s = "main" }, 0 }
};
static const size_t native_script_offsets[] = { offsetof(native_script_instance_t, source_file), offsetof(native_script_instance_t, script_id) };
const node_interface_t native_script_interface = { native_script_init, native_script_update, native_script_draw, native_script_destroy, native_script_properties, 2, sizeof(native_script_instance_t), native_script_offsets };
/* @NODE type=NODE_TYPE_NATIVE_SCRIPT name=NativeScript icon=Code properties=native_script_properties */
