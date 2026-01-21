// Create a proc_demo process that
// must be reparented at exit.

#include "stat.h"
#include "types.h"
#include "user.h"

int main(void) {
	if (fork() > 0)
		sleep(5); // Let child exit before parent.
	exit();
}
