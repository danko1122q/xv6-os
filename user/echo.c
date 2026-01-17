#include "stat.h"
#include "types.h"
#include "user.h"

/**
 * Main entry point for the echo utility.
 * @param argc: Argument count (includes the program name itself).
 * @param argv: Array of strings representing the command-line arguments.
 */
int main(int argc, char *argv[]) {
    int i;

    // Start loop at index 1 to skip argv[0] (which is the command name "echo").
    for (i = 1; i < argc; i++) {
        /*
         * printf arguments:
         * 1: The file descriptor for standard output (STDOUT).
         * %s%s: Format string for the argument and the separator.
         * argv[i]: The current word to print.
         * Ternary Operator (i + 1 < argc ? " " : "\n"): 
         * - If there is another argument coming, append a space (" ").
         * - If this is the final argument, append a newline ("\n").
         */
        printf(1, "%s%s", argv[i], i + 1 < argc ? " " : "\n");
    }

    // Terminate the process and return control to the shell.
    // In many hobby OSes, calling exit() is mandatory to prevent 
    // the CPU from executing garbage memory after the function ends.
    exit();
}
