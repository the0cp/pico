#include "common.h"
#include "vm.h"
#include "repl.h"
#include "file.h"
#include "version.h"

static void printVersion(void){
    printf("Cieto %s\n", CIETO_VERSION);
}

static void printHelp(const char* programName){
    printf("Cieto %s\n", CIETO_VERSION);
    printf("\n");
    printf("A compact C-family scripting language and embeddable register-based virtual machine implemented entirely in C.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s                         Start the REPL\n", programName);
    printf("  %s <file.cies> [args...]    Run a script\n", programName);
    printf("  %s run <file.cies> [args...] Run a script\n", programName);
    printf("  %s --dump, -d <file.cies>   Compile and dump bytecode\n", programName);
    printf("  %s --help                  Show this help message\n", programName);
    printf("  %s --version               Show version information\n", programName);
    printf("\n");
    printf("Examples:\n");
    printf("  %s examples/tour.cies\n", programName);
    printf("  %s examples/argv_echo.cies hello world\n", programName);
}

int main(int argc, const char* argv[]){
    if(argc >= 2){
        if(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0){
            printHelp(argv[0]);
            return 0;
        }

        if(strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0){
            printVersion();
            return 0;
        }
    }
    
    VM vm;

    if(argc == 1){
        initVM(&vm, 0, NULL);
        repl(&vm);
    }else{
        if(strcmp(argv[1], "--dump") == 0 || strcmp(argv[1], "-d") == 0){
            if(argc != 3){
                printHelp(argv[0]);
                return 64;
            }

            initVM(&vm, 0, NULL);

            int status = dumpScript(&vm, argv[2]);

            freeVM(&vm);
            return status;
        }

        int scriptArgsSt = 1;

        if(strcmp(argv[1], "run") == 0){
            scriptArgsSt = 2;
        }
        
        if(scriptArgsSt >= argc){
            fprintf(stderr, "Usage: %s [run] [script] [args...]\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 64;
        }

        initVM(&vm, argc - scriptArgsSt, argv + scriptArgsSt);
        runScript(&vm, argv[scriptArgsSt]);
    }
    
    freeVM(&vm);
    return 0;
}
