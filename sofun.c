#include <dlfcn.h>
#include <err.h>
#include <ffi.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

typedef struct {
	ffi_type **data;
	size_t size;
	size_t max;
} type_l;

typedef struct {
	void **data;
	size_t size;
	size_t max;
} value_l;

#define l_pushback(l, i)\
	do {\
		if ((l)->max <= (l)->size) {\
			if ((l)->max == 0) (l)->max = 8;\
			(l)->max *= 2;\
			(l)->data = realloc((l)->data, sizeof(*(l)->data)*(l)->max);\
		}\
		(l)->data[(l)->size++] = i;\
	} while (0)\

#define l_clear(l) (l)->size = 0

int
main(int argc, char *argv[])
{
	char *exec_name = argv[0], *so_path;
	void *handle, *fun_ptr;
	char line[4096] = {0}, l_storage[4096] = {0};
	stb_lexer l = {0};
	ffi_cif cif = {0};
	type_l ts = {0};
	value_l vs = {0};

	if (argc<2) errx(EXIT_FAILURE, "missing shared object file");

	so_path = argv[1];
	handle = dlopen(so_path, RTLD_NOW);
	if (!handle) errx(EXIT_FAILURE, "%s", dlerror());

	#ifdef DEBUG
	printf("opened %s shared object at %p\n", so_path, handle);
	#endif

	for (;;) {
		loop:
		printf("> ");

		size_t line_size = sizeof(line);
		if (!fgets(line, line_size, stdin)) break;

		stb_c_lexer_init(&l, line, line+line_size, l_storage, sizeof(l_storage));

		/* symbol name */
		if (!stb_c_lexer_get_token(&l) || l.token!=CLEX_id) continue;
		if (!strcmp(l.string, "exit")) break;
		if (!(fun_ptr = dlsym(handle, l.string))) {
			warnx("symbol \"%s\" not found", l.string);
			continue;
		}

		#ifdef DEBUG
		if (fun_ptr) printf("\"%s\" at %p\n", l.string, fun_ptr);
		#endif

		l_clear(&vs);
		l_clear(&ts);

		while (stb_c_lexer_get_token(&l)) {
			if (!l.token) break;
			switch (l.token) {
			case CLEX_intlit:
				l_pushback(&ts, &ffi_type_uint32);
				int *i = malloc(sizeof(int));
				*i = l.int_number;
				l_pushback(&vs, i);
				break;

			case CLEX_dqstring:
				l_pushback(&ts, &ffi_type_pointer);
				char **s = malloc(sizeof(char *));
				*s = strdup(l.string);
				l_pushback(&vs, s);
				break;
		
			case CLEX_floatlit:
				l_pushback(&ts, &ffi_type_float);
				float *f = malloc(sizeof(float));
				*f = l.real_number;
				l_pushback(&vs, f);
				break;

			default:
				warnx("token type not implemented");
				goto loop;
			}
		}

		if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, ts.size, &ffi_type_void, ts.data) != FFI_OK) {
			warnx("unmatched signature or abi");
			continue;
		}

		ffi_call(&cif, fun_ptr, NULL, vs.data); 
	}

	return EXIT_SUCCESS;
}
