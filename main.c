#include<time.h>
#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>


typedef uintptr_t tagged_ptr_t;

#define TYPE_OFFSET 63ULL
enum symbol_type_t {
	STRING,
	NONTERMINAL
};

#define REPEATING_TRUE 0
#define REPEATING_FALSE 1
#define REPEATING_OFFSET 62ULL

tagged_ptr_t tp_pack(void* ptr, uintptr_t is_repeating, enum symbol_type_t type) {
	tagged_ptr_t to_ret = 0;
	to_ret |= (uintptr_t)ptr;
	to_ret |= ((uintptr_t)type << TYPE_OFFSET);
	to_ret |= (is_repeating << REPEATING_OFFSET);
	return to_ret;
}

#define POINTER_MASK 0x0000ffffffffffffULL
void* tp_get_ptr(tagged_ptr_t tp) {
	return (void*)(tp & POINTER_MASK);
}

enum symbol_type_t tp_type(tagged_ptr_t tp) {
	return (tp >> TYPE_OFFSET) & 1;
}

uintptr_t tp_is_repeating(tagged_ptr_t tp) {
	return (tp >> REPEATING_OFFSET) & 1;
}


struct expression_t {
	tagged_ptr_t* symbols;
};

struct symbol_t {
	char* name;
	struct expression_t** expression_options;
};

size_t print_tabs = 0;
void print_expression(struct expression_t* e);
void print_tagged(tagged_ptr_t p);
void print_symbol(struct symbol_t* s);
void print_indent() {for(size_t i = 0; i < print_tabs;i++) {printf("    ");}}

void print_tagged(tagged_ptr_t p) {
	print_indent();

	struct symbol_t* nonterm = NULL;
	if(tp_type(p) == STRING) {
		printf("string val: '%s' (%p)\n", (char*)tp_get_ptr(p), tp_get_ptr(p));
	} else {
		nonterm = tp_get_ptr(p);
		print_symbol(nonterm);
	}


}

void print_expression(struct expression_t* e) {
	for(size_t i = 0; e->symbols[i]; i++) {
		print_tagged(e->symbols[i]);
	}
}

void print_symbol(struct symbol_t* s) {
	printf("symbol name: '<%s>' (%p)\n", s->name, s);
	
	print_tabs++;
	for(size_t i = 0; s->expression_options[i]; i++) {
		print_expression(s->expression_options[i]);
	}
	print_tabs--;
}

int SHOW_INFO = 0;
#define INFO(msg, ...) do { if((SHOW_INFO)) { printf("[+] " msg "\n", ##__VA_ARGS__); } }while(0)
#define FATAL(x, msg, ...) do { if((x)) { printf("[!] " msg "\n", ##__VA_ARGS__); exit(-1); } }while(0)

void zta_dump(void* zta) {
	printf("zta_dump(): %p\n", zta);

	void** arr = (void**)zta;
	for(size_t i = 0; arr[i]; i++) printf("%p, ", arr[i]);
	printf("\n");
}

void* zta_rand_choice(void* zta) {
	void** arr = (void**)zta;
	size_t len = 0;
	while(arr[len]) len++;

	size_t idx = rand() % len;
	//printf("zta_rand_choice(): picking %llu out of %llu\n", idx, len);
	return arr[idx];
}

struct zta_decision {
	void* ptr;
	size_t idx;
};

struct zta_decision zta_rand_decide(void* zta) {
	void** arr = (void**)zta;
	size_t len = 0;
	while(arr[len]) len++;

	size_t idx = rand() % len;
	//printf("zta_rand_choice(): picking %llu out of %llu\n", idx, len);

	struct zta_decision out = {.ptr = arr[idx], .idx = idx};
	return out;
}

void zta_push_back(void* ptr_to_array, void* new_val) {
	//printf("zta_push_back(): pushing %p into %p\n", new_val, ptr_to_array);
	void** arr = *((void***)ptr_to_array);
	if(!arr) arr = calloc(1, sizeof(void*));

	size_t len = 0;
	while(arr[len]) len++;
	//printf("len: %llu\n", len);

	void** temp = realloc(arr, (len + 2) * sizeof(void*));
	//printf("new len: %llu\n", len + 2);
	FATAL(temp == NULL, "zta_push_back(): realloc failed!");
	temp[len] = new_val;
	temp[len + 1] = NULL;

	//zta_dump(temp);
	*((void***)ptr_to_array) = temp;
}

void* zta_dup(void* to_dup) {
	void** arr = (void**)to_dup;

	size_t len = 0;
	while(arr[len]) len++;

	void** new = calloc(1, (len + 2) * sizeof(void*));
	memcpy(new, arr, len * sizeof(void*));
	new[len + 1] = NULL;

	return new;
}

#define MAX_SYMBOLS 0x1000
struct symbol_t symbols[MAX_SYMBOLS] = {0};

struct symbol_t* get_symbol(char* name) {
	for(size_t i = 0; i < MAX_SYMBOLS; i++) {
		if(!symbols[i].name) continue;

		if(!strcmp(name, symbols[i].name) && strlen(name) == strlen(symbols[i].name)) {
			return &symbols[i];
		}
	}

	return NULL;

	//FATAL(1 == 1, "get_symbol(): symbol \'%s\' doesn't exist!", name);
}

struct symbol_t* get_next_empty_slot() {
	for(size_t i = 0; i < MAX_SYMBOLS; i++) {
		if(!symbols[i].name) return &symbols[i];
	}
	

	FATAL(1 == 1, "get_next_empty_slot(): no more slots!");
}

void update_symbol_with_expression(char* name, struct expression_t* e) {
	struct symbol_t* cur_symbol = get_symbol(name);
	if(!cur_symbol) {
		cur_symbol = get_next_empty_slot();
		cur_symbol->name = strdup(name);
	}

	zta_push_back(&cur_symbol->expression_options, e);
}

#define REPEAT_PREFIX "repeat_"
#define MAX_SYMBOL_NAME 0x100
struct expression_t* string_to_expression(char* s) {
	tagged_ptr_t* symbols = NULL;

	tagged_ptr_t cur_symbol;
	char* cur_char = s;
	char* ending_char = NULL;
	char* end_of_line = strchr(cur_char, '\n');
	char cur_symbol_name[MAX_SYMBOL_NAME];
	void* cur_backing = NULL;
	size_t cur_token_len;
	uintptr_t is_repeating = 0;
	while(cur_char < end_of_line) {
		if(*cur_char == '<') {
			cur_char++;

			is_repeating = REPEATING_FALSE;
			if(strncmp(cur_char, REPEAT_PREFIX, strlen(REPEAT_PREFIX)) == 0) {
				is_repeating = REPEATING_TRUE;
				cur_char += strlen(REPEAT_PREFIX);
			}

			ending_char = strchr(cur_char, '>');
			FATAL(end_of_line < ending_char, "string_to_expression(): no closing \'>\' found!\n%s", s);

			cur_token_len = ending_char - cur_char;
			memset(cur_symbol_name, 0, MAX_SYMBOL_NAME);
			memcpy(cur_symbol_name, cur_char, cur_token_len);
			cur_backing = get_symbol(cur_symbol_name);

			if(!cur_backing) {
				update_symbol_with_expression(cur_symbol_name, NULL);
				cur_backing = get_symbol(cur_symbol_name);
			}

			//FATAL(cur_backing == NULL, "string_to_expression(): unknown symbol '%s'", cur_symbol_name);

			cur_symbol = tp_pack(cur_backing, is_repeating, NONTERMINAL);

			zta_push_back(&symbols, (void*)cur_symbol);
			cur_char += cur_token_len + 1;
		} else {
			if(*cur_char == '\r') break;
			if(*cur_char == '\n') break;

			ending_char = strchr(cur_char, '<');
			if(ending_char == NULL) ending_char = end_of_line;
			if(end_of_line < ending_char) ending_char = end_of_line;

			cur_token_len = ending_char - cur_char;
			memset(cur_symbol_name, 0, MAX_SYMBOL_NAME);
			memcpy(cur_symbol_name, cur_char, cur_token_len);
			cur_backing = strdup(cur_symbol_name);

			cur_symbol = tp_pack(cur_backing, REPEATING_FALSE, STRING);
			zta_push_back(&symbols, (void*)cur_symbol);
			cur_char += cur_token_len;
		}
	}
	
	//printf("string_to_expression():\n")
	//for(size_t i = 0; symbols[i]; i++) print_tagged(symbols[i]);

	struct expression_t* to_ret = calloc(1, sizeof(struct expression_t));
	to_ret->symbols = symbols;

	return to_ret;
}

void parse_line(char* line) {
	char sym_name[MAX_SYMBOL_NAME] = {0};

	char* sym_name_start = line + 1;
	char* sym_name_end = strchr(sym_name_start, '>');
	FATAL(*line != '<' && sym_name_end == NULL, "malformed rule line: %s", line);

	memcpy(sym_name, sym_name_start, sym_name_end - sym_name_start);
	sym_name[sym_name_end - sym_name_start + 1] = '\0';

	char* eq = strchr(line, '=');
	FATAL(sym_name_end == eq, "malformed rule line: %s", line);

	char* body_start = eq + 1;
	update_symbol_with_expression(sym_name, string_to_expression(body_start));
}

#define MAX_LINE 0x1000
void parse_file(char* filename) {
	char line[MAX_LINE] = {0};

	FILE* f = fopen(filename, "r");
	FATAL(!f, "unable to open file %s", filename);

	while(fgets(line, MAX_LINE, f)) {
		if(line[0] == '#') continue;
		if(line[0] == '\r') continue;
		if(line[0] == '\n') continue;

		parse_line(line);
	}
}


#define STACK_LIMIT 255
#define SAFE_RANGE 240
//returns array of uinptr_ts, which are state transition choices
uintptr_t* gen_log(struct symbol_t* target) {
	uintptr_t* log = calloc(1, sizeof(*log));
	FATAL(log == NULL, "gen_log(): failed to_ret calloc!");

	size_t sp = 0;
	tagged_ptr_t stack[STACK_LIMIT] = {0};

	stack[sp++] = tp_pack(target, REPEATING_FALSE, NONTERMINAL);

	tagged_ptr_t cur;
	struct symbol_t* cur_sym;
	struct expression_t* cur_expansion;
	struct zta_decision decision;
	size_t i = 0;
	while(sp) {
		cur = stack[--sp];
		//print_tagged(cur);

		if(tp_type(cur) == STRING) continue;

		cur_sym = tp_get_ptr(cur);

		decision = zta_rand_decide(cur_sym->expression_options);
		zta_push_back(&log, (void*)decision.idx + 1); //cannot let idx be stored as 0, since we are using zta

		cur_expansion = decision.ptr;
		while(cur_expansion->symbols[i]) i++;
		if(i + sp > STACK_LIMIT) goto BAIL;

		while(i--) {
			stack[sp++] = cur_expansion->symbols[i];
		}
	}

	return log;

BAIL:
	if(log) free(log);
	return NULL;
}


void print_log(struct symbol_t* target, uintptr_t* log) {
	uintptr_t log_idx = 0;
	size_t sp = 0;
	tagged_ptr_t stack[STACK_LIMIT] = {0};

	stack[sp++] = tp_pack(target, REPEATING_FALSE, NONTERMINAL);

	tagged_ptr_t cur;
	struct symbol_t* cur_sym;
	struct expression_t* cur_expansion;
	uintptr_t decision_idx;
	size_t i = 0;
	while(sp) {
		cur = stack[--sp];

		if(tp_type(cur) == STRING) { printf("%s", (char*)tp_get_ptr(cur)); continue; }

		cur_sym = tp_get_ptr(cur);

		decision_idx = log[log_idx++];

		cur_expansion = cur_sym->expression_options[decision_idx - 1];
		while(cur_expansion->symbols[i]) i++;
		if(i + sp > STACK_LIMIT) return;

		while(i--) {
			stack[sp++] = cur_expansion->symbols[i];
		}
	}
}

uintptr_t* mut_log(struct symbol_t* target, uintptr_t* old_log) {
	uintptr_t* new_log = zta_dup(old_log);
	struct zta_decision new_start = zta_rand_decide(new_log);
	INFO("changing choice at %d", new_start.idx);
	new_log[new_start.idx] = 0;

	uintptr_t log_idx = 0;
	size_t sp = 0;
	tagged_ptr_t stack[STACK_LIMIT] = {0};

	stack[sp++] = tp_pack(target, REPEATING_FALSE, NONTERMINAL);

	tagged_ptr_t cur;
	struct symbol_t* cur_sym;
	struct expression_t* cur_expansion;
	struct zta_decision decision;
	size_t i = 0;
	uintptr_t decision_idx = 0;
	while(sp) {
		cur = stack[--sp];

		if(tp_type(cur) == STRING) continue;

		cur_sym = tp_get_ptr(cur);

		if(log_idx < new_start.idx) {
			decision_idx = new_log[log_idx++];

			cur_expansion = cur_sym->expression_options[decision_idx - 1];
		} else {
			decision = zta_rand_decide(cur_sym->expression_options);
			zta_push_back(&new_log, (void*)decision.idx + 1); //cannot let idx be stored as 0, since we are using zta
								      
			cur_expansion = decision.ptr;
		}

		while(cur_expansion->symbols[i]) i++;
		if(i + sp > STACK_LIMIT) goto BAIL;

		while(i--) {
			stack[sp++] = cur_expansion->symbols[i];
		}
	}

	return new_log;

BAIL:
	if(new_log) free(new_log);
	return NULL;
}

void init_constants() {
	char* constants[][2] = {
		{"lt", "<"},
		{"gt", ">"},
		{"hash", "#"},
		{"cr", "\x0d"},
		{"lf", "\x0a"},
		{"crlf", "\x0d\x0a"},
		{"space", " "},
		{"tab", "\t"}
	};

	struct expression_t* cur_expression = NULL;
	tagged_ptr_t* symbols = NULL;
	tagged_ptr_t cur_symbol = 0;
	char* name = NULL;
	char* body = NULL;
	for(size_t i = 0; i < 7; i++) {
		symbols = NULL;
		name = constants[i][0];
		body = constants[i][1];

		cur_symbol = tp_pack(strdup(body), REPEATING_FALSE, STRING);
		zta_push_back(&symbols, (void*)cur_symbol);

		cur_expression = calloc(1, sizeof(*cur_expression));
		cur_expression->symbols = symbols;

		update_symbol_with_expression(name, cur_expression);
	}
}

int main(int argc, char** argv) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	srand(ts.tv_nsec ^ ts.tv_sec);

	FATAL(argc < 2, "usage: %s <grammar file>", argv[0]);

	init_constants();
	parse_file(argv[1]);


	uintptr_t* log = gen_log(get_symbol("root"));

	printf("//original:\n");
	print_log(get_symbol("root"), log);
	printf("\n\n\n");

	uintptr_t* new_log = NULL;
	while(1) {
		new_log = mut_log(get_symbol("root"), log);
		if(!new_log) continue;

		print_log(get_symbol("root"), new_log);

		getchar();
		
		free(log);
		log = new_log;

		printf("\n\n\n");
	}

}
