#ifndef PICO_H
#define PICO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Public opaque handle for a PiCo
*/

typedef struct VM PicoVM;

typedef struct PicoCall PicoCall;

typedef enum PicoValueType{
    PICO_VALUE_NULL = 0,
    PICO_VALUE_BOOL,
    PICO_VALUE_NUMBER,
    PICO_VALUE_STRING,
    PICO_VALUE_OTHER
} PicoValueType;

typedef void (*PicoNativeFn)(PicoCall* call, void* user_data);

/*
 * Result of a public PiCo API operation
*/

typedef enum PicoStatus{
    PICO_STATUS_OK = 0,
    PICO_STATUS_COMPILE_ERROR,
    PICO_STATUS_RUNTIME_ERROR,
    PICO_STATUS_INVALID_ARGUMENT
} PicoStatus;

/*
 * Creates a new PiCo virtual machine.
 * The VM is created without script command-line arguments.
 * Returns NULL when storage for the VM cannot be allocated.
*/

PicoVM* pico_vm_create(void);

/*
 * Destroys a VM created by pico_vm_create().
 * Passing NULL is allowed and has no effect.
*/
void pico_vm_destroy(PicoVM* vm);

/*
 * Compiles and executes a null-terminated PiCo source string.
 * source_name is used in diagnostics. It may be NULL, in which case "<embedded>" is used.
*/
PicoStatus pico_vm_eval(
    PicoVM* vm,
    const char* source,
    const char* source_name
);

/*
 * Registers a host-provided native function as a PiCo global.
 * The user_data pointer is borrowed. PiCo does not free it, so it must remain valid
 * Register native functions before evaluating scripts that use them.
*/
PicoStatus pico_vm_register_native(
    PicoVM* vm,
    const char* name,
    PicoNativeFn function,
    void* user_data
);

/*
 * Returns the number of arguments passed by the PiCo script.
*/
int pico_call_arg_count(const PicoCall* call);

/*
 * Returns the public type of an argument.
 * Invalid indexes return PICO_VALUE_OTHER.
*/
PicoValueType pico_call_arg_type(const PicoCall* call, int index);

bool pico_call_get_bool(
    const PicoCall* call,
    int index,
    bool* result
);

bool pico_call_get_number(
    const PicoCall* call,
    int index,
    double* result
);

/*
 * Returns a borrowed string pointer.
 * The returned pointer belongs to the VM and must not be freed.
 * It is valid only during the native function call.
*/
const char* pico_call_get_string(
    const PicoCall* call,
    int index,
    size_t* length
);

void pico_call_return_null(PicoCall* call);

void pico_call_return_bool(PicoCall* call, bool value);

void pico_call_return_number(PicoCall* call, double value);

/*
 * Copies the supplied string into the PiCo VM.
*/
void pico_call_return_string(
    PicoCall* call,
    const char* value,
    size_t length
);

/*
 * Stops the current PiCo call with a runtime error.
 */
void pico_call_error(PicoCall* call, const char* message);

/*
 * Returns the most recent compilation or runtime error message.
 * Returns NULL if vm is NULL or no error message is currently available.
 * The returned pointer is owned by the VM and must not be freed.
 * It remains valid until the next pico_vm_eval() call or until the VM is destroyed.
*/
const char* pico_vm_last_error(const PicoVM* vm);

/*
 * Returns a static human-readable name for a status value.
 * The returned string must not be freed.
*/
const char* pico_status_string(PicoStatus status);

#ifdef __cplusplus
}
#endif

#endif