#ifndef CIETO_H
#define CIETO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Public opaque handle for a Cieto
*/

typedef struct VM CieVM;

typedef struct CieCall CieCall;

typedef enum CieValueType{
    CIE_VALUE_NULL = 0,
    CIE_VALUE_BOOL,
    CIE_VALUE_NUMBER,
    CIE_VALUE_STRING,
    CIE_VALUE_OTHER
} CieValueType;

typedef struct CieValue{
    CieValueType type;

    union{
        bool boolean;
        double number;
    }as;
} CieValue;

typedef void (*CieWriteFunc)(const char* text, size_t length, void* userData);
typedef void (*CieNativeFunc)(CieCall* call, void* userData);

/*
 * Result of a public Cieto API operation
*/

typedef enum CieStatus{
    CIE_STATUS_OK = 0,
    CIE_STATUS_COMPILE_ERROR,
    CIE_STATUS_RUNTIME_ERROR,
    CIE_STATUS_INVALID_ARGUMENT,
    CIE_STATUS_OUT_OF_MEMORY,
    CIE_STATUS_UNSUPPORTED_TYPE
} CieStatus;

CieValue cie_value_null(void);
CieValue cie_value_bool(bool value);
CieValue cie_value_number(double value);

/*
 * Creates a new Cieto virtual machine.
 * The VM is created without script command-line arguments.
 * Returns NULL when storage for the VM cannot be allocated.
*/

CieVM* cie_vm_create(void);

/*
 * Destroys a VM created by cie_vm_create().
 * Passing NULL is allowed and has no effect.
*/
void cie_vm_destroy(CieVM* vm);

/*
 * Compiles and executes a null-terminated Cieto source string.
 * source_name is used in diagnostics. It may be NULL, in which case "<embedded>" is used.
*/
CieStatus cie_vm_eval(
    CieVM* vm,
    const char* source,
    const char* source_name
);

/*
 * Sets the output function for the VM.
 * The userData pointer is passed to the output function.
*/
void cie_vm_set_output(CieVM* vm, CieWriteFunc func, void* userData);

/*
 * Sets the error output function for the VM.
 * The userData pointer is passed to the error output function.
*/
void cie_vm_set_error_output(CieVM* vm, CieWriteFunc func, void* userData);

/*
 * Registers a host-provided native function as a Cieto global.
 * The user_data pointer is borrowed. Cieto does not free it, so it must remain valid
 * Register native functions before evaluating scripts that use them.
*/
CieStatus cie_vm_register_native(
    CieVM* vm,
    const char* name,
    CieNativeFunc function,
    void* user_data
);

/*
 * Calls a global Cieto function.
 * result may be NULL if the caller does not need the return value.
*/
CieStatus cie_vm_call(
    CieVM* vm,
    const char* name,
    int argCount,
    const CieValue* args,
    CieValue* result
);

/*
 * Returns the number of arguments passed by the Cieto script.
*/
int cie_call_arg_count(const CieCall* call);

/*
 * Returns the public type of an argument.
 * Invalid indexes return CIE_VALUE_OTHER.
*/
CieValueType cie_call_arg_type(const CieCall* call, int index);

bool cie_call_get_bool(
    const CieCall* call,
    int index,
    bool* result
);

bool cie_call_get_number(
    const CieCall* call,
    int index,
    double* result
);

/*
 * Returns a borrowed string pointer.
 * The returned pointer belongs to the VM and must not be freed.
 * It is valid only during the native function call.
*/
const char* cie_call_get_string(
    const CieCall* call,
    int index,
    size_t* length
);

void cie_call_return_null(CieCall* call);

void cie_call_return_bool(CieCall* call, bool value);

void cie_call_return_number(CieCall* call, double value);

/*
 * Copies the supplied string into the Cieto VM.
*/
void cie_call_return_string(
    CieCall* call,
    const char* value,
    size_t length
);

/*
 * Stops the current Cieto call with a runtime error.
 */
void cie_call_error(CieCall* call, const char* message);

/*
 * Returns the most recent compilation or runtime error message.
 * Returns NULL if vm is NULL or no error message is currently available.
 * The returned pointer is owned by the VM and must not be freed.
 * It remains valid until the next cie_vm_eval() call or until the VM is destroyed.
*/
const char* cie_vm_last_error(const CieVM* vm);

/*
 * Returns a static human-readable name for a status value.
 * The returned string must not be freed.
*/
const char* cie_status_string(CieStatus status);

#ifdef __cplusplus
}
#endif

#endif
