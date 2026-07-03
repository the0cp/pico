#ifndef PICO_H
#define PICO_H

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Public opaque handle for a PiCo
*/

typedef struct VM PicoVM;

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
 * Returns a static human-readable name for a status value.
 * The returned string must not be freed.
*/
const char* pico_status_string(PicoStatus status);

#ifdef __cplusplus
}
#endif

#endif