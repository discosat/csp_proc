#include <stdio.h>
#include <string.h>

#include <slash/slash.h>
#include <csp_proc/proc_memory.h>

int proc_new(struct slash * slash);
int proc_del(struct slash * slash);
int proc_pull(struct slash * slash);
int proc_push(struct slash * slash);
int proc_size(struct slash * slash);
int proc_pop(struct slash * slash);
int proc_list(struct slash * slash);
int proc_slots(struct slash * slash);
int proc_run(struct slash * slash);
int proc_block(struct slash * slash);
int proc_ifelse(struct slash * slash);
int proc_noop(struct slash * slash);
int proc_set(struct slash * slash);
int proc_unop(struct slash * slash);
int proc_binop(struct slash * slash);
int proc_call(struct slash * slash);

#define MAX_HOSTS   100
#define MAX_NAMELEN 50

struct host_s {
	int node;
	char name[MAX_NAMELEN];
} known_hosts[100];

int known_hosts_get_node(char * find_name) {
	if (find_name == NULL)
		return 0;

	for (int i = 0; i < MAX_HOSTS; i++) {
		if (strncmp(find_name, known_hosts[i].name, MAX_NAMELEN) == 0) {
			return known_hosts[i].node;
		}
	}

	return 0;
}

int proc_slash_command(const char * command) {
	struct slash slash = {0};

	char * command_copy = proc_strdup(command);
	char * argv[32];
	int argc = 0;
	char * token = strtok(command_copy, " ");
	while (token != NULL && argc < 32) {
		argv[argc++] = token;
		token = strtok(NULL, " ");
	}

	slash.argv = argv;
	slash.argc = argc;

	int result = SLASH_SUCCESS;

	if (strcmp(argv[0], "proc") != 0) {
		printf("Unsupported command: %s\n", argv[0]);
		return SLASH_EINVAL;
	}

	if (argc >= 1) {
		slash.argv += 1;
		slash.argc -= 1;
	} else {
		printf("Invalid command specified\n");
		return SLASH_EINVAL;
	}

	if (strcmp(argv[1], "new") == 0) {
		result = proc_new(&slash);
	} else if (strcmp(argv[1], "del") == 0) {
		result = proc_del(&slash);
	} else if (strcmp(argv[1], "pull") == 0) {
		result = proc_pull(&slash);
	} else if (strcmp(argv[1], "push") == 0) {
		result = proc_push(&slash);
	} else if (strcmp(argv[1], "size") == 0) {
		result = proc_size(&slash);
	} else if (strcmp(argv[1], "pop") == 0) {
		result = proc_pop(&slash);
	} else if (strcmp(argv[1], "list") == 0) {
		result = proc_list(&slash);
	} else if (strcmp(argv[1], "slots") == 0) {
		result = proc_slots(&slash);
	} else if (strcmp(argv[1], "run") == 0) {
		result = proc_run(&slash);
	} else if (strcmp(argv[1], "block") == 0) {
		result = proc_block(&slash);
	} else if (strcmp(argv[1], "ifelse") == 0) {
		result = proc_ifelse(&slash);
	} else if (strcmp(argv[1], "noop") == 0) {
		result = proc_noop(&slash);
	} else if (strcmp(argv[1], "set") == 0) {
		result = proc_set(&slash);
	} else if (strcmp(argv[1], "unop") == 0) {
		result = proc_unop(&slash);
	} else if (strcmp(argv[1], "binop") == 0) {
		result = proc_binop(&slash);
	} else if (strcmp(argv[1], "call") == 0) {
		result = proc_call(&slash);
	} else {
		printf("Unknown command: %s\n", argv[1]);
		result = SLASH_EINVAL;
	}

	proc_free(command_copy);

	return result;
}
